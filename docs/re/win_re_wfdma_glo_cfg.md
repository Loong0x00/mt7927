# Windows RE: WFDMA GLO_CFG 与相关寄存器分析

**生成时间**: 2026-02-21
**来源**: Windows mtkwecx.sys v5705275 + v5603998 Ghidra RE
**目的**: 确认 Windows 精确配置值，与 Linux 驱动对比，找出差异

---

## 1. GLO_CFG (0xd4208 = MT_WFDMA0(0x208))

### Windows 精确行为

Windows 通过**两个函数**配置 GLO_CFG：

#### 第一步: FUN_1401d8724 (enable sub-routine)
从 MT6639WpdmaConfig 开头调用，参数为 `FUN_1401d8724(ctx, 0, enable)`

**ENABLE 路径** (param_3=1, param_2=0):
```asm
READ(0x7c024208, &glo_cfg)
glo_cfg |= 0x50001070   ; param_2=0 时
glo_cfg |= 0x208000
WRITE(0x7c024208, glo_cfg)
```
即 `glo_cfg |= (0x50001070 | 0x208000) = 0x50209070`

**DISABLE 路径** (param_3=0):
```asm
READ(0x7c024208, &glo_cfg)
glo_cfg &= 0xe7df7ffa   ; AND mask (clears specific bits)
WRITE(0x7c024208, glo_cfg)
WaitIdle()
WRITE(0x7c02420c, 0xffffffff)  ; Clear INT_ENA
WRITE(0x7c024280, 0xffffffff)  ; Clear INT_STA
```

#### 第二步: MT6639WpdmaConfig (FUN_1401e5be0)
在 FUN_1401d8724 之后执行:
```c
glo_cfg |= 0x5;  // BIT(0) | BIT(2) = TX_DMA_EN | RX_DMA_EN
WRITE(0x7c024208, glo_cfg)
```

### Windows 合并 ENABLE 操作

Windows 明确设置的位 = `0x50209070 | 0x5 = 0x50209075`:
| 位 | 值 | 含义 |
|----|-----|------|
| BIT(0) | 0x00000001 | TX_DMA_EN |
| BIT(2) | 0x00000004 | RX_DMA_EN |
| **BIT(4)** | **0x00000010** | **WPDMABurstSIZE bit0** |
| **BIT(5)** | **0x00000020** | **WPDMABurstSIZE bit1 (=3, max burst)** |
| BIT(6) | 0x00000040 | TX_WB_DDONE |
| **BIT(12)** | **0x00001000** | **fifo_little_endian** |
| BIT(15) | 0x00008000 | CSR_DISP_BASE_PTR_CHAIN_EN (prefetch chain) |
| BIT(21) | 0x00200000 | OMIT_RX_INFO_PFET2 |
| BIT(28) | 0x10000000 | OMIT_TX_INFO |
| BIT(30) | 0x40000000 | CLK_GATE_DIS |

Windows DISABLE 清除位 (AND ~{bits}):
BIT(0)|BIT(2)|BIT(15)|BIT(21)|BIT(27)|BIT(28)

### Linux 驱动当前行为

```c
// mt7927_wpdma_config enable path:
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);  // READ current
val |= MT_WFDMA_GLO_CFG_TX_DMA_EN        // BIT(0)
    | MT_WFDMA_GLO_CFG_RX_DMA_EN;        // BIT(2)
val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL; // BIT(9) ← 多余!
val |= MT_GLO_CFG_TX_WB_DDONE;           // BIT(6) ✓
val |= MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN; // BIT(15) ✓
val |= MT_GLO_CFG_OMIT_RX_INFO_PFET2;   // BIT(21) ✓ (但在实测值中未出现?)
val |= MT_GLO_CFG_ADDR_EXT_EN;          // BIT(26) ← 多余?
val |= MT_GLO_CFG_OMIT_TX_INFO;         // BIT(28) ✓
val |= MT_GLO_CFG_CLK_GATE_DIS;         // BIT(30) ✓
```

我们明确设置: `BIT(0)|BIT(2)|BIT(6)|BIT(9)|BIT(15)|BIT(21)|BIT(26)|BIT(28)|BIT(30)` = 0x54208245

### 对比分析

| 位 | Windows明确设 | 我们明确设 | 实测运行值 | 差异说明 |
|----|:---:|:---:|:---:|------|
| BIT(0) TX_DMA_EN | ✅ | ✅ | ✅ | MATCH |
| BIT(2) RX_DMA_EN | ✅ | ✅ | ✅ | MATCH |
| BIT(4) BurstSIZE0 | ✅ | ❌ | ✅ | 从硬件默认保留 |
| BIT(5) BurstSIZE1 | ✅ | ❌ | ✅ | 从硬件默认保留 |
| BIT(6) TX_WB_DDONE | ✅ | ✅ | ✅ | MATCH |
| BIT(9) FW_BYPASS | ❌ | ✅ | ✅ | **我们多设了此位** |
| BIT(11) | ❌ | ❌ | ✅ | 硬件默认 |
| BIT(12) fifo_LE | ✅ | ❌ | ✅ | 从硬件默认保留 |
| BIT(13) | ❌ | ❌ | ✅ | 硬件默认 |
| BIT(15) CHAIN_EN | ✅ | ✅ | ✅ | MATCH |
| BIT(20) | ❌ | ❌ | ✅ | 硬件默认 |
| BIT(21) OMIT_RX | ✅ | ✅ | ❌? | 可能实测时已清除 |
| BIT(26) ADDR_EXT | ❌ | ✅ | ✅ | **我们多设了此位** |
| BIT(28) OMIT_TX | ✅ | ✅ | ✅ | MATCH |
| BIT(30) CLK_DIS | ✅ | ✅ | ✅ | MATCH |

### 差异分析

1. **BIT(9) = FW_DWLD_BYPASS_DMASHDL (0x200)** — **我们多设**
   - Windows 在 WpdmaConfig 中不设此位
   - 此位本应只在 FWDL 阶段有效，设置后绕过 DMASHDL
   - **风险**: 如果 Ring 2 帧通过 DMASHDL，此位设置后 DMASHDL 被绕过，可能导致帧被错误处理
   - **注**: 由于 DMASHDL_SW_CONTROL 也有 BYPASS_EN，双重 bypass 可能叠加

2. **BIT(26) = ADDR_EXT_EN (0x4000000)** — **我们多设**
   - Windows 不在 WpdmaConfig 中显式设此位
   - 此位从硬件复位值保留，实际影响可能很小
   - 如果硬件复位值包含此位，Windows 也会有

3. **BIT(4)|BIT(5)|BIT(12)** — Windows 明确设，我们不设
   - 由于使用 read-modify-write，这些位从硬件复位值保留
   - 实测值 0x5410ba75 显示这些位存在 → **不是问题**

### 结论

GLO_CFG 的实际运行值基本正确 (实测 0x5410ba75 已包含所有必要位)。
主要风险: **BIT(9) = FW_DWLD_BYPASS_DMASHDL 不应在 WpdmaConfig enable 中设置**。

---

## 2. GLO_CFG_EXT0 (0xd42b0 = MT_WFDMA0(0x2b0))

### Windows 行为

- **MT6639WpdmaConfig 从不写此寄存器**
- 仅在 debug dump 函数 (FUN_1401e7630) 中作为只读寄存器读取

从 HANDOFF 实测，固件启动后的值:
```
HOST GLO_EXT0 (0xd42b0) = 0x28c004df  (固件设置的默认值)
```

解码 0x28c004df:
- BIT(0-6) = 0x5f: 各种低级配置位
- BIT(7) = 0x80: (可能是 BigEndian 相关?)
- BIT(14) = 0x4000: ...
- BIT(23) = 0x800000: ...
- BIT(27) = 0x8000000: ...
- BIT(29) = 0x20000000: ...

### Linux 驱动

```c
#define MT_WPDMA_GLO_CFG_EXT0_VAL   0x28C004DF  // 定义但从不写入!
```

寄存器 `MT_WPDMA_GLO_CFG_EXT0` 仅在 `mt7927_dump_status()` 中读取，从不被写入。

### 结论 ✅ MATCH

两者都不写 EXT0。固件启动后的值 0x28c004df 由固件本身设置，保持不变。

---

## 3. GLO_CFG_EXT1 (0xd42b4 = MT_WFDMA0(0x2b4))

### Windows 行为

从 FUN_1401e5be0 (MT6639WpdmaConfig):
```asm
READ(0x7c0242b4, &val)
BTS R8D, 0x1c        ; Set BIT(28) = 0x10000000
WRITE(0x7c0242b4, val)
```

即 `EXT1 |= 0x10000000` (BIT(28))，READ-MODIFY-WRITE。

实测值: `HOST GLO_EXT1 (0xd42b4) = 0x9c800404`
- 硬件默认: 0x8C800404
- Windows |= BIT(28): 0x8C800404 | 0x10000000 = **0x9C800404** ✅

### Linux 驱动

```c
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
val |= MT_WPDMA_GLO_CFG_EXT1_WIN;  // BIT(28)
mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, val);
```

### 结论 ✅ MATCH

完全一致。

---

## 4. Packed Prefetch (0xd70f0-0xd70fc)

### Windows 行为

在 MT6639WpdmaConfig enable 路径，**当 `ctx+0x1466a49` flag 为非零时**:
```c
WRITE(0x7c027030, read_current)   // Read-write (no change)
WRITE(0x7c0270f0, 0x00660077)     // d70f0
WRITE(0x7c0270f4, 0x00001100)     // d70f4
WRITE(0x7c0270f8, 0x0030004f)     // d70f8
WRITE(0x7c0270fc, 0x00542200)     // d70fc
```

Flag `0x1466a49` 是运行时特性标志 (可能是 HOST_OFFLOAD/MSI 使能)。正常 PCIe 运行时此 flag 应为非零。

值解析 (每个寄存器包含 2 个 ring 的 prefetch 配置 [base:12][depth:4]):
```
0x00660077: ring_A=base=0x007,depth=7; ring_B=base=0x006,depth=6
0x00001100: ring_C=base=0x110,depth=0; ring_D=base=0x000,depth=0
0x0030004f: ring_E=base=0x004,depth=f; ring_F=base=0x003,depth=0
0x00542200: ring_G=base=0x220,depth=0; ring_H=base=0x005,depth=4
```

### Linux 驱动

```c
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG0, 0x660077);   // ✓
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG1, 0x1100);     // ✓
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG2, 0x30004f);   // ✓
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG3, 0x542200);   // ✓
```

### 结论 ✅ MATCH

完全一致。但我们**无条件写入**，而 Windows 有 flag 条件。由于正常 PCIe 运行时 flag 应非零，这在实践中无差异。

---

## 5. Ring 2 EXT_CTRL (0xd4608 = MT_WFDMA0(0x0608))

### Windows 行为

**MT6639InitTxRxRing (FUN_1401e4580) 不写 EXT_CTRL 寄存器**。
检查函数 immediates 列表，无任何 `0x7c02460x` 地址。

EXT_CTRL 寄存器 (`0x7c024600` - `0x7c02468c`) 只出现在 **STOP 路径** (FUN_1401e6430):
- `0x7c024600 .. 0x7c02460c` (step 4) — TX rings 0-3 EXT_CTRL 清零
- `0x7c024680 .. 0x7c02468c` (step 4) — RX rings EXT_CTRL 清零

Windows **在 normal init 中不设置 per-ring EXT_CTRL**，仅使用 packed prefetch (0xd70f0-0xd70fc)。

### Linux 驱动

```c
// 我们额外设置了 per-ring EXT_CTRL:
mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(2), PREFETCH(0x02C0, 0x4)); // 0x02c00004
```

地址: `MT_WFDMA0(0x0600 + 2*4) = MT_WFDMA0(0x0608) = 0xd4608`

### 差异分析

Windows 不在 init 中写 per-ring EXT_CTRL，我们写了。这两套机制:
- **Packed prefetch** (0xd70f0-0xd70fc): Windows 使用，我们也写
- **Per-ring EXT_CTRL** (0xd4608 等): mt7925 使用，**Windows MT6639 不使用**

如果 WFDMA 硬件**同时**生效两套配置，可能产生冲突。如果只生效其中一套，问题较小。

**风险**: Ring 2 per-ring EXT_CTRL 设置可能与 packed prefetch 冲突，导致 Ring 2 预取配置错误。

---

## 6. WFDMA RST (0xd4100 = MT_WFDMA0(0x100))

### Windows 行为

从 FUN_1401d8724 (disable 路径) 分析:
- **不在 FUN_1401d8724 中写 RST 寄存器**
- FUN_1401d8724 disable 只做: clear GLO_CFG bits, WaitIdle, clear INT_ENA, clear INT_STA

RST 寄存器可能在其他函数 (如 PreFirmwareDownloadInit) 中使用，但不在 WpdmaConfig 流程。

### Linux 驱动

```c
// mt7927_dma_disable:
val = mt7927_rr(dev, MT_WFDMA0_RST);
val &= ~(MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST);
mt7927_wr(dev, MT_WFDMA0_RST, val);
val |= MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST;
mt7927_wr(dev, MT_WFDMA0_RST, val);
```

### 结论

我们在 disable 时做 RST pulse，Windows 的 WpdmaConfig 流程不做。这是 stop path 差异，对 TX 问题影响有限。

---

## 总结: 差异列表与修复建议

### 🔴 高优先级

#### 差异1: BIT(9) = FW_DWLD_BYPASS_DMASHDL 不应在 WpdmaConfig enable 中设置

**问题**: 我们在 `mt7927_wpdma_config(enable=true)` 中设置 `MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL = BIT(9)`。
**Windows**: 在 WpdmaConfig enable 路径完全不设此位。
**影响**: 此位告诉硬件绕过 DMASHDL 进行固件下载模式。如果此位在正常操作时保留设置，可能影响 DMASHDL 对 Ring 2 帧的处理。

**修复**:
```c
// 移除这行:
val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
```

或者在 FWDL 完成后、WpdmaConfig 之前显式清除:
```c
mt7927_rmw(dev, MT_WPDMA_GLO_CFG, MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL, 0);
```

#### 差异2: Ring 2 per-ring EXT_CTRL 可能与 packed prefetch 冲突

**问题**: 我们设置 `MT_WFDMA_TX_RING_EXT_CTRL(2) = PREFETCH(0x02C0, 0x4)`，但 Windows 不设。
**修复方案**: 尝试注释掉 Ring 2 的 per-ring EXT_CTRL，只依赖 packed prefetch。

```c
// 注释掉或删除:
// mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(2), PREFETCH(0x02C0, 0x4));
```

### 🟡 中优先级

#### 差异3: BIT(26) = ADDR_EXT_EN 可能多余

**问题**: 我们明确设 BIT(26)，Windows 不在 WpdmaConfig 中设。
**注**: 此位可能是硬件默认值，两者实际值相同。
**建议**: 诊断时可尝试移除此位，观察影响。

### ✅ 已确认正确的配置

| 寄存器 | 我们的值 | Windows值 | 状态 |
|--------|---------|-----------|------|
| GLO_CFG (enable后) | 0x5410ba75 | ~0x54?09075 + HW defaults | ✅ 基本匹配 |
| GLO_CFG_EXT0 | 从不写 | 从不写 | ✅ MATCH |
| GLO_CFG_EXT1 | `\|= BIT(28)` | `\|= BIT(28)` | ✅ MATCH |
| Packed prefetch | 0x660077,0x1100,0x30004f,0x542200 | 同 | ✅ MATCH |

---

## 附录: 关键函数地址

| 函数 | 地址 (v5705275) | 作用 |
|------|----------------|------|
| MT6639WpdmaConfig | FUN_1401e5be0 | 预取 + GLO_CFG enable |
| FUN_1401d8724 | 0x1401d8724 | GLO_CFG enable/disable sub |
| MT6639InitTxRxRing | FUN_1401e4580 | Ring 初始化 (无 EXT_CTRL) |
| FUN_1401e6430 | 0x1401e6430 | STOP path (清零 EXT_CTRL) |
| connac3x_show_pcie_wfdma_info | FUN_1401dd630 | Debug dump (只读 EXT0) |
