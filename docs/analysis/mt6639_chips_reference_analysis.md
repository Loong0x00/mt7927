# MT6639 社区驱动 与 Motorola chips 仓库 分析报告

## 1. 概述

本文档分析了两个新引入的参考资料目录：

| 目录 | 来源 | 内容 |
|---|---|---|
| `mt6639/` | 社区尝试用 MT6639 驱动兼容 MT7927 的项目 | 完整的 MediaTek WiFi 驱动栈，约 503 文件、~200K 行代码 |
| `chips/` | Motorola 的 MediaTek 内核模块仓库 | 18 款芯片的独立实现，含 MT6639 完整源码 |

**结论：两者都有极高价值，尤其是 `chips/mt6639/mt6639.c` 和 `mt6639/include/chips/coda/mt6639/` 中的寄存器定义头文件，可能包含了我们当前 DMA 阻塞问题的解决方案。**

---

## 2. mt6639/ 目录分析

### 2.1 项目性质

这是 MediaTek 官方 WiFi 驱动的完整移植，支持多种芯片（MT6632/MT6639/MT7961/MT7925 等），采用 CONNAC3x 架构。作者在 PCIe 设备表中添加了 MT7927 的支持。

### 2.2 MT7927 支持方式

在 `mt6639/os/linux/hif/pcie/pcie.c` 中：

```c
#define NIC6639_PCIe_DEVICE_ID  0x3107   // MT6639 的 PCIe ID
#define NIC7927_PCIe_DEVICE_ID  0x7927   // MT7927 的 PCIe ID

// 两者共用同一个驱动数据结构
{ PCI_DEVICE(MTK_PCI_VENDOR_ID, NIC6639_PCIe_DEVICE_ID),
    .driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt6639},
{ PCI_DEVICE(MTK_PCI_VENDOR_ID, NIC7927_PCIe_DEVICE_ID),
    .driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt6639},
```

**关键发现**：MT6639 的 PCIe Device ID 为 `0x3107`，而我们的设备枚举为 `0x6639`。这说明：
- `0x6639` 是 USB 模式的 Device ID
- `0x3107` 是 PCIe 模式的 Device ID
- 我们的设备枚举为 `0x6639` 而非 `0x3107`，这本身就是一个异常

### 2.3 Chip ID 问题

在 `mt6639/include/chips/mt6639.h` 中存在条件编译：

```c
#if defined(_HIF_USB)
#define MT6639_CHIP_ID   0x6639    // USB 模式
#else
#define MT6639_CHIP_ID   0x7961    // PCIe 模式 - 注意不是 0x7927！
#endif
```

PCIe 模式下 Chip ID 为 `0x7961`，这表示芯片内部的硬件版本寄存器返回的值是 `0x7961`，与 PCI Device ID `0x7927` 不同。这可能是作者标注"无法使用"的原因之一。

### 2.4 关键代码资产

#### WFDMA 寄存器定义（CODA 自动生成）

`mt6639/include/chips/coda/mt6639/wf_wfdma_host_dma0.h` 是 MediaTek CODA 工具自动生成的寄存器定义头文件：

```
基地址: WF_WFDMA_HOST_DMA0_BASE = 0x18024000 + CONN_INFRA_REMAPPING_OFFSET
```

关键寄存器（偏移量与我们当前驱动中使用的 `0xd4000` 基地址对比）：

| CODA 寄存器 | 偏移 | 对应功能 | 当前驱动中 |
|---|---|---|---|
| `WPDMA_GLO_CFG` | +0x208 | DMA 全局配置 | `MT_WFDMA0(0x0208)` ✅ 匹配 |
| `HOST_INT_STA` | +0x200 | 主机中断状态 | `MT_WFDMA0(0x0200)` ✅ 匹配 |
| `HOST_INT_ENA` | +0x204 | 主机中断使能 | `MT_WFDMA0(0x0204)` ✅ 匹配 |
| `WPDMA_RST_DTX_PTR` | +0x20C | 重置 TX DMA 指针 | `MT_WFDMA0(0x020c)` ✅ 匹配 |
| `HOST2MCU_SW_INT_SET` | +0x108 | 主机到 MCU 软中断设置 | **未使用** ❌ 可能缺失 |
| `MCU2HOST_SW_INT_STA` | +0x1F0 | MCU 到主机软中断状态 | `MT_WFDMA0(0x01f0)` ✅ 匹配 |
| `CONN_HIF_RST` | +0x100 | HIF 复位 | **未使用** ❌ |

#### PCIe 地址映射表

`chips/mt6639/mt6639.c` 包含完整的 bus2chip 映射表：

```c
{0x7c020000, 0xd0000, 0x10000},  // CONN_INFRA, wfdma → BAR0+0xd0000
{0x7c060000, 0xe0000, 0x10000},  // conn_host_csr_top → BAR0+0xe0000
{0x70020000, 0x1f0000, 0x10000}, // CBTOP → BAR0+0x1f0000
{0x70000000, 0x1e0000, 0x9000},  // 其他 CBTOP
```

**重要**：`0x7c024208`（WPDMA_GLO_CFG 的绝对地址）映射到 `BAR0 + 0xd4208`，这与我们当前驱动的 `MT_WFDMA0_BASE = 0xd4000` 一致。

#### MCU 初始化完整流程

`chips/mt6639/mt6639.c` 中的 `mt6639_mcu_init()` 是**最关键的参考**，定义了从冷启动到 MCU 就绪的完整序列：

```
1. set_cbinfra_remap()
   → 写 CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF = 0x74037001
   → 写 CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF_BT = 0x70007000

2. 检查 EFUSE memory repair 模式
   → 读 TOP_MISC_EFUSE_MBIST_LATCH_16_ADDR

3. mt6639_mcu_reinit() [如需恢复]
   → 强制唤醒 CONNINFRA
   → 切换 GPIO 模式
   → WF/BT 子系统复位：写 0x10351 (assert) → 延时 → 写 0x10340 (de-assert)

4. mt6639_mcu_reset()
   → CB_INFRA_RGU_WF_SUBSYS_RST: 置位 bit4 (assert) → 延时 1ms → 清除 bit4 (de-assert)
   → 检查信号量 CONN_SEMAPHORE

5. 设置 MCU 所有权
   → 写 CB_INFRA_SLP_CTRL_CB_INFRA_CRYPTO_TOP_MCU_OWN_SET = BIT(0)

6. 轮询 MCU 就绪
   → 读 WF_TOP_CFG_ON_ROMCODE_INDEX，等待值 == 0x1D1E (MCU_IDLE)
   → 超时: 1000 * 1ms = 1秒

7. 设置 MCIF 中断重映射
   → 写 CONN_BUS_CR_VON_CONN_INFRA_PCIE2AP_REMAP_WF_1_BA = 0x18051803
```

**这个流程中的步骤 1-5 是我们当前驱动完全缺失的，很可能是 DMA 无法工作的根本原因。**

#### WF 子系统复位寄存器

`coda/mt6639/cb_infra_rgu.h` 定义：
- `CB_INFRA_RGU_BASE = 0x70028000`
- `WF_SUBSYS_RST = BASE + 0x600` → `0x70028600`
- `BT_SUBSYS_RST = BASE + 0x610` → `0x70028610`
- 复位位: bit4 (`WF_SUBSYS_RST_MASK = 0x10`)

#### DMASHDL 配置

`mt6639/include/chips/hal_dmashdl_mt6639.h` 定义了完整的 DMA 调度器配置：
- 16 个组，每组有独立的配额和 refill 策略
- PCIe 模式下 Group 0-2 启用
- 32 个队列到组的映射
- 16 个优先级到组的映射

### 2.5 作者说"无法使用"的可能原因

根据代码分析，失败原因可能包括：

1. **PCI Device ID 不匹配**：代码期望 PCIe 模式为 `0x3107`，而我们的设备报告 `0x6639`（USB ID），说明这块 7927 芯片可能有特殊的 PCI 枚举行为
2. **Chip ID 报告为 `0x7961`** 而非 `0x7927`，固件可能不匹配
3. **固件文件不对**：项目包含 MT6639 版本的固件（`WIFI_RAM_CODE_MT6639_2_1.bin`），而非 MT7927 专用固件
4. **依赖 Android/CONNV3 框架**：大量代码依赖 `CFG_MTK_ANDROID_WMT`、`CFG_MTK_WIFI_CONNV3_SUPPORT` 等宏，这些在标准 Linux 上不可用
5. **Mobile vs CE segment 差异**：MT6639 原生是手机芯片，很多初始化路径通过 `IS_MOBILE_SEGMENT` 条件编译选择不同的 PCIe MAC 映射

---

## 3. chips/ 目录分析

### 3.1 目录结构

```
chips/
├── common/                    # 共用 CONNAC 代码
│   ├── cmm_asic_connac2x.c   # CONNAC2x 通用实现
│   ├── cmm_asic_connac3x.c   # CONNAC3x 通用实现 (2906行) ★
│   ├── fw_dl.c                # 固件下载通用实现 (2988行) ★
│   └── ...
├── mt6639/                    # MT6639 芯片实现 ★★★
│   ├── mt6639.c               # 主驱动 (3751行) - 含完整 mcu_init
│   ├── hal_dmashdl_mt6639.c   # DMA 调度器
│   ├── hal_wfsys_reset_mt6639.c # WF 子系统复位
│   └── dbg_mt6639.c           # 调试函数
├── mt7925/                    # MT7925 芯片实现 ★★
│   ├── mt7925.c               # 主驱动
│   ├── hal_dmashdl_mt7925.c
│   ├── hal_wfsys_reset_mt7925.c
│   └── dbg_mt7925.c
├── mt7961/                    # MT7961 (与 MT6639 PCIe Chip ID 相同)
├── connac/                    # CONNAC 基础实现
└── [其他 14 款芯片]
```

### 3.2 MT6639 vs MT7925 关键差异

| 特性 | MT6639 | MT7925 |
|---|---|---|
| CONNAC 版本 | CONNAC3x | CONNAC3x |
| Patch 地址 | `0x00900000` | `MT7925_PATCH_START_ADDR` |
| MCU 初始化 | `mt6639_mcu_init()` (含完整复位序列) | 通过 `hal_wfsys_reset` 间接初始化 |
| MCU 就绪标志 | `0x1D1E` (MCU_IDLE) | `0x1D1E` (MCU_IDLE) - 相同 |
| WF 子系统复位 | `CB_INFRA_RGU @ 0x70028600` | 使用 `WF_TOP_CFG_ON_ROMCODE_INDEX_REMAP_ADDR` |
| DMASHDL 配置 | MT6639 专用配额 | MT7925 专用配额 |
| bus2chip 映射 | 40+ 条目 | 类似但有差异 |
| cbinfra remap | `0x74037001` / `0x70007000` | 无此步骤 |

**关键发现**：MT7925 **没有** `set_cbinfra_remap()` 步骤和 `mt6639_mcu_reset()` 中的 WF 子系统复位。如果 MT7927 本质上是 MT6639，那么我们需要 MT6639 的初始化路径而非 MT7925 的。

### 3.3 CONNAC3x 通用层的关键函数

`chips/common/cmm_asic_connac3x.c` 中的电源管理：

```c
// Driver Own 获取（与我们的 mt7927_drv_own 对应）
asicConnac3xLowPowerOwnClear():
    写 CONNAC3X_BN0_LPCTL_ADDR → PCIE_LPCR_HOST_CLR_OWN (BIT(1))
    轮询: (LPCTL & PCIE_LPCR_AP_HOST_OWNER_STATE_SYNC) == 0

// Driver Own 释放
asicConnac3xLowPowerOwnSet():
    写 CONNAC3X_BN0_LPCTL_ADDR → PCIE_LPCR_HOST_SET_OWN (BIT(0))
```

这与我们现有的 `mt7927_drv_own()` 实现一致。

### 3.4 固件下载通用层

`chips/common/fw_dl.c` (2988行) 包含 CONNAC3x 的完整固件下载流程，包括：
- 固件解析和校验
- Region 级下载
- MCU 命令封装
- 事件等待和错误处理

---

## 4. 对当前项目的价值评估

### 4.1 直接价值 (可立即应用)

| 资源 | 价值 | 应用方式 |
|---|---|---|
| `mt6639_mcu_init()` 完整流程 | ★★★★★ | **当前驱动完全缺失 cbinfra remap 和 WF 子系统复位**。这极可能是 DMA 无法工作的根因 |
| `coda/mt6639/*.h` 寄存器头文件 | ★★★★★ | 提供所有 MT6639 寄存器的精确地址和位域定义 |
| `mt6639_bus2chip_cr_mapping[]` | ★★★★☆ | 验证我们的地址映射是否完整 |
| `cb_infra_rgu.h` 复位寄存器 | ★★★★☆ | WF 子系统复位的精确寄存器和位域 |
| `hal_dmashdl_mt6639.c` | ★★★☆☆ | DMASHDL 配置可能在 DMA 基础通路打通后需要 |

### 4.2 间接价值 (长期参考)

| 资源 | 价值 | 说明 |
|---|---|---|
| `cmm_asic_connac3x.c` | ★★★★☆ | CONNAC3x 通用 WFDMA 控制逻辑 |
| `fw_dl.c` | ★★★★☆ | 完整的固件下载流程参考 |
| `mt7925/*.c` | ★★★☆☆ | 对比 MT7925 与 MT6639 的差异点 |
| MT6639 固件文件 | ★★☆☆☆ | MT7927 可能需要 MT6639 版本的固件而非 MT7925 |

### 4.3 核心发现：缺失的初始化步骤

**对比我们当前驱动与 MT6639 官方初始化，发现以下严重缺失：**

| 步骤 | MT6639 官方 | 我们的驱动 | 状态 |
|---|---|---|---|
| cbinfra PCIe 重映射 | `0x74037001` / `0x70007000` | 无 | ❌ **缺失** |
| EFUSE memory repair 检查 | 读 `TOP_MISC_EFUSE_MBIST_LATCH_16` | 无 | ❌ **缺失** |
| WF 子系统复位 assert | `CB_INFRA_RGU + 0x600` bit4=1 | 无 | ❌ **缺失** |
| WF 子系统复位 de-assert | `CB_INFRA_RGU + 0x600` bit4=0 | 无 | ❌ **缺失** |
| MCU 所有权设置 | `CB_INFRA_SLP_CTRL` BIT(0) | 无 | ❌ **缺失** |
| 等待 MCU_IDLE (0x1D1E) | 轮询 `WF_TOP_CFG_ON_ROMCODE_INDEX` | 无 | ❌ **缺失** |
| MCIF 中断重映射 | `PCIE2AP_REMAP_WF_1_BA = 0x18051803` | 无 | ❌ **缺失** |
| Driver Own 握手 | `LPCTL` BIT(1) 清除 | ✅ 已有 | ✅ |
| DMA ring 分配 | WFDMA ring setup | ✅ 已有 | ✅ |
| WPDMA_GLO_CFG 使能 | TX/RX DMA EN | ✅ 已有 | ✅ |

**结论：我们的驱动在 `mt7927_drv_own()` 之前缺少了至少 6 个关键初始化步骤。这些步骤负责将芯片从冷启动/锁定状态转换到可接受 DMA 操作的状态。**

---

## 5. 关键寄存器地址清单（从 coda 头文件提取）

### 5.1 需要新增到驱动中的寄存器

```c
/* cbinfra PCIe 重映射 (cb_infra_misc0.h) */
#define CB_INFRA_MISC0_BASE                     0x70001000  /* 需确认映射 */
#define CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF      (CB_INFRA_MISC0_BASE + 偏移)
#define CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF_BT   (CB_INFRA_MISC0_BASE + 偏移)

/* WF 子系统复位 (cb_infra_rgu.h) */
#define CB_INFRA_RGU_BASE                       0x70028000
#define CB_INFRA_RGU_WF_SUBSYS_RST              (CB_INFRA_RGU_BASE + 0x600)  /* = 0x70028600 */
#define CB_INFRA_RGU_WF_SUBSYS_RST_BIT          BIT(4)

/* MCU 所有权 (cb_infra_slp_ctrl.h) */
#define CB_INFRA_SLP_CTRL_MCU_OWN_SET           /* 需从 coda 头提取精确地址 */

/* MCU 状态 (wf_top_cfg_on.h) */
#define WF_TOP_CFG_ON_ROMCODE_INDEX             /* 需从 coda 头提取精确地址 */
#define MCU_IDLE_VALUE                          0x1D1E

/* Host-MCU 软中断 */
#define HOST2MCU_SW_INT_SET                     (WFDMA_BASE + 0x108)
```

### 5.2 地址空间映射关系

```
芯片绝对地址         BAR0 偏移        说明
0x7c020000+      → 0xd0000+       CONN_INFRA (WFDMA)
0x7c060000+      → 0xe0000+       CONN_HOST_CSR_TOP (LPCTL 等)
0x70020000+      → 0x1f0000+      CBTOP
0x70000000+      → 0x1e0000+      CBTOP (含 CB_INFRA_RGU?)
0x81020000+      → 0xc0000+       WF_TOP_MISC_ON (含 ROMCODE_INDEX?)
```

**注意**：`CB_INFRA_RGU_BASE = 0x70028000` 需要通过 bus2chip 映射转换。从映射表看：
- `{0x70020000, 0x1f0000, 0x10000}` → `0x70028000` 映射到 `BAR0 + 0x1f8000`
- 但这行注释为 "Reserved for CBTOP, can't switch" ⚠️

这说明 `CB_INFRA_RGU` 区域可能无法通过标准 BAR0 映射直接访问，需要通过 `set_cbinfra_remap()` 先配置重映射窗口。**这就是为什么 `set_cbinfra_remap()` 必须是第一步。**

---

## 6. 建议的下一步行动

### 6.1 立即行动（高优先级）

1. **从 `coda/mt6639/` 提取所有缺失寄存器的精确地址**
   - `cb_infra_misc0.h` → PCIe remap 寄存器
   - `cb_infra_slp_ctrl.h` → MCU 所有权寄存器
   - `wf_top_cfg_on.h` → MCU 状态寄存器 (ROMCODE_INDEX)

2. **在 `mt7927_init_dma.c` 中实现完整的 MCU 初始化序列**
   - 在 `mt7927_drv_own()` 之前添加：
     - `set_cbinfra_remap()`
     - `mt6639_mcu_reset()` (WF 子系统复位)
     - MCU 所有权设置
     - 轮询 MCU_IDLE (0x1D1E)

3. **验证地址映射**
   - 确认 BAR0 长度是否足以覆盖 `0x1f0000+` 区域
   - 如果不够，可能需要额外的地址窗口配置

### 6.2 后续行动（DMA 通路打通后）

4. **尝试使用 MT6639 固件替代 MT7925 固件**
   - `mt6639/firmware/WIFI_RAM_CODE_MT6639_2_1.bin`
   - `mt6639/firmware/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin`

5. **集成 DMASHDL 配置**

6. **验证 Patch 地址**
   - MT6639 使用 `0x00900000`，我们当前使用 MT7925 的 `0x200000`

---

## 7. PCI Device ID 疑点总结

| 设备 | 标称 PCI ID | 实际枚举 | 官方 PCIe ID | USB ID |
|---|---|---|---|---|
| MT7927 (我们的硬件) | `0x7927` | `0x6639` | `0x7927` | - |
| MT6639 (手机芯片) | - | - | `0x3107` | `0x6639` |

我们的设备枚举为 `0x6639`（MT6639 的 USB Device ID），而非 `0x3107`（MT6639 的 PCIe ID）或 `0x7927`（MT7927 的 PCIe ID）。这个异常值得进一步研究：
- 可能是 MT7927 使用了不同的 PCI 配置空间映射
- 或者芯片固件未正确初始化 PCI ID 寄存器
- 或者这是 MT6639 的一个特殊 PCIe 变体

---

*文档生成日期：2026-02-13*
*结论：mt6639/ 和 chips/ 对本项目有极高参考价值，尤其是 MT6639 的 MCU 初始化流程可能直接解决当前 DMA 阻塞问题。*
