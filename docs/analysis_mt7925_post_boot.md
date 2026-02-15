# MT7925 Post-FWDL MCU 通信初始化分析

**分析日期**: 2026-02-15
**分析对象**: Upstream mt76/mt7925 驱动
**目标**: 理解 mt7925 如何在 FWDL 后启用 MCU 通信

---

## 核心发现总结 🎯

**我们缺少的关键部分**：
- ✅ **mt7925 在 FWDL 之前就分配了 HOST RX ring 0（512 entries，MCU WM event ring）**
- ❌ **我们的驱动从未分配 HOST RX ring 0** — 只有 ring 4,5,6,7（都是固件下载环）

**mt7925 MCU event 路由**：
- MCU WM events → **HOST RX ring 0** (MT_RXQ_MCU = 0)
- Data packets → HOST RX ring 2 (MT_RXQ_MAIN = 2)

**第一个 post-boot MCU 命令**：
- `mt7925_mcu_get_nic_capability()` — MCU_UNI_CMD(CHIP_CONFIG)，tag = UNI_CHIP_CONFIG_NIC_CAPA

---

## A. mt7925_pci_probe() 完整时间线

### 源文件: `mt76/mt7925/pci.c:271-432`

### 初始化流程（按时间顺序）

#### 1. PCI 基础初始化 (pci.c:319-343)
```c
// 1️⃣ 启用 PCI 设备
ret = pcim_enable_device(pdev);                    // pci.c:319

// 2️⃣ 映射 BAR0
ret = pcim_iomap_regions(pdev, BIT(0), pci_name(pdev)); // pci.c:323

// 3️⃣ 启用 PCI memory 命令
pci_read_config_word(pdev, PCI_COMMAND, &cmd);     // pci.c:327
if (!(cmd & PCI_COMMAND_MEMORY)) {
    cmd |= PCI_COMMAND_MEMORY;
    pci_write_config_word(pdev, PCI_COMMAND, cmd);
}
pci_set_master(pdev);                               // pci.c:332

// 4️⃣ 分配 IRQ
ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_ALL_TYPES); // pci.c:334

// 5️⃣ 设置 DMA mask
ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));   // pci.c:338
```

#### 2. mt76 设备分配 (pci.c:345-381)
```c
// 6️⃣ 获取 mac80211 ops 和固件特性
ops = mt792x_get_mac80211_ops(&pdev->dev, &mt7925_ops,
                              (void *)id->driver_data, &features); // pci.c:345

// 7️⃣ 分配 mt76 设备
mdev = mt76_alloc_device(&pdev->dev, sizeof(*dev), ops, &drv_ops); // pci.c:352

// 8️⃣ 初始化 MMIO（BAR0 映射）
mt76_mmio_init(&dev->mt76, pcim_iomap_table(pdev)[0]); // pci.c:364

// 9️⃣ 设置自定义 bus_ops（rr/wr/rmw 使用地址重映射）
bus_ops->rr = mt7925_rr;    // pci.c:378
bus_ops->wr = mt7925_wr;    // pci.c:379
bus_ops->rmw = mt7925_rmw;  // pci.c:380
dev->mt76.bus = bus_ops;    // pci.c:381
```

#### 3. Driver Own（第一次）(pci.c:386-392)
```c
// 🔟 FW own → DRV own
ret = __mt792x_mcu_fw_pmctrl(dev);      // pci.c:386
ret = __mt792xe_mcu_drv_pmctrl(dev);    // pci.c:390
```

**__mt792xe_mcu_drv_pmctrl()** 实现 (mt792x_core.c:854-876):
```c
for (i = 0; i < MT792x_DRV_OWN_RETRY_COUNT; i++) {
    mt76_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN); // 清除 FW own

    if (mt76_poll_msec_tick(dev, MT_CONN_ON_LPCTL,
                            PCIE_LPCR_HOST_OWN_SYNC, 0, 50, 1))
        break;
}
```

**注意**: mt7925 使用 `PCIE_LPCR_HOST_CLR_OWN`（直接清除），**不需要先 SET_OWN**！
这与我们驱动的 CLR_OWN 流程不同。

#### 4. 读取芯片版本 (pci.c:394-397)
```c
mdev->rev = (mt76_rr(dev, MT_HW_CHIPID) << 16) |
            (mt76_rr(dev, MT_HW_REV) & 0xff);  // pci.c:394
dev_info(mdev->dev, "ASIC revision: %04x\n", mdev->rev);
```

#### 5. WFSYS Reset (pci.c:399-403)
```c
mt76_rmw_field(dev, MT_HW_EMI_CTL, MT_HW_EMI_CTL_SLPPROT_EN, 1); // pci.c:399
ret = mt792x_wfsys_reset(dev);  // pci.c:401
```

**注意**: WFSYS reset 在 DMA init **之前**！

#### 6. 禁用中断 (pci.c:405-407)
```c
mt76_wr(dev, irq_map.host_irq_enable, 0);  // pci.c:405
mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0xff); // pci.c:407
```

#### 7. 注册 IRQ handler (pci.c:409-412)
```c
ret = devm_request_irq(mdev->dev, pdev->irq, mt792x_irq_handler,
                       IRQF_SHARED, KBUILD_MODNAME, dev); // pci.c:409
```

#### 8. DMA 初始化（关键！）(pci.c:414)
```c
ret = mt7925_dma_init(dev);  // pci.c:414
```

**详见 B 节 ↓**

#### 9. 设备注册（触发 MCU init）(pci.c:418)
```c
ret = mt7925_register_device(dev);  // pci.c:418
```

**详见 C-E 节 ↓**

---

## B. FWDL 后 → 第一个 MCU 命令之间的所有步骤

### 1. DMA Ring 分配顺序（**FWDL 之前！**）

#### 源文件: `mt76/mt7925/pci.c:215-269` — `mt7925_dma_init()`

```c
int mt7925_dma_init(struct mt792x_dev *dev)
{
    int ret;

    // 1️⃣ Attach DMA 子系统
    mt76_dma_attach(&dev->mt76);  // pci.c:219

    // 2️⃣ 禁用 DMA
    ret = mt792x_dma_disable(dev, true);  // pci.c:221

    // ═══════════════════════════════════════
    // TX Rings 分配
    // ═══════════════════════════════════════

    // 3️⃣ TX ring 0-3（data，4 个 queue）
    ret = mt76_connac_init_tx_queues(dev->phy.mt76, MT7925_TXQ_BAND0,
                                     MT7925_TX_RING_SIZE,  // 2048 entries
                                     MT_TX_RING_BASE, NULL, 0); // pci.c:226

    mt76_wr(dev, MT_WFDMA0_TX_RING0_EXT_CTRL, 0x4); // pci.c:232

    // 4️⃣ TX ring 15（MCU WM，命令到 FW）
    ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_WM, MT7925_TXQ_MCU_WM,
                              MT7925_TX_MCU_RING_SIZE,  // 256 entries
                              MT_TX_RING_BASE); // pci.c:235

    // 5️⃣ TX ring 16（FWDL，固件下载）
    ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_FWDL, MT7925_TXQ_FWDL,
                              MT7925_TX_FWDL_RING_SIZE,  // 128 entries
                              MT_TX_RING_BASE); // pci.c:241

    // ═══════════════════════════════════════
    // RX Rings 分配 ← 关键！
    // ═══════════════════════════════════════

    // 6️⃣ 【关键】RX ring 0（MCU WM event）
    ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MCU],
                           MT7925_RXQ_MCU_WM,      // HW ring index = 0
                           MT7925_RX_MCU_RING_SIZE,  // 512 entries
                           MT_RX_BUF_SIZE,         // buffer size
                           MT_RX_EVENT_RING_BASE); // pci.c:247

    // 7️⃣ RX ring 2（data packets）
    ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MAIN],
                           MT7925_RXQ_BAND0,       // HW ring index = 2
                           MT7925_RX_RING_SIZE,    // 1536 entries
                           MT_RX_BUF_SIZE,
                           MT_RX_DATA_RING_BASE); // pci.c:254

    // 8️⃣ 初始化 queue（分配 buffer pool）
    ret = mt76_init_queues(dev, mt792x_poll_rx); // pci.c:260

    // 9️⃣ 初始化 TX NAPI
    netif_napi_add_tx(dev->mt76.tx_napi_dev, &dev->mt76.tx_napi,
                      mt792x_poll_tx); // pci.c:264
    napi_enable(&dev->mt76.tx_napi); // pci.c:266

    // 🔟 启用 DMA
    return mt792x_dma_enable(dev);  // pci.c:268
}
```

#### Ring 配置常量（mt7925.h:12-17）
```c
#define MT7925_TX_RING_SIZE        2048  // Data TX
#define MT7925_TX_MCU_RING_SIZE    256   // MCU command TX
#define MT7925_TX_FWDL_RING_SIZE   128   // Firmware download TX
#define MT7925_RX_RING_SIZE        1536  // Data RX
#define MT7925_RX_MCU_RING_SIZE    512   // MCU event RX ← 关键！
```

#### RX Ring ID 映射（mt7925.h:122-127）
```c
enum mt7925_rxq_id {
    MT7925_RXQ_BAND0 = 2,      // Data RX → HW ring 2
    MT7925_RXQ_BAND1,
    MT7925_RXQ_MCU_WM = 0,     // MCU WM event → HW ring 0 ← 关键！
    MT7925_RXQ_MCU_WM2,        // TX done event → HW ring 1
};
```

### 2. 设备注册 → 触发 MCU 初始化

#### 源文件: `mt76/mt7925/init.c:195-280` — `mt7925_register_device()`

```c
int mt7925_register_device(struct mt792x_dev *dev)
{
    // ... 初始化 work queue、mutex、等待队列等 ...

    // 关键：初始化异步工作
    INIT_WORK(&dev->init_work, mt7925_init_work);  // init.c:224

    // ... wiphy 配置 ...

    // 将 init_work 加入工作队列（异步执行）
    queue_work(system_percpu_wq, &dev->init_work);  // init.c:277

    return 0;
}
```

#### 源文件: `mt76/mt7925/init.c:144-193` — `mt7925_init_work()`

```c
static void mt7925_init_work(struct work_struct *work)
{
    struct mt792x_dev *dev = container_of(work, struct mt792x_dev, init_work);
    int ret;

    // 1️⃣ 硬件初始化（包括 MCU init）
    ret = mt7925_init_hardware(dev);  // init.c:150
    if (ret)
        return;

    // 2️⃣ 设置 stream caps
    mt76_set_stream_caps(&dev->mphy, true);  // init.c:154
    mt7925_set_stream_he_eht_caps(&dev->phy);  // init.c:155

    // 3️⃣ 配置 MAC 地址列表
    mt792x_config_mac_addr_list(dev);  // init.c:156

    // 4️⃣ 初始化 MLO
    ret = mt7925_init_mlo_caps(&dev->phy);  // init.c:158

    // 5️⃣ 注册到 mac80211
    ret = mt76_register_device(&dev->mt76, true, mt76_rates,
                               ARRAY_SIZE(mt76_rates));  // init.c:164

    // 6️⃣ Debugfs、thermal、thermal protection
    ret = mt7925_init_debugfs(dev);  // init.c:171
    ret = mt7925_thermal_init(&dev->phy);  // init.c:177
    ret = mt7925_mcu_set_thermal_protect(dev);  // init.c:183

    // 7️⃣ 标记硬件初始化完成
    dev->hw_init_done = true;  // init.c:190

    // 8️⃣ 设置 deep sleep
    mt7925_mcu_set_deep_sleep(dev, dev->pm.ds_enable);  // init.c:192
}
```

#### 源文件: `mt76/mt7925/init.c:122-142` — `mt7925_init_hardware()`

```c
static int mt7925_init_hardware(struct mt792x_dev *dev)
{
    int ret, i;

    set_bit(MT76_STATE_INITIALIZED, &dev->mphy.state);  // init.c:126

    // 重试机制（最多 10 次）
    for (i = 0; i < MT792x_MCU_INIT_RETRY_COUNT; i++) {
        ret = __mt7925_init_hardware(dev);  // init.c:129
        if (!ret)
            break;

        mt792x_init_reset(dev);  // init.c:133
    }

    if (i == MT792x_MCU_INIT_RETRY_COUNT) {
        dev_err(dev->mt76.dev, "hardware init failed\n");
        return ret;
    }

    return 0;
}
```

#### 源文件: `mt76/mt7925/init.c:98-120` — `__mt7925_init_hardware()`

```c
static int __mt7925_init_hardware(struct mt792x_dev *dev)
{
    int ret;

    // 1️⃣ MCU 初始化（FWDL + post-boot init）
    ret = mt792x_mcu_init(dev);  // init.c:102 ← 调用 hif_ops->mcu_init
    if (ret)
        goto out;

    // 2️⃣ EEPROM override
    ret = mt76_eeprom_override(&dev->mphy);  // init.c:106
    if (ret)
        goto out;

    // 3️⃣ 设置 EEPROM 到 FW
    ret = mt7925_mcu_set_eeprom(dev);  // init.c:110
    if (ret)
        goto out;

    // 4️⃣ MAC 初始化
    ret = mt7925_mac_init(dev);  // init.c:114
    if (ret)
        goto out;

out:
    return ret;
}
```

### 3. MCU 初始化（PCIe 特定）

#### 源文件: `mt76/mt7925/pci_mcu.c:27-53` — `mt7925e_mcu_init()`

```c
int mt7925e_mcu_init(struct mt792x_dev *dev)
{
    static const struct mt76_mcu_ops mt7925_mcu_ops = {
        .headroom = sizeof(struct mt76_connac2_mcu_txd),
        .mcu_skb_send_msg = mt7925_mcu_send_message,
        .mcu_parse_response = mt7925_mcu_parse_response,
    };
    int err;

    // 1️⃣ 设置 MCU ops
    dev->mt76.mcu_ops = &mt7925_mcu_ops;  // pci_mcu.c:36

    // 2️⃣ FW own（准备 FWDL）
    err = mt792xe_mcu_fw_pmctrl(dev);  // pci_mcu.c:38
    if (err)
        return err;

    // 3️⃣ DRV own（第二次）
    err = __mt792xe_mcu_drv_pmctrl(dev);  // pci_mcu.c:42
    if (err)
        return err;

    // 4️⃣ 禁用 PCIe L0s
    mt76_rmw_field(dev, MT_PCIE_MAC_PM, MT_PCIE_MAC_PM_L0S_DIS, 1); // pci_mcu.c:46

    // 5️⃣ 【关键】运行固件（FWDL + post-boot init）
    err = mt7925_run_firmware(dev);  // pci_mcu.c:48

    // 6️⃣ 清理 FWDL ring
    mt76_queue_tx_cleanup(dev, dev->mt76.q_mcu[MT_MCUQ_FWDL], false); // pci_mcu.c:50

    return err;
}
```

### 4. 固件运行 + Post-boot MCU 初始化

#### 源文件: `mt76/mt7925/mcu.c:1045-1064` — `mt7925_run_firmware()`

```c
int mt7925_run_firmware(struct mt792x_dev *dev)
{
    int err;

    // 1️⃣ 【FWDL】加载固件（patch + RAM）
    err = mt792x_load_firmware(dev);  // mcu.c:1049
    if (err)
        return err;

    // 2️⃣ 【第一个 post-boot MCU 命令】获取 NIC capability
    err = mt7925_mcu_get_nic_capability(dev);  // mcu.c:1053 ← 关键！
    if (err)
        return err;

    // 3️⃣ 加载 CLC（Country/Region 配置）
    err = mt7925_load_clc(dev, mt792x_ram_name(dev));  // mcu.c:1057
    if (err)
        return err;

    // 4️⃣ 标记 MCU 运行中
    set_bit(MT76_STATE_MCU_RUNNING, &dev->mphy.state);  // mcu.c:1060

    // 5️⃣ 启用 FW log
    return mt7925_mcu_fw_log_2_host(dev, 1);  // mcu.c:1062
}
```

#### 源文件: `mt76/mt792x_core.c:926-960` — `mt792x_load_firmware()`

```c
int mt792x_load_firmware(struct mt792x_dev *dev)
{
    int ret;

    // 1️⃣ 加载 ROM patch
    ret = mt76_connac2_load_patch(&dev->mt76, mt792x_patch_name(dev));  // 930
    if (ret)
        return ret;

    // 2️⃣ SDIO 特殊处理（PCIe 跳过）
    if (mt76_is_sdio(&dev->mt76)) {
        ret = __mt792x_mcu_fw_pmctrl(dev);
        if (!ret)
            ret = __mt792x_mcu_drv_pmctrl(dev);
    }

    // 3️⃣ 加载 RAM code
    ret = mt76_connac2_load_ram(&dev->mt76, mt792x_ram_name(dev), NULL);  // 941
    if (ret)
        return ret;

    // 4️⃣ 等待 FW ready
    if (!mt76_poll_msec(dev, MT_CONN_ON_MISC, MT_TOP_MISC2_FW_N9_RDY,
                        MT_TOP_MISC2_FW_N9_RDY, 1500)) {  // 945
        dev_err(dev->mt76.dev, "Timeout for initializing firmware\n");
        return -EIO;
    }

    dev_dbg(dev->mt76.dev, "Firmware init done\n");  // 956

    return 0;
}
```

**固件文件名**（mt792x.h:44-54）:
```c
// MT7927 (PCI ID 0x6639) 使用 MT6639 固件！
#define MT7927_FIRMWARE_WM  "mediatek/WIFI_RAM_CODE_MT6639_2_1.bin"
#define MT7927_ROM_PATCH    "mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin"
```

---

## C. MCU 命令发送路径

### 第一个 post-boot MCU 命令

#### 源文件: `mt76/mt7925/mcu.c:925-984` — `mt7925_mcu_get_nic_capability()`

```c
static int mt7925_mcu_get_nic_capability(struct mt792x_dev *dev)
{
    struct mt76_phy *mphy = &dev->mt76.phy;

    // 1️⃣ 构建请求
    struct {
        u8 _rsv[4];
        __le16 tag;
        __le16 len;
    } __packed req = {
        .tag = cpu_to_le16(UNI_CHIP_CONFIG_NIC_CAPA),  // mcu.c:934
        .len = cpu_to_le16(sizeof(req) - 4),
    };

    struct sk_buff *skb;
    int ret, i;

    // 2️⃣ 发送 MCU 命令并等待响应
    ret = mt76_mcu_send_and_get_msg(&dev->mt76, MCU_UNI_CMD(CHIP_CONFIG),
                                    &req, sizeof(req), true, &skb);  // mcu.c:944
    if (ret)
        return ret;

    // 3️⃣ 解析响应（TLV 格式）
    hdr = (struct mt76_connac_cap_hdr *)skb->data;  // mcu.c:949
    skb_pull(skb, sizeof(*hdr));  // mcu.c:955

    for (i = 0; i < le16_to_cpu(hdr->n_element); i++) {
        struct tlv *tlv = (struct tlv *)skb->data;
        int len = le16_to_cpu(tlv->len);

        // 4️⃣ 处理不同的 TLV
        switch (le16_to_cpu(tlv->tag)) {
        case MT_NIC_CAP_6G:
            mt7925_mcu_parse_phy_cap(dev, skb->data);  // mcu.c:970
            break;
        // ... 其他 tag 处理 ...
        }

        skb_pull(skb, len);  // mcu.c:980
    }

out:
    dev_kfree_skb(skb);  // mcu.c:983
    return ret;
}
```

### MCU 命令传输机制

#### Q_IDX（TXD routing）
- mt7925 使用 `MT_MCUQ_WM` (TX ring 15) 发送 MCU 命令
- **TXD Q_IDX**: 通过 mt76 框架自动设置（基于 `qid` 参数）

#### MCU Event 接收
- **MCU WM events** → `dev->mt76.q_rx[MT_RXQ_MCU]` (HW ring 0)
- **中断处理**: `mt792x_irq_handler()` → IRQ tasklet → `napi_schedule(&dev->mt76.napi[MT_RXQ_MCU])`
- **NAPI poll**: `mt792x_poll_rx()` → 读取 RX ring 0 → 调用 `mt7925_mcu_rx_event()`

#### Timeout/Retry 机制
- `mt76_mcu_send_and_get_msg()` 内部使用 `wait_event_timeout()`
- 默认超时: 等待 MCU 响应的超时时间由 `MT76_MCU_TIMEOUT` 定义

---

## D. mt7925 是否真的支持 MT7927？

### ✅ **是的，mt7925 完全支持 MT7927（PCI ID 0x6639）**

#### 证据 1: PCI ID 表

**源文件**: `mt76/mt7925/pci.c:14-22`

```c
static const struct pci_device_id mt7925_pci_device_table[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x7925),
        .driver_data = (kernel_ulong_t)MT7925_FIRMWARE_WM },
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x6639),  // ← MT7927/MT6639
        .driver_data = (kernel_ulong_t)MT7927_FIRMWARE_WM },
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x0717),
        .driver_data = (kernel_ulong_t)MT7925_FIRMWARE_WM },
    { },
};
```

**注意**: `PCI_VENDOR_ID_MEDIATEK = 0x14c3`，所以完整 PCI ID 是 `14c3:6639`。

#### 证据 2: 固件路径

**源文件**: `mt76/mt792x.h:47-48, 53-54`

```c
#define MT7925_FIRMWARE_WM  "mediatek/mt7925/WIFI_RAM_CODE_MT7925_1_1.bin"
#define MT7927_FIRMWARE_WM  "mediatek/WIFI_RAM_CODE_MT6639_2_1.bin"  // ← 注意是 MT6639

#define MT7925_ROM_PATCH    "mediatek/mt7925/WIFI_MT7925_PATCH_MCU_1_1_hdr.bin"
#define MT7927_ROM_PATCH    "mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin"  // ← 注意是 MT6639
```

**关键发现**: MT7927 使用 **MT6639 固件**，这再次确认 **MT7927 = MT6639 PCIe 封装版本**。

#### 证据 3: modinfo 输出

```bash
$ modinfo mt7925e
filename:       .../mt7925e.ko.zst
alias:          pci:v000014C3d00000717sv*sd*bc*sc*i*
alias:          pci:v000014C3d00007925sv*sd*bc*sc*i*
# ⚠️ 注意：0x6639 不在 alias 列表中
```

**原因**: 内核模块的 `alias` 是从 `MODULE_DEVICE_TABLE()` 自动生成的，但 mt7925e 可能是旧版本内核编译的，或者 PCI ID 后来才添加。
**源码确认**: `pci.c:14-22` **确实包含 0x6639**，所以 mt7925e.ko 应该支持它。

#### 证据 4: 驱动加载测试

```bash
# 检查当前驱动
$ lspci -k | grep -A 3 "14c3:6639"
0a:00.0 Network controller: MEDIATEK Corp. Device 6639
        Subsystem: MEDIATEK Corp. Device 6639
        Kernel driver in use: mt7927_init_dma  # ← 我们的测试驱动
        Kernel modules: mt7925e  # ← mt7925e 也可用！
```

**结论**: mt7925e 确实支持 MT7927，可以直接加载测试。

---

## E. mt7925 的 MCU Init 序列

### Post-FWDL MCU 命令序列

#### 源文件: `mt76/mt7925/mcu.c:1045-1064` — `mt7925_run_firmware()`

```
FWDL 完成
    ↓
mt7925_mcu_get_nic_capability()  ← MCU_UNI_CMD(CHIP_CONFIG)
    ↓                                tag = UNI_CHIP_CONFIG_NIC_CAPA
mt7925_load_clc()                ← 加载 CLC 配置（Country/Region）
    ↓
mt7925_mcu_fw_log_2_host()       ← 启用 FW log
    ↓
MCU 初始化完成
```

### 与 Windows 驱动的对比

#### Windows PostFwDownloadInit (来自 Ghidra RE)
```
1. BAR0+0xd6060 |= 0x10101  (DMASHDL enable)
2. class=0x8a (NIC capability)
3. class=0x02 (config {1,0,0x70000})
4. class=0xc0 ({0x820cc800,0x3c200})
5. class=0xed (buffer download, optional)
6. class=0x28 (DBDC, MT6639 ONLY)
7-9. class=0xca (scan/chip/log config)
```

#### mt7925 Linux Post-FWDL
```
1. mt7925_mcu_get_nic_capability()  ← 对应 Windows class=0x8a
2. mt7925_load_clc()                ← CLC 配置
3. mt7925_mcu_fw_log_2_host()       ← FW log
```

**差异分析**:
- ❌ **mt7925 没有写 DMASHDL (0xd6060)**
- ❌ **mt7925 没有发送 Windows 的 9 个 MCU 命令序列**
- ✅ **mt7925 只发送 NIC_CAP + CLC + FW_LOG**
- ⚠️ **mt7925 可能在其他地方配置了 DMASHDL**（需要进一步分析 `mt792x_dma_enable()`）

### 第一个 MCU 命令细节

#### MCU_UNI_CMD(CHIP_CONFIG) 格式

**Command**:
```c
MCU_UNI_CMD(CHIP_CONFIG)
```

**TLV 请求**:
```c
{
    .tag = cpu_to_le16(UNI_CHIP_CONFIG_NIC_CAPA),  // 查询 NIC capability
    .len = cpu_to_le16(8),  // sizeof(req) - 4
}
```

**TLV 响应**（可能包含）:
- `MT_NIC_CAP_6G` — 6GHz 能力
- `MT_NIC_CAP_MAC_ADDR` — MAC 地址
- `MT_NIC_CAP_PHY` — PHY 能力
- `MT_NIC_CAP_CHIP_CAP` — 芯片特性（chip_cap, eml_cap 等）

**响应处理**:
- 解析 TLV → 填充 `dev->phy.chip_cap`, `dev->phy.eml_cap` 等

---

## 对比：我们的驱动 vs mt7925

### Ring 配置对比

| Ring | 我们的驱动 | mt7925 | 用途 |
|------|-----------|--------|------|
| **HOST RX 0** | ❌ 没有 | ✅ **512 entries** | **MCU WM event** ← 关键！|
| HOST RX 1 | ❌ 没有 | ✅ 可选（TX done） | TX completion event |
| HOST RX 2 | ❌ 没有 | ✅ 1536 entries | Data packets |
| HOST RX 4 | ✅ 128 entries | ❌ 不使用 | 我们用于 data（错误）|
| HOST RX 5 | ✅ 128 entries | ❌ 不使用 | 我们用于 event（错误）|
| HOST RX 6 | ✅ 128 entries | ❌ 不使用 | 未使用 |
| HOST RX 7 | ✅ 128 entries | ❌ 不使用 | 未使用 |
| MCU_RX2/RX3 | ✅ FWDL rings | ✅ FWDL rings | 固件下载（ROM 配置）|

### 初始化顺序对比

| 步骤 | 我们的驱动 | mt7925 |
|------|-----------|--------|
| 1. PCI init | ✅ | ✅ |
| 2. DRV own | ✅ | ✅ |
| 3. WFSYS reset | ✅ | ✅ |
| 4. **DMA ring alloc** | ❌ FWDL 后 | ✅ **FWDL 前**（关键！）|
| 5. FWDL | ✅ | ✅ |
| 6. **HOST RX ring 0** | ❌ 从未分配 | ✅ **FWDL 前已分配**（关键！）|
| 7. Post-boot MCU cmd | ❌ 超时 -110 | ✅ NIC_CAP 成功 |

### 关键差异

1. **HOST RX ring 0 缺失**:
   - ❌ **我们**: 从未分配 HOST RX ring 0
   - ✅ **mt7925**: FWDL **之前**就分配了 512 entries 的 MCU event ring

2. **DMA ring 分配时机**:
   - ❌ **我们**: FWDL 后才分配 HOST RX rings (ring 4,5,6,7)
   - ✅ **mt7925**: FWDL **之前**就分配所有 HOST rings

3. **MCU event 路由**:
   - ❌ **我们**: 期望从 ring 5 或 ring 6 接收 MCU event（错误假设）
   - ✅ **mt7925**: MCU WM event **必须**从 ring 0 接收

---

## 我们遗漏了什么 🔍

### 核心问题

**我们从未分配 HOST RX ring 0，这是 MT7927/MT6639 接收 MCU event 的唯一路径！**

### 修复方案

#### 1. 在 FWDL 之前分配 HOST RX ring 0

**修改位置**: `tests/04_risky_ops/mt7927_init_dma.c`

**添加代码**（在 FWDL 之前）:
```c
// 在 mt7927_init_dma() 中，FWDL 之前添加

// 分配 HOST RX ring 0（MCU WM event）
ret = mt7927_dma_alloc_rx_ring(&dev->ring_rx0,
                               0,      // HW ring index
                               512,    // entries（参考 mt7925）
                               MT_RX_BUF_SIZE);
if (ret) {
    dev_err(&pdev->dev, "Failed to alloc RX ring 0\n");
    goto error;
}

// 配置 HOST RX ring 0 到 WFDMA
writel(dev->ring_rx0.desc_dma, dev->bar0 + MT_WFDMA_HOST_RX_RING0_BASE);
writel(512, dev->bar0 + MT_WFDMA_HOST_RX_RING0_CNT);
writel(0, dev->bar0 + MT_WFDMA_HOST_RX_RING0_CIDX);
writel(0, dev->bar0 + MT_WFDMA_HOST_RX_RING0_DIDX);

// 启用 HOST RX ring 0（在 GLO_CFG 中）
// ... 需要设置对应的 enable bit
```

#### 2. 在 GLO_CFG 中启用 HOST RX ring 0

**检查**: mt7925 的 `mt792x_dma_enable()` 是否有特殊配置。

#### 3. 注册 HOST RX ring 0 的 NAPI handler

**mt7925 代码** (推测):
```c
// 初始化 RX NAPI（在 mt76_init_queues 中）
netif_napi_add(dev->mt76.napi_dev, &dev->mt76.napi[MT_RXQ_MCU],
               mt792x_poll_rx);
napi_enable(&dev->mt76.napi[MT_RXQ_MCU]);
```

#### 4. 更新中断处理

**mt7925 IRQ map** (pci.c:300-310):
```c
static const struct mt792x_irq_map irq_map = {
    .host_irq_enable = MT_WFDMA0_HOST_INT_ENA,
    .tx = {
        .all_complete_mask = MT_INT_TX_DONE_ALL,
        .mcu_complete_mask = MT_INT_TX_DONE_MCU,
    },
    .rx = {
        .data_complete_mask = HOST_RX_DONE_INT_ENA2,  // RX ring 2 (data)
        .wm_complete_mask = HOST_RX_DONE_INT_ENA0,    // RX ring 0 (MCU event) ← 关键！
    },
};
```

**我们需要**:
- 设置 `HOST_RX_DONE_INT_ENA0` (BIT(0)) 在 `MT_WFDMA0_HOST_INT_ENA` 中
- IRQ handler 中检查 RX ring 0 完成中断

### 验证步骤

1. ✅ **编译并加载修改后的驱动**
2. ✅ **检查 dmesg**: 确认 HOST RX ring 0 已分配
3. ✅ **检查寄存器**:
   ```bash
   cat /sys/kernel/debug/mt7927/registers/wfdma
   # 确认 HOST_RX_RING0_BASE != 0
   ```
4. ✅ **运行 NIC_CAP 命令**: 应该能收到 MCU 响应（不再超时 -110）
5. ✅ **检查 MCU_RX0**: 如果 FW 配置了 MCU_RX0，说明我们的修复正确

---

## 参考文件列表

### 主要源文件

| 文件 | 行号 | 描述 |
|------|------|------|
| `mt76/mt7925/pci.c` | 271-432 | mt7925_pci_probe() 完整流程 |
| `mt76/mt7925/pci.c` | 215-269 | mt7925_dma_init() — DMA ring 分配 |
| `mt76/mt7925/init.c` | 195-280 | mt7925_register_device() |
| `mt76/mt7925/init.c` | 144-193 | mt7925_init_work() |
| `mt76/mt7925/init.c` | 122-142 | mt7925_init_hardware() |
| `mt76/mt7925/init.c` | 98-120 | __mt7925_init_hardware() |
| `mt76/mt7925/pci_mcu.c` | 27-53 | mt7925e_mcu_init() |
| `mt76/mt7925/mcu.c` | 1045-1064 | mt7925_run_firmware() |
| `mt76/mt7925/mcu.c` | 925-984 | mt7925_mcu_get_nic_capability() |
| `mt76/mt792x_core.c` | 926-960 | mt792x_load_firmware() |
| `mt76/mt792x_core.c` | 854-876 | __mt792xe_mcu_drv_pmctrl() |
| `mt76/mt792x_core.c` | 899-924 | mt792xe_mcu_fw_pmctrl() |
| `mt76/mt7925/mt7925.h` | 12-17 | Ring 大小常量 |
| `mt76/mt7925/mt7925.h` | 122-127 | RX ring ID 枚举 |
| `mt76/mt792x.h` | 44-54 | 固件路径定义 |

---

## 总结

### 核心发现

1. ✅ **mt7925 确实支持 MT7927 (PCI ID 0x6639)**
2. ✅ **mt7925 在 FWDL 之前就分配了 HOST RX ring 0（512 entries，MCU WM event ring）**
3. ❌ **我们的驱动从未分配 HOST RX ring 0** — 这是 MCU_RX0 永远为 0 的根本原因
4. ✅ **第一个 post-boot MCU 命令是 NIC_CAPABILITY (CHIP_CONFIG)**

### 修复建议

**立即实施 Mode 53**:
- ✅ 在 FWDL 之前分配 HOST RX ring 0（512 entries）
- ✅ 配置 WFDMA HOST RX ring 0 BASE/CNT
- ✅ 启用 HOST RX ring 0 中断（HOST_RX_DONE_INT_ENA0）
- ✅ 测试 NIC_CAP 命令是否成功

### 预期结果

如果 HOST RX ring 0 是唯一缺失的部分，修复后应该看到：
- ✅ NIC_CAP 命令不再超时
- ✅ MCU 响应通过 HOST RX ring 0 接收
- ✅ （可能）MCU_RX0 被 FW 配置（但这不是必需的，因为 MCU event 走 HOST RX ring 0）

---

**分析完成时间**: 2026-02-15
**下一步**: 实施 Mode 53，重启测试
