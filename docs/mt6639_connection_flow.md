# MT6639 Android WiFi 驱动连接流程分析

**日期**: 2026-02-16
**来源**: MT6639 Android 驱动代码分析 (`/home/user/mt7927/mt6639/`)
**目的**: 找出从 scan 到 auth/assoc 需要的 MCU 命令序列，解决 MT7927 Linux 驱动 auth timeout 问题

---

## 一、状态机架构

MT6639 使用多层状态机协同工作：

### 1. AIS FSM (APS Infrastructure Station 状态机)
文件: `mgmt/ais_fsm.c`

**状态流程**:
```
AIS_STATE_IDLE
  ↓ (用户发起连接)
AIS_STATE_SEARCH (搜索 AP)
  ↓
AIS_STATE_SCAN (执行扫描)
  ↓ (找到目标 AP)
AIS_STATE_REQ_CHANNEL_JOIN (请求信道)
  ↓ (CNM 分配信道)
AIS_STATE_JOIN (开始 Join 流程)
  ↓ (Auth + Assoc 完成)
AIS_STATE_NORMAL_TR (正常数据传输 — 已连接)
```

### 2. SAA FSM (Send Auth/Assoc 状态机)
文件: `mgmt/saa_fsm.c`

**状态流程**:
```
SAA_STATE_SEND_AUTH1 → SAA_STATE_WAIT_AUTH2
  ↓ (Auth 成功)
SAA_STATE_SEND_ASSOC1 → SAA_STATE_WAIT_ASSOC2
  ↓ (Assoc 成功)
AA_STATE_IDLE (完成)
```

### 3. CNM (Channel Manager)
文件: `mgmt/cnm.c`

负责:
- 信道资源分配
- 多 BSS 信道协调
- 发送信道切换命令到固件

---

## 二、连接流程详解

### Phase 1: 扫描完成 → 请求信道 (AIS_STATE_REQ_CHANNEL_JOIN)

**代码位置**: `ais_fsm.c:2213-2326`

```c
case AIS_STATE_REQ_CHANNEL_JOIN:
    // 1. 停止旧 STA 的 TX (如果是 reassociation)
    if (prAisBssInfo->prStaRecOfAP &&
        prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION)
        prAisBssInfo->prStaRecOfAP->fgIsTxAllowed = FALSE;

    // 2. 分配 MSG_CH_REQ 消息
    prMsgChReq = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_CH_REQ));

    // 3. 填充信道请求参数
    prMsgChReq->eRfBand = prBssDesc->eBand;
    prMsgChReq->ucPrimaryChannel = prBssDesc->ucChannelNum;
    prMsgChReq->eRfChannelWidth = ...;  // 从 BSS Desc 获取带宽

    // 4. 发送到 CNM (Mailbox)
    mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prMsgChReq, MSG_SEND_METHOD_BUF);

    prAisFsmInfo->fgIsChannelRequested = TRUE;
    break;
```

**关键点**:
- CNM 收到请求后会处理信道分配，可能发送 **信道切换命令** 到固件
- 信道分配完成后，CNM 会回调触发状态转换到 `AIS_STATE_JOIN`

---

### Phase 2: 开始 Join 流程 (AIS_STATE_JOIN)

**代码位置**: `ais_fsm.c:658-906` (`aisFsmStateInit_JOIN`)

#### 步骤 1: 创建 STA Record

```c
// 1. 从 BSS Descriptor 创建 STA Record
prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
                                      STA_TYPE_LEGACY_AP,
                                      prAisBssInfo->ucBssIndex,
                                      prBssDesc);

prAisFsmInfo->prTargetStaRec = prStaRec;
```

**关键**: `bssCreateStaRecFromBssDesc()` 会:
- 分配 WTBL (Wireless Table) 条目
- 初始化 STA Record 结构
- **可能** 发送初始 STAREC 到固件（待确认）

#### 步骤 2: 同步 STA 状态到固件

```c
// 2.1 同步到固件 (如果 STA 刚创建)
if (prStaRec->ucStaState == STA_STATE_1)
    cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
```

**关键**: `cnmStaRecChangeState()` 函数 **应该** 发送 MCU 命令更新 STAREC:
- 目标: **`CMD_ID_UPDATE_STA_RECORD`** (Legacy) 或 **`EXT_CMD_ID_STAREC_UPDATE`**
- UniCmd: **`UNI_CMD_ID_STAREC_INFO (0x03)`**
- 状态: `STA_STATE_1` (Class 1 — Unauthenticated, unassociated)

**重要发现**: 代码注释说 "sync to firmware domain"，但我在 `cnm.c` 中 **没有找到** `cnmStaRecChangeState()` 的实现！这个函数可能是**内联函数**或在其他文件中。需要进一步调查。

#### 步骤 3: 设置认证类型

```c
// 3. 根据 Auth Mode 设置可用认证类型
switch (prConnSettings->eAuthMode) {
    case AUTH_MODE_OPEN:
    case AUTH_MODE_WPA:
    case AUTH_MODE_WPA2:
        prAisFsmInfo->ucAvailableAuthTypes = AUTH_TYPE_OPEN_SYSTEM;
        break;
    // ...
}

prStaRec->ucAuthAlgNum = AUTH_ALGORITHM_NUM_OPEN_SYSTEM;  // 或 SHARED_KEY/SAE
```

#### 步骤 4: 启动 SAA FSM

```c
// 6. 发送 MSG_SAA_FSM_START 消息触发 SAA FSM
prJoinReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_SAA_FSM_START));
prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
prJoinReqMsg->prStaRec = prStaRec;

mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)prJoinReqMsg, MSG_SEND_METHOD_BUF);
```

---

### Phase 3: SAA FSM — 发送 Auth 帧

**代码位置**: `saa_fsm.c:524-569` (`saaFsmSteps` → `SAA_STATE_SEND_AUTH1`)

```c
case SAA_STATE_SEND_AUTH1:
    // 1. 检查重试次数
    if (prStaRec->ucTxAuthAssocRetryCount >= prStaRec->ucTxAuthAssocRetryLimit) {
        prStaRec->u2StatusCode = STATUS_CODE_AUTH_TIMEOUT;
        eNextState = AA_STATE_IDLE;
        fgIsTransition = TRUE;
    } else {
        prStaRec->ucTxAuthAssocRetryCount++;
        prStaRec->ucAuthTranNum = AUTH_TRANSACTION_SEQ_1;

        // 2. 更新 STA 状态到 Class 1
        cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

        // 3. 发送 Auth Frame (Transaction 1)
        rStatus = authSendAuthFrame(prAdapter, prStaRec,
                                   AUTH_TRANSACTION_SEQ_1);

        // 4. 如果发送失败，设置重试定时器
        if (rStatus != WLAN_STATUS_SUCCESS) {
            cnmTimerStartTimer(prAdapter,
                             &prStaRec->rTxReqDoneOrRxRespTimer,
                             TU_TO_MSEC(TX_AUTHENTICATION_RETRY_TIMEOUT_TU));
        }
    }
    break;
```

**关键点**:
- **再次调用** `cnmStaRecChangeState(STA_STATE_1)` — 确保固件知道 STA 状态
- `authSendAuthFrame()` 构建并通过 mac80211 TX 路径发送 auth 帧
- Auth 帧是 **管理帧**，使用 Q_IDX = 0x10 (MT_LMAC_ALTX0)

---

### Phase 4: SAA FSM — 发送 Assoc 帧

**代码位置**: `saa_fsm.c:622-652` (`saaFsmSteps` → `SAA_STATE_SEND_ASSOC1`)

```c
case SAA_STATE_SEND_ASSOC1:
    if (prStaRec->ucTxAuthAssocRetryCount >= prStaRec->ucTxAuthAssocRetryLimit) {
        prStaRec->u2StatusCode = STATUS_CODE_ASSOC_TIMEOUT;
        eNextState = AA_STATE_IDLE;
    } else {
        prStaRec->ucTxAuthAssocRetryCount++;

        // 发送 (Re)Association Request 帧
        rStatus = assocSendReAssocReqFrame(prAdapter, prStaRec);

        if (rStatus != WLAN_STATUS_SUCCESS) {
            cnmTimerStartTimer(prAdapter,
                             &prStaRec->rTxReqDoneOrRxRespTimer,
                             TU_TO_MSEC(TX_ASSOCIATION_RETRY_TIMEOUT_TU));
        }
    }
    break;
```

**关键点**:
- `assocSendReAssocReqFrame()` 会根据 `prStaRec->fgIsReAssoc` 决定发送 Assoc 还是 ReAssoc
- Assoc 帧同样是管理帧，Q_IDX = 0x10

---

### Phase 5: 连接成功 → 激活网络

**代码位置**: `ais_fsm.c:4294` + `nic.c:1504-1571`

#### 步骤 1: 更新 BSS 信息

```c
// 在 aisFsmJoinCompleteAction() 中
nicUpdateBss(prAdapter, ucBssIndex);
```

**`nicUpdateBss()` 发送 MCU 命令**:

```c
uint32_t nicUpdateBss(struct ADAPTER *prAdapter, uint8_t ucBssIndex) {
    struct CMD_SET_BSS_INFO rCmdSetBssInfo;

    // 填充 BSS 参数
    rCmdSetBssInfo.ucBssIndex = ucBssIndex;
    rCmdSetBssInfo.ucConnectionState = prBssInfo->eConnectionState;
    rCmdSetBssInfo.ucCurrentOPMode = prBssInfo->eCurrentOPMode;
    rCmdSetBssInfo.ucSSIDLen = prBssInfo->ucSSIDLen;
    COPY_MAC_ADDR(rCmdSetBssInfo.aucBSSID, prBssInfo->aucBSSID);
    rCmdSetBssInfo.u2OperationalRateSet = ...;
    rCmdSetBssInfo.ucAuthMode = ...;
    rCmdSetBssInfo.ucEncStatus = ...;

    // 发送 Legacy CMD
    return wlanSendSetQueryCmd(prAdapter,
                              CMD_ID_SET_BSS_INFO,  // 0x12
                              TRUE, FALSE, FALSE,
                              NULL, NULL,
                              sizeof(struct CMD_SET_BSS_INFO),
                              (uint8_t *)&rCmdSetBssInfo, NULL, 0);
}
```

**Legacy → UniCmd 转换** (在 `nic_uni_cmd_event.c:1865-1952`):

```c
uint32_t nicUniCmdSetBssInfo(struct ADAPTER *ad, ...) {
    // 分配 UNI_CMD_ID_BSSINFO 命令
    entry = nicUniCmdAllocEntry(ad, UNI_CMD_ID_BSSINFO, ...);

    // 添加多个 TAG (TLV 结构)
    // - UNI_CMD_BSSINFO_TAG_BASIC (0x00)
    // - UNI_CMD_BSSINFO_TAG_RA (0x01) — Rate
    // - UNI_CMD_BSSINFO_TAG_SEC (0x02) — Security
    // - UNI_CMD_BSSINFO_TAG_QBSS (0x03)
    // - UNI_CMD_BSSINFO_TAG_SAP (0x04)
    // - UNI_CMD_BSSINFO_TAG_HE (0x08) — HE parameters
    // ...
}
```

#### 步骤 2: 激活网络

```c
// 在 aisFsmStateInit_JOIN() 之前或之后调用
nicActivateNetwork(prAdapter, ucBssIndex);
```

**`nicActivateNetwork()` 发送 MCU 命令**:

```c
uint32_t nicActivateNetwork(struct ADAPTER *prAdapter, uint8_t ucBssIndex) {
    struct CMD_BSS_ACTIVATE_CTRL rCmdActivateCtrl;

    rCmdActivateCtrl.ucBssIndex = ucBssIndex;
    rCmdActivateCtrl.ucActive = 1;
    rCmdActivateCtrl.ucNetworkType = prBssInfo->eNetworkType;
    rCmdActivateCtrl.ucOwnMacAddrIndex = prBssInfo->ucOwnMacIndex;
    COPY_MAC_ADDR(rCmdActivateCtrl.aucBssMacAddr, prBssInfo->aucOwnMacAddr);
    rCmdActivateCtrl.ucBMCWlanIndex = prBssInfo->ucBMCWlanIndex;

    return wlanSendSetQueryCmd(prAdapter,
                              CMD_ID_BSS_ACTIVATE_CTRL,  // 0x11
                              TRUE, FALSE, FALSE,
                              NULL, NULL,
                              sizeof(struct CMD_BSS_ACTIVATE_CTRL),
                              (uint8_t *)&rCmdActivateCtrl, NULL, 0);
}
```

**Legacy → UniCmd 转换** (在 `nic_uni_cmd_event.c:638-728`):

```c
uint32_t nicUniCmdBssActivateCtrl(struct ADAPTER *ad, ...) {
    // 1. 发送 UNI_CMD_ID_DEVINFO (0x01)
    dev_entry = nicUniCmdAllocEntry(ad, UNI_CMD_ID_DEVINFO, ...);
    // TAG: UNI_CMD_DEVINFO_TAG_ACTIVE (0x00)
    //   - ucActive = 1
    //   - ucBssIndex = ...

    // 2. 发送 UNI_CMD_ID_BSSINFO (0x02)
    bss_entry = nicUniCmdAllocEntry(ad, UNI_CMD_ID_BSSINFO, ...);
    // TAG: UNI_CMD_BSSINFO_TAG_BASIC (0x00)
    //   - ucActive = 1
    //   - ucOwnMacIdx = ...
    //   - ucBMCWlanIdx = ...
}
```

#### 步骤 3: 通知连接完成

```c
nicPmIndicateBssConnected(prAdapter, prAisBssInfo->ucBssIndex);
```

---

## 三、MCU 命令映射表

### Legacy CMD → UniCmd 映射

| Legacy CMD ID | Hex | UniCmd ID | Hex | 说明 |
|--------------|-----|-----------|-----|------|
| `CMD_ID_BSS_ACTIVATE_CTRL` | 0x11 | `UNI_CMD_ID_DEVINFO`<br>`UNI_CMD_ID_BSSINFO` | 0x01<br>0x02 | 激活/停用 BSS |
| `CMD_ID_SET_BSS_INFO` | 0x12 | `UNI_CMD_ID_BSSINFO` | 0x02 | 更新 BSS 参数 |
| `CMD_ID_UPDATE_STA_RECORD` | 0x13 | `UNI_CMD_ID_STAREC_INFO` | 0x03 | 更新 STA Record |
| `EXT_CMD_ID_STAREC_UPDATE` | 0x25 | `UNI_CMD_ID_STAREC_INFO` | 0x03 | 更新 STA Record (扩展) |
| `EXT_CMD_ID_CHANNEL_SWITCH` | 0x08 | (由 CNM 管理) | - | 信道切换 |
| - | - | `UNI_CMD_ID_RA` | 0x2F | Rate Adaptation |

### UNI_CMD_ID_BSSINFO (0x02) 的 TAG 列表

| TAG ID | 名称 | 用途 |
|--------|------|------|
| 0x00 | `UNI_CMD_BSSINFO_TAG_BASIC` | 基本 BSS 信息 (BSSID, SSID, OPMode) |
| 0x01 | `UNI_CMD_BSSINFO_TAG_RA` | Rate Set (Basic Rate, Operational Rate) |
| 0x02 | `UNI_CMD_BSSINFO_TAG_SEC` | Security (Auth Mode, Encryption) |
| 0x03 | `UNI_CMD_BSSINFO_TAG_QBSS` | QoS BSS 参数 |
| 0x04 | `UNI_CMD_BSSINFO_TAG_SAP` | Soft AP 参数 |
| 0x05 | `UNI_CMD_BSSINFO_TAG_P2P` | P2P 参数 |
| 0x08 | `UNI_CMD_BSSINFO_TAG_HE` | HE (802.11ax) 参数 |
| 0x09 | `UNI_CMD_BSSINFO_TAG_BSS_COLOR` | BSS Color Info |
| 0x0D | `UNI_CMD_BSSINFO_TAG_11V_MBSSID` | Multiple BSSID |

### UNI_CMD_ID_STAREC_INFO (0x03) 的 TAG 列表

| TAG ID | 名称 | 用途 |
|--------|------|------|
| 0x00 | `UNI_CMD_STAREC_TAG_BASIC` | 基本 STA 信息 (MAC, ConnectionType) |
| 0x01 | `UNI_CMD_STAREC_TAG_HT_INFO` | HT (802.11n) 能力 |
| 0x02 | `UNI_CMD_STAREC_TAG_VHT_INFO` | VHT (802.11ac) 能力 |
| 0x03 | `UNI_CMD_STAREC_TAG_RA_UPDATE` | Rate Adaptation 更新 |
| 0x05 | `UNI_CMD_STAREC_TAG_HE_BASIC` | HE (802.11ax) 基本能力 |
| 0x06 | `UNI_CMD_STAREC_TAG_HE_6G_CAP` | HE 6GHz 能力 |
| 0x0B | `UNI_CMD_STAREC_TAG_STATE_INFO` | STA 状态 (STA_STATE_1/2/3) |
| 0x0C | `UNI_CMD_STAREC_TAG_PHY_INFO` | PHY 参数 (Bandwidth, NSS) |
| 0x10 | `UNI_CMD_STAREC_TAG_BA_OFFLOAD` | Block Ack Offload |
| 0x14 | `UNI_CMD_STAREC_TAG_UAPSD_INFO` | U-APSD 参数 |

---

## 四、与 MT7927 Linux 驱动对比

### 当前 MT7927 驱动实现状态

| 阶段 | MT7927 实现 | MT6639 参考 | 状态 |
|------|------------|------------|------|
| **初始化** | ✅ FWDL + PostFwInit | ✅ 相同 | 正常 |
| **Scan** | ✅ `UNI_CMD_ID_SCAN_REQ` | ✅ 相同 | 正常 |
| **信道请求** | ❌ **缺失** | ✅ CNM → CHANNEL_SWITCH | **缺失** |
| **add_interface** | ✅ `mt7927_add_interface()` | ✅ `nicActivateNetwork()` | 实现简化 |
| **DEVINFO** | ✅ 在 PostFwInit 中发送 | ✅ 在 BSS_ACTIVATE 中发送 | 时机不同 |
| **BSSINFO** | ✅ 在 `bss_info_changed()` 中发送 | ✅ `nicUpdateBss()` | 类似 |
| **STAREC** | ❌ **缺失** | ✅ `cnmStaRecChangeState()` | **缺失！** |
| **sta_state()** | ✅ `mt7927_sta_state()` | ✅ 类似 | 实现简化 |
| **TX Auth/Assoc** | ✅ `mt7927_mac80211_tx()` | ✅ `authSendAuthFrame()` | 正常 |

### 关键缺失步骤

#### 1. **STAREC 在 Auth 前未发送** ⚠️⚠️⚠️

**MT6639 流程**:
```c
// SAA_STATE_SEND_AUTH1:
cnmStaRecChangeState(prStaRec, STA_STATE_1);  // 发送 STAREC_UPDATE
authSendAuthFrame(AUTH_TRANSACTION_SEQ_1);    // 然后发送 auth 帧
```

**MT7927 当前实现**:
```c
// mt7927_sta_state():
case IEEE80211_STA_NOTEXIST:
case IEEE80211_STA_NONE:
    // ❌ 没有在这里发送 STAREC！
    break;

case IEEE80211_STA_AUTH:
    // ✅ 这里发送了 STAREC，但太晚了 — auth 帧已经发出去了！
    mt7927_mcu_add_sta(dev, vif, sta, true);
    break;
```

**问题**: MT7927 在 `NOTEXIST→NONE` 转换时没有发送 STAREC，导致固件不知道这个 STA 的存在，所以丢弃 auth 帧（TXFREE stat=1）。

#### 2. **信道切换命令缺失** ⚠️

MT6639 在 `AIS_STATE_REQ_CHANNEL_JOIN` 阶段通过 CNM 发送信道切换命令。MT7927 依赖 mac80211 的 `config()` 回调，但可能不够。

#### 3. **Rate Table 未初始化** ⚠️

MT6639 在 STAREC 中包含 `UNI_CMD_STAREC_TAG_RA_UPDATE` (TAG 0x03)，初始化 Rate Adaptation。MT7927 的 STAREC 实现可能缺少这个 TAG。

---

## 五、修复建议

### 建议 1: 在 NOTEXIST→NONE 时发送 STAREC ✅

**修改文件**: `src/mt7927_pci.c` → `mt7927_sta_state()`

```c
static int mt7927_sta_state(struct ieee80211_hw *hw,
                           struct ieee80211_vif *vif,
                           struct ieee80211_sta *sta,
                           enum ieee80211_sta_state old_state,
                           enum ieee80211_sta_state new_state)
{
    struct mt7927_dev *dev = hw->priv;

    dev_info(dev->dev, "STA %pM state: %d -> %d\n",
             sta->addr, old_state, new_state);

    if (old_state == IEEE80211_STA_NOTEXIST &&
        new_state == IEEE80211_STA_NONE) {
        // ✅ 添加: 在这里发送 STAREC (STA_STATE_1)
        return mt7927_mcu_add_sta(dev, vif, sta, true);
    }

    if (old_state == IEEE80211_STA_NONE &&
        new_state == IEEE80211_STA_AUTH) {
        // ✅ 更新 STAREC 状态到 STA_STATE_2
        return mt7927_mcu_add_sta(dev, vif, sta, true);
    }

    // ...
}
```

### 建议 2: 检查 STAREC 命令完整性

确保 `mt7927_mcu_add_sta()` 发送的 `UNI_CMD_ID_STAREC_INFO` 包含以下 TAG:
- `UNI_CMD_STAREC_TAG_BASIC` (0x00) — 必需
- `UNI_CMD_STAREC_TAG_STATE_INFO` (0x0B) — 设置 `ucStaState = STA_STATE_1`
- `UNI_CMD_STAREC_TAG_RA_UPDATE` (0x03) — Rate table 初始化

### 建议 3: 添加信道切换命令（可选）

在 `mt7927_bss_info_changed()` 的 `BSS_CHANGED_BSSID` 分支中，发送 `EXT_CMD_ID_CHANNEL_SWITCH` 确保固件切换到正确信道。

---

## 六、总结

### MT6639 连接流程 MCU 命令序列

```
1. [扫描阶段]
   UNI_CMD_ID_SCAN_REQ (0x16)

2. [信道请求阶段]
   EXT_CMD_ID_CHANNEL_SWITCH (0x08)  ← MT7927 可能缺失

3. [Join 开始]
   UNI_CMD_ID_STAREC_INFO (0x03)     ← 创建 STA_STATE_1 (MT7927 缺失！)
     TAG: UNI_CMD_STAREC_TAG_BASIC (0x00)
     TAG: UNI_CMD_STAREC_TAG_STATE_INFO (0x0B) → STA_STATE_1
     TAG: UNI_CMD_STAREC_TAG_RA_UPDATE (0x03)   ← Rate table (MT7927 可能缺失)

4. [发送 Auth 帧]
   (通过 TX 路径，Q_IDX=0x10, TXD PKT_FMT=0 + TXP)

5. [Auth 成功后]
   UNI_CMD_ID_STAREC_INFO (0x03)     ← 更新到 STA_STATE_2
     TAG: UNI_CMD_STAREC_TAG_STATE_INFO (0x0B) → STA_STATE_2

6. [发送 Assoc 帧]
   (通过 TX 路径)

7. [Assoc 成功后]
   UNI_CMD_ID_DEVINFO (0x01)         ← nicActivateNetwork
   UNI_CMD_ID_BSSINFO (0x02)         ← nicActivateNetwork + nicUpdateBss
     TAG: UNI_CMD_BSSINFO_TAG_BASIC (0x00)
     TAG: UNI_CMD_BSSINFO_TAG_RA (0x01)
     TAG: UNI_CMD_BSSINFO_TAG_SEC (0x02)
     TAG: UNI_CMD_BSSINFO_TAG_HE (0x08)

   UNI_CMD_ID_STAREC_INFO (0x03)     ← 更新到 STA_STATE_3
     TAG: UNI_CMD_STAREC_TAG_STATE_INFO (0x0B) → STA_STATE_3
```

### 最可能的问题根因

**MT7927 Linux 驱动在 NOTEXIST→NONE 转换时没有发送 STAREC，导致固件的 WTBL (Wireless Table) 没有这个 STA 的条目。当 auth 帧通过 DMA 提交后，固件查表发现 WLAN_IDX 无效，立即丢弃帧并返回 TXFREE (stat=1)。**

**解决方案**: 在 `mt7927_sta_state()` 的 `NOTEXIST→NONE` 分支添加 `mt7927_mcu_add_sta()` 调用，确保固件在 auth 帧发送前就知道这个 STA。
