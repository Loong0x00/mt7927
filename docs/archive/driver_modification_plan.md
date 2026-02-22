# MT7927 驱动修改方案

## 目标文件

`tests/04_risky_ops/mt7927_init_dma.c`

---

## 1. 当前代码的 probe 流程 vs MT6639 官方流程

### 1.1 当前 `mt7927_probe()` 流程（第 1178-1262 行）

```
pci_enable_device
pci_set_master
dma_set_mask_and_coherent(32bit)
pci_iomap(BAR0)
chip status 读取（BAR0[0x0]）
                                    ← 缺失区域开始
mt7927_drv_own()                    ← 过早执行，前置条件未满足
                                    ← 缺失区域结束
mt7927_dma_init()
mt7927_dma_path_probe()
mt7927_mcu_fw_download()
```

### 1.2 MT6639 官方流程（`chips/mt6639/mt6639.c` 第 3155-3234 行）

```
[PCIe probe 基础设置]
set_cbinfra_remap()                 ← 缺失 ❶
EFUSE memory repair 检查            ← 缺失 ❷
mt6639_mcu_reinit() [条件]          ← 缺失 ❸
mt6639_mcu_reset()                  ← 缺失 ❹
MCU 所有权设置                       ← 缺失 ❺
轮询 MCU_IDLE (0x1D1E)             ← 缺失 ❻
MCIF 中断重映射                     ← 缺失 ❼
drv_own (LPCTL)                     ← 已有，但位置错误
DMA 初始化                          ← 已有
固件下载                            ← 已有
```

---

## 2. 缺陷逐项分析

### 缺陷 ❶：缺少 cbinfra PCIe 重映射

**参考代码**：`chips/mt6639/mt6639.c:3143-3153`

```c
static void set_cbinfra_remap(struct ADAPTER *ad)
{
    HAL_MCR_WR(ad, CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF_ADDR, 0x74037001);
    HAL_MCR_WR(ad, CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF_BT_ADDR, 0x70007000);
}
```

**影响**：这是整个初始化链的第一步。没有这个重映射，后续所有对 `0x700xxxxx` 地址区域的访问（包括 CB_INFRA_RGU、CB_INFRA_SLP_CTRL 等）都无法工作。

**寄存器地址推导**：

`CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF_ADDR` 和 `CB_INFRA_MISC0_CBTOP_PCIE_REMAP_WF_BT_ADDR` 的头文件（`cb_infra_misc0.h`）不在我们的仓库中。需要：
- 从 Motorola 完整仓库获取该头文件
- 或通过 Ghidra 逆向 Windows 驱动中的对应写操作确定地址
- 或通过 bus2chip 映射表推算：`CB_INFRA_MISC0` 应在 `0x70000000-0x70020000` 范围内，映射到 BAR0 `0x1e0000+` 或 `0x1f0000+`

**修改位置**：在 `mt7927_probe()` 中 `mt7927_drv_own()` 调用之前新增函数。

---

### 缺陷 ❷：缺少 EFUSE memory repair 检查

**参考代码**：`chips/mt6639/mt6639.c:3174-3181`

```c
HAL_MCR_RD(ad, TOP_MISC_EFUSE_MBIST_LATCH_16_ADDR, &u4Value);
if ((u4Value & MT6639_MEMOEY_REPAIR_CHECK_MASK) !=
    MT6639_MEMOEY_REPAIR_CHECK_MASK) {
    DBGLOG(INIT, ERROR, "Unexpected memory repair pattern\n");
    rStatus = WLAN_STATUS_FAILURE;
    goto exit;
}
```

**影响**：此检查验证芯片 EFUSE 中的 memory repair 模式是否正常。如果异常说明芯片硬件有问题。在开发阶段可先作为诊断日志而非致命错误。

**寄存器地址推导**：

`TOP_MISC_EFUSE_MBIST_LATCH_16_ADDR` 来自 `coda/mt6639/top_misc.h`（不在仓库中）。`TOP_MISC` 基地址可能在 `0x80020000`（WF_TOP_MISC_OFF，映射到 BAR0+0xb0000）区域。

**修改位置**：在 cbinfra remap 之后、MCU reset 之前新增。先作为只读诊断。

---

### 缺陷 ❸：缺少 MCU reinit（条件性恢复）

**参考代码**：`chips/mt6639/mt6639.c:3019-3099`

```c
// 简化版流程：
1. 强制唤醒 CONNINFRA:
   写 CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_ADDR = 0x1
   → 芯片地址: 0x180601A0 → CODA 虚拟地址: 0x7c0601A0 → BAR0+0xe01A0

2. 轮询 CONNINFRA 就绪:
   读 CONN_CFG_IP_VERSION_IP_VERSION_ADDR 直到非零

3. GPIO 模式切换:
   写 CBTOP_GPIO_MODE5/6 寄存器

4. WF+BT 子系统硬复位:
   写 CB_INFRA_RGU_BT_SUBSYS_RST = 0x10351 (assert)
   写 CB_INFRA_RGU_WF_SUBSYS_RST = 0x10351 (assert)
   延时 10ms
   写 CB_INFRA_RGU_BT_SUBSYS_RST = 0x10340 (de-assert)
   写 CB_INFRA_RGU_WF_SUBSYS_RST = 0x10340 (de-assert)
   延时 50ms

5. 清除 CONNINFRA 强制唤醒:
   写 CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_ADDR = 0x0
```

**影响**：此流程在检测到芯片处于异常状态时执行恢复。首次冷启动时可能不需要，但热重载驱动时需要。

**修改位置**：作为可选功能，可用 module_param 控制。

**地址推导（已知）**：
- `CONN_HOST_CSR_TOP_BASE = 0x18060000 + 0x64000000 = 0x7c060000` → 映射 BAR0+0xe0000
- `CONN_INFRA_WAKEPU_TOP = CSR_TOP_BASE + 0x1A0` → `0x7c0601A0` → BAR0+0xe01A0

---

### 缺陷 ❹：缺少 WF 子系统复位（最关键）

**参考代码**：`chips/mt6639/mt6639.c:3102-3140`

```c
// assert 复位
HAL_MCR_RD(ad, CB_INFRA_RGU_WF_SUBSYS_RST_ADDR, &u4Value);
u4Value |= (0x1 << 4);  // bit4 = WF_SUBSYS_RST
HAL_MCR_WR(ad, CB_INFRA_RGU_WF_SUBSYS_RST_ADDR, u4Value);
kalMdelay(1);

// de-assert 复位
HAL_MCR_RD(ad, CB_INFRA_RGU_WF_SUBSYS_RST_ADDR, &u4Value);
u4Value &= ~(0x1 << 4);
HAL_MCR_WR(ad, CB_INFRA_RGU_WF_SUBSYS_RST_ADDR, u4Value);

// 验证信号量
HAL_MCR_RD(ad, CONN_SEMAPHORE_CONN_SEMA_OWN_BY_M0_STA_REP_1_ADDR, &u4Value);
if ((u4Value & BIT(0)) != 0)
    error("L0.5 reset failed");
```

**影响**：**这很可能是 DMA 不工作的根本原因**。不对 WF 子系统做复位，MCU ROM code 不会启动，DMA 引擎不会被激活。

**寄存器地址（已知）**：
- `CB_INFRA_RGU_BASE = 0x70028000`（从 `coda/mt6639/cb_infra_rgu.h`）
- `WF_SUBSYS_RST = BASE + 0x600` = `0x70028600`
- 通过 bus2chip 映射：`{0x70020000, 0x1f0000, 0x10000}` → `0x70028600` = BAR0 + `0x1f8600`

**修改位置**：在 cbinfra remap 之后、MCU 所有权设置之前。

**⚠️ 注意**：bus2chip 映射表标注 `0x70020000` 区域为 "Reserved for CBTOP, can't switch"。这意味着此区域需要 cbinfra remap（❶）先就位才能访问。如果 remap 未设置，直接写 BAR0+0x1f8600 可能无效。

---

### 缺陷 ❺：缺少 MCU 所有权设置

**参考代码**：`chips/mt6639/mt6639.c:3193-3195`

```c
HAL_MCR_WR(ad,
    CB_INFRA_SLP_CTRL_CB_INFRA_CRYPTO_TOP_MCU_OWN_SET_ADDR,
    BIT(0));
```

**影响**：将 MCU 执行权交给固件 ROM code。不设置此位，MCU 不会开始执行，自然不会进入 MCU_IDLE 状态。

**寄存器地址**：来自 `coda/mt6639/cb_infra_slp_ctrl.h`（不在仓库中）。`CB_INFRA_SLP_CTRL` 基地址未知，需从完整 CODA 头文件或 Ghidra 逆向获取。

**修改位置**：WF 子系统复位 de-assert 之后。

---

### 缺陷 ❻：缺少 MCU_IDLE 轮询

**参考代码**：`chips/mt6639/mt6639.c:3197-3213`

```c
#define MCU_IDLE 0x1D1E
while (TRUE) {
    if (u4PollingCnt >= 1000) { /* 超时 */ }
    HAL_MCR_RD(ad, WF_TOP_CFG_ON_ROMCODE_INDEX_ADDR, &u4Value);
    if (u4Value == MCU_IDLE)
        break;
    u4PollingCnt++;
    kalUdelay(1000);  // 1ms
}
```

**影响**：MCU ROM code 启动后需要时间完成自检和初始化。必须等待 `ROMCODE_INDEX` 变为 `0x1D1E` 才表示 MCU 就绪，此后 DMA 引擎才可用。

**寄存器地址（已知）**：
- `WF_TOP_CFG_ON_BASE = 0x81021000`（从 `coda/mt6639/wf_top_cfg_on.h`）
- `ROMCODE_INDEX = BASE + 0x604` = `0x81021604`
- 通过 bus2chip 映射：`{0x81020000, 0xc0000, 0x10000}` → `0x81021604` = BAR0 + `0xc1604`

**修改位置**：MCU 所有权设置之后、drv_own 之前。

---

### 缺陷 ❼：缺少 MCIF 中断重映射

**参考代码**：`chips/mt6639/mt6639.c:3224-3226`

```c
HAL_MCR_WR(ad,
    CONN_BUS_CR_VON_CONN_INFRA_PCIE2AP_REMAP_WF_1_BA_ADDR,
    0x18051803);
```

**影响**：设置 MCIF 中断的 PCIe-to-AP 重映射，使 MCU 产生的中断能正确路由到主机。没有此步骤，即使 DMA 工作，主机也收不到中断通知。

**寄存器地址**：来自 `coda/mt6639/conn_bus_cr_von.h`（不在仓库中），需获取。

**修改位置**：MCU_IDLE 确认之后。

---

### 缺陷 ❽：drv_own 位置错误

**当前代码**：`mt7927_probe()` 第 1225 行

```c
ret = mt7927_drv_own(dev);
```

**问题**：在 MT6639 官方流程中，LPCTL driver-own 握手发生在 MCU 初始化完成之后（MCU_IDLE 确认后），而非之前。当前代码在 MCU 完全未初始化时就试图获取 driver-own，这可能导致 LPCTL 握手失败或无效。

**修改**：将 `mt7927_drv_own()` 移到 MCU 初始化完成（轮询到 0x1D1E）之后。

---

### 缺陷 ❾：Patch 地址可能错误

**当前代码**：第 157 行

```c
#define MCU_PATCH_ADDRESS  0x200000
```

**MT6639 官方**：`mt6639/include/chips/mt6639.h` 第 41 行

```c
#define MT6639_PATCH_START_ADDR  0x00900000
```

**影响**：`0x200000` 是 MT7925 的 patch 地址。如果 MT7927 本质上是 MT6639，正确的 patch 地址应为 `0x00900000`。用错误地址下载 patch 即使 DMA 工作了也会导致固件加载失败。

**修改**：将 `MCU_PATCH_ADDRESS` 改为 `0x00900000`，或作为 module_param 可配置。

---

### 缺陷 ❿：缺少 HOST2MCU 软中断触发

**参考代码**：`coda/mt6639/wf_wfdma_host_dma0.h` 第 43 行

```c
#define WF_WFDMA_HOST_DMA0_HOST2MCU_SW_INT_SET_ADDR  (BASE + 0x108)
```

**当前代码**：完全没有使用 `HOST2MCU_SW_INT_SET` 寄存器。

**影响**：在 MT6639/CONNAC3x 架构中，主机可能需要通过写 `HOST2MCU_SW_INT_SET` 来通知 MCU 有新的 DMA 描述符需要处理，而非仅靠写 CIDX/DIDX doorbell。

**修改位置**：在 `mt7927_kick_ring_buf()` 中 kick 操作之后增加 HOST2MCU 软中断写入。

**寄存器 BAR0 偏移**：`MT_WFDMA0_BASE + 0x0108` = `0xd4108`

---

## 3. 修改后的 probe 流程（建议）

```
pci_enable_device
pci_set_master
dma_set_mask_and_coherent(32bit)
pci_iomap(BAR0)
chip status 读取

// === 新增：MT6639 MCU 初始化 ===
mt7927_set_cbinfra_remap()          ← 新增 ❶
mt7927_check_efuse()                ← 新增 ❷ (诊断，非致命)
mt7927_mcu_reinit() [可选]          ← 新增 ❸ (module_param 控制)
mt7927_mcu_reset()                  ← 新增 ❹
mt7927_mcu_own_set()                ← 新增 ❺
mt7927_poll_mcu_idle()              ← 新增 ❻ (轮询 ROMCODE_INDEX == 0x1D1E)
mt7927_mcif_remap()                 ← 新增 ❼
// === 新增结束 ===

mt7927_drv_own()                    ← 位置调整 ❽
mt7927_dma_init()
mt7927_dma_path_probe()
mt7927_mcu_fw_download()            ← patch 地址修正 ❾
```

---

## 4. 需要新增的宏定义

```c
/* cbinfra PCIe remap - 需从完整 CODA 头或逆向获取精确偏移 */
/* 暂时标记为 TODO，通过 bus2chip 映射推算 */

/* WF 子系统复位 (已知) */
#define MT_CB_INFRA_RGU_BASE             0x70028000
#define MT_CB_INFRA_RGU_WF_SUBSYS_RST   (MT_CB_INFRA_RGU_BASE + 0x600)
/* bus2chip: {0x70020000, 0x1f0000, 0x10000} → BAR0 偏移 = 0x1f0000 + (0x70028600 - 0x70020000) = 0x1f8600 */
#define MT_WF_SUBSYS_RST_BAR0            0x1f8600
#define MT_WF_SUBSYS_RST_BIT             BIT(4)

/* MCU 状态寄存器 (已知) */
#define MT_WF_TOP_CFG_ON_BASE            0x81021000
#define MT_WF_TOP_CFG_ON_ROMCODE_INDEX   (MT_WF_TOP_CFG_ON_BASE + 0x604)
/* bus2chip: {0x81020000, 0xc0000, 0x10000} → BAR0 偏移 = 0xc0000 + (0x81021604 - 0x81020000) = 0xc1604 */
#define MT_ROMCODE_INDEX_BAR0            0xc1604
#define MT_MCU_IDLE                      0x1D1E

/* CONNINFRA 唤醒 (已知) */
#define MT_CONN_HOST_CSR_TOP_BASE        0x7c060000
/* bus2chip: {0x7c060000, 0xe0000, 0x10000} */
#define MT_WAKEPU_TOP_BAR0               0xe01A0   /* CSR_TOP + 0x1A0 */

/* HOST2MCU 软中断 */
#define MT_HOST2MCU_SW_INT_SET           MT_WFDMA0(0x0108)

/* MT6639 Patch 地址 */
#define MCU_PATCH_ADDRESS_MT6639         0x00900000
```

---

## 5. 缺失头文件清单

以下 CODA 头文件在仓库中缺失，包含关键寄存器定义：

| 头文件 | 定义的关键寄存器 | 获取方式 |
|---|---|---|
| `cb_infra_misc0.h` | `CBTOP_PCIE_REMAP_WF`, `CBTOP_PCIE_REMAP_WF_BT` | Motorola 完整仓库 / Ghidra |
| `cb_infra_slp_ctrl.h` | `CB_INFRA_CRYPTO_TOP_MCU_OWN_SET` | Motorola 完整仓库 / Ghidra |
| `top_misc.h` | `TOP_MISC_EFUSE_MBIST_LATCH_16` | Motorola 完整仓库 |
| `conn_bus_cr_von.h` | `PCIE2AP_REMAP_WF_1_BA` (MCIF remap) | Motorola 完整仓库 |
| `conn_semaphore.h` | `CONN_SEMA_OWN_BY_M0_STA_REP_1` | Motorola 完整仓库 |
| `conn_bus_cr.h` | 通用总线控制寄存器 | Motorola 完整仓库 |
| `cbtop_gpio_sw_def.h` | GPIO 模式寄存器（reinit 需要） | 可延后 |

**建议**：优先获取 `cb_infra_misc0.h` 和 `cb_infra_slp_ctrl.h`，这两个包含缺陷 ❶ 和 ❺ 的精确地址。

---

## 6. 风险评估

| 修改项 | 风险 | 说明 |
|---|---|---|
| ❶ cbinfra remap | 低 | 只是写两个配置寄存器，回滚简单 |
| ❷ EFUSE 检查 | 无 | 纯只读诊断 |
| ❸ MCU reinit | 中 | 涉及 WF+BT 子系统硬复位，可能影响蓝牙（如果共用芯片） |
| ❹ MCU reset | 中 | WF 子系统复位，需确保 de-assert 正确执行 |
| ❺ MCU own set | 低 | 单个 bit 写入 |
| ❻ MCU_IDLE 轮询 | 无 | 纯只读轮询 |
| ❼ MCIF remap | 低 | 单寄存器写入 |
| ❽ drv_own 移位 | 低 | 调整调用顺序 |
| ❾ Patch 地址 | 低 | 值修改，可用 module_param 切换 |
| ❿ HOST2MCU 中断 | 低 | 额外写入一个寄存器 |

---

## 7. 建议的实施顺序

**阶段一（最小验证，2步）**：
1. 添加 ROMCODE_INDEX 只读诊断：在 probe 中读 BAR0+0xc1604，打印值。如果读到非 0xFFFFFFFF 且非 0x00000000 的值，说明地址映射正确
2. 添加 WF 子系统复位（❹）+ MCU_IDLE 轮询（❻）：写 BAR0+0x1f8600 做复位，然后轮询 BAR0+0xc1604

**阶段二（补全前置条件，需获取缺失头文件）**：
3. 添加 cbinfra remap（❶）—— 需要 `cb_infra_misc0.h` 中的精确偏移
4. 添加 MCU 所有权设置（❺）—— 需要 `cb_infra_slp_ctrl.h`

**阶段三（完整流程）**：
5. 调整 drv_own 位置（❽）
6. 修正 patch 地址（❾）
7. 添加 HOST2MCU 软中断（❿）
8. DMA path probe 验证

---

*文档生成日期：2026-02-13*
*参考来源：`chips/mt6639/mt6639.c`、`mt6639/include/chips/coda/mt6639/` 头文件*
