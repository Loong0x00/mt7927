# MT7927 (MT6639) Linux WiFi Driver — Architecture Document

**芯片**: MT7927 = MT6639 移动芯片 PCIe 封装 | **PCI ID**: `14c3:6639`
**驱动状态**: 完全工作（S44 — 硬件加密 + DHCP + 互联网 ping 通）
**参考来源**: 唯一权威 — Windows 驱动 `mtkwecx.sys` Ghidra 逆向工程

---

## 目录 / Table of Contents

1. [为什么不能直接改 mt76](#part1)
2. [最大限度复用的内核 API](#part2)
3. [完全独立实现的部分](#part3)
4. [进入 mt76 主线的路径分析](#part4)

---

<a id="part1"></a>
## 第一部分：为什么不能直接改 mt76

### Why Not Simply Patch mt76

---

### 1.1 寄存器空间根本不同 / Completely Different Register Space

MT6639 与 MT7925（mt76 家族最近的芯片）的 BAR0 映射存在根本性差异。这一结论最初由 [@ehausig](https://github.com/ehausig) 在原始探索工作中确立，是整个驱动从零编写的起点。

**MT7925 BAR0 布局（mt76 家族）**：
```
WFDMA HOST_DMA0:   BAR0 + 0x7001_xxxx （PCIe window 基址 0x7001_0000）
MCU RX ring (0):   BAR0 + 0x0200 起
中断寄存器:        BAR0 + 0x2000 区域
```

**MT6639 (MT7927) BAR0 布局**：
```c
// src/mt7927_pci.h 第 34-36 行
#define MT_WFDMA0_BASE              0xd4000    // BAR0 + 0xd4000
#define MT_WFDMA_EXT_CSR_BASE       0xd7000    // BAR0 + 0xd7000
#define MT_HIF_DMASHDL_BASE         0xd6000    // BAR0 + 0xd6000

// 对比 mt7925 (mt76/mt792x_regs.h):
// MT_WFDMA0_BASE = 0x54000000 (通过 PCIe remap)
```

**具体差异**（经 Ghidra 逆向确认，总线地址 → BAR0 偏移）：

| 功能模块 | MT6639 BAR0 偏移 | MT7925 等效 | 差异 |
|----------|-----------------|------------|------|
| GLO_CFG  | `0xd4208`       | `0x54000208` (通过 remap) | 地址范围完全不同 |
| INT_STA  | `0xd4200`       | 同区域但 remap 基址不同 | |
| RX Ring 4 | `0xd4540`      | mt7925 用 Ring 0 (`0x0500`) | **ring 编号不同** |
| DMASHDL  | `0xd6000`       | `0x54006000` | |
| CONN_ON  | `0xe0010`       | 无对应 | MT6639 特有 |
| CB_INFRA_RGU | `0x1f8000` | 无对应 | MT6639 特有 |
| CB_INFRA_SLP | `0x1f5000` | 无对应 | MT6639 特有 |
| MCIF remap   | `0xd1034`  | 无对应 | MT6639 特有 |
| L1 remap     | `0x155024` | 无对应 | MT6639 特有（访问 0x18xxxxxx 芯片地址）|

此外，MT6639 还有大量 mt7925 完全没有的寄存器，如 PCIe 地址重映射（`MT_CB_INFRA_MISC0_PCIE_REMAP_WF` = `0x1f6554`）和 CONNINFRA 唤醒控制（`MT_WAKEPU_TOP` = `0xe01A0`）。

**English summary**: MT6639's BAR0 layout is at `0x7C0xxxxx` bus addresses (mapped to BAR0+0xd4000 and above), while MT7925 uses `0x7001xxxx`. The RX ring numbers differ (MT6639 uses rings 4/6/7, MT7925 uses ring 0), interrupt registers are at different offsets, and MT6639 has entirely unique subsystems (CONNINFRA, CB_INFRA, MCIF) absent from MT7925.

---

### 1.2 WFDMA 初始化差异 / WFDMA Initialization Differences

mt7925 和 MT6639 的 WFDMA 初始化在多个关键点不兼容：

**Ring 编号不同**（src/mt7927_pci.h 第 132-172 行）：
```c
// MT6639 (本驱动)
#define MT_RXQ_DATA      4   // WiFi 数据帧 — RX Ring 4
#define MT_RXQ_MCU_EVENT 6   // MCU 事件 — RX Ring 6 (⚠️ 不是 ring 0!)
#define MT_RXQ_DATA2     7   // 辅助接收

// mt7925 (mt76)
// 使用 RX Ring 0 作为 MCU 事件接收
// 使用 RX Ring 1 作为数据接收
```

**预取配置格式不同**（src/mt7927_pci.h 第 181-193 行）：
```c
// MT6639 使用 4 个 packed 寄存器 (Windows RE 确认)
#define MT_WFDMA_PREFETCH_CFG0   MT_WFDMA_EXT_CSR(0x00f0)  // 0xd70f0 = 0x660077
#define MT_WFDMA_PREFETCH_CFG1   MT_WFDMA_EXT_CSR(0x00f4)  // 0xd70f4 = 0x1100
#define MT_WFDMA_PREFETCH_CFG2   MT_WFDMA_EXT_CSR(0x00f8)  // 0xd70f8 = 0x30004f
#define MT_WFDMA_PREFETCH_CFG3   MT_WFDMA_EXT_CSR(0x00fc)  // 0xd70fc = 0x542200
```

**GLO_CFG 初始值不同**：
- MT6639: `glo_cfg |= 0x5`（TX_DMA_EN | RX_DMA_EN）
- MT7925: `glo_cfg |= 0x4000005`（额外设置 BIT(26) = ADDR_EXT_EN）

**CLR_OWN 副作用**（MT6639 特有）：`CLR_OWN` 操作会清零所有 HOST ring 的 BASE 寄存器，必须在之后重新编程所有 ring 地址（src/mt7927_pci.c 第 424-444 行的 `mt7927_ring_reprogram_*` 函数）。mt7925 无此行为。

---

### 1.3 MCU 命令协议差异 / MCU Command Protocol Differences

**UniCmd header 宽度**：
```c
// MT6639 (src/mt7927_pci.h 第 738-750 行)
struct mt7927_mcu_uni_txd {
    __le32 txd[8];      // 32 字节硬件描述符
    __le16 len;         // 0x20: payload 长度
    __le16 cid;         // 0x22: 16-bit CID ← MT6639 使用 16-bit UniCmd CID
    ...
    u8 option;          // 0x2B: 关键 option 字节
};  // 总共 0x30 字节

// mt7925 使用 mt76_connac_mcu_uni_txd，CID 格式略有不同
```

**CID 映射完全不同**（docs/re/win_re_cid_mapping.md — Ghidra 逆向 58 条目分发表）：

Windows 驱动在虚拟地址 `0x1402507e0` 有一个 58 条目分发表，每条目 13 字节：
- `outer_tag`（2B）：调用 `nicUniCmdAllocEntry` 时传入的路由 key
- `inner_CID`（2B）：写入 UniCmd header 的实际 CID

关键映射（已通过 Ghidra 汇编级验证）：

| 命令 | outer_tag | inner_CID | mt7925 CID | 是否相同 |
|------|-----------|-----------|------------|---------|
| DEV_INFO | `0x11` | `0x01` | `0x01` | 相同 |
| BSS_INFO | `0x05` | `0x02` | `0x02` | 相同 |
| STA_REC | `0xb1` | `0x25` | 不同 | **不同** |
| NIC_CAP | `0x8a` | `0x0e` | `0x8a` | outer≠inner |
| EFUSE_CTRL | `0x58` | `0x05` | `0x2d` | **完全不同** |
| DBDC | `0x28` | `0x28` | 无此命令 | MT6639 特有 |

**历史教训**（CLAUDE.md 记录）：使用 Windows outer_tag 作为 header CID 会导致固件静默忽略命令。outer_tag 仅用于驱动内部分发，inner_CID 才是固件看到的值。

**TLV 结构差异**：BSS_INFO 包含 14 个 TLV（src/mt7927_pci.c `mt7927_mcu_add_bss_info` 函数），STA_REC 包含 13 个 TLV，其中多个字段格式（packed_flags、conn_state 枚举等）与 mt7925 不同且相互不兼容。

---

### 1.4 固件下载流程差异 / Firmware Download Differences

**固件文件格式相同**（CONNAC2/3 共用格式），但下载地址不同：
```c
// src/mt7927_pci.h 第 688 行
#define MCU_PATCH_ADDRESS_MT6639  0x00900000  // MT6639 patch 地址
// vs mt7925: MCU_PATCH_ADDRESS = 0x200000
```

**固件同步寄存器不同**：
- MT6639: 轮询 `MT_CONN_ON_MISC`（BAR0 + `0xe00f0`）的 bit[1:0] = 0x3
- MT7925: 轮询不同寄存器的不同位

**前置初始化序列**（MT6639 完全独有，src/mt7927_pci.c `mt7927_mcu_init_mt6639` 函数）：
1. CONNINFRA 唤醒（`MT_WAKEPU_TOP` = `0xe01A0`）
2. CB_INFRA PCIe 地址重映射（`0x1f6554`/`0x1f6558`）
3. EMI 睡眠保护（通过 L1 remap 访问芯片地址 `0x18011100`）
4. WF 子系统复位（`MT_CB_INFRA_RGU_WF_SUBSYS_RST` = `0x1f8600`）
5. MCU ownership 设置（`MT_CB_INFRA_MCU_OWN_SET` = `0x1f5034`）
6. 轮询 ROMCODE_INDEX（`0xc1604`）= `0x1D1E`
7. MCIF remap（`0xd1034` = `0x18051803`）

mt7925 无此前置序列；mt7925 的 PCIe probe 直接进入 SET_OWN/CLR_OWN。

---

### 1.5 结论 / Conclusion

**中文**：MT7927/MT6639 与 mt76 家族在寄存器空间、ring 布局、MCU 命令 CID、前置初始化序列、固件地址等多个层面存在根本性不兼容。这不是"加几行代码"的问题——需要全新的硬件抽象层、全新的初始化序列和全新的 MCU 命令实现。mt7925 的代码在这个芯片上运行会立即产生寄存器访问越界或固件命令错误，绝不会正常工作。

**English**: MT7927/MT6639 is fundamentally incompatible with the mt76 family at multiple levels: register address space, DMA ring layout (rings 4/6/7 vs 0/1), MCU command CIDs (58-entry dispatch table with outer≠inner CID distinction), pre-firmware initialization sequence (CONNINFRA/CB_INFRA subsystems absent from MT7925), and firmware load addresses. Patching mt7925 is not viable — every layer needs independent implementation.

---

<a id="part2"></a>
## 第二部分：最大限度复用的内核 API

### Standard Kernel APIs Used in This Driver

---

### 2.1 PCI 子系统

| API | 用途 | 文件:函数 |
|-----|------|----------|
| `pci_enable_device()` | 使能 PCI 设备 | `mt7927_pci.c`: `mt7927_probe()` |
| `pci_request_regions()` | 申请 PCI BAR 资源 | `mt7927_pci.c`: `mt7927_probe()` |
| `pci_iomap()` | 映射 BAR0 到内核虚拟地址 | `mt7927_pci.c`: `mt7927_probe()` |
| `pci_set_master()` | 使能 PCI Bus Master（DMA 能力）| `mt7927_pci.c`: `mt7927_probe()` |
| `pci_set_dma_mask()` / `dma_set_coherent_mask()` | 设置 32-bit DMA 掩码 | `mt7927_pci.c`: `mt7927_probe()` |
| `pci_unregister_driver()` / `module_pci_driver()` | 模块注册/注销 | `mt7927_pci.c`: 模块末尾 |
| `pci_device_id` 表 | PCI ID 匹配（14c3:7927, 14c3:6639）| `mt7927_pci.h`: `MT7927_PCI_DEVICE_ID*` |
| `ioread32()` / `iowrite32()` | MMIO 寄存器读写 | `mt7927_pci.c`: `mt7927_rr()`, `mt7927_wr()` |

---

### 2.2 mac80211 框架

| API | 用途 | 文件:函数 |
|-----|------|----------|
| `ieee80211_alloc_hw()` | 分配 mac80211 硬件对象 | `mt7927_pci.c`: `mt7927_mac80211_init()` |
| `ieee80211_register_hw()` | 注册到 mac80211 子系统 | `mt7927_pci.c`: `mt7927_mac80211_init()` |
| `ieee80211_unregister_hw()` | 注销 | `mt7927_pci.c`: `mt7927_remove()` |
| `ieee80211_hw` 能力标志 | 声明 HW 加密、2.4G/5G 频段等 | `mt7927_pci.c`: `mt7927_mac80211_init()` |
| `ieee80211_ops` 回调集 | TX、扫描、ROC、sta_state、set_key 等 15 个回调 | `mt7927_pci.c`: `mt7927_ops` 结构体 |
| `ieee80211_rx_status` | 填充 RX 帧元数据（信号强度、速率、频段等）| `mt7927_mac.c`: `mt7927_mac_fill_rx()` |
| `ieee80211_rx_irqsafe()` | 将 RX 帧提交给 mac80211 | `mt7927_mac.c`: `mt7927_queue_rx_skb()` |
| `ieee80211_tx_info` / `IEEE80211_SKB_CB()` | 访问 TX 帧元数据 | `mt7927_mac.c`: `mt7927_mac_write_txwi()` |
| `ieee80211_scan_completed()` | 通知 mac80211 扫描结束 | `mt7927_pci.c`: `mt7927_scan_work()` |
| `IEEE80211_HW_SIGNAL_DBM` | 声明信号强度单位 | `mt7927_pci.c` |
| `IEEE80211_HW_SUPPORTS_PS` | 省电模式支持标志 | `mt7927_pci.c` |
| `IEEE80211_HW_HAS_RATE_CONTROL` | 声明硬件速率控制 | `mt7927_pci.c` |
| `ieee80211_channel_to_frequency()` | 信道号→频率转换 | `mt7927_mac.c`: `mt7927_mac_fill_rx()` |
| `ieee80211_get_hdrlen_from_skb()` | 获取 802.11 头长度 | `mt7927_mac.c`: `mt7927_mac_write_txwi_80211()` |
| `ieee80211_is_mgmt()` / `ieee80211_is_data_qos()` | 帧类型判断 | `mt7927_mac.c` |
| `ieee80211_supported_band` | 声明 2.4G/5G 频段能力 | `mt7927_pci.c`: `mt7927_band_2ghz`, `mt7927_band_5ghz` |
| `ieee80211_sta` / `ieee80211_vif` | mac80211 STA 和 VIF 抽象 | `mt7927_pci.c`: sta_state 回调 |

mac80211 回调的实现位于 `mt7927_pci.c` 的 `mt7927_ops` 结构体（源码约第 4200 行），包含：`tx`、`start`、`stop`、`add_interface`、`remove_interface`、`config`、`configure_filter`、`hw_scan`、`cancel_hw_scan`、`sta_state`、`sta_notify`、`set_key`、`mgd_prepare_tx`、`set_rts_threshold`。

---

### 2.3 NAPI 网络轮询框架

| API | 用途 | 文件:函数 |
|-----|------|----------|
| `netif_napi_add()` | 注册 NAPI 实例 | `mt7927_pci.c`: 初始化阶段 |
| `napi_enable()` / `napi_disable()` | 启用/禁用 NAPI | `mt7927_pci.c`, `mt7927_dma.c` |
| `napi_schedule()` | 从中断下半部调度 NAPI | `mt7927_dma.c`: `mt7927_irq_tasklet()` |
| `napi_complete_done()` | 通知 NAPI 处理完成 | `mt7927_dma.c`: `mt7927_poll_rx_data()` 等 |
| `napi_struct` | NAPI 控制块 | `mt7927_pci.h`: `mt7927_dev` 结构体中的 `napi_rx_data`, `napi_rx_mcu`, `tx_napi` |

驱动使用 3 个独立 NAPI 实例：
- `napi_rx_data`：处理 RX ring 4 的 WiFi 数据帧（`mt7927_poll_rx_data`）
- `napi_rx_mcu`：处理 RX ring 6 的 MCU 事件（`mt7927_poll_rx_mcu`）
- `tx_napi`：处理 TX 完成（`mt7927_poll_tx`）

一个特殊设计：UniCmd 等待响应期间通过 `napi_disable(&dev->napi_rx_mcu)` 独占 RX ring 6，防止 NAPI 和同步轮询竞争消费同一个 MCU 响应（`mt7927_pci.c` 第 837-854 行）。

---

### 2.4 DMA 子系统

| API | 用途 | 文件:函数 |
|-----|------|----------|
| `dma_alloc_coherent()` | 分配 DMA 一致性内存（ring 描述符、RX buffer、TXD 池）| `mt7927_pci.c`: `mt7927_rx_ring_alloc()`, `mt7927_tx_ring_alloc()` |
| `dma_free_coherent()` | 释放 DMA 内存 | `mt7927_pci.c`: `mt7927_ring_free()` |
| `dma_map_single()` | 映射 TX skb payload 到 DMA 地址 | `mt7927_mac.c`: `mt7927_tx_prepare_skb()` |
| `dma_unmap_single()` | TX 完成后解除映射 | `mt7927_mac.c`: `mt7927_mac_tx_free()` |
| `dma_mapping_error()` | 检查 DMA 映射是否失败 | `mt7927_mac.c`: `mt7927_tx_prepare_skb()` |
| `lower_32_bits()` | 提取物理地址低 32 位 | 贯穿 DMA 操作 |
| `cpu_to_le32()` / `le32_to_cpu()` | 字节序转换（DMA 描述符使用小端）| 贯穿 DMA 操作 |

所有 RX buffer 使用 `dma_alloc_coherent` 预分配（非 per-packet 动态分配），NAPI 处理时 `memcpy` 到新 skb 再提交。

---

### 2.5 SKB 操作

| API | 用途 | 文件:函数 |
|-----|------|----------|
| `dev_alloc_skb()` | 分配 RX skb | `mt7927_dma.c`: `mt7927_rx_process_one()` |
| `skb_put()` | 扩展 skb 数据区 | `mt7927_dma.c` |
| `skb_pull()` | 移除 skb 头部（剥离 RXD 头）| `mt7927_mac.c`: `mt7927_mac_fill_rx()` |
| `skb_push()` | 在 skb 前端添加数据（802.11 头重建）| `mt7927_mac.c`: `mt7927_mac_fill_rx()` 802.3→802.11 转换段 |
| `dev_kfree_skb_any()` | 释放 skb（TX 完成后）| `mt7927_mac.c`: `mt7927_mac_tx_free()` |
| `skb_get_queue_mapping()` | 获取 skb 队列映射（AC 队列）| `mt7927_mac.c`: `mt7927_mac_write_txwi()` |
| `skb_headroom()` | 检查 skb 头部可用空间 | `mt7927_mac.c`: 802.3→802.11 转换 |
| `IEEE80211_SKB_RXCB()` | 获取 RX 控制块指针 | `mt7927_mac.c`: `mt7927_mac_fill_rx()` |

---

### 2.6 工作队列与同步原语

| API / 类型 | 用途 | 文件:字段/函数 |
|-----------|------|--------------|
| `INIT_WORK()` / `schedule_work()` | 异步初始化、管理帧发送 | `mt7927_pci.c`: `init_work`, `mgmt_tx_work` |
| `INIT_DELAYED_WORK()` / `schedule_delayed_work()` | 扫描完成延迟通知 | `mt7927_pci.c`: `scan_work` |
| `spinlock_t` / `spin_lock_irqsave()` | TX token pool 并发保护 | `mt7927_pci.h`: `mt7927_dev.tx_token.lock` |
| `tasklet_struct` / `tasklet_schedule()` | IRQ 下半部 | `mt7927_dma.c`: `mt7927_irq_tasklet` |
| `request_irq()` / `free_irq()` | MSI 中断注册 | `mt7927_pci.c`: `mt7927_probe()` |
| `pci_enable_msi()` | 启用 MSI 中断 | `mt7927_pci.c`: `mt7927_probe()` |
| `init_completion()` / `wait_for_completion_timeout()` / `complete()` | ROC_GRANT 同步等待（最多 4 秒）| `mt7927_pci.c`: `mt7927_mgd_prepare_tx()` |
| `wait_queue_head_t` / `wait_event_interruptible()` | MCU 命令响应等待 | `mt7927_pci.h`: `mt7927_dev.mcu_wait` |
| `sk_buff_head` / `skb_queue_*` | 扫描事件队列、管理帧队列 | `mt7927_pci.h`: `mt7927_phy.scan_event_list` |
| `BITFIELD` 宏 (`FIELD_PREP`, `FIELD_GET`, `GENMASK`) | 寄存器位域操作 | 贯穿所有文件 |

---

### 2.7 固件加载

| API | 用途 | 文件:函数 |
|-----|------|----------|
| `request_firmware()` | 加载 patch 和 RAM 固件文件 | `mt7927_pci.c`: `mt7927_load_patch()`, `mt7927_load_ram()` |
| `release_firmware()` | 释放固件缓冲区 | 同上 |
| `MODULE_FIRMWARE()` | 声明固件依赖（供 depmod 使用）| `mt7927_pci.c` 第 38-39 行 |

固件路径：`mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin` 和 `mediatek/WIFI_RAM_CODE_MT6639_2_1.bin`。

---

### 2.8 cfg80211 / nl80211

mac80211 层自动将 cfg80211 调用转发给 `ieee80211_ops` 回调，因此驱动不直接使用 cfg80211 API。nl80211 接口（`iw` 命令）通过 cfg80211 → mac80211 → `mt7927_ops` 的标准路径工作。

唯一直接使用 cfg80211 结构体的地方是扫描请求处理（`mt7927_pci.c` `mt7927_mcu_hw_scan()`），其中通过 `struct cfg80211_scan_request` 访问 SSID 列表和信道列表。

---

<a id="part3"></a>
## 第三部分：完全独立实现的部分（MT6639 特有）

### Parts Implemented From Scratch (No Kernel Equivalent)

---

### 3.1 WFDMA 初始化序列

**实现位置**：`mt7927_pci.c`:`mt7927_wpdma_config()`（第 1146 行起）

完整初始化流程（严格按 Windows `MT6639WpdmaConfig` 顺序）：

```
1. 触发预取重置 (读回写 MT_WFDMA_PREFETCH_CTRL)
2. 写 4 个 packed 预取配置寄存器 (0xd70f0-0xd70fc)
   CFG0=0x660077, CFG1=0x1100, CFG2=0x30004f, CFG3=0x542200
3. 写 per-ring 预取配置 EXT_CTRL (各 ring 的 SRAM 起始地址和深度)
   RX4: base=0x0000, depth=0x8
   RX6: base=0x0080, depth=0x8
   RX7: base=0x0100, depth=0x4
   TX16: base=0x0140, depth=0x4
   TX15: base=0x0180, depth=0x10
   TX0:  base=0x0280, depth=0x4
4. 写 GLO_CFG: TX_DMA_EN|RX_DMA_EN|TX_WB_DDONE|CHAIN_EN|OMIT_RX_INFO|ADDR_EXT|OMIT_TX|CLK_GATE_DIS
5. 写 GLO_CFG_EXT1: BIT(28) 无条件设置
```

**关键差异**：Windows 明确**不**写 Ring 2 的 EXT_CTRL（`docs/re/win_re_wfdma_glo_cfg.md` 确认），仅使用 packed prefetch。

**CLR_OWN 后重建**（`mt7927_reprogram_prefetch()`）：CLR_OWN 操作会清零 EXT_CTRL 和 packed prefetch 寄存器，因此固件下载完成后必须重新写入。

---

### 3.2 固件下载协议

**实现位置**：`mt7927_pci.c` 第 1258-1452 行

固件下载分两阶段（patch + RAM），使用两条独立 TX ring：

**阶段 1 — Patch 下载（TX ring 16, `ring_fwdl`）**：
```
patch_sem_ctrl(GET)       → MCU 获取 patch 信号量
init_download(addr, len, mode) → MCU 准备目标地址
scatter(chunks)           → DMA 传输固件数据（每块最多 2048 字节）
start_patch()             → MCU 校验 + 激活 patch
patch_sem_ctrl(RELEASE)   → 释放信号量
```

patch 地址固定为 `0x00900000`（MT6639 特有，区别于 mt7925 的 `0x200000`）。

**阶段 2 — RAM 下载（TX ring 16）**：
```
for each region (最多 6 个):
    init_download(region.addr, region.len, mode)
    scatter(chunks)    → 每块最多 4096 字节 (MT_FWDL_MAX_LEN)
fw_start(override, option)
```

RAM 固件 trailer 在文件末尾（`mt76_connac2_fw_trailer`），描述每个 region 的地址、长度和加密模式。

**固件同步**（`mt7927_fw_download()` 第 1425-1436 行）：
```c
// 轮询 MT_CONN_ON_MISC (BAR0+0xe00f0) bits[1:0] = 0x3，最多 500ms
for (i = 0; i < 500; i++) {
    fw_sync = mt7927_rr(dev, MT_CONN_ON_MISC);
    if ((fw_sync & 0x3) == 0x3) break;  // fw_sync=0x3 表示固件完全启动
    usleep_range(1000, 2000);
}
```

---

### 3.3 MCU UniCmd 通信框架

**实现位置**：`mt7927_pci.c`:`mt7927_mcu_send_unicmd()`（第 787 行起）

UniCmd 是固件启动后所有 MCU 命令的通信协议。TXD 头格式（`mt7927_mcu_uni_txd`）：

```
偏移  长度  字段
0x00  32B   TXD[8]（WFDMA 描述符格式，与数据 TXD 相同）
0x20   2B   len（payload 长度 + 16 字节内部头）
0x22   2B   cid（UniCmd ID，16-bit，与 mt7925 结构体不同）
0x24   1B   rsv
0x25   1B   pkt_type（固定 0xa0）
0x26   1B   frag_n
0x27   1B   seq（序列号，1-15 循环）
0x28   2B   checksum（通常 0）
0x2A   1B   s2d_index（0 = H2N，host to WM）
0x2B   1B   option（关键：BIT(0)=ACK, BIT(1)=UNI, BIT(2)=SET）
```

option 字节定义（`mt7927_pci.h` 第 753-757 行）：
```c
#define UNI_CMD_OPT_ACK     BIT(0)  // 需要等待 ACK
#define UNI_CMD_OPT_UNI     BIT(1)  // UniCmd 格式
#define UNI_CMD_OPT_SET     BIT(2)  // SET（不是 QUERY）
// 0x06 = UNI+SET（fire-and-forget）
// 0x07 = ACK+UNI+SET（等待响应）
```

响应匹配机制（`mt7927_mcu_wait_resp()`）：通过 `seq` 字段匹配，在 RX ring 6 上轮询，超时 2000ms。等待期间禁用 RX ring 6 中断并调用 `napi_disable()` 防止 NAPI 竞争。

**CID 值说明**：本驱动使用的 CID 值（如 `UNI_CMD_ID_NIC_CAP = 0x008a`）与 mt7925 共用，均为经过硬件验证可工作的值，不直接使用 Windows RE 的 inner_CID（后者在某些情况下与 mt7925 CID 不同且会破坏 RF 配置）。

---

### 3.4 TX/RX 描述符格式（CONNAC3）

**TX 描述符（TXD）**：

`mt7927_mac.c` 中有两条独立的 TXD 构建路径：

**CT 模式（Cut-Through，TX ring 0，数据帧）**：`mt7927_mac_write_txwi()`
```
TXD[0]: TX_BYTES | PKT_FMT | Q_IDX
TXD[1]: WLAN_IDX(12bit) | OWN_MAC(6bit) | TID(4bit) | HDR_FORMAT(2bit) | HDR_INFO(5bit) | TGID(2bit)
TXD[2]: FRAME_TYPE(2bit) | SUB_TYPE(4bit) | MAX_TX_TIME(10bit)
TXD[3]: REM_TX_COUNT(5bit) | PROTECT_FRAME | NO_ACK | BCM
TXD[5]: PID(8bit) | TX_STATUS_HOST | TX_STATUS_MCU
TXD[6]: MSDU_CNT(6bit) | DIS_MAT | TX_RATE(6bit)
TXD[7]: TXD_LEN=0（无加密时），1（有 PN 时扩展为 48 字节）
```

后跟 TXP（Transmit Packet Protocol，32 字节），包含 MSDU ID 和 payload DMA 地址的 scatter-gather 表（`mt7927_hw_txp` 结构体，`mt7927_pci.h` 第 860-863 行）。

**SF 模式（Store-and-Forward，TX ring 2，管理帧）**：`mt7927_mac_write_txwi_mgmt_sf()`

TXD 格式已通过 Ghidra 逆向 Windows `XmitWriteTxDv1`（VA `0x1401a2ca4`）汇编级验证：
```
TXD[0]: Q_IDX=8（LMAC_ALTX0 前置队列） | TX_BYTES=skb->len（不含TXD）
TXD[1]: WLAN_IDX | OWN_MAC | TGID | HDR_FORMAT=2（802.11） | HDR_INFO | FIXED_RATE=BIT(31)
TXD[2]: FRAME_TYPE | SUB_TYPE | MAX_TX_TIME
TXD[5]: 0x600 | PID（TX_STATUS_HOST + TX_STATUS_FMT，必须设置否则固件不报告完成）
TXD[6]: 0x004B0000（OFDM 6Mbps 固定速率）| MSDU_CNT=1 | DIS_MAT
TXD[7]: 0（固定速率路径清 BIT(30)）
```

SF 模式下 TXD 和 802.11 帧数据连续存放在同一个 DMA buffer，无需 TXP scatter-gather。

**RX 描述符（RXD）**：

CONNAC3 RXD 格式（`mt7927_mac.c`:`mt7927_mac_fill_rx()`，第 1007 行起）：
```
+0x00  RXD[0-7]  基础头（32 字节，强制存在）
+0x20  GROUP_4   802.11 Frame Control / Seq / QoS（4 DW，可选）
+0x30  GROUP_1   IV 信息（4 DW，可选）
+0x40  GROUP_2   时间戳 / AMPDU 信息（4 DW，可选）
+0x50  GROUP_3   P-RXV 速率信息（4 DW，可选）
+0x60  GROUP_5   C-RXV 扩展（24 DW = 96 字节，可选）
+XX    Payload   802.3 或 802.11 帧数据
```

组是否存在由 RXD[1] 的 GROUP_1-5 标志位决定。解析顺序必须严格按照 GROUP_4→1→2→3→5 推进 rxd 指针，最后计算 `hdr_gap = (u8*)rxd - skb->data + 2*remove_pad` 确定实际帧数据偏移。

---

### 3.5 RX 帧处理：802.3→802.11 头重建与硬件加密

**问题背景**：MT6639 固件启用了 `MDP_DCR0` 的 `RX_HDR_TRANS_EN`（BIT(19)），硬件自动将收到的 802.11 帧转换为 802.3 格式。但 mac80211 的 802.3 快速路径（`sta->fast_rx`）只在 STA 进入 AUTHORIZED 状态后才生效，导致 AUTHORIZED 之前的所有帧（包括 EAPOL Key 1/4）会被 mac80211 丢弃。

**解决方案**（`mt7927_mac.c`:`mt7927_mac_fill_rx()` 第 1169-1236 行）：
在 `hdr_trans=1` 时，手动将 802.3 帧反向重建为 802.11+LLC/SNAP：

```c
// 保存 802.3 头的 DA、SA、EtherType
// 剥离 14 字节 802.3 头
// 重建 24 字节 802.11 头（FromDS, FC+Dur+A1+A2+A3+SeqCtrl）
// 前置 8 字节 LLC/SNAP（RFC1042 SNAP + EtherType）
// 不设置 RX_FLAG_8023 → mac80211 走慢路径处理
```

**S44 硬件加密修复**（`mt7927_mac.c` 第 1209-1213 行）：

重建 802.11 头时，如果 `status->flag & RX_FLAG_DECRYPTED` 已设置，则必须在 FC 中设置 `IEEE80211_FCTL_PROTECTED`：

```c
if (status->flag & RX_FLAG_DECRYPTED)
    fc_flags |= IEEE80211_FCTL_PROTECTED;
```

不设置此位会导致 mac80211 安全降级检测丢弃帧（mac80211 检测到：有已安装密钥的 STA 收到了"未加密"帧）。

**加密状态检测**（第 1086-1091 行）：
```c
if (FIELD_GET(MT_RXD2_NORMAL_SEC_MODE, rxd2) != 0 &&
    !(rxd1 & (MT_RXD1_NORMAL_CLM | MT_RXD1_NORMAL_CM))) {
    status->flag |= RX_FLAG_DECRYPTED | RX_FLAG_IV_STRIPPED |
                    RX_FLAG_MMIC_STRIPPED | RX_FLAG_MIC_STRIPPED;
}
```

---

### 3.6 硬件加密（STA_REC_KEY_V3 + WTBL）

**实现位置**：`mt7927_pci.c`:`mt7927_mcu_add_key()`（`set_key` 回调触发）

MT6639 使用 STA_REC_KEY_V3 格式（tag=0x27）通过 `STA_REC_UPDATE` 命令安装密钥：

```c
struct sta_rec_key_v3 {
    __le16 tag;       // 0x27 = STA_REC_KEY_V3
    __le16 len;
    u8  key_id;
    u8  cipher_type;  // CONNAC3 cipher: CCMP=4, TKIP=2, GCMP=11, CCMP_256=10
    u8  is_pairwise;
    u8  tx_key;       // PTK: 1, GTK: 0
    u8  key_len;
    u8  key[32];
    // ...
};
```

**CONNAC3 WTBL cipher 位置**（关键，与 CONNAC2 不同）：
LWTBL DW2 bits[20:16] 存储 `CIPHER_SUIT_PGTK`（NOT bits[3:0]，这是 CONNAC2 位置）：
```c
// mt7927_pci.h 第 461-467 行
#define MT_LWTBL_DW2_AID    GENMASK(11, 0)
#define MT_LWTBL_DW2_QOS    BIT(26)
#define MT_LWTBL_DW2_HT     BIT(27)
// cipher 在 bits[20:16] (CONNAC3 specific)
```

**TX 加密**：在 TXD[3] 设置 `MT_TXD3_PROTECT_FRAME`（BIT(1)），固件自动查 WTBL 获取密钥进行加密和 IV 注入，不设置 `IEEE80211_KEY_FLAG_GENERATE_IV`。

**密钥分类**：
- PTK（成对密钥）：`tx_key=1, key_type=1, peer=sta->addr`
- GTK（组播密钥）：`tx_key=0, key_type=0, peer=bssid`

---

### 3.7 BSS/STA 管理命令序列

**实现位置**：`mt7927_pci.c`:`mt7927_mgd_prepare_tx()` 和各 BSS/STA 函数

完整的 WiFi 连接命令序列（对齐 Windows RE `win_re_full_connect_cmd_sequence.md`）：

```
[0] ChipConfig (CID=0xca)  → "KeepFullPwr 1"（每次 connect 都发）
[1] DEV_INFO (CID=0x01)    → 16字节 flat 格式：
                               byte[0]=omac_idx, byte[1]=0xFF(STA type)
                               byte[4-7]=conn_info=0x000C0000
                               byte[8]=active=1（触发 MUAR 编程！）
                               byte[10-15]=MAC 地址
[2] BssActivateCtrl(BASIC+MLD) (CID=0x02)
                           → BASIC: active=bss_idx, band_idx=0xFF（继承 DEV_INFO）
                           → MLD: link_id=0xff, group_mld_id=bss_idx
[3] PM_DISABLE (CID=0x02, tag=0x1B)
[4] BSS_INFO full (CID=0x02, 13 TLVs)
                           → BASIC(0), RATE(0xB), SEC(0x10), QBSS(0xF)
                              SAP(0xD), P2P(0xE), HE(5), COLOR(4)
                              MBSSID(6), 0C(0xC), IOT(0x18), MLD(0x1A), EHT(0x1E)
[4.5] BSS_RLM (CID=0x02, 3 TLVs)
                           → RLM(2), PROTECT(3), IFS_TIME(0x17)
[5] SCAN_CANCEL (CID=0x16, tag=UNI_SCAN_CANCEL)
[6] ROC/CH_PRIVILEGE (CID=0x27)
                           → tag=0(ACQUIRE), dbdcband=0xff(AUTO)
                           → 等待 ROC_GRANT 事件（最多 4 秒）
[ROC_GRANT] → 清除 RFCR DROP_OTHER_UC bit（允许非 MUAR 匹配的单播帧通过）
[7] STA_REC (CID=0x25, 13 TLVs, STATE=0)
                           → BASIC(0), RA(1), STATE(7=0), HT(9), VHT(0xA)
                              PHY(0x15), BA(0x16), HE_6G(0x17), HE(0x19)
                              MLD_SETUP(0x20), EHT_MLD(0x21), EHT(0x22), UAPSD(0x24)
[8] RX_FILTER (CID=0x08, filter=0x0B)
[9] Auth TX → 通过 TX ring 2 (SF mode)
[AUTH-2 收到] → mac80211 → sta_state AUTH→ASSOC
    BSS_INFO update + STA_REC(STATE=2)
[ASSOC-RESP 收到] → mac80211 → sta_state ASSOC→AUTHORIZED
    STA_REC(STATE=2)
[EAPOL → 4-Way Handshake → set_key(PTK/GTK)]
[CONNECTED]
```

**关键突破点**（记录在 MEMORY.md/CLAUDE.md）：
- S40: `BssActivateCtrl band_idx=0xFF`（继承而非覆盖）
- S40: `BASIC TLV active = mvif->bss_idx`（不是 boolean 1）
- S40: `ROC_GRANT handler` 清除 RFCR `DROP_OTHER_UC`
- S43: 移除 BNRCFR bypass（Windows 从不写此寄存器）
- S43: DEV_INFO byte[8]=active=1（Ghidra 确认）
- S43: 802.3→802.11 头反转（解决 EAPOL 在 AUTHORIZED 前被丢弃）
- S44: FC Protected bit 在头重建时设置（解决 mac80211 安全降级丢弃）

---

<a id="part4"></a>
## 第四部分：进入 mt76 主线的路径分析

### Upstreaming Path to mt76

---

### 4.1 需要在 mt76 框架内新建的内容

mt76 框架的核心抽象层（`mt76.h`、`mt76_connac.h`）设计上支持新芯片扩展，但 MT6639 需要以下新增内容：

**新 chip 目录**：`drivers/net/wireless/mediatek/mt76/mt6639/`（类比 `mt7925/`）

> ⚠️ **注意**：此目录中的所有代码均需**从本驱动迁移并适配 mt76 框架**，或**根据 Windows RE 文档从零实现**。**禁止参考 Android mt6639 驱动源码**——该源码与实际硬件行为存在大量偏差，已在开发中验证不可信。唯一权威参考：`docs/re/` 下的 52 份 Ghidra 逆向文档。
>
> ⚠️ **Note**: All code in this directory must be either **migrated from this driver and adapted to the mt76 framework**, or **implemented from scratch based on Windows RE documentation**. **Do not reference the Android mt6639 driver source** — it contains numerous inaccuracies verified during development. The sole authoritative reference is the 52 Ghidra reverse-engineering documents under `docs/re/`.

需要的文件：
```
mt6639/
├── init.c          # mt6639_probe() + 初始化序列 (MCU init + WFDMA)
├── pci.c           # PCIe probe/remove, DMA ring 注册
├── mcu.c           # UniCmd 命令构建（BSS_INFO 14 TLV, STA_REC 13 TLV 等）
├── mac.c           # TXD/RXD 处理、头重建、加密支持
├── main.c          # mac80211 ops 回调
├── regs.h          # MT6639 寄存器定义（全部 0x7C0xxxxx 地址族）
└── mt6639.h        # 芯片常量、数据结构
```

**mt76 core 需要的扩展**：
- `mt76_chip_cap` 新增 MT6639 标志（CONNAC3、DBDC、HW_HDR_TRANS 等）
- RX ring 编号抽象（目前 mt76 硬编码了 ring 0 作为 MCU 事件接收；MT6639 使用 ring 6）
- WFDMA packed prefetch 支持（目前 mt76 使用 per-ring EXT_CTRL）
- UniCmd CID 分发表（MT6639 有独立的 58 条目映射，与 mt7925 不完全相同）

**需要适配 mt76 抽象层的接口**：
- `mt76_dev` 嵌入（替换本驱动的 `mt7927_dev` 自定义结构体）
- `mt76_wcid` 使用（替换 `mt7927_wcid`）
- `mt76_txwi_cache` token 管理（替换当前的手写 `tx_token` 池）
- `mt76_queue` ring 管理（替换 `mt7927_ring`）
- `mt76_mcu_msg_alloc()` / `mt76_mcu_send_msg()` 通信路径

---

### 4.2 可从本驱动直接迁移的代码

以下代码块经过硬件验证，可直接或微调后迁移：

| 代码段 | 来源文件:行号 | 迁移工作量 |
|--------|-------------|-----------|
| MCU 初始化序列（CONNINFRA/CB_INFRA/EMI/MCIF）| `mt7927_pci.c` 第 981-1053 行 | 低（纯逻辑，无框架依赖）|
| WFDMA 预取配置值（4 个寄存器 + per-ring）| `mt7927_pci.c` 第 1150-1183 行 | 低 |
| 固件下载协议（patch + 6 RAM regions）| `mt7927_pci.c` 第 1258-1452 行 | 中（需适配 `mt76_firmware_request`）|
| PostFwDownloadInit 序列 | `mt7927_pci.c` 第 1543-1787 行 | 中 |
| CONNAC3 TXD 格式（8 DW，完整字段定义）| `mt7927_pci.h` 第 1037-1116 行 | 低（已与 mt76 定义格式兼容）|
| CONNAC3 RXD 格式（6 个可选 GROUP）| `mt7927_pci.h` 第 1133-1205 行 + `mt7927_mac.c`:`mt7927_mac_fill_rx()` | 中 |
| 802.3→802.11 头重建逻辑 | `mt7927_mac.c` 第 1169-1235 行 | 低 |
| BSS_INFO 13 TLV 完整实现 | `mt7927_pci.c`:`mt7927_mcu_add_bss_info()` | 中（需适配 mt76 TLV builder）|
| STA_REC 13 TLV 完整实现 | `mt7927_pci.c`:`mt7927_mcu_sta_update()` | 中 |
| STA_REC_KEY_V3 硬件加密 | `mt7927_pci.c`:`mt7927_mcu_add_key()` | 中 |
| 连接命令序列（ROC→auth 完整流程）| `mt7927_pci.c`:`mt7927_mgd_prepare_tx()` | 高（需重构为 mt76 event 驱动）|
| UniCmd CID 映射表 | `docs/re/win_re_cid_mapping.md` | 低（参考文档，写入 `mt6639.h`）|
| RFCR DROP_OTHER_UC 修复逻辑 | `mt7927_mac.c`:ROC_GRANT 处理 | 低 |
| WTBL rate table 初始化 | `mt7927_pci.c`:`mt7927_mac_init_basic_rates()` | 低 |
| MIB 统计寄存器定义（CODA 验证）| `mt7927_pci.h` 第 547-562 行 | 低 |

---

### 4.3 主要障碍

**技术障碍**：

1. **RX ring 编号差异（高优先级）**：mt76 core 当前假设 MCU 事件在 ring 0，数据帧在其他 ring。MT6639 使用 ring 6 作为 MCU 事件接收。需要在 `mt76_dev` 中增加 `mcu_rx_ring` 配置字段，允许每个芯片指定 MCU event ring 编号。

2. **CONNAC3 CID 分发机制**：当前 mt76 的 UniCmd 路径将 CID 直接写入 header，MT6639 需要维护一个 outer_tag → inner_CID 的 58 条目映射表，确保写入 header 的是 inner_CID 而非 outer_tag。

3. **头重建的上下文依赖**：802.3→802.11 头重建需要知道 `seq_ctrl`（从 GROUP_4 提取），而 GROUP_4 在帧头中位于可选位置。这一逻辑需要在 RXD 解析的最后才能完成，与 mt76 当前 `mt76_connac3_mac_fill_rx()` 的流程存在差异。

4. **DBDC 和 ROC 交互**：MT6639 是 DBDC 芯片（2.4G 和 5G 使用不同的无线电 band_idx 0/1），ROC_GRANT 事件必须更新 VIF 的 band_idx 才能使后续 TXD 的 TGID 字段正确。mt76 当前的 ROC 实现不包含此 DBDC 特有逻辑。

5. **BssActivateCtrl 格式**：Windows RE 确认 DEV_INFO flat 格式（16 字节，非 TLV）是 MT6639 固件要求的格式。mt76 的 `mt7925_mcu_set_dev_info()` 使用不同格式，无法直接复用。

**流程障碍**：

6. **测试环境**：上游 mt76 维护者需要 6GHz/320MHz 等 WiFi 7 特性测试，目前本驱动开发者仅有 WiFi 5 路由器（`CMCC-Pg2Y-2.4G`），5GHz 连接未测试，6GHz/WiFi 7 高级特性均未实现。

7. **代码质量**：当前代码包含大量调试 `dev_info()` 和 `print_hex_dump()` 输出，不符合上游提交标准。需要系统性清理，保留关键错误路径的 `dev_err()`，移除诊断 dump。

8. **TXD 路径统一**：当前维护两条 TXD 构建路径（CT/SF），需要确认是否可以统一，或保留两条路径并清楚地文档化选择标准。

---

### 4.4 推荐的迁移步骤

1. **准备**：清理调试输出，统一代码风格，补充 kernel-doc 注释
2. **寄存器定义**：提交 `mt6639/regs.h`（BAR0 地址映射）作为第一个 patch
3. **基础框架**：PCIe probe、DMA ring 初始化、WFDMA 配置
4. **固件下载**：patch + RAM 下载流程（可复用 mt76 connac2 基础设施）
5. **MCU 通信**：UniCmd 框架（需新增 CID 分发表）
6. **mac80211 集成**：扫描、BSS_INFO、STA_REC（逐步提交，每步包含对应测试结果）
7. **加密支持**：STA_REC_KEY_V3，WTBL cipher 配置

---

## 附录：关键文件索引

| 文件 | 行数 | 核心内容 |
|------|------|---------|
| `src/mt7927_pci.c` | ~4600 | 初始化序列、MCU 通信、mac80211 回调、连接流程 |
| `src/mt7927_pci.h` | ~1970 | 全部寄存器定义、CONNAC3 TXD/RXD 位域、数据结构 |
| `src/mt7927_mac.c` | ~1500 | TXD 构建（CT+SF 两路）、RXD 解析、RX 帧分发 |
| `src/mt7927_dma.c` | ~900 | IRQ 处理、NAPI poll、TX/RX DMA 操作 |
| `docs/re/win_re_cid_mapping.md` | — | Windows 58 条目 UniCmd 分发表（Ghidra 验证）|
| `docs/re/win_re_full_connect_cmd_sequence.md` | — | 完整连接命令序列（Windows RE）|
| `docs/re/win_re_full_txd_dma_path.md` | — | TXD 格式（汇编级验证）|
| `docs/re/win_re_dma_descriptor_format.md` | — | DMA 描述符格式（Ghidra 验证）|
| `tmp/re_results/consensus/` | — | 43 个函数的逆向共识报告 |

---

*文档生成时间：2026-02-24*
*驱动版本：Session 44（硬件加密 + 互联网连接完整工作）*
*参考来源：Windows mtkwecx.sys v5603998 + v5705275 Ghidra 逆向工程*
