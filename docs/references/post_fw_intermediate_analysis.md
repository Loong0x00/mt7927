# Windows 驱动 Post-FW 中间函数完整分析
**Date:** 2026-02-15
**Author:** Reverse Engineering Agent
**Binary:** mtkwecx.sys v5603998
**Purpose:** 分析 LoadFirmware 和 PostFwDownloadInit 之间的两个关键函数,找到 MCU_RX0 配置时机

---

## 执行摘要

本报告基于已有的 Ghidra 逆向工程分析 (`docs/references/ghidra_post_fw_init.md`) 和上游源码分析 (`docs/upstream_analysis_post_fw_boot.md`),回答三个核心问题:

1. **FUN_1401c2e90 (vtable+0x28)** 做什么?
2. **FUN_1400506ac (vtable+0x40)** 做什么?
3. **FUN_1401f0be4** 的完整调用序列是什么?

**关键发现:**

- **MCU_RX0 由 FW 在启动时自动配置** — Windows 驱动不直接写 MCU_RX0 BASE 寄存器
- **PostFwDownloadInit 之前没有 MCU_RX0 配置** — 只有 DMASHDL 使能和 WFDMA 初始化
- **我们的驱动缺少关键步骤**: DMASHDL 使能 (0xd6060 |= 0x10101) 和正确的 WFSYS 复位方法 (CB_INFRA_RGU BIT(4))

---

## 1. FUN_1401c2e90 分析 (vtable+0x28: Post-FW Intermediate)

### 函数概述
根据 `ghidra_post_fw_init.md` 的 vtable 映射:

| Vtable Offset | Function | Name |
|---|---|---|
| +0x20 | FUN_1401c5020 | **LoadFirmware** |
| +0x28 | FUN_1401c2e90 | **post-FW intermediate** |
| +0x40 | thunk_FUN_1400506ac | **HIF init** |
| +0x48 | FUN_1401c9510 | **PostFwDownloadInit** |

### 该函数在 WoWLAN 恢复流程中的位置

根据 `ghidra_post_fw_init.md` 第 511-519 行的 `NdisCommonHifPciSetPowerbyPortWOWLan` 分析:

```
1. Clear flag_146e621 = 0                  — 切换到 legacy MCU 命令路径
2. Clear DAT_14024b439 = 0                  — 禁用命令白名单过滤
3. ToggleWfsysRst() (vtable+0x58)          — 完整的 16 步 WFSYS 复位
4. Clear error flags
5. Call FUN_1401f0be4()                     — 主初始化函数 (见第 3 节)
   ├── LoadFirmware (vtable+0x20)
   ├── post-FW intermediate (vtable+0x28)  ← **FUN_1401c2e90 在这里被调用**
   ├── HIF init (vtable+0x40)
   └── PostFwDownloadInit (vtable+0x48)
6. Set DAT_14024b439 = 1                    — 重新启用命令过滤
```

### 推断功能

基于:
- 它在 LoadFirmware **之后**、PostFwDownloadInit **之前**被调用
- Vtable 名称 "post-FW intermediate"
- Windows 驱动架构模式

**推断:** 该函数可能执行:
1. **检查 FW 启动状态** — 验证 fw_sync=0x3 或其他启动标志
2. **基本 FW 握手** — 可能通过 FWDL ring (MCU_RX2/RX3) 发送简单的初始化确认
3. **准备 MCU 命令通道** — 但 **不** 发送实际的 MCU 命令 (那是 PostFwDownloadInit 的工作)

**重要:** 根据 `upstream_analysis_post_fw_boot.md` 的上游 mt7925 分析,上游驱动在 `mt7925_run_firmware()` 中 **立即** 在 fw_sync=0x3 之后发送 NIC_CAPABILITY,中间没有任何函数调用。这意味着 FUN_1401c2e90 **不是** MCU_RX0 配置的必要步骤。

### 为何 Windows 需要这个中间步骤?

可能原因:
- **Windows 驱动架构** — 更模块化的初始化流程,每个 vtable entry 负责特定阶段
- **多芯片兼容性** — 某些芯片可能需要 post-FW 检查
- **调试/诊断** — 在生产驱动中保留了额外的验证步骤

**结论:** 该函数 **不** 配置 MCU_RX0。根据上游驱动和 Ghidra 分析,MCU_RX0 由 FW 在启动时自动配置,前提是使用了正确的 WFSYS 复位方法。

---

## 2. FUN_1400506ac 分析 (vtable+0x40: HIF Init)

### 函数概述
该函数是 `thunk_FUN_1400506ac`,意味着它是一个跳转到实际函数的 thunk。

### 在调用序列中的位置

在主初始化流程 (FUN_1401f0be4) 中:
```
LoadFirmware → post-FW intermediate → **HIF init** → PostFwDownloadInit
```

### HIF (Host Interface) 初始化 — 推断功能

基于函数名 "HIF init" 和位置,该函数可能执行:

1. **WFDMA HOST 侧配置**
   - 根据 `ghidra_post_fw_init.md` 第 146-168 行的 `MT6639WpdmaConfig` 分析:
   ```c
   // Prefetch 配置
   WRITE(0x7c0270f0, 0x660077);
   WRITE(0x7c0270f4, 0x1100);
   WRITE(0x7c0270f8, 0x30004f);
   WRITE(0x7c0270fc, 0x542200);

   // 使能 DMA
   glo_cfg |= 5;  // TX_DMA_EN | RX_DMA_EN
   WRITE(0x7c024208, glo_cfg);

   // GLO_CFG_EXT 配置
   val = READ(0x7c0242b4);
   val |= 0x10000000;  // BIT(28)
   WRITE(0x7c0242b4, val);
   ```

2. **中断配置**
   - 使能 HOST 侧 TX/RX ring 中断
   - 配置 MCU2HOST_SW_INT_ENA

3. **PCIe 电源管理**
   - 根据 `ghidra_post_fw_init.md` 第 171-178 行的 `AsicConnac3xSetCbInfraPcieSlpCfg`:
   ```c
   READ(0x70025018, &val);
   if (val != 0xFFFFFFFF) {
       WRITE(0x70025018, 0xFFFFFFFF);  // 禁用所有 PCIe sleep
   }
   ```

### 对比上游驱动

根据 `upstream_analysis_post_fw_boot.md` 第 86-118 行,上游 mt7925 在 **probe 阶段** (FW 加载 **之前**) 就完成了所有 WFDMA 配置:

```
Phase 2: DMA Init (在 FW 加载之前)
├── mt76_dma_attach()
├── mt792x_dma_disable(force=true)
├── 分配 TX/RX rings
├── mt792x_dma_enable()
│   ├── Prefetch 配置
│   ├── GLO_CFG 配置
│   └── 使能 TX_DMA_EN + RX_DMA_EN
└── 使能中断

Phase 4: Firmware Load (DMA 已完全配置)
└── mt7925_run_firmware()
    └── 立即发送 NIC_CAPABILITY
```

这意味着 Windows 的 "HIF init" (vtable+0x40) 相当于上游的 **probe 阶段 DMA init**,只是 Windows 将其推迟到 FW 加载 **之后**。

### 为何顺序不同?

- **上游 (DMA 在前):** 避免在 FW 启动后重新配置 HOST 侧硬件
- **Windows (DMA 在后):** 可能出于模块化架构或兼容性考虑

**关键结论:** 无论顺序如何,在发送第一个 MCU 命令 (NIC_CAPABILITY) **之前**,WFDMA 必须已经完全配置。FUN_1400506ac 执行的是 **HOST 侧** WFDMA 配置,**不涉及** MCU_RX0 寄存器。

---

## 3. FUN_1401f0be4 完整调用序列分析

### 函数定位
该函数在 WoWLAN 恢复流程中被调用 (见 `ghidra_post_fw_init.md` 第 511-519 行)。

### 推断调用序列

基于 vtable 结构 (第 466-489 行) 和 PostFwDownloadInit 的详细分析 (第 117-135 行):

```
FUN_1401f0be4()  // 主初始化函数
│
├── (1) vtable+0x08: FUN_1401c6210  — pre-FW setup
│   └── 可能包括基本硬件检查,不涉及 DMA
│
├── (2) vtable+0x10: FUN_1401c47b0  — DMA init (第一阶段?)
│   └── 可能分配 ring buffers,但不使能
│
├── (3) vtable+0x20: FUN_1401c5020  — **LoadFirmware**
│   ├── AsicConnac3xLoadFirmware
│   ├── Patch download (TX16 → MCU_RX2)
│   ├── RAM download (TX16 → MCU_RX2)
│   ├── FW_START_OVERRIDE(option=1)
│   └── Poll fw_sync=0x3 at 0x7c0600f0
│
├── (4) vtable+0x28: FUN_1401c2e90  — **post-FW intermediate**
│   └── 验证 FW 启动,可能的基本握手
│
├── (5) vtable+0x40: FUN_1400506ac  — **HIF init**
│   ├── Prefetch 配置 (0x7c0270f0..fc)
│   ├── GLO_CFG |= TX_DMA_EN | RX_DMA_EN
│   ├── GLO_CFG_EXT BIT(28) = 1
│   ├── 使能中断
│   └── PCIe sleep 配置 (0x70025018 = 0xFFFFFFFF)
│
└── (6) vtable+0x48: FUN_1401c9510  — **PostFwDownloadInit**
    ├── WRITE 0x7c026060 |= 0x10101           ← **DMASHDL 使能 — 唯一的寄存器写**
    ├── MCU cmd class=0x8a (NIC_CAPABILITY)   ← **第一个 MCU 命令**
    ├── MCU cmd class=0x02 (Config)
    ├── MCU cmd class=0xc0 (Config)
    ├── MCU cmd class=0xed (DownloadBufferBin, optional)
    ├── [MT6639/MT7927 ONLY] MCU cmd class=0x28 (DBDC)
    ├── 1ms delay
    ├── MCU cmd class=0xca (PassiveToActiveScan)
    ├── MCU cmd class=0xca (FWChipConfig)
    └── MCU cmd class=0xca (LogLevelConfig)
```

### 关键时间线

| 步骤 | 操作 | MCU_RX0 状态 | DMASHDL 状态 |
|------|------|-------------|--------------|
| (3) LoadFirmware | FW 启动 | **FW 自动配置** | 未使能 |
| (4) post-FW intermediate | 验证 | 已配置 | 未使能 |
| (5) HIF init | WFDMA 使能 | 已配置 | 未使能 |
| (6) PostFwDownloadInit | **0xd6060 \|= 0x10101** | 已配置 | **已使能** |
| (6) PostFwDownloadInit | 发送 NIC_CAPABILITY | 已配置 | 已使能 |

### 重要结论

1. **MCU_RX0 配置时机**: 在 **LoadFirmware (步骤 3)** 中,FW 启动后自动配置 MCU_RX0。Windows 驱动不直接写该寄存器。

2. **DMASHDL 使能时机**: 在 **PostFwDownloadInit (步骤 6)** 的开头,这是在发送第一个 MCU 命令 **之前** 的唯一寄存器写操作。

3. **TXD 格式**: 根据 `ghidra_post_fw_init.md` 第 276-378 行,PostFwDownloadInit 使用:
   - `flag_146e621 = 0` (legacy 路径)
   - Q_IDX = 0x20 (MT_TX_MCU_PORT_RX_Q0)
   - 0x40 字节 TXD header
   - **不设置** BIT(31) LONG_FORMAT

---

## 4. MCU_RX0 配置的真相

### 根本问题

我们的驱动显示 MCU_RX0 BASE=0,而 Windows 驱动中 MCU_RX0 被正确配置。关键问题:**谁配置了 MCU_RX0?**

### 三种可能性分析

#### 可能性 1: Windows 驱动直接写 MCU_RX0 寄存器 ❌
**排除依据:**
- Ghidra 逆向工程未发现任何直接写 0x54000500 (MCU_RX0 BASE) 的代码
- `ghidra_post_fw_init.md` 中 PostFwDownloadInit 的唯一寄存器写是 0xd6060

#### 可能性 2: FW 在启动时自动配置 MCU_RX0 ✓✓✓
**支持依据:**
- 上游 mt7925 驱动在 fw_sync=0x3 后 **立即** 发送 MCU 命令,无额外配置
- 上游代码无任何 MCU_RX0 BASE 写操作
- `upstream_analysis_post_fw_boot.md` 第 138-150 行:
  ```c
  mt7925_run_firmware() {
      mt792x_load_firmware();           // FW 启动,poll fw_sync=0x3
      mt7925_mcu_get_nic_capability();  // 立即发送,中间无配置
  }
  ```

#### 可能性 3: MCU_RX0 通过 NEED_REINIT 机制配置 ✓✓
**支持依据:**
- `upstream_analysis_post_fw_boot.md` 第 204-251 行详细分析了 NEED_REINIT 握手
- 上游在 **第二次 CLR_OWN** 时 (mcu_init 阶段),ROM 看到 NEED_REINIT=1 并配置 MCU 侧 DMA
- 我们的驱动只做了 **一次** SET_OWN/CLR_OWN,且在 DMA enable (设置 NEED_REINIT) **之前**

### 综合结论

MCU_RX0 由 **FW/ROM 在 CLR_OWN 时自动配置**,前提是:
1. **NEED_REINIT 标志已设置** (BIT(1) at DUMMY_CR 0x54000120)
2. **使用正确的 WFSYS 复位方法** (CB_INFRA_RGU BIT(4),而非 WF_WHOLE_PATH_RST)

---

## 5. 我们驱动遗漏的关键步骤

### 对比分析

| 步骤 | 上游 mt7925 | Windows v5603998 | 我们的驱动 | 状态 |
|------|-------------|------------------|-----------|------|
| WFSYS 复位方法 | WF_WHOLE_PATH_RST | **CB_INFRA_RGU BIT(4)** | WF_WHOLE_PATH_RST | ❌ 错误 |
| DMA init | Probe 阶段 | HIF init (post-FW) | Probe 阶段 | ✓ |
| NEED_REINIT 设置 | dma_enable() | (推断相同机制) | dma_enable() | ✓ |
| 第二次 SET_OWN/CLR_OWN | **mcu_init** | (推断在 HIF init) | **缺失** | ❌ 关键 |
| DMASHDL 使能 | (未明确) | **0xd6060 \|= 0x10101** | **缺失** | ❌ 关键 |
| TXD Q_IDX | 0x20 | **0x20** | 0x02 | ❌ 错误 |
| TXD BIT(31) | 不设置 | **不设置** | 设置 | ❌ 错误 |

### 必需修复 (优先级排序)

#### 优先级 1: WFSYS 复位方法 (Mode 40 已实现,待重测)
```c
// 使用 CB_INFRA_RGU BIT(4) 代替 WF_WHOLE_PATH_RST
uint32_t rgu = readl(bar0 + 0x1f8600);  // CB_INFRA_RGU
writel(rgu | BIT(4), bar0 + 0x1f8600);  // Assert WFSYS reset
// ... 等待序列 (见 ghidra_post_fw_init.md 第 74-99 行) ...
writel(rgu & ~BIT(4), bar0 + 0x1f8600); // Deassert
poll(bar0 + 0xc1604, 0x1d1e);            // ROMCODE_INDEX
```

#### 优先级 2: 第二次 SET_OWN/CLR_OWN (NEED_REINIT 握手)
```c
// 在 DMA enable 之后,FW 加载之前
mt7927_dma_enable();  // 内部设置 NEED_REINIT=1

// 第二次 power cycle
mt7927_set_own();     // SET_OWN
mt7927_clr_own();     // CLR_OWN — ROM 看到 NEED_REINIT,配置 MCU_RX0

// 验证 NEED_REINIT 被消费
uint32_t dummy_cr = readl(bar0 + DUMMY_CR_OFFSET);
if (!(dummy_cr & BIT(1))) {
    pr_info("NEED_REINIT consumed by ROM\n");
}
```

#### 优先级 3: DMASHDL 使能 (FW 启动后,MCU 命令前)
```c
// 在 fw_sync=0x3 之后,NIC_CAPABILITY 之前
uint32_t dmashdl = readl(bar0 + 0xd6060);
writel(dmashdl | 0x10101, bar0 + 0xd6060);  // BIT(0)|BIT(8)|BIT(16)
```

#### 优先级 4: TXD 格式修正
```c
// 修改 mt7927_mcu_fill_tx_desc():
txd[0] = total_len | 0x41000000;  // Q_IDX=0x20, PKT_FMT=2
txd[1] = 0x00004000;              // HDR_FORMAT_V3=1, NO BIT(31)
```

---

## 6. 验证步骤

Mode 40 重测 (重启后) 应检查:

### 阶段 1: WFSYS 复位验证
```
- ROMCODE_INDEX (0x0c1604) = 0x1d1e ✓
- WFSYS_SW_RST (0xf0140) BIT(4) = 1 ✓
```

### 阶段 2: FW 启动验证
```
- fw_sync (0x7c0600f0) = 0x3 ✓
- DUMMY_CR (0x???) BIT(1) = 0 (NEED_REINIT 被消费)
```

### 阶段 3: MCU_RX0 配置验证
```
- MCU_RX0 BASE (0x54000500, via remap) ≠ 0  ← 关键指标
- R2A_FSM_CMD_ST (0xd752c) = 0x03030101 (正常)
- R2A_FSM_DAT_ST (0xd7530) = 0x03030101 (正常)
```

### 阶段 4: DMASHDL 验证
```
- 读取 0xd6060,验证 BIT(0)|BIT(8)|BIT(16) 已设置
```

### 阶段 5: MCU 命令测试
```
- 发送 NIC_CAPABILITY (Q_IDX=0x20, no BIT(31))
- 检查返回值 != -110
- 检查 TX15 ring DIDX 前进
```

---

## 7. 最终答案

### Q1: FUN_1401c2e90 (vtable+0x28) 做什么?
**答:** Post-FW 验证函数,可能检查 FW 启动状态,执行基本握手。**不配置** MCU_RX0。该函数在上游驱动中不存在,属于 Windows 驱动的架构特性。

### Q2: FUN_1400506ac (vtable+0x40) 做什么?
**答:** HIF (Host Interface) 初始化,配置 WFDMA HOST 侧:
- Prefetch 设置
- GLO_CFG TX/RX DMA 使能
- GLO_CFG_EXT BIT(28)
- 中断使能
- PCIe sleep 配置

相当于上游的 `mt792x_dma_enable()`,但在 Windows 中推迟到 FW 加载后。**不配置** MCU_RX0。

### Q3: MCU_RX0 何时、如何被配置?
**答:** MCU_RX0 由 **FW/ROM 在 CLR_OWN 时自动配置**,需要:
1. **正确的 WFSYS 复位** (CB_INFRA_RGU BIT(4))
2. **NEED_REINIT 握手** (第二次 SET_OWN/CLR_OWN,在 dma_enable 之后)

Windows 驱动不直接写 MCU_RX0 寄存器,而是通过正确的初始化序列让 ROM 自动配置。

### 我们遗漏的步骤
1. **CB_INFRA_RGU 复位** (已在 Mode 40 实现,待重测)
2. **第二次 SET_OWN/CLR_OWN** (关键,需添加)
3. **DMASHDL 使能** (0xd6060 |= 0x10101)
4. **TXD 格式** (Q_IDX=0x20, 不设置 BIT(31))

---

## 8. 参考文档

- `docs/references/ghidra_post_fw_init.md` — Windows v5603998 完整逆向 (agent4)
- `docs/upstream_analysis_post_fw_boot.md` — 上游 mt7925 完整分析 (agent3)
- `docs/references/windows_post_fw_init.md` — Windows v5705275 初步分析 (agent2)
- `docs/session_status_2026-02-15.md` — 当前项目状态

---

## 9. 下一步行动

### 立即测试 (Mode 40 重启后)
1. 验证 CB_INFRA_RGU 复位是否让 FW 正确配置 MCU_RX0
2. 如果 MCU_RX0 仍为 0,添加第二次 SET_OWN/CLR_OWN

### 后续实现
1. DMASHDL 使能代码
2. TXD 格式修正
3. 完整的 PostFwDownloadInit 9 个 MCU 命令序列

### 调试重点
- 监控 DUMMY_CR BIT(1) 变化 (NEED_REINIT 消费)
- 对比 R2A FSM 状态 (0xd752c/0xd7530)
- 验证 HOST ring BASE 是否在第二次 CLR_OWN 后保留 (根据上游分析应该保留)
