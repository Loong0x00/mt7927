# MT7927 驱动可复用代码分析

**日期**: 2026-02-15
**源文件**: `/home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.c` (4854 行)
**目标**: 为基于 Windows RE 的新驱动提供可复用代码块

---

## A. 可直接复用的代码块

### A1. PCI Probe 框架 (行 4010-4055, 4817-4832)

**位置**:
- `mt7927_probe()`: 行 4010-4055
- `mt7927_remove()`: 行 4818-4832
- `pci_driver` 结构体: 行 4834-4846
- `module_pci_driver`: 行 4848

**代码片段**:
```c
static int mt7927_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct mt7927_dev *dev;
    int ret;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->pdev = pdev;
    pci_set_drvdata(pdev, dev);

    ret = pci_enable_device(pdev);
    if (ret)
        goto err_free;

    pci_set_master(pdev);

    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if (ret)
        goto err_disable;

    ret = pci_request_regions(pdev, "mt7927");
    if (ret)
        goto err_disable;

    dev->bar0 = pci_iomap(pdev, 0, 0);
    if (!dev->bar0) {
        ret = -ENOMEM;
        goto err_release;
    }
    dev->bar0_len = pci_resource_len(pdev, 0);

    // ... 初始化代码 ...

err_unmap:
    pci_iounmap(pdev, dev->bar0);
err_release:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free:
    kfree(dev);
    return ret;
}

static void mt7927_remove(struct pci_dev *pdev)
{
    struct mt7927_dev *dev = pci_get_drvdata(pdev);

    mt7927_dma_cleanup(dev);
    if (dev->bar0)
        pci_iounmap(pdev, dev->bar0);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
    kfree(dev);
}

static struct pci_device_id mt7927_ids[] = {
    { PCI_DEVICE(0x14c3, 0x7927) },
    { PCI_DEVICE(0x14c3, 0x6639) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, mt7927_ids);

static struct pci_driver mt7927_driver = {
    .name = "mt7927",
    .id_table = mt7927_ids,
    .probe = mt7927_probe,
    .remove = mt7927_remove,
};

module_pci_driver(mt7927_driver);
```

**复用方式**: **直接搬运**，仅需修改：
- 模块名 `"mt7927_init_dma"` → `"mt7927"`
- 移除 mode 参数相关代码
- 调整初始化流程调用

---

### A2. 数据结构定义 (行 13-427)

#### A2.1 硬件寄存器宏 (行 13-233)

**位置**: 行 13-233

**内容**:
- WFDMA/WPDMA 寄存器偏移宏（完全可复用）
- CB_INFRA_RGU, ROMCODE_INDEX 等已验证的寄存器
- DMA 控制字段宏

**复用方式**: **直接搬运**，这些宏定义已经过实际验证。

#### A2.2 Ring 和设备结构体 (行 315-427)

**位置**: 行 315-427

```c
struct mt76_desc {
    __le32 buf0;
    __le32 ctrl;
    __le32 buf1;
    __le32 info;
} __packed __aligned(4);

struct mt76_connac2_mcu_txd {
    __le32 txd[8];
    __le16 len;
    __le16 pq_id;
    u8 cid;
    u8 pkt_type;
    u8 set_query;
    u8 seq;
    u8 uc_d2b0_rev;
    u8 ext_cid;
    u8 s2d_index;
    u8 ext_cid_ack;
    __le32 rsv[5];
} __packed __aligned(4);

struct mt7927_ring {
    struct mt76_desc *desc;
    dma_addr_t desc_dma;
    void **buf;
    dma_addr_t *buf_dma;
    u32 buf_size;
    u16 qid;
    u16 ndesc;
    u16 head;
    u16 tail;
};

struct mt7927_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_len;

    struct mt7927_ring ring_wm;
    struct mt7927_ring ring_fwdl;
    struct mt7927_ring ring_evt;
    struct mt7927_ring ring_rx4;
    struct mt7927_ring ring_rx5;  // ⚠️ 应该是 ring_rx6！
    struct mt7927_ring ring_rx7;

    u8 mcu_seq;
};
```

**需要修改**:
- **移除 `ring_rx0`** — Windows 不使用 HOST RX ring 0
- **`ring_rx5` → `ring_rx6`** — 匹配 Windows (HW ring offset 0x60)
- 移除固件头部结构体，新驱动可能使用不同的固件格式

---

### A3. 寄存器 I/O 辅助函数 (行 431-562)

**位置**: 行 431-562

```c
static inline u32 mt7927_rr(struct mt7927_dev *dev, u32 reg)
{
    if (unlikely(reg + sizeof(u32) > dev->bar0_len)) {
        dev_warn(&dev->pdev->dev,
                 "mmio rr out-of-range: reg=0x%08x bar0_len=0x%llx\n",
                 reg, (u64)dev->bar0_len);
        return 0xffffffff;
    }
    return ioread32(dev->bar0 + reg);
}

static inline void mt7927_wr(struct mt7927_dev *dev, u32 reg, u32 val)
{
    if (unlikely(reg + sizeof(u32) > dev->bar0_len)) {
        dev_warn(&dev->pdev->dev,
                 "mmio wr out-of-range: reg=0x%08x val=0x%08x bar0_len=0x%llx\n",
                 reg, val, (u64)dev->bar0_len);
        return;
    }
    iowrite32(val, dev->bar0 + reg);
}

static void mt7927_wr_verify(struct mt7927_dev *dev, u32 reg, u32 val, const char *name)
{
    u32 rb;

    mt7927_wr(dev, reg, val);
    rb = mt7927_rr(dev, reg);
    dev_info(&dev->pdev->dev, "wr %s: reg=0x%08x val=0x%08x rb=0x%08x\n",
             name, reg, val, rb);
}
```

**复用方式**: **直接搬运**，生产代码可移除 `mt7927_wr_verify` 的 verbose 日志。

---

### A4. DMA Ring 分配和释放 (行 678-780)

**位置**:
- TX ring 分配: 行 678-705
- TX/RX ring 释放: 行 707-732
- RX ring 分配: 行 734-780

```c
static int mt7927_ring_alloc(struct mt7927_dev *dev, struct mt7927_ring *ring,
                             u16 qid, u16 ndesc)
{
    ring->qid = qid;
    ring->ndesc = ndesc;
    ring->head = 0;
    ring->desc = dma_alloc_coherent(&dev->pdev->dev,
                                    ndesc * sizeof(struct mt76_desc),
                                    &ring->desc_dma, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    memset(ring->desc, 0, ndesc * sizeof(struct mt76_desc));
    // 初始化描述符为 DMA_DONE=1
    for (u16 i = 0; i < ndesc; i++)
        ring->desc[i].ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);

    // 写入 WFDMA 寄存器
    mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(qid), lower_32_bits(ring->desc_dma));
    mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(qid), ndesc);
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(qid), 0);
    mt7927_wr(dev, MT_WPDMA_TX_RING_DIDX(qid), 0);

    return 0;
}

static int mt7927_rx_ring_alloc(struct mt7927_dev *dev, struct mt7927_ring *ring,
                                u16 qid, u16 ndesc, u32 buf_size)
{
    u16 i;

    ring->qid = qid;
    ring->ndesc = ndesc;
    ring->head = ndesc - 1;  // ✅ Windows: CIDX = ndesc - 1
    ring->tail = 0;
    ring->buf_size = buf_size;

    ring->desc = dma_alloc_coherent(&dev->pdev->dev,
                                    ndesc * sizeof(struct mt76_desc),
                                    &ring->desc_dma, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    ring->buf = kcalloc(ndesc, sizeof(*ring->buf), GFP_KERNEL);
    ring->buf_dma = kcalloc(ndesc, sizeof(*ring->buf_dma), GFP_KERNEL);
    if (!ring->buf || !ring->buf_dma)
        goto err;

    memset(ring->desc, 0, ndesc * sizeof(struct mt76_desc));

    for (i = 0; i < ndesc; i++) {
        ring->buf[i] = dma_alloc_coherent(&dev->pdev->dev, buf_size,
                                          &ring->buf_dma[i], GFP_KERNEL);
        if (!ring->buf[i])
            goto err;

        ring->desc[i].buf0 = cpu_to_le32(lower_32_bits(ring->buf_dma[i]));
        ring->desc[i].buf1 = cpu_to_le32(0);
        ring->desc[i].info = cpu_to_le32(0);
        ring->desc[i].ctrl = cpu_to_le32(FIELD_PREP(MT_DMA_CTL_SD_LEN0, buf_size));
    }

    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(qid), lower_32_bits(ring->desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(qid), ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(qid), 0);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(qid), ring->head);

    return 0;

err:
    mt7927_ring_free(dev, ring);
    return -ENOMEM;
}

static void mt7927_ring_free(struct mt7927_dev *dev, struct mt7927_ring *ring)
{
    u16 i;

    if (!ring->desc)
        return;

    if (ring->buf && ring->buf_dma) {
        for (i = 0; i < ring->ndesc; i++) {
            if (!ring->buf[i])
                continue;
            dma_free_coherent(&dev->pdev->dev, ring->buf_size,
                              ring->buf[i], ring->buf_dma[i]);
        }
    }
    kfree(ring->buf);
    kfree(ring->buf_dma);

    dma_free_coherent(&dev->pdev->dev,
                      ring->ndesc * sizeof(struct mt76_desc),
                      ring->desc, ring->desc_dma);
    ring->desc = NULL;
}
```

**复用方式**: **直接搬运**，已正确实现 Windows 的 CIDX 初始化 (ndesc-1)。

---

### A5. 固件下载 (FWDL) — 完整流程 (行 1145-1307)

**位置**:
- `mt7927_load_patch()`: 行 1145-1201
- `mt7927_load_ram()`: 行 1224-1285
- `mt7927_mcu_fw_download()`: 行 1287-1307
- 辅助函数: 行 1066-1143

**功能**:
- Patch 下载（sem → init_dl → scatter → finish → sem_release）
- RAM 区域下载（每个区域: init_dl → scatter）
- FW_START_OVERRIDE 并轮询 fw_sync=0x3

**复用方式**: **直接搬运**，这部分已经过验证工作正常（fw_sync=0x3）。

**关键点**:
- `max_len=4096` for PCIe scatter
- RAM 区域必须设置 `DL_MODE_ENCRYPT | DL_MODE_RESET_SEC_IV`
- Patch 地址: MT6639 = 0x900000, mt7925 = 0x200000

---

### A6. MCU 命令发送 (行 922-983)

**位置**: `mt7927_mcu_send_cmd()` 行 922-983

```c
static int mt7927_mcu_send_cmd(struct mt7927_dev *dev, u8 cid,
                               const void *payload, size_t plen)
{
    struct mt76_connac2_mcu_txd *txd;
    dma_addr_t dma;
    void *buf;
    size_t len = sizeof(*txd) + plen;
    u32 val;
    int ret;

    buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    memset(buf, 0, len);
    txd = buf;
    memcpy((u8 *)buf + sizeof(*txd), payload, plen);

    // TXD[0]: Q_IDX=0x20 (MCU 命令端口)
    val = FIELD_PREP(MT_TXD0_TX_BYTES, len) |
          FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) |
          FIELD_PREP(MT_TXD0_Q_IDX, 0x20);  // MT_TX_MCU_PORT_RX_Q0
    txd->txd[0] = cpu_to_le32(val);

    // TXD[1]: HDR_FORMAT_V3, 永不设置 BIT(31) LONG_FORMAT
    val = FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
    txd->txd[1] = cpu_to_le32(val);

    txd->len = cpu_to_le16(len - sizeof(txd->txd));
    txd->pq_id = cpu_to_le16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU, 0));
    txd->cid = cid;
    txd->pkt_type = MCU_PKT_ID;
    txd->seq = ++dev->mcu_seq & 0xf;
    if (!txd->seq)
        txd->seq = ++dev->mcu_seq & 0xf;
    txd->s2d_index = 0;

    ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
    if (!ret)
        ret = mt7927_wait_mcu_event(dev, 200);

    dma_free_coherent(&dev->pdev->dev, len, buf, dma);
    return ret;
}
```

**需要修改**:
- 移除 `evt_ring_qid` 等条件分支
- 使用固定的 CONNAC3 格式（已正确）
- 可选: 移除 `wait_mcu_event` 调用，改用中断驱动的响应处理

---

### A7. MCU 事件轮询 (行 797-868)

**位置**: `mt7927_wait_mcu_event()` 行 797-868

**复用方式**: **需要重写**为中断驱动模式，当前的轮询方式适合调试但不适合生产代码。

---

### A8. L1 Remap 辅助函数 (行 1372-1408)

**位置**: 行 1372-1408

```c
static u32 mt7927_rr_l1(struct mt7927_dev *dev, u32 chip_addr)
{
    u32 base = (chip_addr >> 16) & 0xFFFF;
    u32 offset = chip_addr & 0xFFFF;
    u32 old_l1, val;

    old_l1 = ioread32(dev->bar0 + MT_HIF_REMAP_L1);
    iowrite32(FIELD_PREP(MT_HIF_REMAP_L1_MASK, base),
              dev->bar0 + MT_HIF_REMAP_L1);
    ioread32(dev->bar0 + MT_HIF_REMAP_L1);  // readback

    val = ioread32(dev->bar0 + MT_HIF_REMAP_BASE_L1 + offset);

    iowrite32(old_l1, dev->bar0 + MT_HIF_REMAP_L1);  // restore

    return val;
}

static void mt7927_wr_l1(struct mt7927_dev *dev, u32 chip_addr, u32 val)
{
    u32 base = (chip_addr >> 16) & 0xFFFF;
    u32 offset = chip_addr & 0xFFFF;
    u32 old_l1;

    old_l1 = ioread32(dev->bar0 + MT_HIF_REMAP_L1);
    iowrite32(FIELD_PREP(MT_HIF_REMAP_L1_MASK, base),
              dev->bar0 + MT_HIF_REMAP_L1);
    ioread32(dev->bar0 + MT_HIF_REMAP_L1);

    iowrite32(val, dev->bar0 + MT_HIF_REMAP_BASE_L1 + offset);

    iowrite32(old_l1, dev->bar0 + MT_HIF_REMAP_L1);
}
```

**复用方式**: **直接搬运**，用于访问 0x18xxxxxx 芯片地址。

---

### A9. MT6639 MCU 初始化序列 (行 1422-1626)

**位置**: `mt7927_mcu_init_mt6639()` 行 1422-1626

**功能**:
1. CONNINFRA 唤醒
2. CB_INFRA PCIe remap
3. EMI 睡眠保护
4. WFSYS 复位 (CB_INFRA_RGU 或 WFSYS_SW_RST)
5. MCU ownership 设置
6. 轮询 ROMCODE_INDEX = 0x1D1E
7. MCIF 中断 remap

**复用方式**: **需要修改**，保留核心逻辑但移除 mode 分支和过度日志。

**关键寄存器**:
```c
// 1. CONNINFRA 唤醒
MT_WAKEPU_TOP = 0xe01A0, write 0x1

// 2. PCIe remap
MT_CB_INFRA_MISC0_PCIE_REMAP_WF = 0x1f6554, write 0x74037001

// 3. EMI 睡眠保护 (L1 remap)
MT_HW_EMI_CTL = 0x18011100, |= BIT(1)

// 4a. CB_INFRA_RGU WFSYS 复位
MT_CB_INFRA_RGU_WF_SUBSYS_RST = 0x1f8600, set BIT(4) → wait → clear BIT(4)

// 5. MCU ownership
MT_CB_INFRA_MCU_OWN_SET = 0x1f5034, write BIT(0)

// 6. 轮询 MCU_IDLE
MT_ROMCODE_INDEX = 0xc1604, poll for 0x1D1E

// 7. MCIF remap
MT_MCIF_REMAP_WF_1_BA = 0xd1034, write 0x18051803
```

---

### A10. DMA 禁用 (行 625-676)

**位置**: `mt7927_dma_disable()` 行 625-676

```c
static void mt7927_dma_disable(struct mt7927_dev *dev)
{
    u32 val;
    int i;

    // 1. 清除 DMA_EN 和 chain_en
    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN |
             MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
             MT_GLO_CFG_OMIT_TX_INFO | MT_GLO_CFG_OMIT_RX_INFO_PFET2);
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
    wmb();

    // 2. 等待 DMA idle
    for (i = 0; i < 100; i++) {
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        if (!(val & (MT_WFDMA_GLO_CFG_TX_DMA_BUSY | MT_WFDMA_GLO_CFG_RX_DMA_BUSY)))
            break;
        usleep_range(500, 1000);
    }

    // 3. WFDMA 逻辑复位
    val = mt7927_rr(dev, MT_WFDMA0_RST);
    val &= ~(MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST);
    mt7927_wr(dev, MT_WFDMA0_RST, val);
    val |= MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST;
    mt7927_wr(dev, MT_WFDMA0_RST, val);
    usleep_range(100, 200);
}
```

**复用方式**: **直接搬运**，关键是清除 chain_en 避免使用旧的预取状态。

---

## B. 需要修改的代码

### B1. DMA 初始化 (行 1705-2163)

**位置**: `mt7927_dma_init()` 行 1705-2163

**需要修改的部分**:

#### 修改 1: Ring 编号 (行 1747-1776)

**当前代码**:
```c
ret = mt7927_rx_ring_alloc(dev, &dev->ring_evt, 6, 128, 2048);  // RX ring 6 ✅
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx4, 4, 4, 512);    // RX ring 4 ✅
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx5, 5, 4, 512);    // ❌ 应该是 ring 6！
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx7, 7, 4, 512);    // RX ring 7 ✅
```

**应该改为**:
```c
ret = mt7927_rx_ring_alloc(dev, &dev->ring_evt, 6, 128, 2048);  // MCU 事件主 ring
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx4, 4, 4, 512);     // 数据 RX
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx6, 6, 4, 512);     // ✅ 改为 ring 6
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx7, 7, 4, 512);     // 辅助 RX
```

**同时删除 ring_rx0** (Mode 53 的 ring 0 不需要，Windows 从未使用):
```c
// 删除:
if (reinit_mode == 53) {
    ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx0, 0, 128, MT_RX_BUF_SIZE);
    ...
}
```

#### 修改 2: Prefetch 配置 (行 1849-1868)

**当前代码**: 使用 per-ring EXT_CTRL 寄存器，每个 ring 单独配置预取。

**Windows 方式**: 使用 **packed prefetch 配置**（4 个 32 位寄存器）:

```c
// 替换当前的 per-ring prefetch 为 packed 配置:
writel(readl(bar0 + 0xd7030), bar0 + 0xd7030);  // 触发预取重置
writel(0x660077, bar0 + 0xd70f0);  // Prefetch cfg 0
writel(0x1100,   bar0 + 0xd70f4);  // Prefetch cfg 1
writel(0x30004f, bar0 + 0xd70f8);  // Prefetch cfg 2
writel(0x542200, bar0 + 0xd70fc);  // Prefetch cfg 3
```

**参考**: `docs/analysis_windows_full_init.md` 行 135-140

#### 修改 3: GLO_CFG_EXT1 (行 1915-1917)

**当前代码**:
```c
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
val |= MT_WPDMA_GLO_CFG_EXT1_WIN;
mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG_EXT1, val, "glo_cfg_ext1");
```

**需要确认**: `MT_WPDMA_GLO_CFG_EXT1_WIN` 是否等于 `BIT(28)`。Windows 明确设置 BIT(28):
```c
val = readl(bar0 + 0xd42b4);  // GLO_CFG_EXT1
val |= 0x10000000;  // BIT(28)
writel(val, bar0 + 0xd42b4);
```

#### 修改 4: 中断掩码 (行 1933-1937)

**当前代码**:
```c
mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_ENA,
                 HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                 HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                 HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                 HOST_TX_DONE_INT_ENA17, "host_int_ena");
```

**Windows 值**: `0x2600f000`

**位分析**:
```
BIT(29) = MCU2HOST_SW_INT
BIT(25) = 未确定
BIT(15:12) = 0xF = RX ring 4/5/6/7 完成中断
```

**应该改为**:
```c
mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA, 0x2600f000);

// 可选: MT6639/MT7927 特定中断 (0x74030188 BIT(16))
// 需要确认 BAR0 映射
```

#### 修改 5: PCIe 睡眠配置

**添加**（Windows HIF init 中的操作）:
```c
// 禁用 PCIe 睡眠
val = mt7927_rr(dev, 0x1f5018);  // CB_INFRA_SLP_CTRL
if (val != 0xFFFFFFFF)
    mt7927_wr(dev, 0x1f5018, 0xFFFFFFFF);
```

---

### B2. MCU 命令调度 (需要新增)

**当前代码**: 只有 `mt7927_mcu_send_cmd()`，缺少 Windows 的多级调度路径。

**需要添加**（参考 `docs/win_v5705275_mcu_send_backends.md`）:
1. **命令路由表查找** — 根据 class/target 查找对应的 ring 和格式
2. **CONNAC3 UniCmd 路径** — 0x30 字节头部（legacy 是 0x40 字节）
3. **命令队列管理** — 多命令并发时的队列

**当前可用**: 基础的 legacy 路径已可用，CONNAC3 路径需要时再添加。

---

## C. 必须丢弃的代码（mt7925 污染）

### C1. 所有 mode 相关代码 (行 525-527, 4105-4801)

**位置**:
- `reinit_mode` 参数: 行 525-527
- Mode 40-54 实验代码: 行 2166-4008, 4105-4801

**删除原因**: 这些是调试/实验代码，54 个 mode 分支完全不需要。

---

### C2. ring_rx0 相关代码 (Mode 53)

**位置**: 行 1755-1761, 2565-2570, 等

**删除原因**: Windows 不使用 HOST RX ring 0，Mode 53 假设错误。

---

### C3. ring_rx5 引用

**位置**: 所有 `ring_rx5` 引用

**替换为**: `ring_rx6`（HW ring offset 0x60）

---

### C4. mt7925 风格的条件分支

**例如**:
```c
if (evt_ring_qid == MT_RXQ_MCU_EVENT_RING_CONNAC3)
    val = (no_long_format ? 0 : MT_TXD1_LONG_FORMAT) | ...
else
    val = MT_TXD1_LONG_FORMAT | ...
```

**删除原因**: 新驱动固定使用 CONNAC3 格式（MT6639），不需要 CONNAC2 兼容性。

---

### C5. 过度的诊断日志

**例如**: `mt7927_dump_dma_state()`, `mt7927_dump_win_key_regs()` 等

**处理**: 保留核心逻辑，移除 verbose 日志或改为可选的 debug 级别。

---

## D. 新驱动需要但现有代码没有的

### D1. PreFirmwareDownloadInit 状态轮询

**缺失**: 行 73-86 的 Windows 流程 — 轮询芯片状态最多 500ms。

**需要添加**: 在 MCU init 后调用状态检查函数，等待就绪。

---

### D2. Packed Prefetch 配置

**缺失**: Windows 使用 4 个寄存器 (0xd70f0-0xd70fc) 的 packed 格式。

**当前**: per-ring EXT_CTRL 配置。

**需要替换**: 见 B1.修改2。

---

### D3. 完整的 PostFwDownloadInit MCU 命令序列

**当前**: 只发送 NIC_CAP (class=0x8a)。

**Windows 发送 9 条命令**:
1. class=0x8a (NIC capability)
2. class=0x02 (config {1,0,0x70000})
3. class=0xc0 ({0x820cc800, 0x3c200})
4. class=0xed (buffer download, optional)
5. class=0x28 (DBDC, MT6639 ONLY)
6. class=0xca (scan config)
7. class=0xca (chip config)
8. class=0xca (log config)

**需要添加**: 完整的命令序列实现。

---

### D4. 中断处理程序

**缺失**: 完全没有中断处理代码。

**需要添加**:
1. `request_irq()` / `pci_enable_msi()`
2. 中断处理函数 (ISR)
3. NAPI/tasklet 用于 RX 处理
4. MCU 事件响应队列

---

### D5. TX 数据路径

**当前**: 只有 MCU 命令 TX（`mt7927_mcu_send_cmd`）。

**需要添加**: WiFi 数据帧 TX 路径（netdev 操作）。

---

### D6. RX 数据路径

**当前**: 只有轮询式 MCU 事件读取（`mt7927_wait_mcu_event`）。

**需要添加**:
1. 中断驱动的 RX 处理
2. SKB 分配和上报到网络栈
3. RX ring refill

---

### D7. 0x74030188 中断寄存器配置

**位置**: `docs/analysis_windows_full_init.md` 行 165-168

```c
READ(0x74030188, &val);
val |= BIT(16);
WRITE(0x74030188, val);
```

**问题**: 总线地址 0x74030188 可能不在我们的 bus2chip 映射表中。

**需要确认**: BAR0 映射或 L1 remap。

---

## E. 依赖关系图

```
mt7927_probe()
├── pci_enable_device()
├── pci_iomap() → BAR0 映射
├── mt7927_mcu_init_mt6639() → MCU 初始化
│   ├── CONNINFRA 唤醒
│   ├── PCIe remap
│   ├── EMI 睡眠保护 (L1 remap)
│   ├── WFSYS 复位
│   ├── MCU ownership
│   ├── 轮询 ROMCODE_INDEX
│   └── MCIF remap
├── mt7927_drv_own() → CLR_OWN
├── mt7927_dma_init() → DMA/Ring 初始化
│   ├── mt7927_dma_disable() → 清除旧状态
│   ├── mt7927_ring_alloc() × 2 → TX rings (WM, FWDL)
│   ├── mt7927_rx_ring_alloc() × 4 → RX rings (evt=6, 4, 6, 7)
│   ├── Prefetch 配置 (需要改为 packed)
│   ├── GLO_CFG 启用 (TX_DMA_EN | RX_DMA_EN)
│   ├── GLO_CFG_EXT1 BIT(28)
│   ├── 中断掩码 (0x2600f000)
│   └── DMASHDL bypass
├── mt7927_mcu_fw_download() → 固件下载
│   ├── mt7927_load_patch()
│   │   ├── mt7927_mcu_patch_sem_ctrl(true) → patch_sem
│   │   ├── mt7927_mcu_init_download() → init_dl
│   │   ├── mt7927_mcu_send_scatter() → scatter
│   │   ├── mt7927_mcu_start_patch() → patch_finish
│   │   └── mt7927_mcu_patch_sem_ctrl(false) → sem_release
│   └── mt7927_load_ram()
│       ├── mt7927_mcu_init_download() → init_dl (每个区域)
│       ├── mt7927_mcu_send_scatter() → scatter (带加密)
│       └── mt7927_mcu_start_firmware() → FW_START_OVERRIDE
└── PostFwDownloadInit (需要添加)
    ├── DMASHDL 启用 (0xd6060 |= 0x10101)
    ├── 清除 FWDL bypass
    └── MCU 命令序列 (9 条)
        └── mt7927_mcu_send_cmd()
            ├── 构造 TXD (Q_IDX=0x20, 无 BIT(31))
            ├── mt7927_kick_ring_buf() → 提交到 ring
            └── mt7927_wait_mcu_event() → 等待响应 (待改为中断驱动)

辅助函数:
mt7927_rr() / mt7927_wr() ← 所有寄存器 I/O
mt7927_rr_l1() / mt7927_wr_l1() ← L1 remap (0x18xxxxxx 地址)
mt7927_ring_free() ← DMA 清理
mt7927_dma_cleanup() ← 所有 ring 释放
```

---

## F. 最小可用新驱动结构

基于可复用代码，最小可用新驱动应包含：

### 必需文件
```
src/
├── mt7927_pci.c       ← 主驱动文件
├── mt7927_regs.h      ← 寄存器宏定义 (从现有代码复制)
├── mt7927_dma.c       ← DMA/Ring 管理
├── mt7927_mcu.c       ← MCU 命令和固件下载
└── mt7927_core.h      ← 公共头文件
```

### 核心函数清单

**mt7927_pci.c**:
- `mt7927_probe()` — PCI probe（A1）
- `mt7927_remove()` — 清理（A1）
- `mt7927_interrupt()` — 中断处理（需要新增）

**mt7927_dma.c**:
- `mt7927_ring_alloc()` — TX ring 分配（A4）
- `mt7927_rx_ring_alloc()` — RX ring 分配（A4）
- `mt7927_ring_free()` — Ring 释放（A4）
- `mt7927_dma_init()` — DMA 初始化（B1，需要修改）
- `mt7927_dma_disable()` — DMA 禁用（A10）
- `mt7927_kick_ring_buf()` — 提交描述符（现有代码 870-920）

**mt7927_mcu.c**:
- `mt7927_mcu_init_mt6639()` — MCU 初始化（A9，需要修改）
- `mt7927_mcu_fw_download()` — 固件下载（A5）
- `mt7927_mcu_send_cmd()` — MCU 命令（A6，需要修改）
- `mt7927_post_fw_init()` — PostFwDownloadInit（需要新增）

**mt7927_regs.h**:
- 寄存器宏定义（A2.1，直接复制）

**mt7927_core.h**:
- 数据结构（A2.2，需要修改 ring 定义）
- 函数声明

---

## G. 优先级建议（Mode 55 候选）

### 立即修复（最高优先级）

**Mode 55: 三项关键修复**

1. **ring_rx5 → ring_rx6** (HW ring offset 0x50 → 0x60)
2. **Prefetch 配置** (改为 packed 格式: 0xd70f0-0xd70fc)
3. **移除 ring_rx0**（Windows 不使用）

**预期结果**: 如果 ring 6 是 MCU 事件接收 ring，此修复可能直接解决 MCU 响应接收问题。

---

### 后续修复（高优先级）

4. 中断掩码改为 `0x2600f000`
5. GLO_CFG_EXT1 确认 BIT(28)
6. PCIe 睡眠禁用 (`0x1f5018 = 0xFFFFFFFF`)
7. 芯片特定中断 (`0x74030188 BIT(16)`)

---

### 长期改进（中优先级）

8. 完整的 PostFwDownloadInit MCU 命令序列（9 条命令）
9. 中断驱动的 RX 处理（替换轮询）
10. TX/RX 数据路径实现

---

**文档结束 — 2026-02-15**
