# Ring 2 管理帧 TX 固件静默 — 完整差异分析报告

> 研究日期: 2026-02-17
> 对比对象: 我们的驱动代码 vs Windows 驱动逆向 (Ghidra RE)
> 问题: Ring 2 SF mode 管理帧 DMA 消费成功 (DIDX 前进)，但固件完全静默 — 无 TX_DONE、无 TXFREE、无错误

---

## 目录

1. [TXD (Transmit Descriptor) 对比](#1-txd-对比)
2. [DMA 描述符对比](#2-dma-描述符对比)
3. [Ring 2 DMA 初始化流程对比](#3-ring-2-dma-初始化流程对比)
4. [Packed Prefetch vs Per-ring EXT_CTRL](#4-packed-prefetch-vs-per-ring-ext_ctrl)
5. [DMASHDL 配置对比](#5-dmashdl-配置对比)
6. [N6PciTxSendPkt 完整路径对比](#6-n6pcitxsendpkt-完整路径对比)
7. [NAPI TX Complete 处理对比](#7-napi-tx-complete-处理对比)
8. [缺失的 MCU 命令](#8-缺失的-mcu-命令)
9. [差异列表 (按可能性排序)](#9-差异列表-按可能性排序)
10. [修复建议](#10-修复建议)

---

## 1. TXD 对比

### 1.1 DW0 — TX_BYTE_COUNT + PKT_FT + Q_IDX + SN_EN

| 字段 | 位域 (MT6639) | 我们的代码 | Windows RE | 匹配? |
|------|--------------|-----------|-----------|-------|
| TX_BYTE_COUNT | [15:0] | `skb->len + 32` (TXD size) | 同 | ✅ |
| EthTypeOffset | [22:16] | 0 | 0 (mgmt不用) | ✅ |
| PKT_FT | [30:29] | 0 (SF mode) | 0 (SF mode) | ✅ |
| Q_IDX | [28:24] | 1 | 1 (auth) | ✅ |
| SN_EN | [31] | 1 | 1 | ✅ |

**注意**: `mt7927_pci.h` 中的宏定义是**错误的** (来自 mt7925):
```c
// 当前 (错误，但未使用于 SF 路径):
#define MT_TXD0_PKT_FMT     GENMASK(24, 23)  // mt7925 layout
#define MT_TXD0_Q_IDX       GENMASK(31, 25)  // mt7925 layout

// 正确的 MT6639 layout:
// PKT_FT = GENMASK(30, 29)
// Q_IDX  = GENMASK(28, 24)
// SN_EN  = BIT(31)
```

SF 路径使用 raw hex `0x84000000` 绕开了宏，**实际值是正确的**:
- `0x84000000` = SN_EN(bit31)=1, PKT_FT(bits30:29)=0b00(SF), Q_IDX(bits28:24)=0b00001(=1)

**结论: DW0 匹配 ✅** (宏定义错误但未影响 SF 路径)

---

### 1.2 DW1 — WLAN_IDX + HDR_FORMAT + HDR_INFO + TID + OWN_MAC

| 字段 | 位域 (MT6639) | 我们的代码 | Windows RE | 匹配? |
|------|--------------|-----------|-----------|-------|
| WLAN_IDX | [7:0] | 0 (sta_rec idx) | 0 | ✅ |
| HDR_INFO | [12:8] | 24 (bytes) | 24 (bytes) | ✅ |
| HDR_FORMAT | [15:14] | 0b11 (802.11 ext) | 0b11 | ✅ |
| TID | [23:21] | 7 (mgmt) | 7 | ✅ |
| BIT(25) | [25] | 1 (802.11 mode) | 1 | ✅ |
| OWN_MAC | [30:25] | 0 | 0 | ✅ |

DW1 最终值: `0x02E0D800` (for WLAN_IDX=0, OWN_MAC=0)

**注意**: `mt7927_pci.h` 的 DW1 宏也是错误的 (基于 mt7925 layout):
```c
// 当前 (错误):
#define MT_TXD1_HDR_INFO     GENMASK(20, 16)  // mt7925: bits[20:16]
// 正确 MT6639:
// HDR_INFO = bits[12:8]
```

同样，SF 路径使用 raw hex 绕开了宏，实际值正确。

**结论: DW1 匹配 ✅**

---

### 1.3 DW2 — Fixed Rate Flags

| 字段 | 我们的代码 | Windows RE (full_txd_dma_path.md) | Windows RE (dw0_dw1_precise.md) | 匹配? |
|------|-----------|----------------------------------|--------------------------------|-------|
| DW2 | **0x00000000** | **0xA000000B** | 条件性 (取决于 param_3+0x4c 标志) | ⚠️ **可能不匹配** |

Windows RE 汇编:
```asm
OR dword ptr [RBX + 0x8], 0xa0000000   ; bits 31+29 = fixed rate flags
```

- `bit31` = FR_RATE (Force Rate)
- `bit29` = LDPC (或另一个固定速率控制位)
- `bits[3:0]` = 0xB (可能是 antenna/spatial stream 设置)

**两个 RE 文档存在冲突**:
- `full_txd_dma_path.md` Section 10 明确列出 auth 帧 DW2=0xA000000B
- `dw0_dw1_precise.md` 指出 DW2 是条件性的，取决于 `param_3+0x4c` 标志值

**结论: DW2 可能不匹配 ⚠️** — 我们设 0，Windows 可能设 0xA000000B

---

### 1.4 DW3 — REM_TX_COUNT

| 字段 | 我们的代码 | Windows RE | 匹配? |
|------|-----------|-----------|-------|
| REM_TX_COUNT | 30 (bits[15:11]) | 未明确 RE | ❓ |
| 其余 DW3 bits | 0 | 未明确 RE | ❓ |

**结论: DW3 不确定 ❓** — REM_TX_COUNT=30 是合理值，但未从 Windows 确认

---

### 1.5 DW4

| 字段 | 我们的代码 | Windows RE | 匹配? |
|------|-----------|-----------|-------|
| DW4 | 0 | 0 | ✅ |

**结论: DW4 匹配 ✅**

---

### 1.6 DW5 — Power Offset / Timing

| 字段 | 我们的代码 | Windows RE | 匹配? |
|------|-----------|-----------|-------|
| DW5 | **0x00000000** | **BTS bit9 / OR 0x600 / byte from [RDI+0xf]** | ⚠️ **不匹配** |

Windows RE 汇编:
```asm
BTS dword ptr [RBX + 0x14], 9     ; bit 9 = 1
OR  dword ptr [RBX + 0x14], 0x600  ; bits [10:9] = 0b11
; 另外还从 [RDI+0xf] 读一个 byte 写入
```

**结论: DW5 不匹配 ⚠️** — Windows 至少设置了 bits[10:9]，我们是 0

---

### 1.7 DW6 — TX_RATE

| 字段 | 我们的代码 | Windows RE (full_txd_dma_path.md) | Windows RE (dw0_dw1_precise.md) | 匹配? |
|------|-----------|----------------------------------|--------------------------------|-------|
| DW6 | **0x00000000** | **0x004B0000** | 条件性 | ⚠️ **可能不匹配** |

Windows RE 汇编:
```asm
AND EAX, 0x7e00ffff    ; 清除 bits[22:16]
OR  EAX, 0x4b0000      ; TX_RATE = 0x4B (OFDM 6Mbps)
```

- `bits[22:16]` = TX_RATE = 0x4B = OFDM 6Mbps

**两个 RE 文档冲突**: `full_txd_dma_path.md` 说 auth DW6=0x004B0000, `dw0_dw1_precise.md` 说条件性。

**结论: DW6 可能不匹配 ⚠️** — 如果 DW2 设了 fixed rate，DW6 必须提供速率值

---

### 1.8 DW7 — TXD_LENGTH + Frame Type/Subtype

| 字段 | 我们的代码 | Windows RE | 匹配? |
|------|-----------|-----------|-------|
| TXD_LENGTH (bits[31:30]) | 1 (= 1 page) | 1 (BTS bit30) | ✅ |
| Frame type bits | 0x000B0000 | 未完全 RE | ❓ |
| 完整 DW7 值 | **0x400B0000** | **不确定** | ⚠️ |

Windows RE 汇编确认:
```asm
BTS dword ptr [RBX + 0x1c], 0x1e    ; bit 30 = 1 → TXD_LENGTH=1
```

`full_txd_dma_path.md` Section 10 说 DW7=0x00000000 (不含 TXD_LENGTH?)，但 Windows 汇编明确设置了 bit30。

**结论: DW7 部分匹配 ⚠️** — TXD_LENGTH=1 正确，frame type bits 未确认

---

### 1.9 TXD 总结表

| DW | 我们的值 | Windows 值 | 状态 |
|----|---------|-----------|------|
| DW0 | `0x84000000 \| len` | `0x84000000 \| len` | ✅ 匹配 |
| DW1 | `0x02E0D800` | `0x02E0D800` | ✅ 匹配 |
| DW2 | `0x00000000` | `0xA000000B` (可能) | ⚠️ **高度可疑** |
| DW3 | `REM_TX_COUNT=30` | 未确认 | ❓ 未知 |
| DW4 | `0x00000000` | `0x00000000` | ✅ 匹配 |
| DW5 | `0x00000000` | `≥ 0x00000600` | ⚠️ **不匹配** |
| DW6 | `0x00000000` | `0x004B0000` (可能) | ⚠️ **高度可疑** |
| DW7 | `0x400B0000` | `0x40000000` + ? | ⚠️ **部分匹配** |

---

## 2. DMA 描述符对比

### 2.1 Ring 2 DMA 描述符格式 (16 bytes per descriptor)

| 字段 | Offset | 我们的代码 | Windows RE | 匹配? |
|------|--------|-----------|-----------|-------|
| buf0 (addr_low) | +0x00 | `lower_32_bits(dma_addr)` | `*(param_2+0x3c)` | ✅ |
| ctrl (len+flags) | +0x04 | `len << 0 \| LAST_SEC0` | `(len << 16 \| bit30)` for mgmt | ⚠️ **见下** |
| buf1 (addr_high) | +0x08 | 0 | `*(param_2+0x40)` (通常0) | ✅ |
| info | +0x0C | 0 | 0 | ✅ |

### 2.2 DMA ctrl (DW1) 格式差异

**我们的代码** (`mt7927_dma.c:507-509`):
```c
ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, total_len) | MT_DMA_CTL_LAST_SEC0;
// MT_DMA_CTL_SD_LEN0 = GENMASK(29, 16)
// MT_DMA_CTL_LAST_SEC0 = BIT(30)
// 结果: (total_len << 16) | BIT(30)
```

**Windows RE** (`N6PciTxSendPkt`, mgmt path):
```c
uVar8 = (*(int *)(param_2 + 0x68) << 0x10 ^ puVar4[1]) & 0x3fff0000U ^ puVar4[1];
puVar4[1] = uVar8 & 0x7fffffff | 0x40000000;  // bit30=1, bit31=0
```

分析:
- `0x3fff0000` = GENMASK(29, 16) — 14-bit length in bits[29:16] ✅
- `0x40000000` = BIT(30) = LAST_SEC0 ✅
- `0x7fffffff` = ~BIT(31) — 确保 bit31 (BURST) = 0 ✅

**结论: DMA 描述符格式匹配 ✅**

---

### 2.3 DMA 地址位宽

| 项目 | 我们的代码 | Windows | 匹配? |
|------|-----------|---------|-------|
| DMA mask | `DMA_BIT_MASK(32)` | 32-bit PCI | ✅ |
| buf1 (high addr) | 0 | 0 (32-bit 够用) | ✅ |

**结论: DMA 地址匹配 ✅**

---

## 3. Ring 2 DMA 初始化流程对比

### 3.1 初始化时序

| 步骤 | 我们的代码 | Windows | 匹配? |
|------|-----------|---------|-------|
| 1. Ring 2 在 PostFwDownloadInit **之后**初始化 | ✅ (`mt7927_dma_init_data_rings()` 在 PostFwDownloadInit 完成后) | ✅ (Windows 同样) | ✅ |
| 2. 写 BASE 寄存器 | `0xd4320` | `0xd4320` | ✅ |
| 3. 写 CNT 寄存器 | 256 | 256 (Windows default) | ✅ |
| 4. 写 CIDX | 0 | 0 | ✅ |

### 3.2 Ring 2 寄存器

| 寄存器 | 偏移 | 我们设的值 | 备注 |
|--------|------|-----------|------|
| TX_RING2_BASE | 0xd4320 | ring->desc_dma | DMA 物理地址 |
| TX_RING2_CNT | 0xd4324 | 256 | 描述符数量 |
| TX_RING2_CIDX | 0xd4328 | 0 | CPU index |
| TX_RING2_DIDX | 0xd432c | (HW writes) | DMA index |

### 3.3 缺失的初始化步骤

| 步骤 | 我们 | Windows | 差距 |
|------|------|---------|------|
| Ring 2 prefetch 配置 | Per-ring EXT_CTRL 已配置 | Packed + Per-ring 都有 | ⚠️ 见 Section 4 |
| Ring 2 中断使能 | BIT(6) 在 INT_MASK 中 | 同 | ✅ |
| HIF_CTRL 命令 (启用 TX/RX) | **未发送** | mt7925 发送 | ⚠️ 见 Section 8 |

**结论: Ring 2 初始化基本匹配，但可能缺少 HIF_CTRL 命令**

---

## 4. Packed Prefetch vs Per-ring EXT_CTRL

### 4.1 Packed Prefetch (0xd70f0-0xd70fc)

| 寄存器 | 偏移 | 值 | 编码的 Ring |
|--------|------|----|------------|
| CFG0 | 0xd70f0 | 0x00660077 | TX0, TX15 (从 Windows 复制) |
| CFG1 | 0xd70f4 | 0x00001100 | TX16, RX4 |
| CFG2 | 0xd70f8 | 0x0030004f | RX6, RX7 |
| CFG3 | 0xd70fc | 0x00542200 | ? |

**问题: Packed prefetch 值是在 PostFwDownloadInit 期间 (`mt7927_wpdma_config`) 写入的，这是在 data rings (Ring 0, Ring 2) 初始化之前。**

Packed prefetch 编码的 ring 列表: TX0, TX15, TX16, RX4, RX6, RX7
- **Ring 2 (管理帧) 不在 packed prefetch 列表中!**
- 但这可能是正确的 — data rings 在后面初始化，Windows 可能也是这个顺序

### 4.2 Per-ring EXT_CTRL

| Ring | EXT_CTRL 偏移 | Prefetch 值 | 我们配置? |
|------|--------------|------------|----------|
| TX Ring 0 | 计算值 | `PREFETCH(0x0240, 0x4)` | ✅ |
| TX Ring 2 | 计算值 | `PREFETCH(0x02C0, 0x4)` | ✅ |
| TX Ring 15 | 计算值 | `PREFETCH(0x0040, 0x4)` | ✅ |
| TX Ring 16 | 计算值 | `PREFETCH(0x00C0, 0x4)` | ✅ |
| RX Ring 4 | 计算值 | `PREFETCH(0x0340, 0x10)` | ✅ |
| RX Ring 6 | 计算值 | `PREFETCH(0x0440, 0x10)` | ✅ |
| RX Ring 7 | 计算值 | `PREFETCH(0x0540, 0x4)` | ✅ |

Ring 2 per-ring EXT_CTRL 已配置: `PREFETCH(0x02C0, 0x4)` — base offset=0x02C0, count=4

### 4.3 分析

Per-ring EXT_CTRL 为 Ring 2 提供了 prefetch 配置。Packed prefetch 和 per-ring EXT_CTRL 是"互补的" (代码注释说 "complementary")。

**可能的问题**: Packed prefetch 不包含 Ring 2，这可能导致 DMA 引擎不知道如何 prefetch Ring 2 的描述符。但 per-ring EXT_CTRL 应该足够。

**结论: Prefetch 配置可能足够 ⚠️** — Ring 2 有 per-ring 但无 packed prefetch，不确定是否会导致问题

---

## 5. DMASHDL 配置对比

### 5.1 DMASHDL 寄存器

| 寄存器 | 偏移 | 我们的值 | 功能 |
|--------|------|---------|------|
| SW_CONTROL | 0xd6004 | keep BIT(28) ON | **BYPASS 模式** |
| QMAP0 | 0xd6010 | 0x21112000 | Q0-2→G0, Q3→G2, Q4-6→G1, Q7→G2 |
| QMAP1 | 0xd6014 | 0x00000000 | Q8-15 → G0 |
| Optional control | 0xd6050 | 0x00040007 | GUP_ACT_MAP bits[2:0]=0x7 (Ring 0,1,2) |

### 5.2 BYPASS 模式分析

**关键**: DMASHDL SW_CONTROL BIT(28) = BYPASS 一直保持 ON。

这意味着:
- Queue mapping (QMAP0/QMAP1) 配置了但被 bypass 忽略
- Group quota 不生效
- DMA scheduler 不参与 TX 调度

**但是**: Windows 驱动也 bypass 了 DMASHDL (根据观察)。GLO_CFG BIT(9) (FWDL bypass) 在 PostFwDownloadInit 中正确清除了。

**两种不同的 bypass**:
1. `GLO_CFG BIT(9)` = FWDL bypass — 阻止所有非固件下载 TX → **已正确清除** ✅
2. `DMASHDL SW_CONTROL BIT(28)` = DMASHDL bypass — 跳过 DMA 调度 → **保持 ON** (可能正确)

### 5.3 GUP_ACT_MAP

`0x00040007`:
- bits[2:0] = 0x7 → Ring 0, Ring 1, Ring 2 active
- bit[18] = 1 → 未知功能

Ring 2 在 GUP_ACT_MAP 中被标记为 active ✅

**结论: DMASHDL 配置可能正确 ✅** — BYPASS 模式与 Windows 一致

---

## 6. N6PciTxSendPkt 完整路径对比

### 6.1 Windows 路径 (N6PciTxSendPkt @ 0x14005d1a4)

```
N6PciTxSendPkt(dev, pkt_info, pkt_type)
  ├── pkt_type: 0/1=data, >=2=mgmt
  ├── if mgmt (pkt_type>=2):
  │   ├── ring_idx = 2 (hardcoded)
  │   ├── FUN_14000cc44(dev, ring_idx, ?, 0xb, ?)  // 资源检查, 第4参数=0xb
  │   │   └── 检查 ≥11 个空闲描述符
  │   ├── FUN_1400359cc(dev, pkt_info, ring_idx)    // mgmt 队列入队
  │   ├── 构建 DMA descriptor (见 Section 2)
  │   └── FUN_140009a18(dev, ring_idx)              // ring kick
  └── if data (pkt_type<2):
      └── ... (不同路径)
```

### 6.2 对比

| 步骤 | 我们的代码 | Windows | 差距 |
|------|-----------|---------|------|
| Ring 选择 | Ring 2 (hardcoded for mgmt) | Ring 2 (hardcoded) | ✅ |
| 空闲描述符检查 | **1 个** (`ring->head != ring->tail-1`) | **11 个** (`FUN_14000cc44` 第4参数=0xb) | ⚠️ 差异但不致命 |
| TXD 构建 | `mt7927_mac_write_txwi_mgmt_sf()` | `XmitWriteTxDv1` | ⚠️ 见 Section 1 |
| DMA descriptor 写入 | 直接写 4 DWORDs | 通过函数写 | ✅ 格式匹配 |
| Ring kick | `writel(ring->head, CIDX reg)` | `FUN_140009a18` | ✅ |
| mgmt 队列入队 | 无 (直接写 ring) | `FUN_1400359cc` | ⚠️ 可能有额外处理 |

### 6.3 FUN_14000cc44 的第4参数

- mgmt: `0xb` = 11 → 检查 11 个空闲描述符
- data: `0x1` = 1 → 检查 1 个空闲描述符

为什么 mgmt 需要 11 个? 可能是因为:
1. Windows 可能将大管理帧拆分为多个描述符
2. 或者是保守的安全余量
3. 或者 ring wrap-around 保护

**我们只检查 1 个，不太可能导致固件静默**，因为 DMA 消费成功说明描述符格式正确。

### 6.4 FUN_1400359cc (mgmt 队列入队)

这个函数在 Windows 中处理 mgmt 帧的队列管理。我们没有等效实现 — 直接写 ring。

**这可能涉及**:
- 帧的序号 (sequence number) 分配
- 帧的优先级标记
- 某些固件状态的更新

但因为我们设了 SN_EN=1 (DW0 bit31)，固件应该自动分配序号。

**结论: N6PciTxSendPkt 路径基本匹配，主要差异在 TXD 内容**

---

## 7. NAPI TX Complete 处理对比

### 7.1 我们的 TX complete 路径

```
mt7927_irq_handler()
  → INT_STA & MT_INT_TX_DONE_MGMT (BIT(6))
  → napi_schedule(&dev->tx_napi)
    → mt7927_poll_tx()
      → mt7927_tx_complete_sf(ring2)
        → 读 DIDX，释放已消费的描述符
        → dev_kfree_skb_any(skb)
```

### 7.2 当前问题

**固件完全静默 = 无中断触发**:
- 不是 TX complete 处理有问题
- 是固件根本没有处理帧 → 不会产生 TX_DONE 中断
- TX_DONE 中断使能 (BIT(6)) 已在 INT_MASK 中 ✅

### 7.3 手动 TX complete

我们的 `mt7927_tx_complete_sf()` 通过轮询 DIDX 前进来释放描述符。当 DIDX 前进但固件静默时:
- DMA 引擎已将数据从 ring 读出 ✅
- 但数据可能被丢弃 (TXD 格式错误 → 固件内部丢帧)
- 或者数据正确但固件不知道如何路由 (缺少 MCU 配置)

**结论: TX complete 处理不是问题所在，问题在上游 (TXD 或 MCU 配置)**

---

## 8. 缺失的 MCU 命令

### 8.1 HIF_CTRL 命令

| 项目 | 我们 | mt7925 参考 | 差距 |
|------|------|-----------|------|
| HIF_CTRL (启用 TX/RX) | **未发送** | 在 init 后发送 | ⚠️ **缺失** |

mt7925 中的 HIF_CTRL:
```c
// mt7925 发送 HIF_CTRL 命令通知固件:
// "HOST DMA 已准备好，可以开始 TX/RX 数据帧处理"
```

**如果固件等待 HIF_CTRL 才开始处理 TX ring 上的数据帧**，那么即使 DMA 消费了数据，固件也不会处理。

**但是**:
- 我们的扫描工作正常 (RX 帧处理正常)
- MCU 命令通过 Ring 15/16 工作正常
- 只有 Ring 0/2 上的数据/管理帧有问题

这恰好符合 HIF_CTRL 的作用 — 它可能只影响数据/管理帧处理，不影响 MCU 命令。

**注意**: HIF_CTRL 是 mt7925 (不同芯片) 的实现，**必须从 Windows RE 确认 MT6639 是否需要**。

### 8.2 BSS_INFO 缺失的 TLV

| TLV | 我们发? | MT6639 发? | 可能影响 |
|-----|---------|-----------|---------|
| BASIC | ✅ | ✅ | - |
| RLM | ✅ | ✅ | - |
| MLD | ✅ | ✅ | - |
| **RATE** | ❌ | ✅ | **固件不知道用什么速率发帧** |
| PROTECT | ❌ | ✅ | CTS/RTS 保护 |
| IFS_TIME | ❌ | ✅ | 帧间距 |
| SEC | ❌ | ✅ | 安全设置 |
| QBSS | ❌ | ✅ | QoS |
| SAP | ❌ | ✅ | SoftAP |
| P2P | ❌ | ✅ | P2P |
| HE | ❌ | ✅ | HE 参数 |
| BSS_COLOR | ❌ | ✅ | BSS Color |

**RATE TLV** 可能尤其关键 — 如果固件不知道 BSS 的基本速率集 (basic rate set)，它可能无法选择正确的 TX 速率。

### 8.3 STA_REC 缺失的 TLV

| TLV | 我们发? | MT6639 发? | 可能影响 |
|-----|---------|-----------|---------|
| BASIC | ✅ | ✅ | - |
| RA (Rate Adaptation) | ✅ | ✅ | - |
| STATE | ✅ | ✅ | - |
| PHY | ✅ | ✅ | - |
| HDR_TRANS | ✅ | ❌ | 多余? |
| HT_INFO | ❌ | ✅ | HT 参数 |
| VHT_INFO | ❌ | ✅ | VHT 参数 |
| HE_BASIC | ❌ | ✅ | HE 参数 |
| HE_6G_CAP | ❌ | ✅ | 6GHz |
| BA_OFFLOAD | ❌ | ✅ | Block Ack |
| UAPSD | ❌ | ✅ | 省电 |

---

## 9. 差异列表 (按可能性排序)

以下按"导致固件静默"的可能性从高到低排序:

### 🔴 高可能性 (很可能是根因)

| # | 差异 | 描述 | 原因推理 |
|---|------|------|---------|
| **1** | **TXD DW2 = 0 vs 0xA000000B** | 我们设 DW2=0，Windows 设 fixed rate 标志 0xA000000B | 如果固件要求 DW2 指明速率控制方式，DW2=0 可能导致固件无法确定如何发射帧 → 静默丢弃 |
| **2** | **TXD DW6 = 0 vs 0x004B0000** | 我们设 DW6=0，Windows 设 TX_RATE=0x4B (OFDM 6Mbps) | DW2 说 fixed rate 但 DW6 没有速率 → 固件找不到发射速率 → 静默丢弃 |
| **3** | **HIF_CTRL 命令未发送** | 可能需要通知固件 "HOST 数据 TX 已就绪" | DMA 可以消费数据 (硬件层)，但固件不处理 (软件层) → 完美解释 "DMA 消费但固件静默" 的现象 |

### 🟡 中等可能性

| # | 差异 | 描述 | 原因推理 |
|---|------|------|---------|
| **4** | **TXD DW5 = 0 vs ≥0x600** | Windows 设 bits[10:9]，我们是 0 | DW5 可能包含 TX 功率或定时参数，缺失可能导致固件拒绝帧 |
| **5** | **BSS_INFO RATE TLV 缺失** | 固件不知道 BSS 的基本速率集 | 即使 TXD 指定了 fixed rate，固件可能检查 BSS 速率配置是否允许该速率 |
| **6** | **DW7 frame type bits 不确定** | 我们的 0x400B0000 vs Windows 的 0x40000000+? | frame type bits 如果错误，固件可能不知道帧类型 |

### 🟢 低可能性

| # | 差异 | 描述 | 原因推理 |
|---|------|------|---------|
| **7** | **Packed prefetch 不含 Ring 2** | Ring 2 只有 per-ring EXT_CTRL | per-ring EXT_CTRL 应该足够，两者互补 |
| **8** | **空闲描述符检查: 1 vs 11** | 我们只检查 1 个空闲 | DMA 消费成功说明描述符数量不是问题 |
| **9** | **FUN_1400359cc (mgmt 队列入队) 缺失** | Windows 有额外的 mgmt 帧处理 | 可能只是 Windows 的内部队列管理，不影响硬件 |
| **10** | **DMASHDL BYPASS 模式** | 保持 ON | Windows 也是 bypass，应该正确 |

---

## 10. 修复建议

### 第一优先级: TXD DW2 + DW6 (Fixed Rate)

**需要先从 Ghidra RE 确认**: auth 帧的 DW2 到底是 0 还是 0xA000000B。两份 RE 文档有冲突。

如果确认需要 fixed rate:
```c
// mt7927_mac.c: mt7927_mac_write_txwi_mgmt_sf()
// DW2: fixed rate flags
txwi[2] = cpu_to_le32(0xA000000B);  // bit31=FR_RATE, bit29=?, bits[3:0]=0xB

// DW6: TX_RATE = OFDM 6Mbps
txwi[6] = cpu_to_le32(0x004B0000);  // bits[22:16] = 0x4B
```

### 第二优先级: HIF_CTRL 命令

**需要从 Windows RE 确认 MT6639 是否发送类似命令**。在 Windows .sys 中搜索 PostFwDownloadInit 末尾是否有通知固件 "HOST TX 就绪" 的命令。

如果需要:
```c
// 在 PostFwDownloadInit 完成 + data rings 初始化后:
mt7927_mcu_hif_ctrl(dev, true);  // 通知固件 HOST TX/RX 已准备好
```

### 第三优先级: TXD DW5

从 Windows RE 确认 DW5 bits[10:9] 的含义和值。

### 第四优先级: BSS_INFO RATE TLV

添加 RATE TLV 到 BSS_INFO 命令，告诉固件基本速率集。

### 第五优先级: 更多 RE 确认

1. 在 Ghidra 中找到 `XmitWriteTxDv1` 的调用者 `FUN_1401a2c8c`，确认 `param_3+0x4c` 标志对 auth 帧是什么值
2. 确认 DW7 的 frame type bits 是否正确
3. 搜索 Windows 驱动中是否有 PostFwDownloadInit 后的额外 HIF/DMA 启用步骤

---

## 附录 A: 文件位置参考

| 代码/文档 | 路径 | 关键行号 |
|-----------|------|---------|
| TXD SF 构建 | `src/mt7927_mac.c` | 339-453 |
| TXD CT 构建 | `src/mt7927_mac.c` | 133-315 |
| DMA 描述符写入 | `src/mt7927_dma.c` | 468-523 |
| Ring 2 初始化 | `src/mt7927_dma.c` | 744-795 |
| TX complete SF | `src/mt7927_dma.c` | 382-383 |
| DMASHDL init | `src/mt7927_pci.c` | 1454-1519 |
| Prefetch config | `src/mt7927_pci.c` | 450-474 |
| DMA mask | `src/mt7927_pci.c` | 3754 |
| TXD DW0 宏 (错误) | `src/mt7927_pci.h` | grep PKT_FMT |
| Windows TXD RE | `docs/win_re_full_txd_dma_path.md` | Section 10 |
| Windows DW0/DW1 RE | `docs/win_re_txd_dw0_dw1_precise.md` | 全文 |

## 附录 B: 关键 Ghidra 地址

| 函数 | 地址 | 用途 |
|------|------|------|
| XmitWriteTxDv1 | 0x1401a2ca4 | TXD DW0-DW7 构建 |
| FUN_1401a2c8c | (caller) | XmitWriteTxDv1 的调用者，设置参数 |
| N6PciTxSendPkt | 0x14005d1a4 | DMA 描述符写入 + ring kick |
| FUN_14000cc44 | (内部) | 资源检查 (空闲描述符) |
| FUN_1400359cc | (内部) | mgmt 队列入队 |
| FUN_140009a18 | (内部) | ring kick |
