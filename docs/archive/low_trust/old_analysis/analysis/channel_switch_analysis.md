# MT6639/MT7927 Channel Switching & TX Path Analysis

## 问题现状 (2026-02-16)

**现象**: wpa_supplicant 发起连接时，auth 帧通过 DMA 提交成功，但固件**立即丢弃**帧 (TXFREE stat=1, count=15)，auth timeout，无法建立连接。

**已尝试方案**:
1. ✅ `mgd_prepare_tx()` ROC (Remain on Channel) — **失败**，固件仍丢弃
2. ✅ `UNI_CHANNEL_SWITCH` (CID=0x09) — **失败**，固件仍丢弃
3. ✅ `BSS_INFO_RLM` TLV (在 `bss_info_changed()` 时发送) — **失败**

**关键疑问**: MT6639 固件是否需要**特定的 MCU 命令序列**来配置 TX 信道，或者是否缺少某个关键的 BSS/STA 状态更新？

---

## MT7925 连接流程分析 (参考驱动)

### 1. mgd_prepare_tx 的作用

**调用时机**: mac80211 在发送 auth/assoc 管理帧**之前**调用此回调。

**MT7925 实现** (`mt76/mt7925/main.c:1814-1828`):
```c
static void mt7925_mgd_prepare_tx(struct ieee80211_hw *hw,
                                   struct ieee80211_vif *vif,
                                   struct ieee80211_prep_tx_info *info)
{
    struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
    struct mt792x_dev *dev = mt792x_hw_dev(hw);
    u16 duration = info->duration ? info->duration : jiffies_to_msecs(HZ);

    mt792x_mutex_acquire(dev);
    mt7925_set_roc(mvif->phy, &mvif->bss_conf,
                   mvif->bss_conf.mt76.ctx->def.chan, duration,
                   MT7925_ROC_REQ_JOIN);  // ← 关键: 使用 JOIN 类型
    mt792x_mutex_release(dev);
}
```

**关键发现**:
- MT7925 使用 `MCU_UNI_CMD_ROC` (CID=0x27) 命令
- `reqtype = MT7925_ROC_REQ_JOIN` (值=0) — **不是** `MT7925_ROC_REQ_ROC` (值=1)
- `dbdcband = 0xfe` (表示 BAND_ALL with DBDC enabled)
- 固件会返回 `UNI_EVENT_ROC_GRANT` 事件确认信道已切换
- `mgd_complete_tx()` 时发送 `UNI_ROC_ABORT` 命令取消 ROC

**ROC TLV 结构** (完整参数):
```c
struct roc_acquire_tlv {
    __le16 tag;                 // UNI_ROC_ACQUIRE = 0
    __le16 len;
    u8 bss_idx;
    u8 tokenid;                 // 每次递增的 token
    u8 control_channel;         // chan->hw_value
    u8 sco;                     // Secondary Channel Offset (0/1/3)
    u8 band;                    // 1=2.4G, 2=5G, 3=6G
    u8 bw;                      // CMD_CBW_20MHZ = 0
    u8 center_chan;             // = control_channel (20MHz)
    u8 center_chan2;            // 0 (80+80MHz only)
    u8 bw_from_ap;              // = bw
    u8 center_chan_from_ap;     // = center_chan
    u8 center_chan2_from_ap;    // 0
    u8 reqtype;                 // 0=JOIN, 1=ROC
    u8 dbdcband;                // 0xfe=BAND_ALL, 0xff=BAND_AUTO
    u8 rsv[2];
    __le32 maxinterval;         // 持续时间 (ms)
} __packed;
```

**MT7927 当前实现问题**:
- ✅ 已实现 `mgd_prepare_tx()` + ROC 命令 (src/mt7927_pci.c:2881-2954)
- ✅ 参数格式与 mt7925 一致
- ❌ **但固件仍丢弃 auth 帧** → 说明 ROC 本身不足以解决问题

---

### 2. BSS_INFO_RLM 的作用

**调用时机**: 在 `sta_state()` 的 `NOTEXIST→NONE` 和 `AUTH→ASSOC` 转换时。

**MT7925 实现** (`mt76/mt7925/mcu.c:2283-2352`):
```c
static void mt7925_mcu_bss_rlm_tlv(struct sk_buff *skb, struct mt76_phy *phy,
                                   struct ieee80211_bss_conf *link_conf,
                                   struct ieee80211_chanctx_conf *ctx)
{
    struct cfg80211_chan_def *chandef = ctx ? &ctx->def : &link_conf->chanreq.oper;
    struct bss_rlm_tlv *req;
    struct tlv *tlv;

    tlv = mt76_connac_mcu_add_tlv(skb, UNI_BSS_INFO_RLM, sizeof(*req));
    req = (struct bss_rlm_tlv *)tlv;
    req->control_channel = chandef->chan->hw_value;
    req->center_chan = ieee80211_frequency_to_channel(chandef->center_freq1);
    req->bw = CMD_CBW_20MHZ;
    req->tx_streams = hweight8(phy->antenna_mask);
    req->rx_streams = hweight8(phy->antenna_mask);
    req->band = 1;  // 根据 chan->band 设置 1=2.4G, 2=5G, 3=6G
    req->sco = 0;   // Secondary Channel Offset
    req->ht_op_info = 4;  // HT 40M allowed
}
```

**发送时机** (mt7925/main.c):
1. **sta_add (NOTEXIST→NONE)** 之前:
   ```c
   mt7925_mcu_add_bss_info(phy, ctx, link_conf, link_sta, false);
   ```
2. **sta_assoc (AUTH→ASSOC)** 之前:
   ```c
   mt7925_mcu_add_bss_info(phy, ctx, link_conf, link_sta, true);
   ```
3. **bss_info_changed (BSS_CHANGED_ASSOC)** 时:
   ```c
   if (changed & BSS_CHANGED_ASSOC)
       mt7925_mcu_add_bss_info(...);
   ```

**BSS_INFO_UPDATE 完整结构**:
```c
struct bss_info_req {
    struct bss_req_hdr hdr;        // bss_idx = mvif->idx
    struct bss_info_basic basic;   // tag=0, active/bssid/...
    struct bss_rlm_tlv rlm;        // tag=2, channel config
    // ... 其他可选 TLVs (MBSSID, EHT, etc)
} __packed;
```

**MT7927 当前实现**:
- ✅ `bss_info_changed()` 在 `BSS_CHANGED_BSSID` 时发送 `BSS_INFO_UPDATE`
- ✅ 包含 `BSS_INFO_RLM` TLV (src/mt7927_pci.c:2190-2211)
- ✅ 信道参数正确 (control_channel, band, bw)
- ❌ **但固件仍丢弃 auth 帧** → BSS_INFO 配置**不足以启用 TX**

---

### 3. STA_REC_UPDATE 的作用

**关键发现**: MT7925 在 `sta_state()` 转换时发送 `STA_REC_UPDATE` (CID=0x19)，包含**完整的 WTBL 配置**。

#### sta_add (NOTEXIST→NONE) 时

**调用路径**: `mt76_sta_state()` → `mt76_sta_add()` → `mt7925_mac_link_sta_add()`

**操作顺序** (mt76/mt7925/main.c:851-900):
```c
1. 分配 WCID (WTBL index)
2. mt7925_mac_wtbl_update(idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR)
3. mt7925_mcu_add_bss_info(..., link_sta, false)  // ← BSS_INFO 先行
4. mt7925_mcu_sta_update(..., true, MT76_STA_INFO_STATE_NONE)
```

**STA_REC_UPDATE TLVs** (NOTEXIST→NONE):
- `STA_REC_BASIC` (tag=0x00): `conn_state=PORT_SECURE`, `extra_info=VER|NEW`, `conn_type=INFRA_AP`
- `STA_REC_PHY` (tag=0x15): 带宽、NSS、LDPC 等
- `STA_REC_HT/VHT/HE/EHT` (tag=0x02/0x03/0x11/0x22): 能力协商
- `STA_REC_STATE` (tag=未知): `state=MT76_STA_INFO_STATE_NONE`
- `STA_REC_HDR_TRANS` (tag=未知): `dis_rx_hdr_tran=true`, `to_ds=true`

**注意**: MT7925 **不包含 STA_REC_WTBL** TLV！WTBL 通过独立的寄存器操作 `mt7925_mac_wtbl_update()` 更新。

#### sta_assoc (AUTH→ASSOC) 时

**调用路径**: `mt76_sta_state()` → `dev->drv->sta_event(MT76_STA_EVENT_ASSOC)` → `mt7925_mac_link_sta_assoc()`

**操作顺序** (mt76/mt7925/main.c:1024-1055):
```c
1. mt7925_mcu_add_bss_info(..., link_sta, true)  // ← 关联后 BSS_INFO
2. mt7925_mac_wtbl_update(idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR)
3. mt7925_mcu_sta_update(..., true, MT76_STA_INFO_STATE_ASSOC)
```

**关键差异**:
- `STA_REC_BASIC.extra_info = VER` (不含 NEW，因为 `newly=false`)
- `STA_REC_STATE.state = MT76_STA_INFO_STATE_ASSOC` (从 0 变为 2)
- 新增 `STA_REC_MLD` 和 `STA_REC_EHT_MLD` TLVs (仅 MLO 模式)

**MT7927 当前实现**:
- ✅ `sta_state()` 在 `NOTEXIST→NONE` 时调用 `mt7927_mcu_sta_update()`
- ✅ 包含 `STA_REC_BASIC`, `STA_REC_HDR_TRANS`, `STA_REC_WTBL` TLVs
- ❌ **缺少 `sta_assoc()` 路径** — AUTH→ASSOC 转换时**没有**更新 STA_REC
- ❌ **缺少 `BSS_INFO_UPDATE` 在 sta_add 之前** — MT7925 顺序是 BSS→STA，我们是 STA→BSS

---

### 4. UNI_CHANNEL_SWITCH 的作用

**MT7925 不使用 UNI_CHANNEL_SWITCH (CID=0x09)！**

搜索 `mt76/mt7925/` 全部代码，**没有**发送 `MCU_UNI_CMD(CHANNEL_SWITCH)` 的地方。

**MT7996 使用场景** (mt76/mt7996/mcu.c:817):
```c
// 仅在 AP 模式或 monitor 模式下使用
static int mt7996_mcu_set_chan_info(struct mt7996_phy *phy, u16 tag)
{
    struct cfg80211_chan_def *chandef = &phy->mt76->chandef;
    // ...
    if (tag == UNI_CHANNEL_SWITCH) {
        // 设置 rx_path, tx_path_num 等射频链路参数
    }
    // ...
    return mt76_mcu_send_msg(&dev->mt76, MCU_UNI_CMD(CHANNEL_SWITCH), ...);
}
```

**结论**: `UNI_CHANNEL_SWITCH` 是 **AP/monitor 模式**的射频配置命令，**STA 模式不需要**。MT7925 STA 模式完全通过 `ROC` + `BSS_INFO_RLM` 配置信道。

**MT7927 实验失败原因**: 我们在 `mgd_prepare_tx()` 中同时发送了 `CHANNEL_SWITCH` + `ROC`，但这是**错误的组合** — STA 模式应该只用 ROC。

---

## MT6639 Android 驱动分析

### 信道切换机制 (rlm + cnm 模块)

**MT6639 Android 驱动**使用 **CNM (Channel Manager)** 模块管理信道切换，关键函数:

1. **`cnmChMngrRequestPrivilege()`** (mt6639/mgmt/cnm.c):
   - 请求在特定信道上发送帧的权限
   - 用于 scan、auth、assoc 等需要切换信道的场景

2. **`rlmReqRf()`** (mt6639/mgmt/rlm.c):
   - 向固件发送射频配置请求
   - 包含 band, channel, bandwidth 等参数

3. **`aisUpdateBssInfoForJOIN()`** (mt6639/mgmt/ais_fsm.c:4132-4302):
   - 关联成功后更新 BSS_INFO
   - 包含 `ePrimaryChannel`, `eBand`, `ucVhtChannelWidth` 等信道参数

**关键差异**: Android 驱动是**全软件栈** (包含 MLME 状态机)，Linux mac80211 驱动的 MLME 在 mac80211 层，驱动只负责**硬件抽象**。

**Android 驱动的信道切换流程**:
```
scan → aisFsmScanRequest() → cnmChMngrRequestPrivilege(REQ_TYPE_SCAN)
     → 固件切换到扫描信道 → 返回 scan results
     → aisFsmStateJoin() → aisUpdateBssInfoForJOIN() → 设置 BSS 信道
     → saaFsmSendAuth() → cnmChMngrRequestPrivilege(REQ_TYPE_JOIN)
     → 固件切换到 AP 信道 → 发送 auth 帧
```

**Linux mac80211 驱动的信道切换流程**:
```
hw_scan → MCU_UNI_CMD_HW_SCAN → 固件自动管理扫描信道
        → scan_completed → mac80211 选择 BSS
        → bss_info_changed(BSS_CHANGED_BSSID) → BSS_INFO_UPDATE
        → mgd_prepare_tx() → ROC (JOIN 类型) → 固件切换到 AP 信道
        → mac80211_tx(auth) → TX DMA → 固件发送
```

**关键问题**: MT6639 固件是否**期望** Android 驱动的 `cnmChMngrRequestPrivilege()` 序列，而我们的 `ROC` 命令不足以触发信道切换？

---

## 可能的根本原因

### 假设 1: BSS_INFO_RLM 与 ROC 的时序问题

**MT7925 序列**:
```
1. bss_info_changed(BSS_CHANGED_BSSID)
   └→ mt7925_mcu_add_bss_info(..., false)  // BSS_INFO_RLM with channel
2. mgd_prepare_tx()
   └→ mt7925_set_roc(..., JOIN)            // ROC to same channel
3. mac80211_tx(auth)
   └→ tx_prepare_skb() → DMA submit
4. 固件在 ROC 信道上发送 auth 帧
```

**MT7927 当前序列**:
```
1. bss_info_changed(BSS_CHANGED_BSSID)
   └→ mt7927_mcu_add_bss_info(vif, true)   // BSS_INFO_RLM with channel
2. mgd_prepare_tx()
   └→ mt7927_mcu_set_chan_info(CHANNEL_SWITCH)  // ← 多余! STA 模式不需要
   └→ mt7927_mcu_send_unicmd(ROC)                // ROC JOIN
3. mac80211_tx(auth)
   └→ mt7927_tx_prepare_skb() → DMA submit
4. ❌ 固件丢弃 auth 帧 (TXFREE stat=1, count=15)
```

**可能的 Bug**:
- `CHANNEL_SWITCH` 命令可能**干扰** ROC 的信道配置
- MT6639 固件可能有内部状态冲突: "CHANNEL_SWITCH 说要配置射频，ROC 说只是临时切信道"

**建议修复**:
1. **移除 `mgd_prepare_tx()` 中的 `mt7927_mcu_set_chan_info(CHANNEL_SWITCH)` 调用**
2. 只保留 `ROC (JOIN)` 命令
3. 确保 `BSS_INFO_RLM` 在 ROC 之前发送

---

### 假设 2: 缺少 STA_REC_UPDATE at sta_assoc

**MT7925 关键序列** (AUTH→ASSOC 转换):
```c
mt7925_mac_link_sta_assoc() {
    // 1. 先更新 BSS_INFO (enable=true)
    mt7925_mcu_add_bss_info(phy, ctx, link_conf, link_sta, true);

    // 2. 清空 WTBL admission count
    mt7925_mac_wtbl_update(idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR);

    // 3. 更新 STA_REC state=ASSOC
    mt7925_mcu_sta_update(dev, link_sta, vif, true, MT76_STA_INFO_STATE_ASSOC);
}
```

**MT7927 当前实现**:
- `sta_state()` 在 **AUTH→ASSOC 转换时什么都不做**！
- 只在 NOTEXIST→NONE 时调用 `mt7927_mcu_sta_update()`
- 固件可能认为 STA 还在 `STATE_NONE`，不允许发送数据帧

**可能的 Bug**:
- 固件需要 `STA_REC_STATE = ASSOC` 才允许 TX
- 但我们在 **auth 阶段**就尝试发送，此时 STA state 还是 `NONE`
- 固件检查发现 state 不对，直接丢弃帧

**建议修复**:
1. 在 `sta_state()` 中添加 `IEEE80211_STA_AUTH → IEEE80211_STA_ASSOC` 分支
2. 调用 `mt7927_mcu_add_bss_info(vif, true)` 和 `mt7927_mcu_sta_update(..., STATE_ASSOC)`

---

### 假设 3: 缺少 ROC_GRANT 事件确认

**MT7925 ROC 流程**:
```c
mt7925_set_roc() {
    phy->roc_grant = false;
    err = mt7925_mcu_set_roc(...);  // 发送 ROC 命令

    // ← 等待固件返回 UNI_EVENT_ROC_GRANT 事件
    if (!wait_event_timeout(phy->roc_wait, phy->roc_grant, 4 * HZ)) {
        dev_err(..., "ROC request timeout");
        return -ETIMEDOUT;
    }
}

// RX event 处理
mt7925_mcu_rx_event(UNI_EVENT_ROC_GRANT) {
    phy->roc_grant = true;
    wake_up(&phy->roc_wait);
    // 信道切换完成，可以发送管理帧了
}
```

**MT7927 当前实现**:
- `mgd_prepare_tx()` 发送 ROC 命令后**立即返回**
- **不等待** `UNI_EVENT_ROC_GRANT` 事件
- mac80211 可能在固件还没切换信道时就发送了 auth 帧

**可能的 Bug**:
- auth 帧到达 TX queue 时，固件还在**扫描信道** (上一次 scan 的残留)
- 固件检查发现"当前信道不是目标 AP 信道"，丢弃帧

**建议修复**:
1. 实现 `UNI_EVENT_ROC_GRANT` 事件处理 (在 `mt7927_mac.c` RX event 分发中)
2. `mgd_prepare_tx()` 等待 `roc_grant` 标志，超时返回错误

---

### 假设 4: MT6639 固件不支持 ROC 机制

**最悲观假设**: MT6639 固件 (Android 芯片) 从未实现 `MCU_UNI_CMD_ROC` 命令！

**证据**:
- MT6639 Android 驱动使用 `cnmChMngrRequestPrivilege()` — 这是**软件层**的信道管理
- Android 驱动**从不**发送 `MCU_UNI_CMD_ROC` 命令
- MT7925 (WiFi 6E PCIe 芯片) 才引入 ROC 机制，MT6639 (移动芯片) 可能不支持

**如果成立，解决方案**:
- 移除 `mgd_prepare_tx()` / `mgd_complete_tx()` 回调
- 完全依赖 `BSS_INFO_RLM` 配置信道
- 假设固件在收到 TX 帧时**自动切换**到 `BSS_INFO_RLM` 配置的信道

**但这引入新问题**: scan 完成后，固件可能还在扫描的最后一个信道 (例如 channel 161)，而 auth 帧需要发送到 AP 信道 (例如 channel 36)。如果固件不自动切换，auth 帧会在错误信道发送 → 无响应。

---

## 实验方案

### 实验 A: 移除 CHANNEL_SWITCH，只保留 ROC

**修改** (src/mt7927_pci.c `mgd_prepare_tx()`):
```c
static void mt7927_mgd_prepare_tx(...) {
    // ... 构建 ROC 请求 ...

    // ❌ 删除此行
    // mt7927_mcu_set_chan_info(dev, chan, UNI_CHANNEL_SWITCH);

    // ✅ 只发送 ROC
    mt7927_mcu_send_unicmd(dev, 0x27, UNI_CMD_OPT_SET_ACK, &req, sizeof(req));
}
```

**预期**:
- 如果 CHANNEL_SWITCH 干扰 ROC，移除后应该能正常发送 auth 帧
- 如果仍失败，说明问题不在 CHANNEL_SWITCH

---

### 实验 B: 实现 ROC_GRANT 等待

**修改**:
1. 在 `struct mt7927_dev` 中添加:
   ```c
   wait_queue_head_t roc_wait;
   bool roc_grant;
   ```
2. 在 `mt7927_mac.c` RX event 处理中添加 `UNI_EVENT_ROC_GRANT` (tag=0) 分支:
   ```c
   if (tag == 0) {  // UNI_EVENT_ROC_GRANT
       dev->roc_grant = true;
       wake_up(&dev->roc_wait);
   }
   ```
3. 在 `mgd_prepare_tx()` 中等待:
   ```c
   dev->roc_grant = false;
   mt7927_mcu_send_unicmd(...);
   if (!wait_event_timeout(dev->roc_wait, dev->roc_grant, HZ)) {
       dev_err(..., "ROC grant timeout");
   }
   ```

**预期**:
- 如果固件支持 ROC，应该收到 `ROC_GRANT` 事件
- 如果超时，说明固件**不支持** ROC 机制

---

### 实验 C: 移除 ROC，只保留 BSS_INFO_RLM

**修改**:
1. 完全移除 `mgd_prepare_tx()` 和 `mgd_complete_tx()` 回调
2. 只在 `bss_info_changed(BSS_CHANGED_BSSID)` 时发送 `BSS_INFO_UPDATE` (已实现)
3. 假设固件在 TX 帧时**自动切换**到 BSS_INFO_RLM 配置的信道

**预期**:
- 如果 MT6639 固件不支持 ROC，这是唯一可行方案
- 如果固件不自动切换信道，auth 帧会在错误信道发送 → 仍然失败

---

### 实验 D: 添加 sta_assoc 路径

**修改** (src/mt7927_pci.c `mt7927_sta_state()`):
```c
static int mt7927_sta_state(...) {
    // ... 现有 NOTEXIST→NONE 代码 ...

    // 新增: AUTH → ASSOC 转换
    if (old_state == IEEE80211_STA_AUTH &&
        new_state == IEEE80211_STA_ASSOC) {
        // 1. 更新 BSS_INFO (enable=true)
        mt7927_mcu_add_bss_info(dev, vif, true);

        // 2. 更新 STA_REC (state=ASSOC)
        mt7927_mcu_sta_update(dev, vif, sta, true,
                              MT76_STA_INFO_STATE_ASSOC);
    }

    return 0;
}
```

**预期**:
- 如果固件需要 `STA_REC_STATE=ASSOC` 才允许 TX，此修改应该解决问题
- 但注意: **auth 阶段还是 STATE_NONE**，如果固件在 auth 时就检查 state，此修改无效

---

## 建议的调试步骤

1. **实验 A + B 组合**: 移除 CHANNEL_SWITCH + 实现 ROC_GRANT 等待
   - 验证 MT6639 固件是否支持 ROC 机制
   - 如果收到 `ROC_GRANT` 事件 → 说明 CHANNEL_SWITCH 是干扰源
   - 如果超时 → 说明固件不支持 ROC，转实验 C

2. **实验 C**: 移除 ROC，完全依赖 BSS_INFO_RLM
   - 简化到最小配置
   - 观察 TXFREE 的 stat 和 count 是否改变

3. **实验 D**: 添加 sta_assoc 路径
   - 完善 STA state 转换逻辑
   - 即使不解决 auth 问题，也为后续 assoc 阶段做准备

4. **dmesg 日志分析**:
   - 监控 `mgd_prepare_tx` 的 "ROC acquire" 日志
   - 搜索是否有 `UNI_EVENT_ROC_GRANT` 事件到达 (添加 debug 打印)
   - 记录 auth 帧发送时的时间戳，对比 ROC 命令发送时间

5. **固件事件抓取**:
   - 在 `mt7927_mac.c` RX event 处理中添加 hexdump
   - 查看 ROC 命令后是否有**任何**响应事件 (即使 tag 不是 0)

---

## 参考代码位置

### MT7925 驱动
- **ROC 实现**: `mt76/mt7925/main.c:503-585` (`mt7925_set_roc`, `mt7925_abort_roc`)
- **ROC 事件处理**: `mt76/mt7925/mcu.c:323-339` (`mt7925_mcu_handle_roc_grant`)
- **mgd_prepare_tx**: `mt76/mt7925/main.c:1814-1828`
- **BSS_INFO_RLM**: `mt76/mt7925/mcu.c:2283-2352` (`mt7925_mcu_bss_rlm_tlv`)
- **sta_state 流程**: `mt76/mt7925/main.c:851-1055` (`sta_add`, `sta_assoc`)

### MT6639 Android 驱动
- **信道管理**: `mt6639/mgmt/cnm.c` (`cnmChMngrRequestPrivilege`)
- **RLM 射频管理**: `mt6639/mgmt/rlm.c` (`rlmReqRf`)
- **连接流程**: `mt6639/mgmt/ais_fsm.c` (`aisUpdateBssInfoForJOIN`)

### MT7927 当前驱动
- **ROC 实现**: `src/mt7927_pci.c:2881-2983` (包含错误的 CHANNEL_SWITCH 调用)
- **BSS_INFO_UPDATE**: `src/mt7927_pci.c:2130-2220` (`mt7927_mcu_add_bss_info`)
- **STA_REC_UPDATE**: `src/mt7927_pci.c:2230-2408` (`mt7927_mcu_sta_update`)
- **sta_state**: `src/mt7927_pci.c:2537-2575` (缺少 AUTH→ASSOC 路径)

---

## 结论

**最可能的根本原因**: MT6639 固件**不支持** `MCU_UNI_CMD_ROC` 机制 (Android 驱动从未使用)，或者需要**不同的信道切换命令序列**。

**推荐的修复顺序**:
1. ✅ **立即**: 移除 `mgd_prepare_tx()` 中的 `CHANNEL_SWITCH` 调用 (实验 A)
2. ✅ **中期**: 实现 `UNI_EVENT_ROC_GRANT` 等待，验证固件是否支持 ROC (实验 B)
3. ✅ **后备方案**: 如果 ROC 不支持，移除 ROC 回调，完全依赖 BSS_INFO_RLM (实验 C)
4. ✅ **并行**: 添加 sta_assoc 路径的 STA_REC_UPDATE (实验 D)

**如果所有实验都失败**: 需要使用逻辑分析仪抓取 Windows 驱动的 PCIe 事务，查看 Windows 在 auth 之前发送了哪些 MCU 命令。
