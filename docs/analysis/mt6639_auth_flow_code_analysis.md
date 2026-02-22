# MT6639 Auth Flow 代码分析报告

## 1. MT6639 连接流程 (AIS FSM 到 auth 发送)

### 完整序列

```
AIS_STATE_IDLE
  → wpa_supplicant 发起连接请求

AIS_STATE_SEARCH
  → scanSearchBssDescByScoreForAis() 找到匹配 BSS
  → 设置 prTargetBssDesc
  → 计算 WMM/DBDC/OpNss

AIS_STATE_REQ_CHANNEL_JOIN
  → 发送 CMD_CH_PRIVILEGE (CH_REQ_TYPE_JOIN)
  → → UniCmd 转换: UNI_CMD_ID_CNM (0x27) + TAG_CH_PRIVILEGE_REQ
  → 等待 CH_GRANT 事件

(CH_GRANT 回调)
  → 记录 band_idx 等信息到 BSS_INFO

AIS_STATE_JOIN  (aisFsmStateInit_JOIN)
  → bssCreateStaRecFromBssDesc():
      分配 STA_RECORD, 设 ucStaState = STA_STATE_1 (=0)
  → cnmStaRecChangeState(STA_STATE_1):    ★★★ 关键！★★★
      1→1 不匹配 skip 条件 → 调用 cnmStaSendUpdateCmd()
      → 触发 UniCmd 转换: UNI_CMD_ID_STAREC_INFO (0x03)
  → 发送 MSG_SAA_FSM_START 消息

SAA_STATE_SEND_AUTH1
  → authSendAuthFrame() — 发送 auth frame
```

### 关键发现: nicActivateNetwork 时机

`nicActivateNetwork()` (发送 DEV_INFO + BSS_INFO) 在 **AIS_STATE_SEARCH 之前** 就已经调用！
- 初始化时: `aisFsmRunEventConnectReq()` → `SET_NET_ACTIVE()` → `nicActivateNetwork()`
- 这意味着 DEV_INFO 和 BSS_INFO(BASIC) 在 **扫描开始前** 就已发给固件
- BSS_INFO 此时 `ucConnectionState = MEDIA_STATE_DISCONNECTED (0)`

### MCU 命令完整时序

```
1. nicActivateNetwork()          — 连接请求时 (扫描前!)
   ├── UNI_CMD_ID_DEVINFO (0x01)
   │   └── TAG_ACTIVE: ucActive=1, OwnMacAddr
   └── UNI_CMD_ID_BSSINFO (0x02)
       ├── TAG_BASIC: ucActive=1, ucConnectionState=0(DISCONNECTED)
       │             ucDbdcIdx=AUTO, ConnType=INFRA_STA
       └── TAG_MLD: ucGroupMldId=NONE, ucOwnMldId=bssIdx

2. CMD_CH_PRIVILEGE              — AIS_STATE_REQ_CHANNEL_JOIN
   └── UNI_CMD_ID_CNM (0x27)
       └── TAG_CH_PRIVILEGE_REQ: channel, band, bandwidth

3. (等待 CH_GRANT 事件返回)

4. cnmStaRecChangeState(STA_STATE_1)  — aisFsmStateInit_JOIN
   └── UNI_CMD_ID_STAREC_INFO (0x03)  ★★★ auth 前唯一的 STA_REC ★★★
       ├── TAG_BASIC (0x00):
       │   ucConnectionState = STATE_CONNECTED (1)  ← 始终是 1!
       │   u4ConnectionType = CONNECTION_INFRA_AP (0x10002)
       │   u2ExtraInfo = V2 | NEWSTAREC (0x03)
       │   ucIsQBSS, u2AID, aucPeerMacAddr
       ├── TAG_HT_BASIC (0x09): u2HtCap, u2HtExtendedCap
       ├── TAG_VHT_BASIC (0x0a): u4VhtCap, VhtMcsMap
       ├── TAG_HE_BASIC (0x19): HeCapInfo (if 11ax)
       ├── TAG_HE_6G_CAP (0x17): He6gBandCapInfo (if 6GHz)
       ├── TAG_STATE_CHANGED (0x07):
       │   ucStaState = STA_STATE_1 (0)
       │   ucActionType = STA_REC_CMD_ACTION_STA (0)
       ├── TAG_PHY_INFO (0x15):
       │   u2BSSBasicRateSet, ucDesiredPhyTypeSet, ucAmpduParam, ucRCPI
       ├── TAG_RA (0x01): legacy rates + MCS bitmap
       ├── TAG_BA_OFFLOAD (0x16): BA sizes
       └── TAG_UAPSD (0x24): UAPSD params

5. authSendAuthFrame()            — SAA_STATE_SEND_AUTH1
   (帧通过 DMA 发送到固件, 固件射频发射)
```

## 2. STA_REC 连接状态值

### MT6639 使用的值

| 字段 | 值 | 说明 |
|------|-----|------|
| `ucConnectionState` (BASIC tag) | **STATE_CONNECTED = 1** | **始终为 1**, 不管 auth 前后! |
| `u2ExtraInfo` (BASIC tag) | **V2 \| NEWSTAREC = 0x03** | 新建 STA 时设 NEWSTAREC |
| `u4ConnectionType` (BASIC tag) | **CONNECTION_INFRA_AP = 0x10002** | BIT(1)\|BIT(16) |
| `ucStaState` (STATE tag) | **STA_STATE_1 = 0** | auth 前为 0 (class 1) |

### EXTRA_INFO_NEW 设置时机

MT6639 在 `nicUniCmdStaRecTagBasic()` 中 **无条件** 设置:
```c
tag->u2ExtraInfo = STAREC_COMMON_EXTRAINFO_V2 |     // BIT(0)
                   STAREC_COMMON_EXTRAINFO_NEWSTAREC; // BIT(1)
// = 0x03, 每次 STA_REC 更新都带 NEW!
```

## 3. 与我们 Linux 驱动的差异对比

### 关键差异表

| 项目 | MT6639 (正确) | 我们的驱动 (当前) | 影响 |
|------|--------------|------------------|------|
| **BASIC.ucConnectionState** | **STATE_CONNECTED (1)** | CONN_STATE_DISCONNECT (0) | ★★★ 致命差异 |
| **BASIC.u2ExtraInfo** | V2 \| NEWSTAREC (0x03) | V2 \| NEW 仅在 DISCONNECT 时 | 可能影响 WTBL 创建 |
| **STA_REC TLV 数量** | 9-10 个 TLV | 4 个 TLV | 中等影响 |
| **TAG_PHY_INFO (0x15)** | ✅ 包含 | ❌ 缺失 | 固件可能不知 PHY 能力 |
| **TAG_HT_BASIC (0x09)** | ✅ 包含 | ❌ 缺失 | HT 能力未告知固件 |
| **TAG_VHT_BASIC (0x0a)** | ✅ 包含 | ❌ 缺失 | VHT 能力未告知固件 |
| **TAG_BA_OFFLOAD (0x16)** | ✅ 包含 | ❌ 缺失 | BA 参数缺失 |
| **TAG_UAPSD (0x24)** | ✅ 包含 | ❌ 缺失 | 省电参数缺失 |
| **BSS_INFO TLV 数量** | 10-12 个 TLV (含 SEC/RATE/QBSS/SAP/MLD) | 2 个 TLV (BASIC + RLM) | 可能影响 |
| **BSS_INFO connState** | 按实际状态 (0=DISCONNECTED) | `!enable` | 匹配 |
| **BSS_INFO conn_type** | INFRA_STA | INFRA_STA | ✅ 匹配 |
| **DEV_INFO + BSS_INFO 时机** | 连接请求时 (扫描前) | add_interface 时 | 基本匹配 |
| **STA_REC 时机** | CH_GRANT 后, auth 前 | sta_state NOTEXIST→NONE | 匹配 |

### 最关键差异分析

#### 差异 1: ucConnectionState = 1 vs 0 ★★★★★

MT6639 驱动在 `nicUniCmdStaRecTagBasic()` 第 2283 行:
```c
tag->ucConnectionState = STATE_CONNECTED;  // = 1
```

我们的驱动在 `mt7927_mcu_sta_update()` 第 2445-2452 行:
```c
if (enable) {
    req.basic.conn_state = conn_state;  // NOTEXIST→NONE 时传入 CONN_STATE_DISCONNECT (0)
```

**结论**: MT6639 固件可能要求 STAREC_BASIC 的 `ucConnectionState` 为 **STATE_CONNECTED (1)** 才会允许该 STA 的帧通过。我们传入 0 (DISCONNECT), 固件可能认为该 STA 未连接, 直接丢弃其帧 (TXFREE stat=1 = DISCARD_BY_FW)。

#### 差异 2: EXTRA_INFO_NEW 始终设置

MT6639 **每次** STA_REC 都带 NEWSTAREC, 我们只在 DISCONNECT 时设。这可能影响 WTBL 条目创建/更新逻辑。

#### 差异 3: 缺少 PHY/HT/VHT TLV

固件可能需要知道目标 AP 的 PHY 能力才能正确配置射频参数来发送帧。缺少这些 TLV 可能导致固件无法确定用什么调制方式/速率发送。

## 4. BSS_INFO 完整 TLV 列表

MT6639 的 `nicUpdateBss()` → `nicUniCmdSetBssInfo()` 发送以下 TLV:

| Tag | 名称 | 值 | 说明 |
|-----|------|-----|------|
| 0x00 | BASIC | Active=1, ConnState=CONNECTED, BSSID, WmmIdx, PhyMode | 基础信息 |
| 0x02 | RLM | Channel, BW, SCO, TxStream, RxStream | 射频参数 |
| 0x03 | PROTECT | ErpProtectMode, ObssGfExist, etc. | 保护模式 |
| 0x17 | IFS_TIME | IFS timing | 帧间隔 |
| 0x0B | RATE | OperationalRateSet, BSSBasicRateSet | 速率集 |
| 0x10 | SEC | ucAuthMode, ucEncStatus | 安全模式 |
| 0x0F | QBSS | fgIsQBSS | QoS BSS |
| 0x0D | SAP | (P2P specific) | P2P |
| 0x0E | P2P | (P2P specific) | P2P |
| 0x05 | HE | (if 11ax) | HE 参数 |
| 0x04 | BSS_COLOR | (if 11ax) | BSS Color |
| 0x06 | 11V_MBSSID | MBSSID | MBSSID |
| 0x0C | WAPI | ucWapiMode | WAPI |
| 0x1A | MLD | GroupMldId, OwnMldId | MLD |

我们只发送: **BASIC (0x00) + RLM (0x02)**

## 5. 修复建议 (按优先级)

### P0 — 立即修复 (最可能修复 stat=1)

1. **STA_REC BASIC.ucConnectionState 改为 STATE_CONNECTED (1)**
   - 当前: `conn_state = CONN_STATE_DISCONNECT (0)`
   - 修改: 新建 STA 时 `conn_state = 1`, 删除时 `conn_state = 0`
   - 或者: BASIC 的 ucConnectionState 字段始终写 1 (像 MT6639 一样)

2. **STA_REC BASIC.u2ExtraInfo 始终设 NEWSTAREC**
   - MT6639 每次都设, 不只是 DISCONNECT 时

### P1 — 强烈建议

3. **添加 TAG_PHY_INFO (0x15)**: BSSBasicRateSet + DesiredPhyTypeSet + AmpduParam + RCPI
4. **添加 TAG_HT_BASIC (0x09)**: HtCapInfo + HtExtendedCap
5. **添加 TAG_VHT_BASIC (0x0a)**: VhtCapInfo + VhtMcsMap

### P2 — 可选优化

6. 添加 TAG_BA_OFFLOAD (0x16)
7. 添加 TAG_UAPSD (0x24)
8. BSS_INFO 添加 TAG_RATE (0x0B) + TAG_SEC (0x10) + TAG_QBSS (0x0F)
9. BSS_INFO 添加 TAG_MLD (0x1A) — MT6639 WiFi 7 可能需要

## 6. STA_STATE 与 ucStaState 映射

| 阶段 | STA_STATE | ucStaState | 说明 |
|------|-----------|------------|------|
| 创建 (auth 前) | STA_STATE_1 | 0 | Accept Class 1 frames |
| Auth 完成 | STA_STATE_2 | 1 | **不发送 FW 更新** (skip) |
| Assoc 完成 | STA_STATE_3 | 2 | 发送 FW 更新 + qmActivateStaRec |

注意: MT6639 的 `cnmStaRecChangeState()` 在 STA_STATE_2 转换时 **不发送固件命令** (line 933-938):
```c
if ((ucNewState == STA_STATE_2 && prStaRec->ucStaState != STA_STATE_3)
    || (ucNewState == STA_STATE_1 && prStaRec->ucStaState == STA_STATE_2)) {
    prStaRec->ucStaState = ucNewState;
    return;  // NO FW update for 1→2 and 2→1
}
```

## 附: 关键数值速查

```
STATE_CONNECTED          = 1
STATE_DISCONNECTED       = 0
STAREC_COMMON_EXTRAINFO_V2       = BIT(0) = 0x01
STAREC_COMMON_EXTRAINFO_NEWSTAREC = BIT(1) = 0x02
CONNECTION_INFRA_AP      = BIT(1) | BIT(16) = 0x10002
CONNECTION_INFRA_STA     = BIT(0) | BIT(16) = 0x10001
STA_STATE_1              = 0  (Class 1 — before auth)
STA_STATE_2              = 1  (Class 2 — after auth)
STA_STATE_3              = 2  (Class 3 — after assoc)
STA_REC_CMD_ACTION_STA   = 0
```
