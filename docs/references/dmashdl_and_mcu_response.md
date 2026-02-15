# DMASHDL 配置与 MCU 响应路径研究报告

**日期**: 2026-02-15
**研究对象**: MT6639 vendor driver (mt6639/) + Windows RE 分析
**目的**: 解决 Mode 46 问题 — TX ring 15 描述符被消费但 MCU 无响应

---

## 执行摘要

### 关键发现

1. **DMASHDL bypass 只是表象**: Mode 46 用 bypass 让 WFDMA 消费了 ring 15，但 MCU 仍不回应。说明问题不在 WFDMA 层，而在 **MCU 命令路由** 或 **MCU 事件接收路径** 配置不全。

2. **缺失的 0xd6060 寄存器配置**: Windows 驱动在发送第一个 MCU 命令前，**只写了一个寄存器** — `BAR0+0xd6060 |= 0x10101`。我们完全没写这个。

3. **HOST2MCU_SW_INT_SET 不是必需的**: Vendor driver 只在 SER (System Error Recovery) 流程中使用这个寄存器，**正常 MCU 命令发送不写它**。

4. **MCU 响应通过 HOST RX Ring 6**: MT6639 的事件 ring 是 `P0R6:AP EVENT` (ring 6)，对应中断位 `rx_done_int_sts_6`。

5. **DMASHDL 完整配置**: Vendor driver 在 `mt6639DmashdlInit()` 配置了 16 个 group 的 quota、refill、queue mapping 和 priority，并设置 `HIF_ACK_CNT_TH=4` 和 `HIF_GUP_ACT_MAP=0x8007`。

---

## 第一部分: DMASHDL 完整配置表

### 1.1 基础参数 (hal_dmashdl_mt6639.h)

```c
// PCIe/AXI variant (我们的硬件)
#define MT6639_DMASHDL_SLOT_ARBITER_EN                 (0)
#define MT6639_DMASHDL_PKT_PLE_MAX_PAGE                (0x1)
#define MT6639_DMASHDL_PKT_PSE_MAX_PAGE                (0x18)
#define MT6639_DMASHDL_HIF_ACK_CNT_TH                  (0x4)
#define MT6639_DMASHDL_HIF_GUP_ACT_MAP                 (0x8007)  // Ring 0/1/2/15
```

**关键**: `HIF_GUP_ACT_MAP = 0x8007` = BIT(0)|BIT(1)|BIT(2)|BIT(15)，对应 TX ring 0/1/2/15 — 正好是我们用的 4 个 ring。

### 1.2 Group Refill Enable (16 个 group)

```c
GROUP_0_REFILL_EN  = 1   // 数据 ring
GROUP_1_REFILL_EN  = 1   // 数据 ring
GROUP_2_REFILL_EN  = 1   // 数据 ring
GROUP_3_REFILL_EN  = 0   // 未使用
...
GROUP_15_REFILL_EN = 0   // 保留
```

**实现**: 写 `REFILL_CONTROL` 寄存器 (0xd0000+0x6010)，每个 group 一个 bit (BIT[16+i] = disable)。

### 1.3 Group Quota 配置

| Group | Max Quota | Min Quota | 用途 |
|-------|-----------|-----------|------|
| 0     | 0xfff     | 0x10      | LMAC AC00/AC01/AC02 (data) |
| 1     | 0xfff     | 0x10      | LMAC AC10/AC11/AC12 (data) |
| 2     | 0xfff     | 0x10      | LMAC AC03/AC13 (data) |
| 3-14  | 0x0       | 0x0       | 未使用 |
| 15    | 0x30      | 0x0       | 保留 |

**实现**: 每个 group 一个 `GROUPx_CONTROL` 寄存器 (0xd0000+0x6020+x*4)，格式:
- `[11:0]` = MIN_QUOTA
- `[27:16]` = MAX_QUOTA

### 1.4 Queue → Group 映射 (32 个 queue)

```c
// PCIe variant 关键映射
QUEUE_0_TO_GROUP  = 0x0   // LMAC AC00
QUEUE_1_TO_GROUP  = 0x0   // LMAC AC01
QUEUE_2_TO_GROUP  = 0x0   // LMAC AC02
QUEUE_3_TO_GROUP  = 0x2   // LMAC AC03
QUEUE_4_TO_GROUP  = 0x1   // LMAC AC10
...
// USB variant 完全不同:
QUEUE_0_TO_GROUP  = 0x0
QUEUE_1_TO_GROUP  = 0x1
QUEUE_2_TO_GROUP  = 0x2
...
```

**实现**: 写 4 个 `QUEUE_MAPPINGx` 寄存器 (0xd0000+0x6060+x*4)，每个寄存器 8 个 queue (每个 queue 4 bit)。

**RED FLAG**: PCIe 和 USB 的 queue mapping **完全不同**。USB 把每个 AC queue 分到不同 group，PCIe 把 AC0x 全放 group 0。这说明 DMASHDL group 不是简单的 AC 映射，而是跟 HIF 接口相关。

### 1.5 Priority → Group 映射 (16 个 priority)

```c
// PCIe: 直接映射 (priority N → group N)
PRIORITY0_GROUP  = 0x0
PRIORITY1_GROUP  = 0x1
...
PRIORITY15_GROUP = 0xF

// USB: 反向映射 (priority 0-3 → group 3-0)
PRIORITY0_GROUP  = 0x3
PRIORITY1_GROUP  = 0x2
PRIORITY2_GROUP  = 0x1
PRIORITY3_GROUP  = 0x0
PRIORITY4_GROUP  = 0x4
...
```

**实现**: 写 2 个 `HIF_SCHEDULER_SETTINGx` 寄存器 (0xd0000+0x6070+x*4)，每个寄存器 8 个 priority (每个 4 bit)。

### 1.6 Optional Control 寄存器

```c
// 地址: 0xd0000+0x6008
u2HifAckCntTh   = 0x4       // [23:16] = CR_HIF_ACK_CNT_TH
u2HifGupActMap  = 0x8007    // [15:0]  = CR_HIF_GUP_ACT_MAP
```

**HIF_GUP_ACT_MAP** 含义: 每个 bit 对应一个 TX ring，标记哪些 ring 参与 DMASHDL arbitration。
- 0x8007 = BIT(15)|BIT(2)|BIT(1)|BIT(0)
- 对应 TX ring 0/1/2/15

### 1.7 初始化顺序 (mt6639DmashdlInit)

```c
void mt6639DmashdlInit(struct ADAPTER *prAdapter)
{
    // 1. Packet max page size
    asicConnac3xDmashdlSetPlePktMaxPage(prAdapter, 0x1);
    asicConnac3xDmashdlSetPsePktMaxPage(prAdapter, 0x18);

    // 2. 配置 16 个 group
    for (idx = 0; idx < 16; idx++) {
        asicConnac3xDmashdlSetRefill(prAdapter, idx, afgRefillEn[idx]);
        asicConnac3xDmashdlSetMaxQuota(prAdapter, idx, au2MaxQuota[idx]);
        asicConnac3xDmashdlSetMinQuota(prAdapter, idx, au2MinQuota[idx]);
    }

    // 3. Queue → Group 映射 (32 个 queue)
    for (idx = 0; idx < 32; idx++)
        asicConnac3xDmashdlSetQueueMapping(prAdapter, idx, aucQueue2Group[idx]);

    // 4. Priority → Group 映射 (16 个 priority)
    for (idx = 0; idx < 16; idx++)
        asicConnac3xDmashdlSetUserDefinedPriority(prAdapter, idx, aucPriority2Group[idx]);

    // 5. Slot arbiter (关闭)
    asicConnac3xDmashdlSetSlotArbiter(prAdapter, 0);

    // 6. Optional control
    asicConnac3xDmashdlSetOptionalControl(prAdapter, 0x4, 0x8007);
}
```

### 1.8 关键寄存器地址 (BAR0 offset)

| 总线地址 | BAR0 Offset | 寄存器名 |
|----------|-------------|----------|
| 0x7c026000 | 0xd6000 | DMASHDL base |
| 0x7c026004 | 0xd6004 | SW_CONTROL |
| 0x7c026008 | 0xd6008 | OPTIONAL_CONTROL (HIF_ACK_CNT_TH + HIF_GUP_ACT_MAP) |
| 0x7c02600c | 0xd600c | PAGE_SETTING |
| 0x7c026010 | 0xd6010 | REFILL_CONTROL |
| 0x7c02601c | 0xd601c | PACKET_MAX_SIZE (PLE + PSE) |
| 0x7c026020 | 0xd6020 | GROUP0_CONTROL |
| ... | ... | GROUP1-15_CONTROL (+4 每个) |
| 0x7c026060 | **0xd6060** | **QUEUE_MAPPING0** (Windows 写 0x10101!) |
| 0x7c026064 | 0xd6064 | QUEUE_MAPPING1 |
| 0x7c026068 | 0xd6068 | QUEUE_MAPPING2 |
| 0x7c02606c | 0xd606c | QUEUE_MAPPING3 |
| 0x7c026070 | 0xd6070 | HIF_SCHEDULER_SETTING0 |
| 0x7c026074 | 0xd6074 | HIF_SCHEDULER_SETTING1 |

**关键发现**: Windows RE 分析显示 `0xd6060 |= 0x10101` 是 PostFwDownloadInit 的 **第一个寄存器写**，在第一个 MCU 命令之前。但这个寄存器在 vendor driver 里叫 `QUEUE_MAPPING0`，不是什么特殊的 "DMASHDL enable"。

---

## 第二部分: MCU 响应路径分析

### 2.1 HOST2MCU_SW_INT_SET — 不是问题所在

**地址**: `0xd0000+0x108` (BAR0+0xd4108)
**定义**: `WF_WFDMA_HOST_DMA0_HOST2MCU_SW_INT_SET`

**用途**: Software interrupt from host to MCU。Vendor driver 只在 3 个场景使用:
1. **SER (System Error Recovery)**: 通知 MCU PDMA 停止/初始化/恢复完成
   - `MCU_INT_PDMA0_STOP_DONE`
   - `MCU_INT_PDMA0_INIT_DONE`
   - `MCU_INT_PDMA0_RECOVERY_DONE`
2. **驱动 SER 触发**: `halSetDrvSer()` 写 `MCU_INT_DRIVER_SER`

**正常 MCU 命令发送**: **不写这个寄存器**。

**代码证据** (os/linux/hif/common/hal_pdma.c):
```c
void halSetDrvSer(struct ADAPTER *prAdapter)
{
    kalDevRegWrite(prAdapter->prGlueInfo, HOST2MCU_SW_INT_SET,
                   MCU_INT_DRIVER_SER);
}
```

**结论**: Mode 46 的问题不是缺 `HOST2MCU_SW_INT_SET`。正常 MCU 命令根本不需要它。

### 2.2 MCU 响应通过 HOST RX Ring 6

**Ring 配置** (chips/mt6639/mt6639.c):
```c
struct wfdma_group_info mt6639_wfmda_host_rx_group[] = {
    {"P0R4:AP DATA0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING4_CTRL0_ADDR, true},
    {"P0R6:AP EVENT", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING6_CTRL0_ADDR, true},  // ← 事件
    {"P0R5:AP DATA1", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING5_CTRL0_ADDR, true},
    {"P0R7:AP TDONE0", WF_WFDMA_HOST_DMA0_WPDMA_RX_RING7_CTRL0_ADDR, true},
};
```

**中断配置** (mt6639ConfigIntMask):
```c
if (enable) {
    u4WrVal =
      WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA4_MASK |  // data0
      WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA5_MASK |  // data1
      WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA6_MASK |  // event ← 这个
      WF_WFDMA_HOST_DMA0_HOST_INT_ENA_HOST_RX_DONE_INT_ENA7_MASK |  // TX done
      ...
      WF_WFDMA_HOST_DMA0_HOST_INT_ENA_mcu2host_sw_int_ena_MASK;     // MCU2HOST SW INT
}
```

**SW Ring → HW Ring 映射** (mt6639SetRxRingHwAddr):
```c
switch (u4SwRingIdx) {
case RX_RING_EVT_IDX_1:    // SW ring 1 (event ring)
    offset = 6 * MT_RINGREG_DIFF;  // → HW ring 6
    break;
case RX_RING_DATA_IDX_0:   // SW ring 0 (data ring 0)
    offset = 4 * MT_RINGREG_DIFF;  // → HW ring 4
    break;
case RX_RING_DATA1_IDX_2:  // SW ring 2 (data ring 1)
    offset = 5 * MT_RINGREG_DIFF;  // → HW ring 5
    break;
case RX_RING_TXDONE0_IDX_3: // SW ring 3 (TX done)
    offset = 7 * MT_RINGREG_DIFF;  // → HW ring 7
    break;
}
```

**结论**: MCU 事件通过 **HW RX ring 6** 返回，SW 层叫 `RX_RING_EVT_IDX_1`。必须:
1. 配置 ring 6 BASE/CIDX/DIDX/CNT 寄存器
2. 使能 `HOST_INT_ENA` 的 BIT(6) (`rx_done_int_sts_6`)
3. 在中断处理中检查 `HOST_INT_STA` BIT(6)
4. 调用 `halRxReceiveRFBs(prAdapter, RX_RING_EVT_IDX_1, FALSE)`

### 2.3 事件包类型识别

**RX 描述符检查** (nic/nic_rxd_v3.c for CONNAC3X):
```c
if ((prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) ==
        CONNAC3X_RX_STATUS_PKT_TYPE_SW_EVENT) {  // 0x3800
    // 这是事件包 → 调用 nicRxProcessEventPacket()
}
```

**常量定义** (nic/nic_connac3x_rx.h):
```c
#define CONNAC3X_RX_STATUS_PKT_TYPE_SW_EVENT  0x3800
```

**事件分发** (nic/nic_rx.c):
```c
static struct RX_EVENT_HANDLER arEventTable[] = {
    {EVENT_ID_RX_ADDBA,     qmHandleEventRxAddBa},
    {EVENT_ID_RX_DELBA,     qmHandleEventRxDelBa},
    {EVENT_ID_LINK_QUALITY, nicEventLinkQuality},
    ...
    {EVENT_ID_CMD_RESULT,   nicCmdEventQueryMcrRead},  // 0x01 (MCU 响应?)
    ...
};
```

**结论**: 事件接收路径需要:
1. RX ring 6 配置正确
2. RXD pkt_type 字段 = 0x3800 时识别为事件
3. 事件分发到对应 handler

### 2.4 缺失的中断配置?

Vendor driver 还配置了一个特殊的中断寄存器 (mt6639InitPcieInt):
```c
static void mt6639InitPcieInt(struct GLUE_INFO *prGlueInfo)
{
    HAL_MCR_WR(prGlueInfo->prAdapter,
        PCIE_MAC_IREG_IMASK_HOST_ADDR,           // 0x10000+0x188
        PCIE_MAC_IREG_IMASK_HOST_DMA_END_EN_MASK);
}
```

**地址**: `0x74030000+0x188` = BAR0+0x10188 (PCIe MAC)
**值**: `PCIE_MAC_IREG_IMASK_HOST_DMA_END_EN_MASK`

这是 **PCIe 层中断 mask**，不是 WFDMA 层。可能需要检查。

---

## 第三部分: Windows RE 的关键线索

### 3.1 PostFwDownloadInit 寄存器写序列

**完整顺序** (来自 ghidra_post_fw_init.md):
```
1. *(ctx + 0x146e61c) = 0                      // 清标志位
2. WRITE BAR0+0xd6060 |= 0x10101                // ← 唯一的寄存器写
3. MCU cmd (class=0x8a, target=0xed)            // NIC_CAPABILITY
4. MCU cmd (class=0x02, target=0xed, data={1,0,0x70000})
5. MCU cmd (class=0xc0, target=0xed, data={0x820cc800,0x3c200})
6. (可选) DownloadBufferBin (class=0xed, subcmd=0x21)
7. (MT6639/MT7927) MCU cmd (class=0x28)         // DBDC
8. KeStallExecutionProcessor(100 * 10)          // 1ms 延迟
9. MCU cmd (class=0xca)                         // PassiveToActiveScan
10. MCU cmd (class=0xca)                        // FWChipConfig
11. MCU cmd (class=0xca)                        // LogLevelConfig
```

**0xd6060 的奥秘**:
- Vendor driver 里这个寄存器是 `QUEUE_MAPPING0` (DMASHDL)
- Windows 写 `|= 0x10101` = BIT(0)|BIT(8)|BIT(16)
- 按 QUEUE_MAPPING0 格式:
  - BIT[3:0] = queue 0 → group
  - BIT[7:4] = queue 1 → group
  - BIT[11:8] = queue 2 → group  (BIT(8) = 1)
  - BIT[15:12] = queue 3 → group
  - BIT[19:16] = queue 4 → group (BIT(16) = 1)

**疑点**: `0x10101` 不符合 QUEUE_MAPPING0 语义 (每个 queue 应该 <16)。可能是:
1. **误导**: Windows 注释错误，这不是 DMASHDL 寄存器
2. **多功能寄存器**: 0xd6060 除了 queue mapping 还有其他控制位
3. **后门**: 隐藏的使能位

### 3.2 MCU 命令 TXD 格式差异

**Windows 驱动有两种模式**:
1. **Legacy mode** (flag_146e621==0): 0x40 byte header
   - TXD[0] = `total_len | 0x40000000` (Q_IDX in upper byte)
   - TXD[1] = `flags | 0x4000` (HDR_FORMAT_V3, **NO BIT(31)**)
   - 头部在 +0x40 开始

2. **CONNAC3 UniCmd mode** (flag_146e621==1): 0x30 byte header
   - TXD[0] = `total_len | 0x41000000` (Q_IDX=0x20, PKT_FMT=2)
   - TXD[1] = `flags | 0x4000`
   - 头部在 +0x30 开始

**我们的驱动** (基于 mt76):
- Q_IDX = 2 (不是 0x20!)
- TXD[1] 设置 BIT(31) (LONG_FORMAT)

**关键**: PostFwDownloadInit **清除 flag_146e621**，说明初始 MCU 命令用 **legacy mode**，不是 CONNAC3 mode。

### 3.3 Q_IDX = 0x20 是什么?

在 vendor driver 里找不到 Q_IDX=0x20 的定义。mt76 的 TX port 定义:
```c
enum mt76_txq_id {
    MT_TXQ_FWDL = 16,   // FWDL
    MT_TXQ_MCU = 15,    // MCU CMD (应该用这个?)
    ...
};
```

但 Windows 用 **0x20** = 32。可能是:
- MT6639 特有的 MCU queue index
- 不是 TXQ ID 而是 WFDMA routing hint

---

## 第四部分: 下一步实验建议

### Mode 47: 完整 DMASHDL + RX ring 6 配置

**目标**: 初始化全部 DMASHDL 参数 + RX ring 6，看 MCU 是否回应。

**步骤**:
1. **DMASHDL 初始化** (在 WFDMA enable 之前):
   ```c
   // 1. Packet max page
   writel(0x1, mmio + 0xd601c);      // PLE_PACKET_MAX_SIZE = 1
   writel(0x18 << 16, mmio + 0xd601c); // PSE_PACKET_MAX_SIZE = 0x18

   // 2. Group 0/1/2 quota
   writel(0x0fff0010, mmio + 0xd6020); // Group 0: max=0xfff, min=0x10
   writel(0x0fff0010, mmio + 0xd6024); // Group 1
   writel(0x0fff0010, mmio + 0xd6028); // Group 2
   writel(0x00000000, mmio + 0xd602c); // Group 3 (disabled)
   // ... (Group 4-14 全 0)
   writel(0x00300000, mmio + 0xd605c); // Group 15: max=0x30, min=0

   // 3. Refill control (enable group 0/1/2)
   uint32_t refill = readl(mmio + 0xd6010);
   refill &= ~0x00070000;  // Clear disable bits for group 0/1/2
   writel(refill, mmio + 0xd6010);

   // 4. Queue mapping (PCIe variant)
   writel(0x22000000, mmio + 0xd6060); // Q0=0, Q1=0, Q2=0, Q3=2
   writel(0x00021111, mmio + 0xd6064); // Q4=1, Q5=1, Q6=1, Q7=2
   // ... (Q8-31 按表配置)

   // 5. Priority mapping (直接映射)
   writel(0x33221100, mmio + 0xd6070); // P0-3 → G0-3
   writel(0x77665544, mmio + 0xd6074); // P4-7 → G4-7

   // 6. Optional control
   writel(0x00048007, mmio + 0xd6008); // HIF_ACK_CNT_TH=4, HIF_GUP_ACT_MAP=0x8007

   // 7. Slot arbiter (disable)
   uint32_t page_set = readl(mmio + 0xd600c);
   page_set &= ~BIT(16);  // SLOT_ARBITER_EN = 0
   writel(page_set, mmio + 0xd600c);
   ```

2. **配置 RX ring 6** (event ring):
   ```c
   // Ring 6 BASE/CNT (假设 512 描述符)
   writel(rx_ring6_dma, mmio + 0xd4500 + 6*0x10);       // BASE
   writel(512, mmio + 0xd4504 + 6*0x10);                // CNT
   writel(0, mmio + 0xd4508 + 6*0x10);                  // CIDX
   writel(0, mmio + 0xd450c + 6*0x10);                  // DIDX

   // EXT_CTRL (DISP_MAX_CNT)
   writel(0x01000000, mmio + 0xd4580 + 6*4);  // CONNAC3X_RX_RING_DISP_MAX_CNT
   ```

3. **使能 RX ring 6 中断**:
   ```c
   uint32_t int_ena = readl(mmio + 0xd4204);
   int_ena |= BIT(6);  // HOST_RX_DONE_INT_ENA6
   writel(int_ena, mmio + 0xd4204);
   ```

4. **中断处理**:
   ```c
   uint32_t int_sta = readl(mmio + 0xd4200);
   if (int_sta & BIT(6)) {  // RX ring 6 有数据
       printk("RX ring 6 interrupt! DIDX=%u\n",
              readl(mmio + 0xd450c + 6*0x10));
       writel(BIT(6), mmio + 0xd4200);  // Clear interrupt
   }
   ```

5. **发送 NIC_CAPABILITY 命令** (仍用 Q_IDX=2):
   - 如果 ring 6 收到响应 → DMASHDL 配置有效
   - 如果仍超时 → 问题在其他地方 (Q_IDX? TXD 格式? 0xd6060?)

### Mode 48: 测试 Q_IDX=0x20

**目标**: 尝试 Windows 的 Q_IDX=0x20，看是否改变 MCU routing。

**修改** (在 TXD 填充函数):
```c
txd[0] = cpu_to_le32(total_len | (0x20 << 16) | 0x41000000);  // Q_IDX=0x20
```

**测试**: 发送 NIC_CAPABILITY 后:
1. 检查 ring 15 DIDX 是否递增 (是否被消费)
2. 检查 ring 6 DIDX 是否递增 (是否有响应)
3. 对比 Q_IDX=2 vs 0x20 的行为差异

### Mode 49: 尝试 0xd6060 = 0x10101

**目标**: 复现 Windows 的神秘写法，排除 QUEUE_MAPPING 以外的功能。

**代码**:
```c
// 在 WFDMA enable 之前 (fw_sync=0x3 后)
writel(0x10101, mmio + 0xd6060);  // 直接写，不 OR
printk("Wrote 0x10101 to 0xd6060\n");

// 然后发送 MCU 命令
```

**验证**:
```c
uint32_t val = readl(mmio + 0xd6060);
printk("0xd6060 read back: 0x%08x\n", val);
```

如果读回不是 0x10101，说明这个寄存器有特殊行为 (写清零某些位?)。

### Mode 50: Legacy TXD 格式测试

**目标**: 用 Windows 的 0x40 byte legacy header 发送命令。

**修改**:
```c
// 使用 0x40 header (不是 0x30)
struct legacy_txd {
    uint32_t txd[16];  // 0x40 bytes
    uint8_t payload[];
} __packed;

legacy_txd txd = {0};
txd.txd[0] = cpu_to_le32((cmd_len + 0x40) | 0x40000000);  // Q_IDX in upper byte
txd.txd[1] = cpu_to_le32(0x4000);  // HDR_FORMAT_V3, NO BIT(31)
txd.txd[8] = cpu_to_le32((cmd_len + 0x20) | (0xa0 << 8) | (0x8a << 0)); // +0x20: len, +0x24: class
txd.txd[9] = cpu_to_le32(seq_num << 24);  // +0x27: seq_num
// memcpy(&txd.payload, cmd_data, cmd_len);
```

**对比**: Legacy vs CONNAC3 的响应差异。

---

## 第五部分: 疑点与盲区

### 5.1 0xd6060 的真实功能

**证据矛盾**:
1. Vendor driver 定义为 `QUEUE_MAPPING0` (DMASHDL)
2. Windows 写 `0x10101`，不符合 queue mapping 语义
3. Windows 在 DMASHDL init **之前** 写，不符合初始化顺序

**假设**:
- 可能是 **多功能寄存器**，高位是控制位，低位是 queue mapping
- 或者 vendor driver 的地址定义有误 (bus2chip 映射错?)

**验证方法**:
1. 读 0xd6060 初始值 (reset 后)
2. 写 0x10101，读回，看是否保持
3. 写正常的 queue mapping (如 0x22000000)，读回，看是否生效

### 5.2 MCU 命令 target=0xed 的含义

Windows MCU 命令全用 `target=0xed`，但 vendor driver 没有定义这个 target。mt76 定义:
```c
enum {
    MCU_CMD_TARGET_WM = 0x1,      // WiFi MCU
    MCU_CMD_TARGET_WA = 0x2,      // WiFi Assistant
    MCU_CMD_TARGET_FW = 0x3,      // Firmware
};
```

**0xed** 可能是 MT6639 特有的 target，表示 "unified MCU" 或 "CONNAC3 dispatcher"。

**验证**: 尝试 target=0x1 (WM) 发送 NIC_CAPABILITY，看 FW 是否响应。

### 5.3 PCIe MAC 中断寄存器

Vendor driver 配置了 `PCIE_MAC_IREG_IMASK_HOST_ADDR` (0x10188)，我们的驱动没有。

**寄存器定义** (coda/mt6639/pcie_mac_ireg.h):
```c
#define PCIE_MAC_IREG_IMASK_HOST_ADDR  (PCIE_MAC_IREG_BASE + 0x188)
#define PCIE_MAC_IREG_IMASK_HOST_DMA_END_EN_MASK  0x...  // 需要查头文件
```

**功能**: PCIe 层 DMA 完成中断 mask。可能影响 WFDMA 中断路由。

**下一步**: 检查这个寄存器的值和使能位。

---

## 附录 A: 完整寄存器地址表

| Bus Address | BAR0 Offset | 寄存器名 | 默认值 | 何时写 |
|-------------|-------------|----------|--------|--------|
| 0x7c026000 | 0xd6000 | DMASHDL_BASE | - | - |
| 0x7c026008 | 0xd6008 | OPTIONAL_CONTROL | ? | DMASHDL init |
| 0x7c02600c | 0xd600c | PAGE_SETTING | ? | DMASHDL init |
| 0x7c026010 | 0xd6010 | REFILL_CONTROL | 0xFFFF0000? | DMASHDL init |
| 0x7c02601c | 0xd601c | PACKET_MAX_SIZE | ? | DMASHDL init |
| 0x7c026020-5c | 0xd6020-5c | GROUP0-15_CONTROL | 0? | DMASHDL init |
| **0x7c026060** | **0xd6060** | **QUEUE_MAPPING0 (?)** | **?** | **PostFwDL 第一步** |
| 0x7c026064-6c | 0xd6064-6c | QUEUE_MAPPING1-3 | ? | DMASHDL init |
| 0x7c026070-74 | 0xd6070-74 | HIF_SCHEDULER_SETTING0-1 | ? | DMASHDL init |
| 0x7c024204 | 0xd4204 | HOST_INT_ENA | 0 | WFDMA enable |
| 0x7c024208 | 0xd4208 | GLO_CFG | ? | WFDMA enable |
| 0x7c0242b4 | 0xd42b4 | GLO_CFG_EXT | ? | WFDMA enable |
| 0x7c024300+x*0x10 | 0xd4300+x*0x10 | TX_RINGx_BASE | 0 | TX init |
| 0x7c024500+x*0x10 | 0xd4500+x*0x10 | RX_RINGx_BASE | 0 | RX init |
| 0x7c024580+x*4 | 0xd4580+x*4 | RX_RINGx_EXT_CTRL | 0 | RX init |
| 0x74030188 | 0x10188 | PCIE_MAC_IREG_IMASK_HOST | ? | PCIe init |

---

## 附录 B: Vendor Driver 调用顺序

### 正常启动流程 (wlanProbe):
```
1. wlanProbe()
   ├─ wlanNetCreate()                // Allocate net_device
   ├─ wlanAdapterStart(prAdapter)
   │  ├─ nicAllocateAdapterMemory()  // Alloc rings
   │  ├─ nicInitializeAdapter()
   │  │  ├─ halWpdmaAllocRing()      // Setup TX/RX ring DMA
   │  │  ├─ mt6639DmashdlInit()      // ← DMASHDL 初始化
   │  │  └─ halWpdmaInitRing()       // Write BASE/CNT/CIDX/DIDX
   │  ├─ wlanDownloadFW()
   │  │  ├─ wlanDownloadPatch()
   │  │  └─ wlanConnacFormatDownload() // FW scatter download
   │  ├─ (check fw_sync == 0x3)
   │  └─ nicpmWakeUpWiFi()
   └─ wlanNetRegister()               // Register net_device
```

### PostFwDownloadInit (Windows 流程):
```
1. AsicConnac3xPostFwDownloadInit()
   ├─ *(ctx+0x146e61c) = 0
   ├─ WRITE 0xd6060 |= 0x10101       // ← 关键
   ├─ MCU cmd: NIC_CAPABILITY (0x8a)
   ├─ MCU cmd: CONFIG (0x02)
   ├─ MCU cmd: 0xc0
   ├─ DownloadBufferBin (optional)
   ├─ MCU cmd: DBDC (0x28, MT6639 only)
   ├─ stall 1ms
   ├─ MCU cmd: PassiveToActiveScan (0xca)
   ├─ MCU cmd: FWChipConfig (0xca)
   └─ MCU cmd: LogLevelConfig (0xca)
```

Vendor driver 里**没有** PostFwDownloadInit 等价物 — 所有 MCU 命令在 `wlanAdapterStart()` 之后由上层触发 (iwconfig/wpa_supplicant)。

---

## 结论与优先级

### 必做 (Mode 47):
1. **完整 DMASHDL 初始化** — 按 mt6639DmashdlInit() 复现全部寄存器
2. **配置 RX ring 6** — 事件响应路径
3. **使能 RX ring 6 中断** — 检查 MCU 是否有回应

### 高优先级 (Mode 48-49):
4. **测试 Q_IDX=0x20** — Windows 的神秘值
5. **测试 0xd6060=0x10101** — 第一个寄存器写

### 中优先级 (Mode 50):
6. **Legacy TXD 格式** — 0x40 header，NO BIT(31)

### 低优先级:
7. PCIe MAC 中断寄存器 (0x10188)
8. target=0xed vs 0x1 对比

### 调查任务:
9. 确认 0xd6060 的真实功能 (读写测试)
10. 查找 Q_IDX=0x20 的定义 (CONNAC3 spec?)
11. 对比 PCIe vs USB 的 DMASHDL 差异原因

---

**报告结束**. 建议立即开始 Mode 47 实验。
