# Windows MT6639/MT7927 驱动完整初始化流程逆向分析

**日期**: 2026-02-15
**来源**: Ghidra RE of mtkwecx.sys v5603998 + v5705275
**分析者**: win-reverser agent

---

## 一、概述

本文档完整分析 Windows MT6639/MT7927 驱动从设备上电到第一条 MCU 命令成功响应的全部初始化流程。基于对两个 Windows 驱动版本的 Ghidra 逆向工程结果。

**核心发现**: Windows 驱动使用 HOST RX ring 4, 6, 7 —— **不使用 ring 0**。我们的驱动使用 ring 4, 5, 7，其中 **ring 5 应该是 ring 6**。

---

## 二、文档汇总 (Phase 1)

### 已分析的关键文档

| 文档 | 内容 | 重要性 |
|------|------|--------|
| `ghidra_post_fw_init.md` | PostFwDownloadInit 完整分析、ToggleWfsysRst 序列、MCU 命令路径、TXD 格式、vtable 布局 | **最关键** |
| `win_v5705275_core_funcs.md` | MT6639PreFirmwareDownloadInit, MT6639WpdmaConfig, **MT6639InitTxRxRing**, MT6639ConfigIntMask 反编译 | **关键** |
| `win_v5705275_dma_enqueue.md` | MtCmdEnqueueFWCmd (FUN_1400c8340) — TX ring 提交核心函数 | 重要 |
| `win_v5705275_mcu_dma_submit.md` | MtCmdSendSetQueryUniCmdAdv — CONNAC3 UniCmd 发送路径、TXD 构造 | 重要 |
| `win_v5705275_mcu_send_backends.md` | MtCmdSendSetQueryCmdHelper — CONNAC3 路由表查找 + 调度 | 重要 |
| `win_v5705275_dma_lowlevel.md` | MtCmdStoreInfo — MCU 命令存储、环形缓冲区管理 | 次要 |
| `win_v5705275_fw_flow.md` | 固件加载函数字符串引用和调用图 | 参考 |
| `win_v5603998_fw_flow.md` | 早期版本固件加载流程 | 参考 |

### 关键函数映射

| 函数名 | 地址 (v5705275) | 地址 (v5603998) | 用途 |
|--------|----------------|----------------|------|
| MT6639PreFirmwareDownloadInit | FUN_1401e5430 | — | 固件下载前初始化 |
| MT6639InitTxRxRing | FUN_1401e4580 | FUN_1401d6d30 | **TX/RX ring 设置** |
| MT6639WpdmaConfig | FUN_1401e5be0 | FUN_1401d8290 | WPDMA GLO_CFG 启用 |
| MT6639ConfigIntMask | FUN_1401e43e0 | — | 中断掩码配置 |
| AsicConnac3xPostFwDownloadInit | — | FUN_1401c9510 | 固件下载后初始化 |
| AsicConnac3xToggleWfsysRst | — | FUN_1401cb360 | WFSYS 复位切换 |
| AsicConnac3xLoadFirmware | FUN_1401d01d0 | FUN_1401c5020 | 固件下载 |
| MtCmdEnqueueFWCmd | FUN_1400c8340 | FUN_1400c3e44 | MCU 命令入队 |
| MtCmdSendSetQueryCmdDispatch | FUN_1400cdc4c | FUN_1400c9468 | MCU 命令调度 |

---

## 三、完整初始化时间线 (Phase 2)

### ASIC 操作 Vtable (0x14024cbd0)

Windows 驱动使用函数指针表驱动初始化：

| 偏移 | 函数 | 阶段名 |
|------|------|--------|
| +0x00 | MT6639PreFirmwareDownloadInit | 芯片特定预初始化 |
| +0x08 | AsicConnac3xLoadRomPatch | ROM 补丁加载 |
| +0x10 | DMA Init | DMA 初始化 |
| +0x20 | AsicConnac3xLoadFirmware | 固件下载 |
| +0x28 | Post-FW intermediate | 固件下载后中间步骤 |
| +0x40 | HIF Init | HIF 初始化 |
| +0x48 | **AsicConnac3xPostFwDownloadInit** | **MCU 命令序列** |
| +0x50 | Post-Init | 最终初始化 |
| +0x58 | AsicConnac3xToggleWfsysRst | WFSYS 复位 |
| +0x78 | MT6639InitTxRxRing | TX/RX ring 配置 |
| +0x80 | MT6639WpdmaConfig | WPDMA 启用 |
| +0x88 | AsicConnac3xWpdmaInitRing | ring 初始化调度 |
| +0x178 | AsicConnac3xGetHeaderSize | MCU 头部大小 |

### 详细初始化流程

#### 阶段 1: MT6639PreFirmwareDownloadInit (FUN_1401e5430)

```
1. 检查 *(ctx + 0x14669e0) == 0（首次初始化标志）
2. 如果首次：记录 "MT6639PreFirmwareDownloadInit" 日志
3. 调用 FUN_1401ce900(ctx) — 芯片状态检查
4. 如果成功：
   a. *(ctx + 0x1464d29) = 0  — 清除某标志
   b. 调用 FUN_14000d410(ctx) — 底层初始化
   c. 轮询 FUN_1401ce900(ctx) 等待返回 1（最多 500 次，每次 1ms）
5. 如果失败：
   a. *(ctx + 0x1464d29) = 1  — 设置错误标志
   b. 调用 FUN_14000d410(ctx) — 尝试恢复
```

#### 阶段 2: DMA 初始化（Pre-FWDL）

**InitTxRxRing → WpdmaConfig → ConfigIntMask**

##### 2a. MT6639InitTxRxRing (FUN_1401e4580) — 关键！

**TX Rings（4 个环）**:

| 序号 | HW Ring 偏移 | 计算方式 | BASE 寄存器 | CNT 寄存器 | CIDX 寄存器 | DIDX 寄存器 |
|------|-------------|----------|-------------|------------|-------------|-------------|
| 0 | 0x00 | 0 << 4 | 0x7c024300 | 0x7c024304 | 0x7c024308 | 0x7c02430c |
| 1 | 0x10 | 1 << 4 | 0x7c024310 | 0x7c024314 | 0x7c024318 | 0x7c02431c |
| 2 | 可配置 | config[0xd1] << 4 | 动态 | 动态 | 动态 | 动态 |
| 3 | 可配置 | config[0xd0] << 4 | 动态 | 动态 | 动态 | 动态 |

> TX ring 2/3 的 HW ring 编号来自配置结构体的 byte 字段（偏移 0xd0, 0xd1），可能是 ring 15/16 或其他值。

**RX Rings（3 个环）**:

| 序号 (uVar7) | HW Ring 偏移 (iVar9) | HW Ring 编号 | BASE 寄存器 | CNT 寄存器 | CIDX 寄存器 | DIDX 寄存器 |
|-------------|---------------------|-------------|-------------|------------|-------------|-------------|
| 0 | **0x40** | **Ring 4** | 0x7c024540 | 0x7c024544 | 0x7c024548 | 0x7c02454c |
| 1 | **0x60** | **Ring 6** | 0x7c024560 | 0x7c024564 | 0x7c024568 | 0x7c02456c |
| 2 | **0x70** | **Ring 7** | 0x7c024570 | 0x7c024574 | 0x7c024578 | 0x7c02457c |

> **关键发现**: Windows 使用 HOST RX ring **4, 6, 7** —— **不使用 ring 0, 不使用 ring 5！**

对每个 RX ring：
1. 从已分配的结构体读取 ring 物理地址和条目数
2. 写入 WFDMA 寄存器：BASE（物理地址）、CNT（条目数）、CIDX（消费者索引 = 条目数-1）
3. 清除 DIDX
4. 遍历每个 RX 描述符，清除 BIT(31)（DMA_DONE 标志），设置 buffer 大小到高 14 位

##### 2b. MT6639WpdmaConfig (FUN_1401e5be0)

```c
// 1. 子设置
FUN_1401d8724(ctx, 0, enable);

// 2. 读取 GLO_CFG
READ(ctx, 0x7c024208, &glo_cfg);  // BAR0+0xd4208

// 3. 如果 enable:
if (enable) {
    // 3a. 预取配置（如果 flag 设置）
    if (*(ctx + 0x1466a49) != 0) {
        READ(ctx, 0x7c027030, &val);   // BAR0+0xd7030
        WRITE(ctx, 0x7c027030, val);   // 回写（可能是触发预取重置）
        WRITE(ctx, 0x7c0270f0, 0x660077);   // BAR0+0xd70f0
        WRITE(ctx, 0x7c0270f4, 0x1100);     // BAR0+0xd70f4
        WRITE(ctx, 0x7c0270f8, 0x30004f);   // BAR0+0xd70f8
        WRITE(ctx, 0x7c0270fc, 0x542200);   // BAR0+0xd70fc
    }

    // 3b. 启用 DMA
    glo_cfg |= 5;  // BIT(0) | BIT(2) = TX_DMA_EN | RX_DMA_EN
    WRITE(ctx, 0x7c024208, glo_cfg);
}

// 4. 设置 GLO_CFG_EXT BIT(28)
READ(ctx, 0x7c0242b4, &val);   // BAR0+0xd42b4
val |= 0x10000000;              // BIT(28)
WRITE(ctx, 0x7c0242b4, val);
```

##### 2c. MT6639ConfigIntMask (FUN_1401e43e0)

```c
// 1. 写入主中断掩码
// 根据 enable 参数选择寄存器:
//   enable=1: 0x7c024228 (INT_ENA_SET?)
//   enable=0: 0x7c02422c (INT_ENA_CLR?)
WRITE(ctx, reg, 0x2600f000);

// 2. 读取 INT_STA_EXT
READ(ctx, 0x7c024204, &val);

// 3. MT6639/MT7927/MT7925/MT738/MT717 特殊处理:
//    操作寄存器 0x74030188 的 BIT(16)
READ(ctx, 0x74030188, &val);
if (enable) val |= BIT(16);  else val &= ~BIT(16);
WRITE(ctx, 0x74030188, val);
```

**中断掩码 0x2600f000 分析**:
```
0x2600f000 = 0010_0110_0000_0000_1111_0000_0000_0000
BIT(29) = MCU2HOST_SW_INT（MCU 命令完成中断）
BIT(25) = 未确定（可能是 WDT 或其他）
BIT(15:12) = 0xF = RX ring 完成中断（ring 4/5/6/7 的完成中断）
```

#### 阶段 3: 固件下载 (AsicConnac3xLoadFirmware)

与我们的实现基本一致（已工作）：
1. patch_sem → init_dl → scatter → patch_finish → sem_release
2. 每个 RAM 区域：init_dl → scatter（带 DL_MODE_ENCRYPT | DL_MODE_RESET_SEC_IV）
3. FW_START_OVERRIDE(option=1) → 轮询 fw_sync=0x3

#### 阶段 4: Post-FW Intermediate (FUN_1401c2e90)

待进一步分析。可能包含 SET_OWN/CLR_OWN 序列。

#### 阶段 5: HIF Init

HIF 层初始化，可能包含 PCIe 中断配置。

#### 阶段 6: PostFwDownloadInit (FUN_1401c9510) — 关键！

**这是固件启动后的第一批 MCU 命令。**

```
步骤 1: 清除标志 *(ctx + 0x146e61c) = 0

步骤 2: DMASHDL 启用
  READ(ctx, 0x7c026060, &val);    // BAR0+0xd6060
  val |= 0x10101;                  // BIT(0)|BIT(8)|BIT(16)
  WRITE(ctx, 0x7c026060, val);
  ↑ 这是 MCU 命令之前的唯一寄存器写入！

步骤 3: 第一条 MCU 命令 — NIC Capability
  mcu_dispatch(ctx, class=0x8a, target=0xed, payload=NULL, len=0)
  → 成功后写入 capability flags

步骤 4: 配置命令
  mcu_dispatch(ctx, class=0x02, target=0xed,
               payload={tag=1, pad=0, value=0x70000}, len=0xc)

步骤 5: 配置命令
  mcu_dispatch(ctx, class=0xc0, target=0xed,
               payload={0x820cc800, 0x3c200}, len=8)

步骤 6: Buffer 下载（可选）
  if (*(ctx + 0x1467608) == 1):
    mcu_dispatch(ctx, class=0xed, target=0xed, subcmd=0x21, ...)
  （通过 NdisOpenFile 读取文件，分 1KB 块发送）

步骤 7: DBDC 设置（仅 MT6639/MT7927）
  mcu_dispatch(ctx, class=0x28, target=0xed,
               payload=0x24字节 DBDC 参数)

步骤 8: 1ms 延迟
  KeStallExecutionProcessor(10) × 100

步骤 9: SetPassiveToActiveScan
  mcu_dispatch(ctx, class=0xca, target=0xed, ...)

步骤 10: SetFWChipConfig
  mcu_dispatch(ctx, class=0xca, target=0xed, ...)

步骤 11: SetLogLevelConfig
  mcu_dispatch(ctx, class=0xca, target=0xed, ...)
```

### MCU 命令发送路径

PostFwDownloadInit 开始时清除 `ctx+0x146e61c`（注意不是 `ctx+0x146e621`）。
MCU 命令调度由 `ctx+0x146e621` 标志控制：

```c
// MtCmdSendSetQueryCmdDispatch
void dispatch(ctx) {
    chip_id = *(short*)(ctx + 0x1f72);
    if (is_connac3(chip_id) && *(ctx + 0x146e621) == 1) {
        MtCmdSendSetQueryCmdHelper(args);  // CONNAC3 UniCmd 路径
    } else {
        MtCmdSendSetQueryCmdAdv(args);     // Generic/legacy 路径
    }
}
```

**两条路径的 TXD 格式**:

| 字段 | Legacy (flag=0) | CONNAC3 UniCmd (flag=1) |
|------|----------------|------------------------|
| 头部大小 | 0x40 (64 字节) | 0x30 (48 字节) |
| TXD[0] | total_len \| 0x41000000 | total_len \| 0x41000000 |
| TXD[1] | flags \| 0x4000 (HDR_V3, 无 BIT(31)) | flags \| 0x4000 |
| class 位置 | +0x24 | +0x24 |
| type 位置 | +0x25 = 0xa0 | +0x25 = 0xa0 |
| seq 位置 | +0x27 | +0x27 |
| payload 位置 | +0x40 | +0x30 |

**注意**: 两条路径最终都调用同一个 `MtCmdEnqueueFWCmd` 函数提交到 TX ring。

### WoWLAN 恢复流程（包含 ToggleWfsysRst）

```
1. 清除 flag_146e621 = 0
2. 清除 DAT_14024b439 = 0
3. ToggleWfsysRst() — 完整 19 步复位序列
4. 清除错误标志
5. 主初始化（LoadFW + PostFwDownloadInit via vtable）
6. 设置 DAT_14024b439 = 1
```

---

## 四、五个核心问题的答案 (Phase 3)

### Q1: Windows 驱动分配了多少 HOST RX ring？Ring 0 是否存在？

**确定性答案**: Windows 驱动分配 **3 个 HOST RX ring**:
- **Ring 4** (HW offset 0x40) — 数据 RX
- **Ring 6** (HW offset 0x60) — MCU 事件/数据
- **Ring 7** (HW offset 0x70) — MCU 事件/数据

**Ring 0 不存在！** Windows 驱动从未分配 HOST RX ring 0。

**证据**: `MT6639InitTxRxRing` (FUN_1401e4580) 的 RX ring 循环：
```c
// uVar7=0: iVar9 = 0x40 → Ring 4
// uVar7=1: iVar9 = 0x60 → Ring 6
// uVar7=2: iVar9 = 0x70 → Ring 7
// 循环条件: uVar7 < 3
```

**影响**: 我们的 Mode 53（添加 HOST RX ring 0）假设是**错误的方向**。Windows 根本不需要 ring 0。

### Q2: 每个 ring 的大小和用途？

**TX Rings（4 个）**:

| Ring | 用途 | 大小 |
|------|------|------|
| TX 0 | 数据发送 | 来自配置，未在 InitTxRxRing 中硬编码 |
| TX 1 | 数据发送 | 同上 |
| TX 2 | 可配置（config[0xd1]） | 同上 |
| TX 3 | 可配置（config[0xd0]），可能是 MCU 命令 TX (ring 15) | 同上 |

> Ring 大小在 InitTxRxRing **之前**已通过其他函数（如 WpdmaInitRing）分配。InitTxRxRing 只是将已分配的物理地址和大小写入 WFDMA 寄存器。

**RX Rings（3 个）**:

| Ring | HW 编号 | 可能用途 |
|------|---------|---------|
| RX 0 | Ring 4 | HOST 数据接收（WiFi 帧） |
| RX 1 | Ring 6 | MCU 事件接收 |
| RX 2 | Ring 7 | 数据/事件接收 |

> 具体用途需要进一步分析 MCU 事件处理代码来确认。

### Q3: Pre-FWDL 和 Post-FWDL DMA 初始化有什么区别？

**Pre-FWDL DMA 初始化**:
1. `InitTxRxRing`: 分配 ring buffer，将物理地址/大小写入 WFDMA 寄存器
2. `WpdmaConfig`: 设置预取配置，启用 GLO_CFG（TX_DMA_EN | RX_DMA_EN），设置 GLO_CFG_EXT BIT(28)
3. `ConfigIntMask`: 设置中断掩码 0x2600f000，配置芯片特定中断寄存器

**Post-FWDL DMA 初始化（PostFwDownloadInit）**:
1. **仅一个寄存器写入**: `BAR0+0xd6060 |= 0x10101`（DMASHDL 启用）
2. 之后全部是 MCU 命令，不涉及直接 DMA 寄存器操作

**关键区别**: Pre-FWDL 完成所有 WFDMA ring 配置，Post-FWDL 只做 DMASHDL 启用 + MCU 命令。没有"重新配置 ring"的操作。

### Q4: MCU DMA RX ring 是驱动配置的还是固件配置的？

**不确定**。根据可用信息：

- **HOST RX ring（ring 4, 6, 7）**: 由驱动在 `InitTxRxRing` 中配置
- **MCU RX ring（MCU_RX0, MCU_RX1 等）**: 可能由固件在启动后自行配置
  - 在 `InitTxRxRing` 中完全没有操作 MCU DMA 寄存器空间（0x54000000/0x55000000）
  - 0x54000000 (WF_WFDMA_MCU_DMA0) 在驱动中出现 93 次，但可能只用于调试/状态读取
  - 驱动写入 HOST ring 寄存器，固件写入 MCU ring 寄存器 —— 各自负责自己的域

**推测**: MCU_RX0 等 ring 由**固件**在启动后配置，驱动不直接操作。如果固件不配置 MCU_RX0，问题可能在固件启动过程或 DMASHDL 配置中。

### Q5: 从上电到第一条成功 MCU 命令的完整初始化序列？

```
1. PCI probe → BAR0 MMIO 映射
2. MT6639PreFirmwareDownloadInit:
   a. 芯片状态检查
   b. 底层初始化
   c. 轮询就绪状态（最多 500ms）
3. LoadRomPatch:
   a. patch_sem → init_dl → scatter → patch_finish → sem_release
4. DMA Init:
   a. InitTxRxRing — 配置 4 个 TX ring + 3 个 RX ring (4,6,7) 的 WFDMA 寄存器
   b. WpdmaConfig — 预取配置 + GLO_CFG 启用 (TX_DMA_EN|RX_DMA_EN) + GLO_CFG_EXT BIT(28)
   c. ConfigIntMask — INT_ENA = 0x2600f000 + 芯片特定中断
5. LoadFirmware:
   a. 每个 RAM 区域: init_dl → scatter (带加密标志)
   b. FW_START_OVERRIDE(option=1)
   c. 轮询 fw_sync=0x3
6. Post-FW Intermediate（具体内容待分析）
7. HIF Init
8. PostFwDownloadInit:
   a. BAR0+0xd6060 |= 0x10101  ← DMASHDL 启用
   b. MCU cmd class=0x8a (NIC_CAP)  ← 第一条 MCU 命令！
   c. 如果成功 → 继续后续 MCU 命令
```

---

## 五、确定性结论

### 1. Windows 使用 HOST RX ring 4, 6, 7（不含 ring 0）

**确定度: 100%**

代码清楚显示 RX ring 循环固定使用 iVar9 = {0x40, 0x60, 0x70}，即 HW ring 4, 6, 7。没有任何条件分支会分配 ring 0。

### 2. 我们的 ring 5 应该是 ring 6

**确定度: 100%**

Windows: ring 4(0x40), ring 6(0x60), ring 7(0x70)
我们: ring 4(0x40), **ring 5(0x50)**, ring 7(0x70)

Ring 5 vs Ring 6 的寄存器地址完全不同：
- Ring 5: BASE=0x7c024550, CIDX=0x7c024558, DIDX=0x7c02455c
- Ring 6: BASE=0x7c024560, CIDX=0x7c024568, DIDX=0x7c02456c

### 3. Post-FWDL 唯一的寄存器操作是 DMASHDL 启用

**确定度: 100%**

PostFwDownloadInit 中，MCU 命令之前只有一个寄存器写入：`BAR0+0xd6060 |= 0x10101`。

### 4. MCU 命令的 TXD 格式

**确定度: 95%**

- TXD[0]: total_len | 0x41000000 (Q_IDX=0x20, PKT_FMT=2)
- TXD[1]: flags | 0x4000 (HDR_FORMAT_V3=1, **永不**设置 BIT(31))
- 这已经在我们的驱动中正确实现

### 5. InitTxRxRing 在 FWDL 之前执行

**确定度: 100%**

Vtable 顺序: +0x10(DMA Init, 含 InitTxRxRing) → +0x20(LoadFirmware)。
Ring 配置发生在固件下载之前。

### 6. 预取配置值

**确定度: 100%**

```
0x7c0270f0 = 0x660077
0x7c0270f4 = 0x1100
0x7c0270f8 = 0x30004f
0x7c0270fc = 0x542200
```

### 7. GLO_CFG_EXT BIT(28) 必须设置

**确定度: 100%**

WpdmaConfig 在启用 DMA 后，无条件地在 0x7c0242b4 (BAR0+0xd42b4) 设置 BIT(28)。

### 8. 中断掩码值

**确定度: 100%**

INT_ENA = 0x2600f000。对 MT6639/MT7927 还需操作 0x74030188 的 BIT(16)。

---

## 六、仍不确定的部分

### 1. TX Ring 2/3 的实际 HW ring 编号

TX ring 2 和 3 的编号来自配置结构体（`config[0xd0]` 和 `config[0xd1]`），运行时值不确定。其中一个可能是 ring 15（MCU 命令 TX ring）。

### 2. 各 RX ring 的精确用途

- Ring 4 可能用于 WiFi 数据帧接收
- Ring 6 可能用于 MCU 事件接收
- Ring 7 用途不确定
- 需要分析中断处理和 RX 回调代码才能确认

### 3. RX Ring 大小

Ring 条目数在 `InitTxRxRing` 之前已分配，代码中没有硬编码。需要分析 `WpdmaInitRing` (FUN_1401d8bb0) 来确定每个 ring 的大小。

### 4. Post-FW Intermediate 阶段的具体内容

Vtable +0x28 处的函数 (FUN_1401c2e90) 尚未完整分析。可能包含 CLR_OWN 序列或其他关键操作。

### 5. HIF Init 阶段的具体内容

Vtable +0x40 处的 thunk 函数尚未分析。

### 6. MCU_RX0 由谁配置

虽然 InitTxRxRing 不操作 MCU DMA 寄存器，但其他初始化阶段可能写入这些寄存器，或者固件自行配置。

### 7. 0x74030188 寄存器的 BAR0 映射

总线地址 0x74030188 在我们的 bus2chip 表中可能没有映射，需要确认。

### 8. MCU 命令在 PostFwDownloadInit 中使用哪条路径（Legacy vs CONNAC3）

取决于 `flag_146e621` 在进入 PostFwDownloadInit 时的值。在正常冷启动中可能是 0（legacy 路径），但在 WoWLAN 恢复中明确为 0。

---

## 七、我们的驱动遗漏了什么（完整清单）

### 严重遗漏（可能直接导致 MCU 命令失败）

#### 1. **HOST RX Ring 5 应该是 Ring 6** ⚠️⚠️⚠️

| | 我们的驱动 | Windows 驱动 |
|---|-----------|-------------|
| RX ring 0 | ring 0 (Mode 53 only) | **不存在** |
| RX ring 1 | ring 4 | ring 4 |
| RX ring 2 | **ring 5** ❌ | **ring 6** ✅ |
| RX ring 3 | ring 7 | ring 7 |

如果 ring 6 是 MCU 事件接收 ring，使用错误的 ring 编号将导致固件发送的 MCU 响应无法被接收。

**修复**: 将 `ring_rx5` 改为 `ring_rx6`（HW ring offset 0x60 → 寄存器 0x7c024560-0x7c02456c）。

#### 2. **缺少预取配置** ⚠️⚠️

WpdmaConfig 中的预取寄存器从未在我们的驱动中配置：
```c
WRITE(0xd70f0, 0x660077);
WRITE(0xd70f4, 0x1100);
WRITE(0xd70f8, 0x30004f);
WRITE(0xd70fc, 0x542200);
```

预取配置影响 WFDMA 如何从 DDR 读取 ring 描述符，错误的配置可能导致 DMA 无法正常工作。

#### 3. **缺少 GLO_CFG_EXT BIT(28)** ⚠️⚠️

```c
READ(0xd42b4, &val);
val |= 0x10000000;  // BIT(28)
WRITE(0xd42b4, val);
```

这是 WpdmaConfig 中的无条件操作，我们从未设置。

#### 4. **中断掩码不匹配** ⚠️

Windows: 0x2600f000
我们的驱动: 需要检查（可能使用了不同的值或未设置）

### 重要遗漏（可能影响稳定性）

#### 5. **缺少 CB_INFRA PCIe 睡眠配置**

```c
// AsicConnac3xSetCbInfraPcieSlpCfg
READ(0x1f5018, &val);
if (val != 0xFFFFFFFF)
    WRITE(0x1f5018, 0xFFFFFFFF);  // 禁用所有睡眠
```

#### 6. **缺少 0x74030188 中断寄存器配置**

MT6639/MT7927 需要操作此寄存器的 BIT(16)：
```c
READ(0x74030188, &val);
val |= BIT(16);
WRITE(0x74030188, val);
```

#### 7. **RX 描述符初始化不完整**

Windows 在 InitTxRxRing 中对每个 RX 描述符做了：
- 清除 BIT(31)（DMA_DONE 标志）
- 验证并设置 buffer 大小到描述符的高 14 位

#### 8. **缺少完整的 PostFwDownloadInit MCU 命令序列**

我们只发送 NIC_CAP (class=0x8a)，Windows 发送 9 条命令。虽然后续命令可能不影响第一条命令的成功，但完整序列对正常运行是必需的。

### 次要遗漏

#### 9. **RX Ring CIDX 初始值**

Windows: CIDX = ring_count - 1（预设消费者索引）
我们的驱动: 需要检查

#### 10. **PreFirmwareDownloadInit 中的状态轮询**

Windows 有一个最多 500 次的轮询循环（每次 1ms），等待 FUN_1401ce900 返回 1。我们可能缺少等效的就绪检查。

#### 11. **TX Ring 2/3 可能使用不同的 HW ring 编号**

如果 config[0xd0]/[0xd1] 指向 ring 15（MCU 命令 TX），我们需要确认我们的 ring 15 配置与 Windows 一致。

---

## 八、优先修复建议

### 立即尝试（Mode 54 候选）

1. **将 ring_rx5 改为 ring_rx6**
   - 最小改动，可能是 MCU 事件无法接收的直接原因
   - 改：HW ring offset 从 0x50 改为 0x60

2. **添加预取配置**
   ```c
   writel(readl(bar0 + 0xd7030), bar0 + 0xd7030);  // 触发预取重置
   writel(0x660077, bar0 + 0xd70f0);
   writel(0x1100,   bar0 + 0xd70f4);
   writel(0x30004f, bar0 + 0xd70f8);
   writel(0x542200, bar0 + 0xd70fc);
   ```

3. **添加 GLO_CFG_EXT BIT(28)**
   ```c
   uint32_t val = readl(bar0 + 0xd42b4);
   writel(val | BIT(28), bar0 + 0xd42b4);
   ```

### 后续修复

4. 设置正确的中断掩码 0x2600f000
5. 配置 0x1f5018 = 0xFFFFFFFF（禁用睡眠）
6. 配置 0x74030188 BIT(16)
7. 移除 ring_rx0（Mode 53 的 ring 0 不需要）
8. 实现完整的 PostFwDownloadInit MCU 命令序列

---

## 九、附录：关键寄存器速查表

### WFDMA HOST DMA0 寄存器 (BAR0 基础偏移 0xd4000)

| BAR0 偏移 | 总线地址 | 名称 | 值 |
|-----------|---------|------|-----|
| 0xd4200 | 0x7c024200 | INT_STA | 中断状态 |
| 0xd4204 | 0x7c024204 | INT_ENA | 中断使能 (0x2600f000) |
| 0xd4208 | 0x7c024208 | GLO_CFG | DMA 全局配置 (\|= 5) |
| 0xd4228 | 0x7c024228 | INT_ENA_SET? | 中断使能设置 |
| 0xd422c | 0x7c02422c | INT_ENA_CLR? | 中断使能清除 |
| 0xd42b4 | 0x7c0242b4 | GLO_CFG_EXT | \|= BIT(28) |
| 0xd4300 | 0x7c024300 | TX Ring 0 BASE | |
| 0xd4540 | 0x7c024540 | RX Ring 4 BASE | |
| 0xd4560 | 0x7c024560 | **RX Ring 6 BASE** | ← 我们遗漏了 |
| 0xd4570 | 0x7c024570 | RX Ring 7 BASE | |
| 0xd6060 | 0x7c026060 | DMASHDL enable | \|= 0x10101 |
| 0xd7030 | 0x7c027030 | Prefetch control | 读回写 |
| 0xd70f0 | 0x7c0270f0 | Prefetch cfg 0 | 0x660077 |
| 0xd70f4 | 0x7c0270f4 | Prefetch cfg 1 | 0x1100 |
| 0xd70f8 | 0x7c0270f8 | Prefetch cfg 2 | 0x30004f |
| 0xd70fc | 0x7c0270fc | Prefetch cfg 3 | 0x542200 |

---

*文档结束 — 2026-02-15*
