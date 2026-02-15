# Windows MT6639/MT7927 驱动 TX/连接流程逆向分析

**日期**: 2026-02-16
**分析者**: researcher agent
**来源**: 现有 Ghidra RE 文档 + Windows 驱动 v5603998/v5705275 + mt7925 参考代码 + MT6639 Android 驱动

---

## 一、核心结论

### 现有文档不覆盖连接流程

**关键发现**: 现有 Windows RE 文档主要覆盖了 **初始化阶段** (PreFwDownloadInit → FWDL → PostFwDownloadInit) 和 MCU 命令发送机制。**没有**任何文档分析 Windows 驱动从 scan 到 auth 到 assoc 的完整连接流程。

这是因为 Ghidra 逆向工作集中在启动/初始化路径上，Windows 的 WLAN 连接逻辑在更上层 (NDIS/WDI 框架)，涉及大量回调函数，逆向难度更高。

### 但可以从已知信息推断

根据以下三个来源的交叉分析，可以推断出 auth 帧被固件丢弃的可能原因：

1. **Windows RE**: PostFwDownloadInit MCU 命令序列 (已知的 CID)
2. **mt7925 参考驱动**: sta_state 转换 + MCU 命令序列
3. **MT6639 Android 驱动**: 信道管理 + BSS/STA 状态机
4. **当前驱动 dmesg**: TXFREE stat=1, count=15 = 零重试就丢弃

---

## 二、Windows 驱动 PostFwDownloadInit 完整 MCU 命令序列

根据 Ghidra 逆向 (ghidra_post_fw_init.md + analysis_windows_full_init.md):

| 步骤 | CID/class | target | 描述 | 我们的驱动 |
|------|-----------|--------|------|-----------|
| 1 | (寄存器) | - | DMASHDL 0xd6060 \|= 0x10101 | ✅ 已实现 |
| 2 | 0x8a | 0xed | NIC_CAP (查询固件能力) | ✅ 已实现 |
| 3 | 0x02 | 0xed | Config: {tag=1, pad=0, value=0x70000} | ✅ 已实现 |
| 4 | 0xc0 | 0xed | Config: {0x820cc800, 0x3c200} | ✅ 已实现 |
| 5 | 0xed | 0xed | DownloadBufferBin (可选，subcmd=0x21) | ❌ 未实现 (可选) |
| 6 | 0x28 | 0xed | DBDC 设置 (MT6639/MT7927 专属) | ✅ 已实现 |
| 7 | - | - | 1ms 延迟 | ✅ 已实现 |
| 8 | 0xca | 0xed | SetPassiveToActiveScan | ✅ 已实现 (ScanConfig) |
| 9 | 0xca | 0xed | SetFWChipConfig | ✅ 已实现 (ChipConfig) |
| 10 | 0xca | 0xed | SetLogLevelConfig | ✅ 已实现 (LogConfig) |

**结论**: PostFwDownloadInit 已基本完整实现。**缺失的步骤不在这里。**

---

## 三、Windows 驱动 MCU 命令路由表 (CONNAC3)

从 Ghidra RE 文档中提取的路由表 (57 条目 @ 0x14023fcf0):

| Index | CID/class | 功能 | 备注 |
|-------|-----------|------|------|
| 0 | 0x8a | NIC_CAP | PostFwInit 步骤 2 |
| 1 | 0x02 | Config | PostFwInit 步骤 3 |
| 2 | 0xc0 | Config | PostFwInit 步骤 4 |
| 3 | 0x28 | DBDC | PostFwInit 步骤 6 |
| 4 | 0xca | ScanConfig/ChipConfig/LogConfig | PostFwInit 步骤 8-10 |
| ... | ... | 其他命令 (连接相关) | 未详细分析 |

### 已知的 MCU 命令 CID 映射 (从 mt7925 参考)

| CID | 名称 | 用途 | 我们的驱动 |
|-----|------|------|-----------|
| 0x01 | DEV_INFO_UPDATE | 激活/停用虚拟接口 | ✅ 已实现 |
| 0x02 | BSS_INFO_UPDATE | BSS 配置 (信道、BSSID 等) | ✅ 已实现 |
| 0x03 | STA_REC_UPDATE | STA 记录 (WTBL 条目) | ✅ 已实现 |
| 0x05 | CHANNEL_SWITCH | 信道切换 (AP 模式) | ⚠️ 已实现但可能不需要 |
| 0x09 | HW_SCAN | 硬件扫描 | ✅ 已实现 |
| 0x0a | SET_DOMAIN_INFO | 国家码设置 | ✅ 已实现 |
| 0x19 | STA_REC_UPDATE (新) | mt7925 格式 | ⚠️ CID 冲突待确认 |
| 0x27 | ROC | Remain on Channel | ✅ 已实现 |
| 0x28 | DBDC | DBDC 设置 | ✅ 已实现 |
| 0x8a | NIC_CAP | 固件能力查询 | ✅ 已实现 |
| 0xca | MISC_CONFIG | 扫描/芯片/日志配置 | ✅ 已实现 |

---

## 四、Auth 帧被丢弃的根本原因分析

### TXFREE stat=1, count=15 的含义

- **stat=1**: 固件主动丢弃 (不是 DMA 错误、不是信道忙、不是 ACK 超时)
- **count=15**: TX count 寄存器的初始值 = 最大重试次数，但 0 次重试就丢弃 = **固件根本没尝试发送**
- **含义**: 固件认为"没有理由发送这个帧"或"条件不满足"

### 最可能的根本原因 (按可能性排序)

#### 原因 1: 固件不在正确的信道上 (90% 可能)

**证据链**:
1. scan 完成后，固件停留在**最后一个扫描信道** (可能是 channel 161 等)
2. auth 帧需要发送到 AP 信道 (例如 channel 36)
3. 如果固件认为"当前信道 != 帧目标信道"，直接丢弃

**MT7925 解决方案**:
- `mgd_prepare_tx()` → `ROC (JOIN)` → 等待 `ROC_GRANT` 事件 → 信道切换完成 → 然后发帧
- **关键**: 必须**等待** `ROC_GRANT` 事件！不能发完 ROC 命令就立即返回

**我们的问题**:
- ✅ 发送了 ROC 命令
- ❌ 没有等待 `ROC_GRANT` 事件 → mac80211 立即发送 auth 帧 → 固件还没切信道 → 丢弃

**或者**: MT6639 固件可能**不支持** ROC 命令 (Android 驱动从不使用 ROC)

#### 原因 2: 缺少 UNI_CHANNEL_SWITCH (或参数错误)

**Windows 驱动角度**:
- Windows WLAN 框架 (WDI) 在连接前会通过 OID 通知驱动切换信道
- 驱动可能发送 `CHANNEL_SWITCH` (CID=0x05) 或等效的 MCU 命令

**MT7925 角度**:
- STA 模式**不使用** `CHANNEL_SWITCH` — 完全通过 `ROC` + `BSS_INFO_RLM` 配置信道
- `CHANNEL_SWITCH` 是 AP/monitor 模式命令

**Android 驱动角度**:
- MT6639 Android 驱动使用 `cnmChMngrRequestPrivilege(REQ_TYPE_JOIN)` — 这是软件层信道管理
- 底层可能通过不同的 MCU 命令切换信道

**我们的问题**:
- 在 `mgd_prepare_tx()` 中**同时**发送了 `CHANNEL_SWITCH` + `ROC` → 可能冲突
- `CHANNEL_SWITCH` 可能覆盖了 ROC 的信道设置

#### 原因 3: STA_REC_UPDATE 参数不正确

**MT7925 关键发现**:
- mt7925 在 STA_REC_UPDATE 中**不包含 STA_REC_WTBL** TLV
- WTBL 通过独立的 `mt7925_mac_wtbl_update()` 寄存器操作更新
- MT7927 当前实现包含了 `STA_REC_WTBL` — 固件可能无法解析

**MT7925 STA_REC 在 sta_add 时的 TLV**:
1. STA_REC_BASIC (tag=0x00): `conn_state=PORT_SECURE(2)`, `extra_info=VER|NEW(0x03)`
2. STA_REC_PHY (tag=0x15)
3. STA_REC_HT/VHT/HE/EHT (tag=0x02/0x03/0x11/0x22)
4. STA_REC_STATE (tag=?): `state=NONE(0)`
5. STA_REC_HDR_TRANS (tag=0x2B)
6. ~~STA_REC_WTBL~~ (mt7925 不使用!)

**我们的 STA_REC TLV**:
1. STA_REC_BASIC (tag=0x00): OK
2. STA_REC_HDR_TRANS (tag=0x2B): OK
3. STA_REC_WTBL (tag=0x0D): **可能不兼容!**

**风险**: 包含 `STA_REC_WTBL` 可能导致固件解析错误，STA 记录创建失败，进而无法发送帧。

#### 原因 4: BSS_INFO_UPDATE 缺少关键 TLV

**MT7925 BSS_INFO_UPDATE 的完整 TLV 列表** (在 sta_add 之前发送):
1. BSS_INFO_BASIC (tag=0x00): active, bssid, bmc_tx_wlan_idx, 等
2. BSS_INFO_RLM (tag=0x02): 信道配置 (control_channel, bw, band)
3. BSS_INFO_RATE (tag=?): 基础速率设置 (如果 AP 模式)
4. BSS_INFO_SEC (tag=?): 安全设置
5. BSS_INFO_IFS_TIME (tag=?): IFS 时间配置
6. BSS_INFO_EHT (tag=?): EHT 配置 (如果 EHT 支持)

**我们的 BSS_INFO_UPDATE TLV**:
1. BSS_INFO_BASIC (tag=0x00): ✅
2. BSS_INFO_RLM (tag=0x02): ✅

**缺失**: 可能缺少 IFS_TIME、RATE 等 TLV，但这些通常不影响帧发送。

---

## 五、MT7925 完整连接流程 (参考)

```
scan 完成 → mac80211 选择 BSS

1. bss_info_changed(BSS_CHANGED_BSSID):
   └→ 无直接 MCU 命令 (mt7925 在这里不发 BSS_INFO)

2. sta_state(NOTEXIST → NONE) [sta_add]:
   a. 分配 WCID
   b. mt7925_mac_wtbl_update(idx, ADM_COUNT_CLEAR)
   c. BSS_INFO_UPDATE (enable=false, BASIC+RLM TLVs)  ← BSS 先行!
   d. STA_REC_UPDATE (enable=true, state=NONE, BASIC+PHY+HT/VHT/HE+STATE+HDR_TRANS)

3. mgd_prepare_tx():
   └→ ROC (JOIN, 信道=AP 信道)
   └→ 等待 ROC_GRANT 事件 (最多 4 秒)  ← 关键!

4. mac80211 发送 auth 帧:
   └→ tx_prepare_skb() → TXD + DMA → 固件在 ROC 信道上发送

5. 收到 auth 响应 → sta_state(NONE → AUTH) [无操作]

6. sta_state(AUTH → ASSOC) [sta_assoc]:
   a. BSS_INFO_UPDATE (enable=true, BASIC+RLM+更多 TLVs)
   b. mt7925_mac_wtbl_update(idx, ADM_COUNT_CLEAR)
   c. STA_REC_UPDATE (enable=true, state=ASSOC, BASIC+PHY+...)

7. mac80211 发送 assoc 请求:
   └→ tx_prepare_skb() → 同上

8. 收到 assoc 响应 → sta_state(ASSOC → AUTHORIZED) [无操作]

9. mgd_complete_tx():
   └→ ROC_ABORT → 取消 ROC
```

---

## 六、当前驱动缺失的关键步骤

### 缺失 1: ROC_GRANT 等待 (最关键!)

```
当前流程:
  mgd_prepare_tx()
    → 发送 ROC 命令
    → 立即返回 ← 没有等待 ROC_GRANT!

正确流程 (mt7925):
  mgd_prepare_tx()
    → dev->roc_grant = false
    → 发送 ROC 命令
    → wait_event_timeout(roc_wait, roc_grant, 4*HZ) ← 等待固件确认!
    → 确认信道已切换后才返回
```

**不等待 ROC_GRANT 的后果**: mac80211 在 `mgd_prepare_tx()` 返回后立即发送 auth 帧，但此时固件可能还没有切换到目标信道 → 帧在错误信道发送或被固件丢弃。

### 缺失 2: 多余的 CHANNEL_SWITCH 命令

```
当前 mgd_prepare_tx():
  → mt7927_mcu_set_chan_info(CHANNEL_SWITCH)  ← 多余! STA 模式不需要!
  → mt7927_mcu_send_unicmd(ROC)

正确做法:
  → mt7927_mcu_send_unicmd(ROC)  ← 只需 ROC
```

**CHANNEL_SWITCH 可能干扰 ROC 的信道设置。** mt7925 STA 模式从不使用 CHANNEL_SWITCH。

### 缺失 3: BSS_INFO_UPDATE 发送时机和顺序

```
当前流程:
  bss_info_changed(BSS_CHANGED_BSSID)
    → BSS_INFO_UPDATE + RLM TLV

MT7925 流程:
  sta_state(NOTEXIST→NONE) 内部:
    → BSS_INFO_UPDATE (enable=false, BASIC+RLM)  ← 在 STA_REC 之前!
    → STA_REC_UPDATE
```

**MT7925 的 BSS_INFO_UPDATE 是在 sta_add 内部发送的**，而不是在 bss_info_changed 中。时序差异可能导致固件状态不一致。

### 缺失 4: STA_REC_WTBL TLV 的兼容性问题

MT7925 在 STA_REC_UPDATE 中**不包含** STA_REC_WTBL TLV。MT7927 当前实现包含了它。如果 MT6639 固件不支持在 STA_REC 中嵌套 WTBL，可能导致 STA 记录创建失败。

### 缺失 5: ROC_GRANT 事件处理

当前 RX event 处理代码中没有 `UNI_EVENT_ROC_GRANT` (CID=0x27, tag=0) 的处理分支。即使固件发送了 ROC_GRANT 事件，我们也会忽略它。

---

## 七、Windows RE 中未找到的信息

以下信息在现有逆向文档中**不存在**:

1. ❌ Windows 从 scan 完成到 auth 发送的 MCU 命令序列
2. ❌ Windows 的 BSS_INFO_UPDATE / STA_REC_UPDATE 参数细节
3. ❌ Windows 是否使用 ROC 机制
4. ❌ Windows 的信道切换命令 (scan 后回到 AP 信道的方式)
5. ❌ Windows 的 TX 帧信道验证逻辑
6. ❌ auth 帧是 host 发送还是 firmware offload

**原因**: 逆向工作集中在固件初始化路径 (FUN_1401c9510 = PostFwDownloadInit)。连接流程在 NDIS/WDI 层的回调中，涉及更多函数，未被分析。

---

## 八、建议的修复方案 (按优先级排序)

### 修复 1 (最高优先): 移除 CHANNEL_SWITCH + 实现 ROC_GRANT 等待

**修改 `mgd_prepare_tx()`**:
1. 移除 `mt7927_mcu_set_chan_info(CHANNEL_SWITCH)` 调用
2. 添加 `roc_grant` 等待机制:
   ```c
   dev->roc_grant = false;
   mt7927_mcu_send_unicmd(dev, ROC, ...);
   wait_event_timeout(dev->roc_wait, dev->roc_grant, 4 * HZ);
   if (!dev->roc_grant)
       dev_err("ROC grant timeout — 固件可能不支持 ROC");
   ```
3. 在 RX event 处理中添加 ROC_GRANT (CID=0x27, tag=0):
   ```c
   dev->roc_grant = true;
   wake_up(&dev->roc_wait);
   ```

**如果 ROC_GRANT 超时**: 说明 MT6639 固件不支持 ROC → 需要换用其他信道切换机制。

### 修复 2: STA_REC_UPDATE 去掉 STA_REC_WTBL

参考 mt7925 的做法:
1. STA_REC_UPDATE 只包含: BASIC + HDR_TRANS + STATE (可选: PHY, HT, VHT, HE)
2. **不包含** STA_REC_WTBL — 固件可能通过 BASIC TLV 的 `extra_info=NEW` 标志自动创建 WTBL

### 修复 3: 调整 BSS_INFO_UPDATE 发送位置

在 `sta_state(NOTEXIST→NONE)` 中，确保 BSS_INFO_UPDATE 在 STA_REC_UPDATE **之前**发送:
```c
if (old_state == NOTEXIST && new_state == NONE) {
    mt7927_mcu_add_bss_info(dev, vif, false);   // ← 先!
    mt7927_mcu_sta_update(dev, vif, sta, true, STATE_NONE);  // ← 后!
}
```

### 修复 4 (备用): 如果 ROC 不支持，移除 ROC 回调

如果修复 1 中 ROC_GRANT 超时:
1. 移除 `mgd_prepare_tx()` / `mgd_complete_tx()` 回调
2. 完全依赖 BSS_INFO_RLM 配置信道
3. 期望固件在收到 TX 帧时自动切换到 BSS 配置的信道

### 修复 5: 添加 STA_REC_STATE TLV

mt7925 在 STA_REC_UPDATE 中包含 `STA_REC_STATE` TLV，告诉固件 STA 的状态:
```c
struct {
    __le16 tag;    // STA_REC_STATE
    __le16 len;
    u8 state;      // 0=NONE, 1=AUTH, 2=ASSOC
    u8 vht_opmode;
    u8 pad[2];
} __packed;
```

固件可能通过此 TLV 判断是否允许发送帧。

---

## 九、附录: Windows RE 函数中的 MCU 命令 CID 值

从 Ghidra 反编译中提取到的 MCU 命令调用:

| 函数 | class/CID | target | 描述 |
|------|-----------|--------|------|
| FUN_1401cb88c | 0x0d | 0xed | `AsicConnac3xAddressLenReqCmd` — 地址/长度请求 |
| FUN_1401cde70 | 0x10 | 0xee | 固件下载相关 (target=0xee = FW_SCATTER) |
| FUN_1401ce7c0 | 0x17 | 0x02 | 配置命令 (类似 BSS_INFO?) |
| PostFwInit sub2 | 0x8a | 0xed | NIC_CAP |
| PostFwInit sub3 | 0x02 | 0xed | Config ({1,0,0x70000}) |
| PostFwInit sub4 | 0xc0 | 0xed | Config ({0x820cc800, 0x3c200}) |
| PostFwInit sub5 | 0xed | 0xed | DownloadBufferBin (subcmd=0x21) |
| PostFwInit sub6 | 0x28 | 0xed | DBDC (MT6639/MT7927) |
| PostFwInit sub7-9 | 0xca | 0xed | Scan/Chip/Log Config |

### MCU 命令调度路径 (MtCmdSendSetQueryCmdDispatch)

```c
// FUN_1400cdc4c
void dispatch(ctx) {
    chip_id = *(short*)(ctx + 0x1f72);
    if (is_connac3(chip_id) && *(ctx + 0x146cde9) == 1) {
        // CONNAC3 UniCmd 路径 → MtCmdSendSetQueryCmdHelper
        FUN_14014e644();  // ← 路由表查找 + 子命令调度
    } else {
        // Generic/legacy 路径 → MtCmdSendSetQueryCmdAdv
        FUN_1400cd2a8();
    }
}
```

**flag `ctx+0x146cde9`** (注意: v5705275 中偏移略不同于 v5603998 的 `ctx+0x146e621`):
- `0` = Generic path (legacy TXD, 0x40 byte header)
- `1` = CONNAC3 UniCmd path (0x30 byte header)

**PostFwDownloadInit 中**: flag 被清除 → 使用 Generic/legacy 路径
**正常运行时**: flag = 1 → 使用 CONNAC3 UniCmd 路径

---

## 十、关键对比: 我们的驱动 vs Windows vs mt7925

| 步骤 | 我们的驱动 | Windows (推测) | mt7925 |
|------|-----------|---------------|--------|
| PostFwInit MCU 命令 | ✅ 完整 | ✅ 完整 | ✅ 完整 |
| DEV_INFO_UPDATE | ✅ | ✅ (推测) | ✅ |
| BSS_INFO_UPDATE 时机 | bss_info_changed | 未知 | sta_add 内部 (BSS 先于 STA) |
| BSS_INFO_RLM | ✅ | 未知 | ✅ (在 sta_add 中) |
| STA_REC_UPDATE | ✅ (含 WTBL) | 未知 | ✅ (不含 WTBL) |
| STA_REC_STATE TLV | ❌ 缺失 | 未知 | ✅ |
| STA_REC_PHY TLV | ❌ 缺失 | 未知 | ✅ |
| CHANNEL_SWITCH 用于连接 | ⚠️ 使用了 (错误) | 未知 | ❌ STA 模式不用 |
| ROC (JOIN) | ✅ | 未知 | ✅ |
| ROC_GRANT 等待 | ❌ 缺失 | 未知 | ✅ (最多 4 秒) |
| ROC_ABORT | ✅ | 未知 | ✅ (mgd_complete_tx) |
| sta_assoc (AUTH→ASSOC) | ✅ (最近添加) | 未知 | ✅ (BSS+STA 更新) |

---

## 十一、总结

### 最可能的 auth 帧丢弃原因

**信道问题 + 命令冲突**:
1. `mgd_prepare_tx()` 发送了多余的 `CHANNEL_SWITCH` + `ROC`，可能造成固件内部信道状态冲突
2. 没有等待 `ROC_GRANT` 事件，mac80211 在信道未切换时就发送 auth 帧
3. 固件检查发现"当前工作信道 != 帧目标信道"（或 "STA 状态不满足发送条件"），直接丢弃

### 需要进一步逆向的方向

如果上述修复仍然失败，需要逆向 Windows 驱动的以下函数:
1. **连接回调** — WDI/NDIS OID 处理中的 connect/join 回调
2. **TX 路径** — 数据/管理帧发送前的信道检查逻辑
3. **BSS_INFO/STA_REC 构建** — Windows 发送的完整 TLV 结构

这些函数没有在现有 Ghidra 分析中覆盖，需要新的逆向工作。

---

*文档结束 — 2026-02-16*
