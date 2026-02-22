# Windows/Android 驱动 Auth 流程深度分析 — 与 MT7927 Linux 驱动差异对比

**日期**: 2026-02-16
**分析者**: win-reverser agent
**来源**: Ghidra RE 文档 + Windows mtkwecx.sys v5603998/v5705275 + MT6639 Android 驱动 + mt7925 参考代码
**目标**: 找出 auth 帧被固件丢弃 (TXFREE stat=1) 的根本原因

---

## 一、核心结论

### 现有 Windows RE 文档不覆盖连接流程

**关键事实**: 所有现有 Windows 逆向文档集中在 **初始化阶段** (PreFwDownloadInit → FWDL → PostFwDownloadInit)。
Windows 驱动中 scan→auth→assoc 的完整连接流程 **从未被逆向分析**。

**原因**:
- Ghidra 逆向集中在 `FUN_1401c9510` (AsicConnac3xPostFwDownloadInit) 及其子函数
- 连接逻辑在 NDIS/WDI 框架的回调函数中，涉及大量间接调用，未被分析
- 已知的 57 条 MCU 命令路由表条目中，只有初始化相关的被分析

### 但 Android 驱动 + mt7925 参考 提供了完整的连接命令序列

通过 MT6639 Android 驱动 (`cnmStaRecChangeState`) 和 mt7925 (`mt7925_mac_link_sta_add`) 的交叉分析，
可以确定固件在 auth 帧发送前需要的 MCU 命令。

---

## 二、三方参考的 Auth 前 MCU 命令序列

### MT6639 Android 驱动流程

```
AIS_STATE_REQ_CHANNEL_JOIN:
  → cnmChMngrRequestPrivilege(REQ_TYPE_JOIN) — 信道分配

AIS_STATE_JOIN:
  1. bssCreateStaRecFromBssDesc() — 创建 STA Record, 分配 WTBL
  2. cnmStaRecChangeState(prStaRec, STA_STATE_1)
     → cnmStaSendUpdateCmd() → CMD_ID_UPDATE_STA_RECORD (0x13)
     → UniCmd 映射: UNI_CMD_ID_STAREC_INFO (0x03)
     → 状态过滤: STA_STATE_1→STA_STATE_1 通过! (1→1 会触发 FW 更新)

SAA_STATE_SEND_AUTH1:
  3. cnmStaRecChangeState(prStaRec, STA_STATE_1) — 再次确认
  4. authSendAuthFrame() — 发送 auth 帧
```

**关键发现: Android `cnmStaRecChangeState` 状态过滤逻辑**:
```c
// 以下转换 **不** 触发固件命令 (仅本地状态更新):
// - New=STA_STATE_2, Current≠STA_STATE_3 → 跳过
// - New=STA_STATE_1, Current=STA_STATE_2 → 跳过
//
// 以下转换 **触发** CMD_ID_UPDATE_STA_RECORD:
// - 1→1: 触发! (初始创建后同步到固件)
// - 3→1: 触发! (断开连接)
// - 3→2: 触发!
// - any→3: 触发! (连接完成, + qmActivateStaRec)
```

### mt7925 Linux 驱动流程

```
mt7925_mac_link_sta_add() [在 sta_state NOTEXIST→NONE 时调用]:
  1. mt76_wcid_alloc() — 分配 WCID
  2. mt7925_mac_wtbl_update(idx, ADM_COUNT_CLEAR) — 清空 WTBL 计数器
  3. mt7925_mcu_add_bss_info() — BSS_INFO_UPDATE (在 STA_REC 之前!)
  4. mt7925_mcu_sta_update(state=NONE) — STA_REC_UPDATE

mt7925_mgd_prepare_tx():
  5. mt7925_set_roc(type=JOIN) — ROC 请求
  6. wait_event_timeout(roc_wait, roc_grant, 4*HZ) — 等待 ROC_GRANT
  7. [ROC_GRANT 超时则 abort + return -ETIMEDOUT]

mt7925_vif_cfg_changed(BSS_CHANGED_ASSOC):
  8. mt7925_mcu_sta_update(state=ASSOC)
```

### 我们的 MT7927 Linux 驱动流程

```
mt7927_sta_state(NOTEXIST→NONE):
  1. 分配 WCID
  2. mt7927_mcu_sta_update(enable=true, CONN_STATE_DISCONNECT)  ✅
     → STA_REC: BASIC(EXTRA_INFO_NEW) + RA + STATE(0) + HDR_TRANS

mt7927_mgd_prepare_tx():
  3. [如果 roc_active: abort 旧 ROC + msleep(10)]
  4. ROC acquire (0x27, reqtype=JOIN)
  5. wait_for_completion_timeout(roc_complete, 4*HZ)  ✅
  6. [收到 ROC_GRANT]: 更新 band_idx + 重发 DEV_INFO + BSS_INFO  ✅

mac80211 发送 auth 帧:
  7. mt7927_mac80211_tx() → TXD + DMA
```

---

## 三、逐项差异对比

| 步骤 | mt7925 参考 | MT6639 Android | 我们的驱动 | 差异 |
|------|-----------|---------------|-----------|------|
| WTBL ADM_COUNT_CLEAR | ✅ `mt7925_mac_wtbl_update()` | N/A (固件内部) | ❌ **缺失** | 可能影响计数器 |
| BSS_INFO 在 STA_REC 之前 | ✅ 明确先 BSS 后 STA | N/A | ⚠️ 只在 ROC_GRANT 后发 | 时序不同 |
| STA_REC NOTEXIST→NONE | ✅ state=NONE | ✅ CMD_UPDATE (state=1) | ✅ state=0(DISCONNECT) | **值不同!** |
| STA_REC TLV 数量 | ~10 (BASIC+PHY+HT+VHT+HE+RA+STATE+HDR_TRANS+...) | 完整 legacy CMD | 4 (BASIC+RA+STATE+HDR_TRANS) | 可能够用 |
| ROC_GRANT 等待 | ✅ `wait_event_timeout` | N/A (CNM 信道管理) | ✅ `wait_for_completion_timeout` | 一致 |
| ROC 超时处理 | abort + return error | N/A | 仅 warn, 继续 | ⚠️ 应该 abort |
| CHANNEL_SWITCH (STA模式) | ❌ 不使用 | ✅ CNM 内部 | ❌ 已移除 | 正确 |
| BSS_INFO_RLM (信道) | ✅ 在 sta_add 中 | ✅ nicUpdateBss | ✅ 在 bss_info_changed 和 ROC_GRANT 后 | OK |
| band_idx 从 ROC_GRANT 更新 | ✅ `mt7925_mcu_roc_iter()` | N/A | ✅ `roc_grant_band_idx` | 一致 |

---

## 四、最可能的 TXFREE stat=1 原因分析

### 已排除的原因

1. **STA_REC 缺失**: 已在 NOTEXIST→NONE 发送 — 排除
2. **CHANNEL_SWITCH 冲突**: 已移除 — 排除
3. **ROC_GRANT 不等待**: 已实现 wait — 排除
4. **STA_REC_WTBL TLV**: 已去掉 — 排除
5. **STA_REC_STATE TLV 缺失**: 已添加 — 排除

### 仍然可能的原因 (按优先级排序)

#### 原因 1: ROC abort 后的 ROC 超时 (任务 #3)

**问题**: 如果前一次 `mgd_prepare_tx()` 的 ROC 没有被正确 abort (例如固件还没来得及处理 abort),
新的 ROC 请求可能被固件拒绝或忽略 → ROC_GRANT 超时 → 信道未切换 → auth 帧被丢弃。

**证据**:
- 当前代码在 abort 后只 `msleep(10)` — 可能不够
- mt7925 使用 `test_and_set_bit(MT76_STATE_ROC)` 做互斥, 我们没有

**建议**: 检查 dmesg 是否出现 "ROC grant timeout (4s)" 日志

#### 原因 2: BSS_INFO 时序问题

**mt7925 流程**: BSS_INFO → STA_REC (在 sta_add 中, auth 之前)
**我们的流程**: STA_REC 先 → BSS_INFO 后 (在 ROC_GRANT 之后)

固件可能需要有效的 BSS 上下文才能正确处理 STA_REC 和后续 TX。

**建议**: 在 `mt7927_sta_state(NOTEXIST→NONE)` 中, 先发 BSS_INFO 再发 STA_REC

#### 原因 3: STA_REC_STATE 的 state 值映射问题

**Android**: `ucStaState = STA_STATE_1` (值=1, Class 1)
**mt7925**: `MT76_STA_INFO_STATE_NONE` (值可能=0)
**我们**: `state = 0` (STATE_NONE)

如果 MT6639 固件内部状态机期望的是 STA_STATE_1 (=1) 而不是 STATE_NONE (=0),
可能导致固件不认为此 STA 已准备好接收 TX。

**建议**: 尝试将 NOTEXIST→NONE 时的 state 从 0 改为 1

#### 原因 4: TXD 的 TGID/band_idx 在 STA_REC 发送时还是 0xff

**问题**: `add_interface` 时 `mvif->band_idx = 0xff` (BAND_AUTO)
- STA_REC 在 NOTEXIST→NONE 发送, 此时 band_idx 还是 0xff
- ROC_GRANT 后才更新 band_idx
- 但 TXD 构建时使用的 TGID 是从 `msta->vif->band_idx` 取的
- 如果 auth 帧在 ROC_GRANT 之后发送, band_idx 应该已更新 → OK

**但**: 如果 STA_REC 中包含了 band_idx=0xff, 固件可能在内部表中记录了错误的 band 归属

#### 原因 5: 缺少 WTBL ADM_COUNT_CLEAR

**mt7925 在 sta_add 的第一步**:
```c
mt7925_mac_wtbl_update(dev, idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR);
```
这是一个寄存器写操作 (不是 MCU 命令), 清除 WTBL 条目的 admission count。

**我们缺少这一步**, 可能导致 WTBL 中有残留的 admission count, 影响 TX 权限。

---

## 五、Windows RE 中能找到的间接证据

### 已知的 MCU 命令 CID 映射

从 Ghidra 路由表 (57 条目 @ 0x14023fcf0):

| 路由索引 | CID/class | 可能的连接功能 |
|----------|-----------|--------------|
| [0] | 0x8a | NIC_CAP (PostFwInit) |
| [1] | 0x02 | BSS_INFO_UPDATE |
| [2] | 0xc0 | Config |
| [3] | 0x28 | DBDC |
| [4] | 0xca | ScanConfig/ChipConfig |
| ... | ... | 未分析的 ~52 条目可能包含 STA_REC, ROC, RA 等 |

**遗憾**: 路由表中只有前 5 条被详细分析, 其他条目的 CID 未被提取。
如果能获取完整的 57 条路由表, 就能确认 Windows 驱动支持哪些 MCU 命令。

### MCU 命令白名单 (FW 下载阶段)

在 `MtCmdSendSetQueryCmdAdv` 中有明确白名单:
```
允许的 target: [0x01, 0x02, 0x03, 0x05, 0x07, 0x10, 0x11, 0xee, 0xef]
```

**关键**: target 0x03 在白名单中 → 说明 STA_REC_UPDATE (CID=0x03) 是固件支持的标准命令。

### Windows CONNAC3 UniCmd 与 Legacy 路径

- **PostFwDownloadInit**: 使用 Legacy/Generic 路径 (flag_146e621=0)
- **正常运行时**: 使用 CONNAC3 UniCmd 路径 (flag_146e621=1)
- **连接相关命令**: 必然使用 UniCmd 路径 (因为发生在 PostFwInit 之后)

---

## 六、修复建议 (按优先级排序)

### 建议 1: 调查 ROC 超时问题 (任务 #3)

检查 dmesg 中是否出现:
- `"mgd_prepare_tx: ROC grant timeout (4s)!"` → ROC 超时是直接原因
- `"mgd_prepare_tx: aborting previous ROC before re-acquire"` → abort 竞争

如果 ROC 超时:
1. 检查 RX event 处理中是否正确解析 ROC_GRANT (eid=0x27)
2. 检查 ROC 命令的 option 是否使用 0x07 (需要响应)
3. 尝试不 abort, 直接发新 ROC

### 建议 2: 对齐 BSS_INFO → STA_REC 发送时序

```c
// mt7927_sta_state(NOTEXIST→NONE):
if (old_state == IEEE80211_STA_NOTEXIST && new_state == IEEE80211_STA_NONE) {
    // ... 分配 WCID ...
    mt7927_mcu_add_bss_info(dev, vif, false);  // ← 先发 BSS_INFO
    ret = mt7927_mcu_sta_update(dev, vif, sta, true, CONN_STATE_DISCONNECT);
}
```

参考 mt7925 line 892: `/* should update bss info before STA add */`

### 建议 3: 添加 WTBL ADM_COUNT_CLEAR

在 sta_state NOTEXIST→NONE 中, 在发 STA_REC 之前:
```c
// 清除 WTBL admission count — mt7925 在 sta_add 第一步做这个
// MT_WTBL_UPDATE 寄存器: MT_WFDMA0(0x230) 或类似
mt7927_wr(dev, MT_WTBL_UPDATE, FIELD_PREP(MT_WTBL_UPDATE_WLAN_IDX, idx) |
           MT_WTBL_UPDATE_ADM_COUNT_CLEAR);
// 轮询直到 MT_WTBL_UPDATE_BUSY 清除
```

### 建议 4: 尝试 STA_REC_STATE.state = 1 (STA_STATE_1)

Android 驱动使用 `ucStaState = STA_STATE_1` (=1, Class 1, 未认证但已存在)。
mt7925 使用 `MT76_STA_INFO_STATE_NONE` (=0)。

两者可能等价 (固件内部映射), 但值得验证:
```c
// 在 NOTEXIST→NONE 时:
req.state.state = 0; // 当前值
// 尝试改为:
req.state.state = 1; // STA_STATE_1 (Class 1)
```

### 建议 5: ROC 超时后 abort 而非继续

```c
if (!wait_for_completion_timeout(&dev->roc_complete, 4 * HZ)) {
    dev_warn("ROC grant timeout");
    // 添加: abort ROC 以清理固件状态
    mt7927_mcu_send_unicmd(dev, 0x27, UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
                           &abort_req, sizeof(abort_req));
    dev->roc_active = false;
}
```

---

## 七、需要进一步逆向的方向

如果上述修复仍然失败, 需要:

1. **Windows 路由表完整提取**: 在 Ghidra 中读取 0x14023fcf0 处的 57 条目,
   获取所有支持的 MCU CID → 确认 0x01/0x02/0x03/0x27 是否在路由表中

2. **Windows 连接回调**: 定位 WDI OID_CONNECT/OID_ASSOCIATE 的回调函数,
   分析其中调用的 MCU 命令序列

3. **Windows TX 路径**: 定位管理帧发送函数, 分析 TXD 构建 + 信道检查逻辑

4. **固件 TXFREE stat 值含义**: 如果能找到固件中 stat 字段的定义,
   可以精确知道 stat=1 代表什么错误

---

## 八、总结

### 当前驱动 vs 参考实现的关键差异

| 差异项 | 严重度 | 说明 |
|--------|--------|------|
| ROC abort 后 ROC 超时 | **高** | 任务 #3, 可能是直接原因 |
| BSS_INFO 在 STA_REC 之后 | **中** | mt7925 明确要求 BSS_INFO 先于 STA_REC |
| 缺少 WTBL ADM_COUNT_CLEAR | **中** | mt7925 在 sta_add 第一步做 |
| STA_REC_STATE.state=0 vs 1 | **低** | 可能是等价的 |
| STA_REC TLV 数量较少 | **低** | 4 vs 10, 但核心 TLV 都有 |

### 最优修复路径

1. 先确认 ROC_GRANT 是否收到 (检查 dmesg)
2. 如果 ROC 正常, 调整 BSS_INFO/STA_REC 发送顺序
3. 添加 WTBL ADM_COUNT_CLEAR
4. 如果仍然失败, 考虑 STA_REC_STATE.state=1

---

*分析完成 — 2026-02-16*
