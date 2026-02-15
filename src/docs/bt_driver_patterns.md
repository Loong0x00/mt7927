# MT7927/MT6639 蓝牙驱动设计模式分析

**分析日期**: 2026-02-15
**代码来源**: `/home/user/mt7927/linux-driver-mediatek-mt7927-bluetooth-main/`
**核心文件**: `btmtk.c` (1525 行), `btusb.c` (4435 行), 总计 ~6000 行

---

## 1. 代码结构总览

### 1.1 文件组织

```
linux-driver-mediatek-mt7927-bluetooth-main/
├── btmtk.c              # MediaTek 核心功能实现 (1525 行)
├── btmtk.h              # 头文件，定义数据结构和常量 (299 行)
├── btusb.c              # USB 总线驱动 (4435 行，通用蓝牙 USB)
├── debug_mtk_bt_init.c  # 用户态调试工具 (libusb)
├── extract_firmware.py  # 固件提取脚本
└── Makefile             # 内核模块编译
```

**关键发现**:
- **主要修改集中在 btmtk.c** — 只有 ~1500 行，实现了完整的固件下载和初始化
- **btusb.c 是通用代码** — MediaTek 特定的代码只占一小部分（probe 时设置回调函数）
- **单文件驱动模式** — 核心功能全在 btmtk.c，没有复杂的多文件分层

### 1.2 模块化结构

| 功能模块 | 位置 | 说明 |
|---------|------|------|
| USB 枚举/probe | btusb.c:btusb_probe() | 通用 USB 蓝牙驱动入口 |
| MediaTek 初始化入口 | btusb.c:btusb_mtk_setup() | 设置 btmtk_data 回调函数 |
| 固件下载 (79xx) | btmtk.c:btmtk_setup_firmware_79xx() | **核心功能** |
| 子系统复位 | btmtk.c:btmtk_usb_subsys_reset() | 硬件复位序列 |
| WMT 协议同步 | btmtk.c:btmtk_usb_hci_wmt_sync() | 厂商命令封装 |
| 寄存器读写 | btmtk.c:btmtk_usb_uhw_reg_{read,write}() | USB 控制传输封装 |

---

## 2. 初始化流程

### 2.1 probe 流程（btusb_probe）

```c
btusb_probe()
├── USB 设备枚举（通用代码）
├── 分配 btusb_data + btmtk_data（如果是 BTUSB_MEDIATEK）
├── 设置 MediaTek 特定回调：
│   └── hdev->setup = btusb_mtk_setup
│       hdev->shutdown = btusb_mtk_shutdown
│       hdev->send = btusb_send_frame_mtk
│       data->recv_acl = btmtk_usb_recv_acl
└── 注册 HCI 设备（hci_register_dev）
```

**关键模式**: probe 时不做硬件初始化，只设置回调函数。真正的初始化在 `hdev->setup()` 中。

### 2.2 setup 流程（btmtk_usb_setup）

```c
btmtk_usb_setup()
├── 1. 读取设备 ID（寄存器 0x80000008, 0x70010200）
│   → dev_id = 0x6639
├── 2. 读取固件版本/flavor（0x80021004, 0x70010020）
├── 3. 调用 btmtk_usb_subsys_reset() — 子系统复位
│   ├── 读取 MTK_BT_RESET_REG_CONNV3 (0x70028610)
│   ├── 设置 BIT(5), BIT(13), BIT(0)
│   ├── 写入 MTK_EP_RST_OPT (0x74011890) = 0x00010001
│   ├── 清除中断状态 (0x74000024, 0x74000028) = 0xFF
│   └── 轮询 MTK_BT_RST_DONE (MTK_BT_MISC[8])
├── 4. 构造固件文件名（btmtk_fw_get_filename）
│   → "mediatek/mt6639/BT_RAM_CODE_MT6639_2_1_hdr.bin"
├── 5. 下载固件 btmtk_setup_firmware_79xx()
│   ├── request_firmware()
│   ├── 解析固件头（section_num）
│   ├── 遍历每个 section:
│   │   ├── **过滤**: if ((dlmodecrctype & 0xFF) != 0x01) continue;
│   │   ├── 发送 WMT_PATCH_DWNLD 元数据（flag=0）
│   │   ├── 分块下载（250 字节/块）:
│   │   │   ├── flag=1 (第一块)
│   │   │   ├── flag=2 (中间块)
│   │   │   └── flag=3 (最后一块)
│   │   └── 重试机制（status=PATCH_PROGRESS 时重试）
│   └── release_firmware()
├── 6. 写入 MTK_EP_RST_OPT = 0x00010001
├── 7. 发送 WMT_FUNC_CTRL (param=1) — 启用蓝牙协议
├── 8. 应用低功耗设置（HCI cmd 0xfc7a）
└── 9. 返回成功
```

### 2.3 关键特性

- **无状态机设计** — 线性流程，每步失败直接 `goto err_release_fw`
- **硬编码寄存器地址** — 所有地址都是 magic number，来自 Windows 驱动逆向
- **vendor-specific 协议** — WMT (Wireless MediaTek) 协议，非标准 HCI

---

## 3. 固件下载实现

### 3.1 关键数据结构

```c
struct btmtk_patch_header {
    u8 datetime[16];
    u8 platform[4];
    __le16 hwver;
    __le16 swver;
    __le32 magicnum;
} __packed;

struct btmtk_global_desc {
    __le32 patch_ver;
    __le32 sub_sys;
    __le32 feature_opt;
    __le32 section_num;  // 固件有多少个 section
} __packed;

struct btmtk_section_map {
    __le32 sectype;
    __le32 secoffset;
    __le32 secsize;
    union {
        struct {
            __le32 dlAddr;     // 下载地址
            __le32 dlsize;     // 数据大小
            __le32 seckeyidx;
            __le32 alignlen;
            __le32 sectype;
            __le32 dlmodecrctype;  // **过滤关键字段**
            __le32 crc;
            __le32 reserved[6];
        } bin_info_spec;
    };
} __packed;
```

### 3.2 固件过滤机制（THE CRITICAL FIX）

```c
// btmtk_setup_firmware_79xx() 中的核心逻辑

for (i = 0; i < section_num; i++) {
    sectionmap = (struct btmtk_section_map *)(fw_ptr +
                  MTK_FW_ROM_PATCH_HEADER_SIZE +
                  MTK_FW_ROM_PATCH_GD_SIZE +
                  MTK_FW_ROM_PATCH_SEC_MAP_SIZE * i);

    section_offset = le32_to_cpu(sectionmap->secoffset);
    dl_size = le32_to_cpu(sectionmap->bin_info_spec.dlsize);

    /* MT6639: 只下载 dlmode byte0 == 0x01 的 section
     * 匹配 Windows 驱动行为，跳过 WiFi/其他 section 避免芯片 hang
     */
    if (dl_size > 0 &&
        (le32_to_cpu(sectionmap->bin_info_spec.dlmodecrctype) & 0xff) != 0x01) {
        continue;  // ← 这 3 行代码是让蓝牙工作的关键
    }

    // ... 下载该 section
}
```

**说明**:
- MT6639 固件有 9 个 section，但只有 5 个是蓝牙 section（0-4）
- Section 5-7 是 WiFi section，发给蓝牙芯片会导致**不可恢复的 hang**
- 通过 `dlmodecrctype & 0xFF == 0x01` 过滤出蓝牙 section
- 这是通过分析 Windows USBPcap 抓包发现的

### 3.3 WMT 协议实现

```c
// WMT 命令头
struct btmtk_wmt_hdr {
    u8  dir;      // 方向（0x01=host→device）
    u8  op;       // 操作码（PATCH_DWNLD=0x1, FUNC_CTRL=0x6）
    __le16 dlen;  // 数据长度
    u8  flag;     // 标志（下载分块：1=first, 2=mid, 3=last）
} __packed;

// 发送 WMT 命令的封装
int btmtk_usb_hci_wmt_sync(struct hci_dev *hdev,
                           struct btmtk_hci_wmt_params *wmt_params)
{
    // 1. 构造 WMT 命令
    // 2. 通过 __hci_cmd_send() 发送（HCI vendor cmd）
    // 3. 等待 HCI_EV_WMT (0xe4) 响应
    // 4. 解析响应状态（PATCH_DONE/PROGRESS/UNDONE）
}
```

### 3.4 分块下载模式

```c
while (dl_size > 0) {
    dlen = min_t(int, 250, dl_size);  // 每块最大 250 字节

    if (first_block == 1) {
        flag = 1;          // 第一块
        first_block = 0;
    } else if (dl_size - dlen <= 0) {
        flag = 3;          // 最后一块
    } else {
        flag = 2;          // 中间块
    }

    wmt_params.flag = flag;
    wmt_params.dlen = dlen;
    wmt_params.data = fw_ptr;

    err = wmt_cmd_sync(hdev, &wmt_params);  // 发送一块
    if (err < 0)
        goto err_release_fw;

    dl_size -= dlen;
    fw_ptr += dlen;
}
```

---

## 4. 错误处理模式

### 4.1 goto cleanup 模式

```c
int btmtk_setup_firmware_79xx(...)
{
    const struct firmware *fw;
    int err;

    err = request_firmware(&fw, fwname, &hdev->dev);
    if (err < 0) {
        bt_dev_err(hdev, "Failed to load firmware file (%d)", err);
        return err;  // ← 早期失败直接返回
    }

    // ... 多步操作

    err = wmt_cmd_sync(hdev, &wmt_params);
    if (err < 0) {
        bt_dev_err(hdev, "Failed to send wmt cmd (%d)", err);
        goto err_release_fw;  // ← 后期失败跳转清理
    }

    // ... 更多操作

err_release_fw:
    release_firmware(fw);  // ← 统一清理点
    return err;
}
```

**特点**:
- **单一清理路径** — 避免重复的 `release_firmware()` 调用
- **错误日志详细** — 每个失败点都有 `bt_dev_err()` 记录
- **不回滚状态** — 没有复杂的状态回滚，失败后设备需要重新 probe

### 4.2 重试机制

```c
retry = 20;
while (retry > 0) {
    wmt_params.op = BTMTK_WMT_PATCH_DWNLD;
    err = wmt_cmd_sync(hdev, &wmt_params);

    if (status == BTMTK_WMT_PATCH_UNDONE) {
        break;  // 成功，继续
    } else if (status == BTMTK_WMT_PATCH_PROGRESS) {
        msleep(100);  // 固件忙，等待重试
        retry--;
    } else if (status == BTMTK_WMT_PATCH_DONE) {
        goto next_section;  // 已完成
    } else {
        err = -EIO;
        goto err_release_fw;  // 失败
    }
}
```

**模式**: 同步轮询 + 固定次数重试（20 次 × 100ms = 最多 2 秒）

### 4.3 超时处理

```c
// 读取寄存器（USB 控制传输）
static int btmtk_usb_uhw_reg_read(...)
{
    buf = kzalloc(4, GFP_KERNEL);

    err = usb_control_msg(udev, pipe, 0x63,  // bRequest
                          USB_TYPE_VENDOR | USB_DIR_IN,
                          reg >> 16, reg & 0xffff,
                          buf, 4,
                          USB_CTRL_SET_TIMEOUT);  // 5000ms 超时
    if (err < 0)
        goto err_free_buf;

    *val = get_unaligned_le32(buf);

err_free_buf:
    kfree(buf);
    return err;
}
```

**特点**: 所有 USB 操作都有固定超时（`USB_CTRL_SET_TIMEOUT` = 5000ms）

---

## 5. 与 vendor 行为的对应关系

### 5.1 vendor 行为复现策略

**完全翻译 Windows 驱动行为**:

```c
// README.md 中记录的 Windows 初始化序列：

Phase 1: 识别
  - Read 0x80000008 → 0x00004254 ("BT")
  - Read 0x70010200 → 0x00006639 (硬件变体)
  - Read 0x80021004 → fw_version
  - Read 0x70010020 → fw_flavor

Phase 2: 子系统复位
  - RMW MTK_BT_RESET_REG_CONNV3 (0x70028610)
    → Set BIT(5), BIT(13), clear BIT(8-15), set BIT(0)
  - Write MTK_EP_RST_OPT (0x74011890) = 0x00010001
  - Write MTK_UDMA_INT_STA_BT (0x74000024) = 0x000000FF
  - Write MTK_UDMA_INT_STA_BT1 (0x74000028) = 0x000000FF

Phase 3: 固件下载
  - 遍历固件 section，过滤 dlmodecrctype & 0xFF == 0x01
  - 每个 section: WMT_PATCH_DWNLD 元数据 → 分块下载（250 字节）

Phase 4: 激活
  - Write MTK_EP_RST_OPT = 0x00010001
  - WMT_FUNC_CTRL (param=1) → 启用蓝牙
```

**Linux 驱动实现**: `btmtk_usb_subsys_reset()` + `btmtk_setup_firmware_79xx()` **完全按照这个序列**

### 5.2 Magic Number 来源

| 代码 | Windows 逆向来源 |
|------|------------------|
| `0x70028610` (MTK_BT_RESET_REG_CONNV3) | USBPcap 抓包，vendor read |
| `0x74011890` (MTK_EP_RST_OPT) | 反编译的 Windows 驱动 |
| `0x00010001` (EP_RST_IN_OUT_OPT) | 固定值，Windows 写入的 |
| `dlmodecrctype & 0xFF == 0x01` | USBPcap 分析 — Windows 只发送 5/9 section |

**注释风格**:
```c
// btmtk.c 中的注释直接说明了来源
/* MT6639: only download sections where dlmode byte0 == 0x01,
 * matching the Windows driver behavior which skips WiFi/other
 * sections that would cause the chip to hang.
 */
```

### 5.3 调试日志（验证行为一致性）

```c
bt_dev_info(hdev, "MT7927-DBG: section %d/%u offset=0x%x dlsize=%u dlmode=0x%08x",
            i, section_num, section_offset, dl_size,
            le32_to_cpu(sectionmap->bin_info_spec.dlmodecrctype));

bt_dev_info(hdev, "MT7927-DBG: skipping section %d (dlmode byte0 != 0x01)", i);
```

**作用**: 运行时验证过滤逻辑是否正确匹配 Windows 行为

---

## 6. 对 WiFi 驱动的借鉴意义

### 6.1 架构设计

| 蓝牙驱动模式 | WiFi 驱动对应 |
|-------------|--------------|
| **单文件核心** (btmtk.c 1500 行) | → WiFi 也可以单文件驱动（mt7927_init_dma.c） |
| **总线层分离** (btusb.c 通用) | → PCIe probe 通用代码可复用（pci_driver） |
| **无状态机** — 线性初始化 | → 简化开发，失败即退出 |
| **goto cleanup** 错误处理 | → 内核推荐模式，清理路径单一 |

**启示**: 不要过度设计。1500 行就能实现完整的蓝牙驱动，WiFi 也不需要复杂分层。

### 6.2 固件下载模式

```c
// 蓝牙模式（可直接迁移）
btmtk_setup_firmware_79xx()
├── request_firmware()           → 用内核 API
├── 解析固件头                    → struct 映射到二进制
├── 遍历 section                 → 简单 for 循环
├── 过滤 section                 → vendor 特定逻辑（WiFi 也需要）
├── 分块下载（vendor cmd）        → WiFi 用 FWDL DMA
└── release_firmware()

// WiFi 对应实现
mt7927_load_firmware()
├── request_firmware("WIFI_RAM_CODE_MT7927.bin")
├── 解析 section（patch_sem → scatter）
├── 过滤 section（DL_MODE_ENCRYPT 标志）
├── DMA 传输（不是 USB 控制传输）
└── release_firmware()
```

**共同点**:
- 都需要 section 过滤（不是所有 section 都发给硬件）
- 都有 vendor 特定的元数据格式
- 都需要重试机制

### 6.3 vendor 行为复现策略

**蓝牙成功的原因**:
1. **完全不参考上游 mt76** — mt76 是错误的芯片家族
2. **100% 复现 Windows 驱动** — USBPcap 抓包 + Ghidra 逆向
3. **无抽象层** — 直接写寄存器，不做"正确的"抽象

**WiFi 应该做的**:
1. ✅ **已在做**: 用 Windows 驱动的寄存器地址（PostFwDownloadInit）
2. ✅ **已在做**: 不参考 mt76（mt76 != mt6639）
3. ❌ **还缺少**: MCU 命令格式需要完全匹配 Windows（Q_IDX=0x20，无 BIT(31)）
4. ❌ **还缺少**: DMASHDL 配置（Windows 在 PostFwDownloadInit 中设置 0xd6060）

### 6.4 固件加载 API 对比

| 蓝牙（USB） | WiFi（PCIe） | 说明 |
|------------|--------------|------|
| `request_firmware()` | `request_firmware()` | 相同，内核统一 API |
| USB 控制传输 | DMA scatter | 传输方式不同 |
| WMT vendor cmd | MCU command | 协议不同 |
| 250 字节/块 | 4096 字节/块 | PCIe 可传更大块 |

### 6.5 错误恢复机制

**蓝牙的恢复策略**:
```c
// btusb_mtk_reset()
int btusb_mtk_reset(struct hci_dev *hdev, void *rst_data)
{
    btmtk_usb_subsys_reset(hdev, btmtk_data->dev_id);
    usb_queue_reset_device(data->intf);  // 重新枚举 USB
    return err;
}
```

**WiFi 的对应**:
```bash
# 当前的设备恢复方法（用户态）
echo 1 | sudo tee /sys/bus/pci/devices/0000:0a:00.0/remove
sleep 2
echo 1 | sudo tee /sys/bus/pci/rescan
```

**可借鉴**: 驱动内部实现 `pci_remove()` + `pci_rescan()` 封装，避免致命的 `pcie_flr()`

---

## 7. 关键代码片段（可参考的模式）

### 7.1 probe 入口（USB → PCIe 迁移）

```c
// 蓝牙 USB probe
static int btusb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct btusb_data *data;
    struct hci_dev *hdev;

    // 分配设备结构
    data = devm_kzalloc(&intf->dev, sizeof(*data), GFP_KERNEL);

    // 枚举端点
    for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
        ep_desc = &intf->cur_altsetting->endpoint[i].desc;
        if (!data->intr_ep && usb_endpoint_is_int_in(ep_desc))
            data->intr_ep = ep_desc;
    }

    // MediaTek 特定初始化
    if (id->driver_info & BTUSB_MEDIATEK) {
        hdev->setup = btusb_mtk_setup;       // 延迟初始化
        hdev->shutdown = btusb_mtk_shutdown;
    }

    return hci_register_dev(hdev);
}

// WiFi PCIe probe 对应（已有的 mt7927_pci_probe）
static int mt7927_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct mt7927_dev *dev;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);

    // PCI 资源枚举
    pci_set_master(pdev);
    dev->bar0 = pci_iomap(pdev, 0, 0);

    // 延迟初始化（类似蓝牙的 setup）
    return mt7927_setup(dev);  // ← 在这里做固件下载
}
```

### 7.2 寄存器读写封装

```c
// 蓝牙的 USB 控制传输封装
static int btmtk_usb_uhw_reg_read(struct hci_dev *hdev, u32 reg, u32 *val)
{
    struct btmtk_data *data = hci_get_priv(hdev);
    int pipe = usb_rcvctrlpipe(data->udev, 0);
    void *buf;
    int err;

    buf = kzalloc(4, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    err = usb_control_msg(data->udev, pipe, 0x63,
                          USB_TYPE_VENDOR | USB_DIR_IN,
                          reg >> 16, reg & 0xffff,
                          buf, 4, USB_CTRL_SET_TIMEOUT);
    if (err < 0)
        goto err_free_buf;

    *val = get_unaligned_le32(buf);

err_free_buf:
    kfree(buf);
    return err;
}

// WiFi PCIe 直接 MMIO（更简单）
static inline u32 mt7927_read32(struct mt7927_dev *dev, u32 offset)
{
    return readl(dev->bar0 + offset);
}

static inline void mt7927_write32(struct mt7927_dev *dev, u32 offset, u32 val)
{
    writel(val, dev->bar0 + offset);
}
```

**启示**: PCIe 的 MMIO 比 USB 控制传输简单得多，不需要复杂的缓冲区管理。

### 7.3 固件下载主循环

```c
// 蓝牙的 section 遍历（可直接迁移到 WiFi）
for (i = 0; i < section_num; i++) {
    sectionmap = (struct btmtk_section_map *)(fw_ptr + header_offset);

    section_offset = le32_to_cpu(sectionmap->secoffset);
    dl_size = le32_to_cpu(sectionmap->bin_info_spec.dlsize);

    // ← CRITICAL: 过滤逻辑（vendor 特定）
    if (dl_size > 0 &&
        (le32_to_cpu(sectionmap->bin_info_spec.dlmodecrctype) & 0xff) != 0x01) {
        bt_dev_info(hdev, "Skipping section %d (not BT)", i);
        continue;
    }

    // 下载该 section
    if (dl_size > 0) {
        err = download_section(hdev, fw_ptr + section_offset, dl_size);
        if (err < 0)
            goto err_release_fw;
    }
}

err_release_fw:
    release_firmware(fw);
    return err;
```

**WiFi 对应**:
```c
// mt7927 固件下载（需要添加类似的过滤）
for (i = 0; i < num_regions; i++) {
    region = &fw_regions[i];

    // ← 需要添加: WiFi 的过滤逻辑（DL_MODE_ENCRYPT 检查）
    if (!(region->flags & DL_MODE_ENCRYPT)) {
        pr_info("Skipping non-encrypted region %d\n", i);
        continue;
    }

    err = mt7927_download_region(dev, region);
    if (err)
        goto err_release_fw;
}
```

### 7.4 日志模式

```c
// 蓝牙的日志风格（详细的调试信息）
bt_dev_info(hdev, "MT7927-DBG: section %d/%u offset=0x%x dlsize=%u dlmode=0x%08x",
            i, section_num, section_offset, dl_size,
            le32_to_cpu(sectionmap->bin_info_spec.dlmodecrctype));

bt_dev_info(hdev, "MT7927-DBG: calling btmtk_setup_firmware_79xx(%s)", fw_bin_name);
bt_dev_info(hdev, "MT7927-DBG: firmware setup done, writing EP_RST_OPT");

// WiFi 应该使用类似的模式
pr_info("mt7927: FW section %d/%u addr=0x%08x size=%u flags=0x%08x\n",
        i, num_regions, region->addr, region->size, region->flags);
pr_info("mt7927: FWDL complete, fw_sync=0x%x\n", fw_sync);
```

**作用**: 详细的日志是调试 vendor 驱动的关键（特别是在没有文档的情况下）

---

## 总结：对 MT7927 WiFi 驱动的实用建议

### 1. 架构层面
- ✅ **保持单文件驱动** — 不需要复杂分层，btmtk.c 只有 1500 行
- ✅ **线性初始化流程** — 无状态机，失败即退出（goto cleanup）
- ✅ **延迟硬件初始化** — probe 时只分配资源，真正初始化在 setup() 中

### 2. 固件下载层面
- ✅ **已有**: 固件下载基础框架（request_firmware + scatter）
- ❌ **缺少**: section 过滤逻辑（类似蓝牙的 `dlmodecrctype & 0xFF == 0x01`）
- ❌ **缺少**: 详细的下载日志（每个 section 的 offset/size/flags）

### 3. vendor 行为复现层面
- ✅ **已有**: 寄存器地址匹配 Windows（BAR0 映射）
- ❌ **缺少**: PostFwDownloadInit 序列（DMASHDL 0xd6060, MCU 命令）
- ❌ **缺少**: MCU 命令格式匹配（Q_IDX=0x20，无 BIT(31)，legacy header）

### 4. 错误处理层面
- ✅ **已有**: goto cleanup 模式
- ❌ **缺少**: 固件下载重试机制（类似蓝牙的 20 次重试）
- ❌ **缺少**: 详细的错误日志（每个失败点的 pr_err）

### 5. 调试策略层面
- ✅ **已有**: Windows 驱动逆向（Ghidra）
- ❌ **缺少**: PCIe 抓包工具（类似 USBPcap for PCIe）
- ✅ **可用**: dmesg 日志 + 寄存器 dump

---

## 最关键的启示

**蓝牙驱动成功的核心原因**:
1. **完全不参考错误的参考代码**（没有盲目复制 upstream）
2. **100% 复现 vendor 行为**（USBPcap 抓包 → 逐字节匹配）
3. **简单直接**（单文件，无抽象，1500 行解决问题）

**WiFi 驱动当前的问题**:
1. ✅ 固件下载成功（fw_sync=0x3）— 已经走得比蓝牙驱动的初始阶段更远
2. ❌ MCU 命令不工作 — 可能是格式不匹配（参考蓝牙的 WMT 协议实现）
3. ❌ 缺少 PostFwDownloadInit — 需要添加 Windows 驱动的 9 条 MCU 命令序列

**下一步建议**:
1. **添加 HOST RX ring 0**（Mode 53 正在测试）— 类似蓝牙驱动分配 ISO endpoint
2. **实现 PostFwDownloadInit 序列** — 参考蓝牙的 `btmtk_usb_setup()` 后半段（FUNC_CTRL 等）
3. **修复 MCU 命令格式** — 用 Windows TXD 格式（Q_IDX=0x20，无 BIT(31)）

---

**文档生成**: 2026-02-15
**分析者**: bt-patterns agent
**目的**: 为 MT7927 WiFi 驱动开发提供蓝牙驱动的成功模式参考
