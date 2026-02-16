# MT6639 管理帧 TX 完整路径分析

> 基于 MT6639 Android 驱动源码的深入分析，针对 auth 帧 TX 失败问题。

---

## 1. 管理帧创建与 TC 分配

### Auth 帧创建流程

```
authSendAuthFrame() [mgmt/auth.c:299]
  → cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen)  // 分配 MSDU_INFO
  → authComposeAuthFrameHeaderAndFF()  [auth.c:133]  // 填充 802.11 header + fixed fields
  → TX_SET_MMPDU() = nicTxSetMngPacket()  [nic_tx.c:3816]
  → nicTxEnqueueMsdu()  [nic_tx.c:3492]
```

### `nicTxSetMngPacket()` 设置关键字段 (nic_tx.c:3816-3863)

```c
prMsduInfo->fgIs802_11 = TRUE;          // ★ 802.11 帧
prMsduInfo->ucPacketType = TX_PACKET_TYPE_MGMT;  // ★ 管理帧类型
prMsduInfo->eSrc = TX_PACKET_MGMT;      // ★ 来源标记
prMsduInfo->ucRateMode = ucRateMode;    // 由调用者传入，auth 用 MSDU_RATE_MODE_AUTO
prMsduInfo->u4Option = 0;
prMsduInfo->cPowerOffset = 0;
prMsduInfo->ucPID = NIC_TX_DESC_PID_RESERVED;
```

### TC 分配: nicTxProcessMngPacket() (nic_tx.c:3246-3303)

```c
// line 3273-3277: MMPDU 强制使用 TC4
if (prMsduInfo->fgMgmtUseDataQ)
    prMsduInfo->ucTC = TC0_INDEX;     // 特殊情况走数据队列
else
    prMsduInfo->ucTC = TC4_INDEX;     // ★ 标准管理帧走 TC4

prMsduInfo->fgIsTXDTemplateValid = FALSE;  // 管理帧不用模板

// line 3283-3297: 固定速率设置
if (prMsduInfo->ucRateMode == MSDU_RATE_MODE_AUTO) {
    nicTxSetPktLowestFixedRate(prAdapter, prMsduInfo);  // ★ 使用最低速率
}
```

### TC4 → PORT/QUEUE 映射 (nic_tx.c:96-105)

```c
static const struct TX_RESOURCE_CONTROL arTcResourceControl[TC_NUM] = {
    {PORT_INDEX_LMAC, MAC_TXQ_AC0_INDEX, HIF_TX_AC0_INDEX},  // TC0
    {PORT_INDEX_LMAC, MAC_TXQ_AC1_INDEX, HIF_TX_AC1_INDEX},  // TC1
    {PORT_INDEX_LMAC, MAC_TXQ_AC2_INDEX, HIF_TX_AC2_INDEX},  // TC2
    {PORT_INDEX_LMAC, MAC_TXQ_AC3_INDEX, HIF_TX_AC3_INDEX},  // TC3
    {PORT_INDEX_MCU,  MCU_Q0_INDEX,      HIF_TX_CPU_INDEX},   // ★ TC4 → MCU port
};
```

**结论**:
- `ucTarPort = nicTxGetTxDestPortIdxByTc(TC4)` = **PORT_INDEX_MCU** (1)
- `ucTarQueue = nicTxGetTxDestQIdxByTc(TC4)` = **MCU_Q0_INDEX** (0)
- HIF queue = **HIF_TX_CPU_INDEX** → CMD ring

---

## 2. TXD 完整格式 (DW0-DW7)

管理帧 TXD 由 `nic_txd_v3_compose()` (nic_txd_v3.c:219-462) 构建：

### DW0 — Byte Count + EtherType Offset + PKT_FORMAT + Q_IDX

```c
// PKT_FORMAT: line 1865-1866 (nic_tx.c)
if (prMsduInfo->eSrc == TX_PACKET_MGMT)
    prMsduInfo->ucPacketFormat = TXD_PKT_FORMAT_COMMAND;  // ★ PKT_FMT = 2
// → HAL_MAC_CONNAC3X_TXD_SET_PKT_FORMAT(prTxDesc, 2)

// Q_IDX: line 268-273 (nic_txd_v3.c)
ucTarQueue = nicTxGetTxDestQIdxByTc(TC4) = MCU_Q0_INDEX = 0;
// ucTarPort = PORT_INDEX_MCU, 所以不加 WMM offset
// → HAL_MAC_CONNAC3X_TXD_SET_QUEUE_INDEX(prTxDesc, 0)  // ★ Q_IDX = 0

// TX_BYTE_COUNT: fillConnac3xTxDescTxByteCount (cmm_asic_connac3x.c:1114)
u4TxByteCount = NIC_TX_DESC_LONG_FORMAT_LENGTH + prMsduInfo->u2FrameLength;
// 管理帧: 不加 u4ExtraTxByteCount (只对 TX_PACKET_TYPE_DATA 加)
// → TxByteCount = 32 + frame_length
```

**DW0 位域**:
| 字段 | Bits | 值 | 说明 |
|------|------|-----|------|
| TX_BYTE_COUNT | [15:0] | 32+帧长 | TXD长度+802.11帧长 |
| ETHER_TYPE_OFFSET | [22:16] | 0 | 802.11帧不设置 |
| PKT_FORMAT | [24:23] | **2** | TXD_PKT_FORMAT_COMMAND |
| QUEUE_INDEX | [31:25] | **0** | MCU_Q0_INDEX |

### DW1 — MLD_ID + TGID + HDR_FORMAT + TID + OWN_MAC + FIXED_RATE

```c
// MLD_ID (WLAN_IDX): line 291-295
prMsduInfo->ucWlanIndex = nicTxGetWlanIdx(prAdapter, ucBssIdx, ucStaRecIdx);
HAL_MAC_CONNAC3X_TXD_SET_MLD_ID(prTxDesc, prMsduInfo->ucWlanIndex);

// HEADER_FORMAT: line 301-307 (fgIs802_11 = TRUE)
HAL_MAC_CONNAC3X_TXD_SET_HEADER_FORMAT(prTxDesc,
    HEADER_FORMAT_802_11_NORMAL_MODE);  // ★ HDR_FORMAT = 2
HAL_MAC_CONNAC3X_TXD_SET_802_11_HEADER_LENGTH(prTxDesc,
    prMsduInfo->ucMacHeaderLength >> 1);  // 24/2 = 12
HAL_MAC_CONNAC3X_TXD_SET_HEADER_PADDING(prTxDesc,
    NIC_TX_DESC_HEADER_PADDING_TAIL_PAD);  // = 1

// 注意: line 315-316 又覆盖了 HEADER_PADDING:
HAL_MAC_CONNAC3X_TXD_SET_HEADER_PADDING(prTxDesc,
    NIC_TX_DESC_HEADER_PADDING_LENGTH);  // = 0
// ★ 所以最终 HEADER_PADDING = 0

// TID_MGMT_TYPE: line 319-321 (fgIs802_11 = TRUE)
HAL_MAC_CONNAC3X_TXD_SET_TID_MGMT_TYPE(prTxDesc,
    TYPE_NORMAL_MANAGEMENT);  // ★ TID = 0

// OWN_MAC: line 328-329
HAL_MAC_CONNAC3X_TXD_SET_OWN_MAC_INDEX(prTxDesc,
    prBssInfo->ucOwnMacIndex);

// FIXED_RATE: 由 nicTxSetPktLowestFixedRate 设置 ucRateMode = MSDU_RATE_MODE_MANUAL_DESC
// → line 453-456: HAL_MAC_CONNAC3X_TXD_SET_FIXED_RATE_ENABLE(prTxDesc)  ★
//   HAL_MAC_CONNAC3X_TXD_SET_FIXED_RATE_IDX(prTxDesc, 0)
```

**DW1 位域**:
| 字段 | Bits | 值 | 说明 |
|------|------|-----|------|
| MLD_ID | [11:0] | WLAN_IDX | 来自 StaRec/BssInfo |
| TGID | [13:12] | 0 | 未设置 |
| HEADER_FORMAT | [15:14] | **2** | 802.11 Normal Mode |
| HDR_LENGTH | [20:16] | 12 | 24字节/2 |
| TID_MGMT_TYPE | [24:21] | **0** | TYPE_NORMAL_MANAGEMENT |
| OWN_MAC | [30:25] | OwnMacIdx | 来自 BssInfo |
| FIXED_RATE | [31] | **1** | 使用固定速率 |

### DW2 — TYPE + SUB_TYPE + HEADER_PADDING + REMAINING_LIFE_TIME + POWER_OFFSET

```c
// TYPE/SUB_TYPE: line 333-346 (fgIs802_11 = TRUE)
// 从 802.11 header 的 FrameCtrl 中提取
HAL_MAC_CONNAC3X_TXD_SET_TYPE(prTxDesc,
    (prWlanHeader->u2FrameCtrl & MASK_FC_TYPE) >> 2);      // Auth: 0 (Management)
HAL_MAC_CONNAC3X_TXD_SET_SUB_TYPE(prTxDesc,
    (prWlanHeader->u2FrameCtrl & MASK_FC_SUBTYPE) >> 4);   // Auth: 11 (0xB)

// HEADER_PADDING: line 315-316
HAL_MAC_CONNAC3X_TXD_SET_HEADER_PADDING(prTxDesc, 0);  // = 0

// REMAINING_LIFE_TIME: line 359-363
// TC4 → arTcTrafficSettings[4] → NIC_TX_MGMT_REMAINING_TX_TIME = 2000ms
HAL_MAC_CONNAC3X_TXD_SET_REMAINING_LIFE_TIME_IN_MS(prTxDesc, 2000);

// POWER_OFFSET: line 366-367
HAL_MAC_CONNAC3X_TXD_SET_POWER_OFFSET(prTxDesc, 0);  // prMsduInfo->cPowerOffset = 0
```

**DW2 位域**:
| 字段 | Bits | 值 | 说明 |
|------|------|-----|------|
| SUB_TYPE | [3:0] | 0xB | Auth 子类型 |
| TYPE | [5:4] | 0 | Management |
| FORCE_RTS_CTS | [9] | 0 | |
| HDR_PAD_LENGTH | [10] | 0 | |
| HDR_PAD_MODE | [11] | 0 | |
| REMAINING_LIFE_TIME | [25:16] | ~31 | 2000ms in 64TU units |
| POWER_OFFSET | [31:26] | 0 | |

### DW3 — NO_ACK + PROTECTION + BMC + REMAINING_TX_COUNT

```c
// REMAINING_TX_COUNT: line 374-380
// TC4 → arTcTrafficSettings[4] → NIC_TX_MGMT_DEFAULT_RETRY_COUNT_LIMIT = 30
HAL_MAC_CONNAC3X_TXD_SET_REMAINING_TX_COUNT(prTxDesc, 30);

// nic_txd_v3_fill_by_pkt_option: line 125-206
// Auth 帧: u4Option = 0 (除非特殊设置), 所以大部分 option 不生效
```

**DW3 位域**:
| 字段 | Bits | 值 | 说明 |
|------|------|-----|------|
| NO_ACK | [0] | 0 | 需要 ACK |
| PROTECTION | [1] | 0 | Auth1 不加密 |
| BMC | [4] | 0 | 单播 |
| REMAINING_TX_COUNT | [15:11] | **30** | 30次重试 |
| BA_DISABLE | [28] | 0 | |

### DW4 — PN (全零)

### DW5 — PID + TXS

```c
// line 421-434:
if (prMsduInfo->pfTxDoneHandler) {
    prMsduInfo->ucPID = nicTxAssignPID(...);
    HAL_MAC_CONNAC3X_TXD_SET_PID(prTxDesc, prMsduInfo->ucPID);
    HAL_MAC_CONNAC3X_TXD_SET_TXS_TO_MCU(prTxDesc);  // ★ TXS 发给 MCU
}
```

### DW6 — DIS_MAT + MSDU_COUNT + FIXED_RATE_IDX

```c
// line 440-441: 管理帧禁用 MLD 地址转换
if (prMsduInfo->ucPacketType == TX_PACKET_TYPE_MGMT)
    HAL_MAC_CONNAC3X_TXD_SET_DIS_MAT(prTxDesc);  // ★ DIS_MAT = 1

// line 444:
HAL_MAC_CONNAC3X_TXD_SET_MSDU_COUNT(prTxDesc, 1);  // ★ MSDU_CNT = 1

// line 454-456: (MSDU_RATE_MODE_MANUAL_DESC)
HAL_MAC_CONNAC3X_TXD_SET_FIXED_RATE_IDX(prTxDesc, 0);  // ★ FIXED_RATE_IDX = 0
```

**DW6 位域**:
| 字段 | Bits | 值 | 说明 |
|------|------|-----|------|
| DAS | [2] | 0 | |
| DIS_MAT | [3] | **1** | 禁用 MLD→Link 地址转换 |
| MSDU_COUNT | [9:4] | **1** | |
| FIXED_RATE_IDX | [21:16] | **0** | |

### DW7 — TXD_LENGTH ★★★

```c
// line 447: ★★★ 关键！
HAL_MAC_CONNAC3X_TXD_SET_TXD_LENGTH(prTxDesc, TXD_LEN_1_PAGE);
// TXD_LEN_1_PAGE = 1 (enum, bits[31:30] = 0b01)
```

**DW7 位域**:
| 字段 | Bits | 值 | 说明 |
|------|------|-----|------|
| TXD_LENGTH | [31:30] | **1** | ★ TXD_LEN_1_PAGE |

---

## 3. 帧数据布局 (★ 关键发现)

### 管理帧 vs 数据帧的 TXD 后续数据

在 `nicTxFillDesc()` (nic_tx.c:1859-1877):

```c
// line 1864-1868: 设置 PKT_FORMAT
if (prMsduInfo->eSrc == TX_PACKET_MGMT)
    prMsduInfo->ucPacketFormat = TXD_PKT_FORMAT_COMMAND;  // 管理帧: PKT_FMT=2
else
    prMsduInfo->ucPacketFormat = prChipInfo->ucPacketFormat;  // 数据帧: PKT_FMT=0或1

// line 1873-1877: TXP (TxD Append) 只给数据帧！
if (prMsduInfo->ucPacketType == TX_PACKET_TYPE_DATA)
    nicTxComposeDescAppend(prAdapter, prMsduInfo,
                           prTxDescBuffer + u4TxDescLength);
// ★★★ 管理帧不走这个分支 → 没有 TXP!
```

### 管理帧在 HIF 层的提交 (nicTxCmd)

管理帧通过 `nicTxEnqueueMsdu()` → `nicTxCmd()` (nic_tx.c:2488-2523):

```c
case COMMAND_TYPE_MANAGEMENT_FRAME:
    prMsduInfo = prCmdInfo->prMsduInfo;
    ASSERT(prMsduInfo->fgIs802_11 == TRUE);

    prCmdInfo->pucTxd = prMsduInfo->aucTxDescBuffer;  // ★ TXD (32 bytes)
    prCmdInfo->u4TxdLen = NIC_TX_DESC_LONG_FORMAT_LENGTH;  // = 32

    prCmdInfo->pucTxp = prMsduInfo->prPacket;  // ★ 802.11 帧数据 (inline)
    prCmdInfo->u4TxpLen = prMsduInfo->u2FrameLength;  // 帧长度

    HAL_WRITE_TX_CMD(prAdapter, prCmdInfo, ucTC);  // 走 CMD ring!
```

### 帧布局总结

**管理帧 (auth)**: `[TXD 32字节] + [802.11 帧数据 (header + body)]`
- **没有 TXP** (TxD Append / scatter-gather 指针)
- **没有 padding** (MAC_TX_RESERVED_FIELD = 0)
- TXD 和帧数据通过 `halWpdmaWriteCmd()` 拼接到一个 DMA buffer

**数据帧**: `[TXD 32字节] + [TXP (txd_append)] + [帧数据]`
- 有 TXP (scatter-gather), 通过 token/DMA map 方式提交

---

## 4. HIF 提交层

### halWpdmaWriteCmd() (hal_pdma.c:2485-2582)

管理帧和 MCU 命令都通过这个函数提交到 CMD ring:

```c
bool halWpdmaWriteCmd(struct GLUE_INFO *prGlueInfo,
                      struct CMD_INFO *prCmdInfo, uint8_t ucTC)
{
    uint16_t u2Port = TX_RING_CMD_IDX_2;  // ★ ring index = 2 (映射到HW TX ring 15)

    // 分配运行时内存
    u4TotalLen = prCmdInfo->u4TxdLen + prCmdInfo->u4TxpLen;
    pucSrc = prMemOps->allocRuntimeMem(u4TotalLen);

    // ★ 关键: TXD + 帧数据拷贝到连续 buffer
    prMemOps->copyCmd(prHifInfo, pTxCell, pucSrc,
                      prCmdInfo->pucTxd, prCmdInfo->u4TxdLen,   // TXD
                      prCmdInfo->pucTxp, prCmdInfo->u4TxpLen);  // 802.11 帧

    // DMA 描述符: 单 buffer, 所有数据在 SDPtr0/SDLen0
    pTxD->SDPtr0 = pTxCell->PacketPa;
    pTxD->SDLen0 = u4TotalLen;   // ★ TXD + 帧数据总长度
    pTxD->SDPtr1 = 0;
    pTxD->SDLen1 = 0;
    pTxD->LastSec0 = 1;
    pTxD->LastSec1 = 0;
    pTxD->Burst = 0;
    pTxD->DMADONE = 0;

    // 更新 CPU index
    INC_RING_INDEX(prTxRing->TxCpuIdx, TX_RING_SIZE);
    kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);
}
```

### 关键: SFmode (Store-and-Forward)

CMD ring 使用 **SFmode** — 所有数据 inline 在 DMA buffer 中:
- `SDLen0` = TXD + 802.11 帧数据总长度
- `SDPtr1` = 0, `SDLen1` = 0 — 没有第二段
- 这与数据 ring 的 CT mode (只传 TXD+TXP, 数据通过 token 引用) 完全不同

### halWpdmaWriteData() vs halWpdmaWriteCmd() 对比

| 特性 | halWpdmaWriteCmd (管理帧) | halWpdmaWriteData (数据帧) |
|------|--------------------------|---------------------------|
| Ring | TX_RING_CMD_IDX_2 (ring 15) | TX_RING_DATA0_IDX_0 (ring 0) |
| 模式 | SFmode (inline) | CT mode (token) |
| SDLen0 | TXD + payload 总长 | TXD + txd_append 长度 |
| SDPtr1 | 0 (不使用) | 0 (数据通过 token 引用) |
| DMA 方式 | allocRuntimeMem + copyCmd | token + DMA map |

---

## 5. PCIe HIF 特殊处理

### Ring 映射 (hif_pdma.h:227)

```c
enum {
    TX_RING_DATA0_IDX_0 = 0,  // 数据 ring 0
    TX_RING_DATA1_IDX_1,      // 数据 ring 1
    TX_RING_CMD_IDX_2,        // ★ CMD ring = index 2 → HW ring 15
    TX_RING_FWDL_IDX_3,      // FWDL ring
    TX_RING_WA_CMD_IDX_4,    // WA CMD ring (CONNAC2X)
};
```

### PCIe CMD ring 的 DMA 描述符格式

```c
struct TXD_STRUCT {
    uint32_t SDPtr0;      // DMA buffer 物理地址低32位
    uint32_t SDPtr0Ext;   // 高4位地址扩展
    uint32_t SDLen0;      // buffer 长度
    uint32_t SDPtr1;      // 第二段 (CMD ring 不用)
    uint32_t SDLen1;      // 第二段长度 (CMD ring = 0)
    uint32_t LastSec0;    // = 1
    uint32_t LastSec1;    // = 0
    uint32_t Burst;       // = 0
    uint32_t DMADONE;     // = 0 (由 HW 设置为1表示完成)
};
```

### PCIe vs AXI/USB 差异

PCIe 通过 `kalDevWriteCmd()` (kal_pdma.c:526):
```c
u_int8_t kalDevWriteCmd(struct GLUE_INFO *prGlueInfo,
                        struct CMD_INFO *prCmdInfo, uint8_t ucTC)
{
    if (nicSerIsTxStop(prGlueInfo->prAdapter))
        return kalDevWriteCmdByQueue(prGlueInfo, prCmdInfo, ucTC);
    return halWpdmaWriteCmd(prGlueInfo, prCmdInfo, ucTC);  // 正常路径
}
```

对于 PCIe:
- CMD ring 直接使用 `allocRuntimeMem` (dma_alloc_coherent) 分配 DMA buffer
- TXD + payload 拷贝到 coherent buffer
- 写 CPU index 寄存器触发 DMA 传输

---

## 6. MCU 命令 vs 管理帧区分

### 两者都走 CMD ring (TX ring 15) + PKT_FMT=2

固件通过 **HEADER_FORMAT** 字段区分:

| 帧类型 | PKT_FMT | HDR_FORMAT | Q_IDX | 内容 |
|--------|---------|------------|-------|------|
| MCU 命令 | 2 | 0 (NON_802_11) | 0 | TXD + MCU cmd payload |
| **管理帧 (auth)** | **2** | **2 (802_11_NORMAL)** | **0** | **TXD + 802.11 帧** |
| EAPOL (安全帧) | 2 | 0 (NON_802_11) | 0 | TXD + Ethernet 帧 |

### 证据

1. MCU 命令: `nicTxInitCmd()` (nic_tx.c:3107) — 使用 `HAL_WRITE_HIF_TXD`, HDR_FORMAT 通过 HIF TXD header 设置

2. 管理帧: `nic_txd_v3_compose()` line 301-303:
```c
if (prMsduInfo->fgIs802_11) {
    HAL_MAC_CONNAC3X_TXD_SET_HEADER_FORMAT(prTxDesc,
        HEADER_FORMAT_802_11_NORMAL_MODE);  // = 2
```

3. EAPOL 安全帧: `nic_txd_v3_compose_security_frame()` line 515-516:
```c
HAL_MAC_CONNAC3X_TXD_SET_HEADER_FORMAT(prTxDesc,
    HEADER_FORMAT_NON_802_11);  // = 0
```

### 固件区分方式

- **HDR_FORMAT = 0 (NON_802_11)**: 固件将 TXD 后面的数据视为 MCU command 或 Ethernet 帧
- **HDR_FORMAT = 2 (802_11_NORMAL)**: 固件将 TXD 后面的数据视为原始 802.11 帧, 直接发送到空中

此外, **TID_MGMT_TYPE** 字段 (DW1 bits[24:21]) 也提供信息:
- TYPE_NORMAL_MANAGEMENT (0): 普通管理帧
- TYPE_TIMING_MEASUREMENT (1): 时间测量帧
- TYPE_ADDBA_FRAME (2): BA 帧

以及 **TYPE** (DW2 bits[5:4]) 和 **SUB_TYPE** (DW2 bits[3:0]) 提供精确的帧类型。

---

## 7. DMASHDL 与 TX 队列配置

### TC4/CMD ring 的流量设置 (nic_tx.c:131-167)

```c
static const struct TX_TC_TRAFFIC_SETTING arTcTrafficSettings[NET_TC_NUM] = {
    // TC0 (AC_BE): 2000ms, 7次重试
    {NIC_TX_DESC_LONG_FORMAT_LENGTH, NIC_TX_AC_BE_REMAINING_TX_TIME,
     NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT},
    // TC1-TC3 类似...

    // ★ TC4 (MGMT): 2000ms, 30次重试
    {NIC_TX_DESC_LONG_FORMAT_LENGTH, NIC_TX_MGMT_REMAINING_TX_TIME,   // 2000ms
     NIC_TX_MGMT_DEFAULT_RETRY_COUNT_LIMIT},                          // 30
};
```

### DMASHDL 初始化

MT6639 使用 `halDmashdlInit()` (chips/common/cmm_asic_connac.c), 为 TC4/CMD ring 配置:
- Group 分配: TC4 对应独立的 DMASHDL group
- Quota 配置: CMD ring 有专用的 PSE/PLE 页面配额

---

## 8. Auth 帧特殊 TXD 设置

### Auth 帧走普通管理帧路径

Auth 帧 **不是** EAPOL (安全帧), 走 `nic_txd_v3_compose()` 的普通路径:

```c
// nic_txd_v3_compose_security_frame() — 给 EAPOL 用, 不给 auth
//   特征: HDR_FORMAT = NON_802_11, PKT_FMT = COMMAND, FIXED_RATE_ENABLE
//   但 auth 帧走 nic_txd_v3_compose() 的 fgIs802_11=TRUE 分支

// auth 帧的完整 TXD 设置:
// DW0: PKT_FMT=2, Q_IDX=0, TxByteCount=32+帧长
// DW1: HDR_FORMAT=2, HDR_LEN=12, TID=0, OWN_MAC=BssInfo, FIXED_RATE=1
// DW2: TYPE=0(Mgmt), SUB_TYPE=0xB(Auth), LIFE_TIME=~31, POWER_OFFSET=0
// DW3: REM_TX_COUNT=30
// DW5: PID (if TxDone handler), TXS_TO_MCU
// DW6: DIS_MAT=1, MSDU_CNT=1, FIXED_RATE_IDX=0
// DW7: TXD_LENGTH=1 (TXD_LEN_1_PAGE) ★
```

### 固定速率设置

`nicTxSetPktLowestFixedRate()` (nic_tx.c:4036-4095):
- 设置 `ucRateMode = MSDU_RATE_MODE_MANUAL_DESC`
- 选择 StaRec 或 BssInfo 的 `u2HwDefaultFixedRateCode`
- 在 `nic_txd_v3_compose()` 中: DW1 bit31 = FIXED_RATE_ENABLE, DW6 FIXED_RATE_IDX = 0

---

## 总结: 我们的驱动需要做哪些改动

### 当前驱动的问题 vs MT6639 正确行为

| 项目 | 我们当前 | MT6639 正确值 | 严重性 |
|------|---------|-------------|--------|
| **TX Ring** | Ring 0 (DATA) | **Ring 15 (CMD)** | ★★★ 致命 |
| **PKT_FORMAT** | 0 (TXD) | **2 (COMMAND)** | ★★★ 致命 |
| **Q_IDX** | 0x10 (ALTX0) | **0 (MCU_Q0)** | ★★★ 致命 |
| **HDR_FORMAT** | 不确定 | **2 (802_11_NORMAL)** | ★★★ 致命 |
| **帧数据格式** | CT mode + TXP | **SFmode inline** | ★★★ 致命 |
| **DW7 TXD_LENGTH** | 0 | **1 (TXD_LEN_1_PAGE)** | ★★★ 致命 |
| **DIS_MAT** (DW6) | 不确定 | **1** (管理帧) | ★★ |
| **MSDU_COUNT** (DW6) | 不确定 | **1** | ★★ |
| **FIXED_RATE** | 不确定 | **1** (DW1 bit31) | ★★ |
| **REM_TX_COUNT** | 15 | **30** | ★ |
| **REMAINING_LIFE_TIME** | 不确定 | **~31** (2000ms) | ★ |

### 必需的改动 (按优先级)

1. **[P0] 管理帧走 CMD ring (TX ring 15) + SFmode**
   - TXD + 802.11 帧数据拼接到单个 DMA coherent buffer
   - 用 halWpdmaWriteCmd 方式提交 (SDPtr0=buf, SDLen0=总长, SDPtr1=0)
   - 不要使用 token/CT mode

2. **[P0] TXD 字段修复**
   - DW0: PKT_FORMAT = 2, Q_IDX = 0
   - DW1: HDR_FORMAT = 2, HDR_LENGTH = (mac_hdr_len/2), TID = 0, FIXED_RATE = 1
   - DW7: TXD_LENGTH = 1 (TXD_LEN_1_PAGE)

3. **[P1] DW6 字段**
   - DIS_MAT = 1 (管理帧必须)
   - MSDU_COUNT = 1

4. **[P1] 802.11 TYPE/SUB_TYPE 填充**
   - DW2: TYPE = (FC & MASK_FC_TYPE) >> 2, SUB_TYPE = (FC & MASK_FC_SUBTYPE) >> 4

5. **[P2] 速率/重试/生命期**
   - FIXED_RATE_ENABLE = 1, FIXED_RATE_IDX = 0
   - REM_TX_COUNT = 30
   - REMAINING_LIFE_TIME = 2000ms
