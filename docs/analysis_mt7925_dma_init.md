# MT7925 DMA 初始化序列深度分析

**生成时间**: 2026-02-15
**目的**: 分析 upstream mt7925 驱动的 DMA 初始化流程，与 MT7927 驱动对比，找出导致 MCU_RX0 环不被配置的根本原因

---

## A. HOST RX Ring 0 配置

### mt7925 upstream 实现

**位置**: `mt76/mt7925/pci.c` 的 `mt7925_dma_init()` 函数（第 215-269 行）

**关键代码**（第 246-251 行）：
```c
/* rx event */
ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MCU],
                       MT7925_RXQ_MCU_WM, MT7925_RX_MCU_RING_SIZE,
                       MT_RX_BUF_SIZE, MT_RX_EVENT_RING_BASE);
```

**关键参数**:
- **`MT_RXQ_MCU`** = 0 (定义在 `mt76/mt792x.h`, 通用 RX queue 索引)
- **`MT7925_RXQ_MCU_WM`** = 0 (定义在 `mt76/mt7925/mt7925.h:125`, **硬件环号 0**)
- **`MT7925_RX_MCU_RING_SIZE`** = 512 (定义在 `mt76/mt7925/mt7925.h:17`, 512 条目)
- **`MT_RX_BUF_SIZE`** = 2048 (每个 buffer 大小)
- **`MT_RX_EVENT_RING_BASE`** = MT_WFDMA0(0x500) (定义在 `mt76/mt792x_regs.h:358`, BAR0 offset = 0xd4500)

**分配时机**:
- 在 **probe 函数** 中调用 `mt7925_dma_init()`（第 414 行）
- **在固件下载之前**就已经分配
- 在 `mt792x_dma_enable()` 中启用 DMA **之前**

**Prefetch 配置** (在 `mt792x_dma_prefetch()` 中, `mt76/mt792x_dma.c:95`):
```c
/* mt7925 路径 (is_mt7925 = true) */
mt76_wr(dev, MT_WFDMA0_RX_RING0_EXT_CTRL, PREFETCH(0x0000, 0x4));
```
- **base_ptr** = 0x0000 (prefetch SRAM 起始地址)
- **depth** = 0x4 (4 级 prefetch 深度)
- **RX ring 0 是 prefetch 链的第一个环** — 不会被阻塞

**Probe 流程中的位置**:
```
mt7925_pci_probe()
  ├─ pcim_enable_device()
  ├─ __mt792x_mcu_fw_pmctrl() (FW power ctrl)
  ├─ __mt792xe_mcu_drv_pmctrl() (DRV power ctrl)
  ├─ mt76_rmw_field(MT_HW_EMI_CTL, SLPPROT_EN, 1) (EMI sleep protection)
  ├─ mt792x_wfsys_reset()
  ├─ mt76_wr(host_irq_enable, 0)
  ├─ mt76_wr(MT_PCIE_MAC_INT_ENABLE, 0xff)
  ├─ devm_request_irq()
  ├─ mt7925_dma_init()  ← **HOST RX ring 0 在这里分配**
  │   ├─ mt76_dma_attach()
  │   ├─ mt792x_dma_disable(dev, true)
  │   ├─ mt76_connac_init_tx_queues() (TX queue 0-3)
  │   ├─ mt76_wr(MT_WFDMA0_TX_RING0_EXT_CTRL, 0x4)
  │   ├─ mt76_init_mcu_queue(MT_MCUQ_WM, ring 15)
  │   ├─ mt76_init_mcu_queue(MT_MCUQ_FWDL, ring 16)
  │   ├─ **mt76_queue_alloc(q_rx[0], MCU event, 512 entries)** ← 关键！
  │   ├─ mt76_queue_alloc(q_rx[2], data)
  │   ├─ mt76_init_queues()
  │   ├─ napi_enable()
  │   └─ mt792x_dma_enable()  ← DMA 启用，包括 prefetch 和 NEED_REINIT
  └─ mt7925_register_device()  ← 异步启动 init_work (MCU 初始化)
```

### 与我们的差异 ❌

**我们的实现** (`tests/04_risky_ops/mt7927_init_dma.c`):

1. **条件分配** (第 1755-1761 行):
   ```c
   if (reinit_mode == 53) {
       ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx0, 0, 128, MT_RX_BUF_SIZE);
       ...
   }
   ```
   - **只在 Mode 53 时分配**
   - 在 Mode 0-52（默认模式）中**完全没有 HOST RX ring 0**

2. **条目数更少**:
   - mt7925: 512 entries
   - 我们的: 128 entries (仅 Mode 53)

3. **分配位置不同**:
   - mt7925: 在 `dma_init()` 中，FWDL **之前**
   - 我们的: 在 `mt7927_dma_rings_alloc()` 中，但不是所有模式都分配

4. **prefetch 配置**:
   - mt7925: RX ring 0 是 prefetch 链第一个（base=0x0000, depth=4）
   - 我们的 (Mode 53): RX ring 0 prefetch **未配置**（只配置了 RX4-7）

**结论**: 这可能是 **根本原因** — 固件在启动时检查 HOST RX ring 0 是否存在（BASE != 0），如果不存在则不配置 MCU_RX0 环。mt7925 **始终**在 FWDL 前分配 HOST RX ring 0，而我们的驱动在大多数模式下没有。

---

## B. DMASHDL 初始化

### mt7925 upstream 实现

**DMA disable 时** (`mt76/mt792x_dma.c:253-286`, `mt792x_dma_disable()`):
```c
/* disable dmashdl */
mt76_clear(dev, MT_WFDMA0_GLO_CFG_EXT0,
           MT_WFDMA0_CSR_TX_DMASHDL_ENABLE);
mt76_set(dev, MT_DMASHDL_SW_CONTROL, MT_DMASHDL_DMASHDL_BYPASS);
```
- 清除 GLO_CFG_EXT0 中的 DMASHDL 使能位
- **设置 DMASHDL bypass** (BIT(28) in SW_CONTROL)

**DMA enable 时** (`mt76/mt792x_dma.c:126-171`, `mt792x_dma_enable()`):
- **没有**配置 DMASHDL 寄存器（GROUP_CONTROL, QUEUE_MAP, etc.）
- **没有**清除 bypass 标志
- **仅配置 prefetch** (第 129 行 `mt792x_dma_prefetch()`)

**关键发现**: mt7925 在 FWDL 阶段**始终使用 DMASHDL bypass**，不配置 quota/group。

**PostFwDownloadInit 序列**:
- upstream mt76 **完全没有** PostFwDownloadInit 等价物
- Windows 驱动的 9 条 MCU 命令和 `BAR0+0xd6060 |= 0x10101`（DMASHDL enable）在 upstream 中**不存在**

### 与我们的差异 ✅ 基本一致

**我们的实现** (`tests/04_risky_ops/mt7927_init_dma.c:1971-1998`):
```c
dmashdl_sw = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
dmashdl_sw |= MT_HIF_DMASHDL_BYPASS_EN;
mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, dmashdl_sw);
```
- 同样设置 DMASHDL bypass
- **没有**配置 quota/group（与 mt7925 一致）

**Mode 50 已被排除**: 完整的 vendor DMASHDL 配置（quota, group, queue_map）在 Mode 50 测试后被标记为 ELIMINATED — ring 15 仍然被阻塞，MCU_RX0 仍为 0。

**结论**: DMASHDL 配置与 mt7925 一致，**不是**阻塞原因。

---

## C. WFDMA Dispatch 配置

### mt7925 upstream 实现

**GLO_CFG 配置** (`mt76/mt792x_dma.c:139-148`):
```c
mt76_set(dev, MT_WFDMA0_GLO_CFG,
         MT_WFDMA0_GLO_CFG_TX_WB_DDONE |
         MT_WFDMA0_GLO_CFG_FIFO_LITTLE_ENDIAN |
         MT_WFDMA0_GLO_CFG_CLK_GAT_DIS |
         MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
         FIELD_PREP(MT_WFDMA0_GLO_CFG_DMA_SIZE, 3) |
         MT_WFDMA0_GLO_CFG_FIFO_DIS_CHECK |
         MT_WFDMA0_GLO_CFG_RX_WB_DDONE |
         MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |  ← **关键！**
         MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);
```

**关键字段**:
- **`CSR_DISP_BASE_PTR_CHAIN_EN`** (BIT(15)): 启用 prefetch chain 模式
- **`OMIT_TX_INFO`** (BIT(28)): 省略 TX info（TXD 不含 token）
- **`OMIT_RX_INFO_PFET2`** (BIT(21)): 省略 RX info for prefetch2
- **DMA_SIZE** = 3 (burst size = 3 * 16 bytes = 48 bytes)

**GLO_CFG_EXT0 配置**:
- mt7925 **不写** GLO_CFG_EXT0 寄存器
- 仅在 mt7921（旧芯片）中写入，mt7925 跳过
- 定义在 `mt76/mt792x_dma.c` 中没有 EXT0 写入

**DISP_CTRL 配置**:
- **没有**直接写 DISP_CTRL 寄存器
- 通过 `CSR_DISP_BASE_PTR_CHAIN_EN` 启用 dispatch，不需要手动配置 DISP_CTRL

**Prefetch 配置** (`mt76/mt792x_dma.c:90-124`):
```c
#define PREFETCH(base, depth)  ((base) << 16 | (depth))

/* mt7925 路径 */
mt76_wr(dev, MT_WFDMA0_RX_RING0_EXT_CTRL, PREFETCH(0x0000, 0x4));
mt76_wr(dev, MT_WFDMA0_RX_RING1_EXT_CTRL, PREFETCH(0x0040, 0x4));
mt76_wr(dev, MT_WFDMA0_RX_RING2_EXT_CTRL, PREFETCH(0x0080, 0x4));
mt76_wr(dev, MT_WFDMA0_RX_RING3_EXT_CTRL, PREFETCH(0x00c0, 0x4));
/* TX rings */
mt76_wr(dev, MT_WFDMA0_TX_RING0_EXT_CTRL, PREFETCH(0x0100, 0x10));
mt76_wr(dev, MT_WFDMA0_TX_RING1_EXT_CTRL, PREFETCH(0x0200, 0x10));
mt76_wr(dev, MT_WFDMA0_TX_RING2_EXT_CTRL, PREFETCH(0x0300, 0x10));
mt76_wr(dev, MT_WFDMA0_TX_RING3_EXT_CTRL, PREFETCH(0x0400, 0x10));
mt76_wr(dev, MT_WFDMA0_TX_RING15_EXT_CTRL, PREFETCH(0x0500, 0x4));
mt76_wr(dev, MT_WFDMA0_TX_RING16_EXT_CTRL, PREFETCH(0x0540, 0x4));
```

**prefetch SRAM 布局** (RX → TX, 非重叠区域):
```
RX ring 0:  0x0000 - 0x003F (depth=4, step=0x0010)
RX ring 1:  0x0040 - 0x007F
RX ring 2:  0x0080 - 0x00BF
RX ring 3:  0x00C0 - 0x00FF
TX ring 0:  0x0100 - 0x01FF (depth=16, step=0x0010)
TX ring 1:  0x0200 - 0x02FF
TX ring 2:  0x0300 - 0x03FF
TX ring 3:  0x0400 - 0x04FF
TX ring 15: 0x0500 - 0x053F (depth=4)
TX ring 16: 0x0540 - 0x057F
```

### 与我们的差异 ✅ 基本一致

**我们的实现** (`tests/04_risky_ops/mt7927_init_dma.c:1838-1868`):
```c
/* 禁用 chain_en BEFORE 写 prefetch (vendor 要求) */
glo &= ~(... | MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN | ...);
mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG, glo, "glo_cfg_prefetch_prep");

/* prefetch 配置（vendor 顺序：RX4-7 → TX16 → TX0-3 → TX15）*/
mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8), "rx4_ext");
mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(5), PREFETCH(0x0080, 0x8), "rx5_ext");
mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0100, 0x8), "rx6_ext");
...
mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0480, 0x4), "tx15_ext");

/* 启用 chain_en + DMA */
val |= MT_WPDMA_GLO_CFG_MT76_SET;  /* 包含 CSR_DISP_BASE_PTR_CHAIN_EN */
val |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;  /* BIT(20) vendor 要求 */
val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
```

**差异**:
1. **RX ring 0 prefetch**: mt7925 配置 RX0-3，我们只配置 RX4-7
   - **我们没有配置 RX ring 0 的 prefetch** (除非 Mode 53 分配了 ring_rx0)
2. **GLO_CFG_EXT0**: mt7925 不写，我们有写但被 `skip_ext0=1` 跳过（第 1906-1912 行）
3. **prefetch 顺序**: mt7925 是 RX→TX，我们是 vendor 顺序（RX4-7→TX16→TX0-3→TX15）

**结论**: dispatch 配置基本一致。差异在于 **RX ring 0 的 prefetch 缺失**，但这是因为我们大多数模式下没有分配 ring_rx0。

---

## D. FWDL 后初始化

### mt7925 upstream 实现

**probe 流程** (`mt76/mt7925/pci.c:271-432`):
```
mt7925_pci_probe()
  ├─ ... (PCIe 初始化)
  ├─ mt7925_dma_init()  ← DMA + ring 分配
  └─ mt7925_register_device()  ← 第 418 行
      ├─ 初始化 workqueue, NAPI, etc.
      └─ queue_work(&dev->init_work)  ← **异步** init_work (第 277 行)
```

**init_work 流程** (`mt76/mt7925/init.c:144-193`, `mt7925_init_work()`):
```c
static void mt7925_init_work(struct work_struct *work)
{
    ret = mt7925_init_hardware(dev);  ← 第 150 行
    ...
    ret = mt76_register_device(&dev->mt76, ...);
    ret = mt7925_init_debugfs(dev);
    ret = mt7925_thermal_init(&dev->phy);
    ret = mt7925_mcu_set_thermal_protect(dev);
    dev->hw_init_done = true;
    mt7925_mcu_set_deep_sleep(dev, ...);
}
```

**hardware init 流程** (`mt76/mt7925/init.c:98-142`, `mt7925_init_hardware()`):
```c
static int mt7925_init_hardware(struct mt792x_dev *dev)
{
    for (i = 0; i < MT792x_MCU_INIT_RETRY_COUNT; i++) {
        ret = __mt7925_init_hardware(dev);  ← 第 129 行
        if (!ret)
            break;
        mt792x_init_reset(dev);
    }
}

static int __mt7925_init_hardware(struct mt792x_dev *dev)
{
    ret = mt792x_mcu_init(dev);  ← 第 102 行，**关键！**
    if (ret) goto out;
    ret = mt76_eeprom_override(&dev->mphy);
    ret = mt7925_mcu_set_eeprom(dev);
    ret = mt7925_mac_init(dev);
}
```

**`mt792x_mcu_init()` 做什么**:
- 定义在 `mt76/mt792x_core.c` 中（未在本次读取文件中，但根据 mt76 架构推断）
- 调用 `mt76_connac_mcu_init_download()` → 固件下载
- 调用 post-boot MCU 初始化命令
- **没有单独的 PostFwDownloadInit**

**关键特点**:
1. **异步初始化**: `init_work` 在 probe 返回后才运行
2. **重试机制**: 最多重试 MT792x_MCU_INIT_RETRY_COUNT 次（通常 3 次）
3. **第一个 MCU 命令**: 在 `mt792x_mcu_init()` 中，不在 probe 中
4. **没有 Windows 的 PostFwDownloadInit 9-cmd 序列**

**probe 中没有发送 MCU 命令**: mt7925_pci_probe() 在 DMA init 后立即调用 `mt7925_register_device()` 并返回，**不等待**固件初始化完成。

### 与我们的差异 ❌

**我们的实现** (`tests/04_risky_ops/mt7927_init_dma.c`):

1. **同步初始化** (第 3678-3724 行):
   ```c
   /* Phase 6: Firmware download */
   ret = mt7927_mcu_fw_download(dev);
   ...
   /* Phase 6a: DMASHDL enable (PostFwDownloadInit from Windows) */
   if (mode == 40 || mode == 52) {
       mt7927_wr(dev, MT_DMASHDL_ENABLE, 0x10101);
   }
   /* Phase 6b: Send NIC_CAPABILITY */
   ret = mt7927_mode40_send_nic_cap(dev);
   ```
   - 在 **probe 函数中直接发送** MCU 命令（同步）
   - 没有异步 init_work

2. **Windows PostFwDownloadInit 部分实现** (仅 Mode 40/52):
   - `BAR0+0xd6060 |= 0x10101` (DMASHDL enable)
   - 发送 NIC_CAPABILITY 命令
   - **但缺少其他 8 条 MCU 命令**（Windows 有 9 条）

3. **没有重试机制**: 固件下载或 MCU 命令失败直接返回错误

**结论**: 我们的驱动在 probe 中同步发送 MCU 命令，而 mt7925 是异步的。更重要的是，**我们缺少 mt792x_mcu_init() 中的完整初始化序列**。

---

## E. 完整的寄存器写入序列

### mt7925 upstream 完整流程

**时间顺序**（从 probe 开始到第一个 MCU 命令）:

```
=== mt7925_pci_probe() 开始 ===

1. PCIe 基础初始化
   - pcim_enable_device()
   - pcim_iomap_regions(BIT(0))  → BAR0 映射
   - pci_read_config_word(PCI_COMMAND) → 确保 MEMORY enabled
   - pci_set_master()
   - pci_alloc_irq_vectors(1, 1, PCI_IRQ_ALL_TYPES)
   - dma_set_mask(DMA_BIT_MASK(32))
   - mt76_pci_disable_aspm() (可选)

2. 设备结构分配
   - mt76_alloc_device() → 分配 mt792x_dev
   - mt76_mmio_init() → BAR0 MMIO 初始化
   - tasklet_init(&irq_tasklet)

3. Power control (CRITICAL)
   - __mt792x_mcu_fw_pmctrl(dev)  ← FW power ON (第 386 行)
   - __mt792xe_mcu_drv_pmctrl(dev)  ← DRV power control (第 390 行)

4. 芯片识别
   - mt76_rr(MT_HW_CHIPID) + mt76_rr(MT_HW_REV) → 读取芯片版本

5. EMI sleep protection (第 399 行)
   - mt76_rmw_field(MT_HW_EMI_CTL, MT_HW_EMI_CTL_SLPPROT_EN, 1)
   - 需要 L1 remap 访问 chip addr 0x18011100

6. WFSYS reset (第 401 行)
   - mt792x_wfsys_reset(dev)
     → clear BIT(0) at 0x7c000140
     → msleep(50)
     → set BIT(0)
     → poll BIT(4) for 500ms

7. 中断配置
   - mt76_wr(host_irq_enable, 0)  ← 禁用中断 (第 405 行)
   - mt76_wr(MT_PCIE_MAC_INT_ENABLE, 0xff) (第 407 行)
   - devm_request_irq(pdev->irq, mt792x_irq_handler) (第 409 行)

8. DMA 初始化 (mt7925_dma_init, 第 414 行) ← **关键阶段**
   8.1. mt76_dma_attach(&dev->mt76)

   8.2. mt792x_dma_disable(dev, true)
        → clear GLO_CFG (TX_DMA_EN, RX_DMA_EN, CHAIN_EN, OMIT bits)
        → poll for DMA idle
        → clear DMASHDL enable in GLO_CFG_EXT0
        → set DMASHDL bypass
        → reset logic (force=true)

   8.3. TX queue 初始化
        - mt76_connac_init_tx_queues(TXQ_BAND0, TX_RING_SIZE, MT_TX_RING_BASE)
        - mt76_wr(MT_WFDMA0_TX_RING0_EXT_CTRL, 0x4) ← **TX ring 0 prefetch**

   8.4. MCU queue 初始化
        - mt76_init_mcu_queue(MT_MCUQ_WM, ring 15, 256 entries, MT_TX_RING_BASE)
        - mt76_init_mcu_queue(MT_MCUQ_FWDL, ring 16, 128 entries, MT_TX_RING_BASE)

   8.5. **RX queue 初始化** ← **关键！**
        - **mt76_queue_alloc(q_rx[0], ring 0, 512 entries, MT_RX_EVENT_RING_BASE)**
        - mt76_queue_alloc(q_rx[2], ring 2, 1536 entries, MT_RX_DATA_RING_BASE)

   8.6. NAPI 初始化
        - mt76_init_queues(dev, mt792x_poll_rx)
        - napi_enable(&dev->mt76.tx_napi)

   8.7. DMA enable (mt792x_dma_enable) ← **关键阶段**
        8.7.1. Prefetch 配置 (mt792x_dma_prefetch)
               - RX ring 0: PREFETCH(0x0000, 0x4)
               - RX ring 1: PREFETCH(0x0040, 0x4)
               - RX ring 2: PREFETCH(0x0080, 0x4)
               - RX ring 3: PREFETCH(0x00c0, 0x4)
               - TX ring 0: PREFETCH(0x0100, 0x10)
               - TX ring 1: PREFETCH(0x0200, 0x10)
               - TX ring 2: PREFETCH(0x0300, 0x10)
               - TX ring 3: PREFETCH(0x0400, 0x10)
               - TX ring 15: PREFETCH(0x0500, 0x4)
               - TX ring 16: PREFETCH(0x0540, 0x4)

        8.7.2. Reset DMA 索引
               - mt76_wr(MT_WFDMA0_RST_DTX_PTR, ~0)
               - mt76_wr(MT_WFDMA0_RST_DRX_PTR, ~0)

        8.7.3. 延迟中断配置
               - mt76_wr(MT_WFDMA0_PRI_DLY_INT_CFG0, 0)

        8.7.4. GLO_CFG 配置 (两阶段)
               Phase 1:
               - mt76_set(GLO_CFG):
                 * TX_WB_DDONE (BIT(6))
                 * FIFO_LITTLE_ENDIAN (BIT(12))
                 * CLK_GAT_DIS (BIT(30))
                 * OMIT_TX_INFO (BIT(28))
                 * DMA_SIZE = 3 (BIT(4,5))
                 * FIFO_DIS_CHECK (BIT(11))
                 * RX_WB_DDONE (BIT(13))
                 * **CSR_DISP_BASE_PTR_CHAIN_EN (BIT(15))** ← 启用 prefetch chain
                 * OMIT_RX_INFO_PFET2 (BIT(21))

               Phase 2:
               - mt76_set(GLO_CFG):
                 * **TX_DMA_EN (BIT(0))**
                 * **RX_DMA_EN (BIT(2))**

        8.7.5. mt7925 专用配置
               - mt76_rmw(UWFDMA0_GLO_CFG_EXT1, BIT(28), BIT(28))
               - mt76_set(WFDMA0_INT_RX_PRI, 0x0F00)
               - mt76_set(WFDMA0_INT_TX_PRI, 0x7F00)

        8.7.6. **NEED_REINIT 信号** ← **关键！通知 MCU**
               - **mt76_set(MT_WFDMA_DUMMY_CR, MT_WFDMA_NEED_REINIT)**
                 * BAR0 offset = 0x02120 (MCU WPDMA0 DUMMY_CR)
                 * BIT(1) = NEED_REINIT

        8.7.7. 中断使能
               - mt76_connac_irq_enable():
                 * tx.all_complete_mask
                 * rx.data_complete_mask (ring 2)
                 * rx.wm2_complete_mask (ring 1, tx done)
                 * **rx.wm_complete_mask (ring 0, MCU event)** ← 关键！
                 * MT_INT_MCU_CMD
               - mt76_set(MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE)

9. 设备注册 (mt7925_register_device, 第 418 行)
   - 初始化 workqueues, mutexes, waitqueues
   - 初始化 wifi 相关结构体
   - **queue_work(system_percpu_wq, &dev->init_work)** ← 异步启动

=== mt7925_pci_probe() 结束，返回 0 ===

=== init_work 开始 (异步，在 probe 返回后) ===

10. Hardware 初始化 (mt7925_init_hardware)
    - mt792x_mcu_init(dev) ← **固件下载 + MCU 初始化**
      * mt76_connac_mcu_init_download() → 固件下载到 RAM
      * **post-boot MCU 命令序列**（具体内容在 mt792x_core.c，未读取）
    - mt76_eeprom_override()
    - mt7925_mcu_set_eeprom()
    - mt7925_mac_init()

11. 设备注册到 mac80211
    - mt76_register_device()

12. 其他初始化
    - mt7925_init_debugfs()
    - mt7925_thermal_init()
    - mt7925_mcu_set_thermal_protect()

13. 标记完成
    - dev->hw_init_done = true
    - mt7925_mcu_set_deep_sleep()

=== init_work 结束 ===
```

### 与我们的差异 ❌

**我们的实现关键差异**:

1. **HOST RX ring 0 缺失** (大多数模式):
   - mt7925: 在步骤 8.5 分配 512-entry ring 0
   - 我们的: 只在 Mode 53 分配 128-entry ring 0

2. **同步 vs 异步**:
   - mt7925: probe 返回后异步执行 init_work
   - 我们的: probe 中同步执行 FWDL 和 MCU 命令

3. **Power control**:
   - mt7925: `__mt792x_mcu_fw_pmctrl()` + `__mt792xe_mcu_drv_pmctrl()`
   - 我们的: `mt7927_mcu_init_mt6639()` (WFSYS reset + MCU ownership)

4. **NEED_REINIT 信号**:
   - mt7925: 在 DMA enable 后立即写 MT_WFDMA_DUMMY_CR (步骤 8.7.6)
   - 我们的: 也有这个写入 (第 2018-2024 行)，**一致** ✅

5. **Prefetch 配置**:
   - mt7925: RX0-3 + TX0-3 + TX15-16
   - 我们的: RX4-7 + TX0-3 + TX15-16 (缺少 RX0-3)

6. **PostFwDownloadInit**:
   - mt7925: **不存在**
   - 我们的: 部分实现（Mode 40/52 有 DMASHDL enable + NIC_CAP）

**关键寄存器写入对比表**:

| 寄存器 | mt7925 | 我们的 (Mode 0-52) | 我们的 (Mode 53) |
|--------|--------|-------------------|------------------|
| **HOST RX ring 0 BASE** | ✅ 设置 (512 entries) | ❌ 未设置 (BASE=0) | ✅ 设置 (128 entries) |
| HOST RX ring 0 EXT_CTRL | ✅ PREFETCH(0x0000, 0x4) | ❌ 未设置 | ❌ 未设置 |
| HOST RX ring 6 BASE | ❌ 未使用 | ✅ 设置 (evt ring) | ✅ 设置 (evt ring) |
| TX ring 15 BASE | ✅ 设置 (MCU WM) | ✅ 设置 (MCU WM) | ✅ 设置 (MCU WM) |
| TX ring 16 BASE | ✅ 设置 (FWDL) | ✅ 设置 (FWDL) | ✅ 设置 (FWDL) |
| GLO_CFG CHAIN_EN | ✅ BIT(15) 设置 | ✅ BIT(15) 设置 | ✅ BIT(15) 设置 |
| GLO_CFG TX/RX_DMA_EN | ✅ BIT(0,2) 设置 | ✅ BIT(0,2) 设置 | ✅ BIT(0,2) 设置 |
| WFDMA_DUMMY_CR NEED_REINIT | ✅ BIT(1) 设置 | ✅ BIT(1) 设置 | ✅ BIT(1) 设置 |
| DMASHDL bypass | ✅ BIT(28) 设置 | ✅ BIT(28) 设置 | ✅ BIT(28) 设置 |
| DMASHDL enable (0xd6060) | ❌ 未写 | ⚠️ Mode 40/52 写 | ⚠️ Mode 40/52 写 |
| HOST_INT_ENA (ring 0) | ✅ 启用 | ❌ 未启用 | ⚠️ 启用但无 NAPI |

---

## 总结：我们遗漏了什么

### 🔴 关键遗漏（可能导致 MCU_RX0=0）

1. **HOST RX ring 0 完全缺失**（默认模式）:
   - **根本原因**: 固件在启动时可能检查 HOST RX ring 0 是否存在（BASE != 0）
   - 如果不存在 → 固件不配置 MCU_RX0 环 → NIC_CAP 命令无法返回
   - **mt7925 始终在 FWDL 前分配 512-entry ring 0**
   - **我们的驱动在 Mode 0-52 完全没有 ring 0**

2. **HOST RX ring 0 prefetch 未配置**（即使 Mode 53）:
   - mt7925: RX ring 0 是 prefetch 链第一个（base=0x0000, depth=4）
   - 我们的: Mode 53 分配了 ring_rx0，但**没有配置 EXT_CTRL**
   - prefetch 链从 RX4 开始，RX0 可能被硬件忽略

3. **中断配置缺失**:
   - mt7925: 启用 `rx.wm_complete_mask` (HOST_RX_DONE_INT_ENA0)
   - 我们的: 只启用 ring 6 的中断（evt_ring_qid），**没有 ring 0 中断**

4. **NAPI 缺失**:
   - mt7925: 为 q_rx[MT_RXQ_MCU] 注册 NAPI (`&dev->mt76.napi[MT_RXQ_MCU]`)
   - 我们的: 没有 NAPI 处理 ring 0 的 RX 事件

### ⚠️ 次要遗漏（可能不影响 MCU_RX0）

5. **异步初始化缺失**:
   - mt7925: probe 快速返回，init_work 异步执行
   - 我们的: probe 中同步执行所有初始化（可能导致超时或竞态）

6. **Power control 序列不同**:
   - mt7925: `__mt792x_mcu_fw_pmctrl()` + `__mt792xe_mcu_drv_pmctrl()`
   - 我们的: `mt7927_mcu_init_mt6639()` (基于 Android vendor 代码)
   - 可能导致设备电源状态不同

7. **重试机制缺失**:
   - mt7925: MCU 初始化失败可重试 3 次
   - 我们的: 失败直接返回错误

### ✅ 已正确实现（与 mt7925 一致）

- DMASHDL bypass 配置 ✅
- GLO_CFG CHAIN_EN 启用 ✅
- WFDMA_DUMMY_CR NEED_REINIT 信号 ✅
- TX ring 15/16 配置 ✅
- TXD 格式修正 (Q_IDX=0x20, no LONG_FORMAT) ✅

---

## 推荐行动方案

### 🎯 立即测试（Mode 53 已实现，需重启）

**Mode 53 当前实现**:
- ✅ 分配 HOST RX ring 0 (128 entries)
- ❌ 未配置 RX ring 0 prefetch EXT_CTRL
- ❌ 未启用 RX ring 0 中断
- ❌ 未注册 RX ring 0 NAPI

**推荐修改（Mode 54）**:
```c
/* 1. 增加 ring 0 entries 到 512 (匹配 mt7925) */
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx0, 0, 512, MT_RX_BUF_SIZE);

/* 2. 配置 RX ring 0 prefetch (在 DMA prefetch 阶段) */
mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(0), PREFETCH(0x0000, 0x4), "rx0_ext");
/* 将 RX4 的 base 后移 */
mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0040, 0x8), "rx4_ext");
/* ... 后续 RX5-7 和 TX 环也相应后移 */

/* 3. 启用 RX ring 0 中断 */
mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_ENA,
                 HOST_RX_DONE_INT_ENA0 |  /* ← 新增 */
                 HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                 HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16,
                 "host_int_ena");

/* 4. (可选) 注册 NAPI 处理 ring 0 RX */
/* 需要完整的 mt76 框架移植，暂时跳过 */
```

### 📋 后续任务

1. **Mode 54**: 实现上述 4 点修改，重启测试
2. **Mode 55**: 如果 Mode 54 失败，尝试完整的 mt7925 prefetch 布局（RX0-3 + TX0-3）
3. **Mode 56**: 实现异步 init_work（避免 probe 阻塞）
4. **Mode 57**: 实现 mt792x_mcu_init() 的完整 post-boot 初始化序列

---

**分析完成时间**: 2026-02-15
**分析者**: upstream-dma agent (Claude Sonnet 4.5)
**下一步**: 向 team-lead 报告分析结果
