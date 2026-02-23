# mt76 (Linux) vs Windows mt7927 (mtkwecx.sys) — DMA/WFDMA/WPDMA comparison

This document compares the mt76 Linux driver (now in `mt76/`) with Windows `mtkwecx.sys` (mt7927) for
DMA bring‑up and register programming. It focuses on WFDMA/WPDMA/PCIe‑related registers.

Sources:
- Linux: `mt76/mt792x_dma.c`, `mt76/mt792x_regs.h`
- Windows: `docs/windows_register_map.md` (from Ghidra analysis)

---

## 1) Address mapping: Windows absolute vs mt76 symbols

`mt76/mt792x_regs.h` defines `MT_UWFDMA0(ofs) = 0x7c024000 + ofs` and
`MT_WFDMA_HOST_CONFIG = 0x7c027030`. This matches Windows absolute addresses.

| Windows absolute | mt76 symbol | Location / meaning |
|---|---|---|
| `0x7c024208` | `MT_UWFDMA0_GLO_CFG` | WFDMA0 global config (aka WPDMA_GLO_CFG) |
| `0x7c0242b0` | `MT_UWFDMA0_GLO_CFG_EXT0` | WFDMA0 EXT0 |
| `0x7c0242b4` | `MT_UWFDMA0_GLO_CFG_EXT1` | WFDMA0 EXT1 |
| `0x7c024298` | `MT_WFDMA0_INT_RX_PRI` | RX interrupt priority select |
| `0x7c02429c` | `MT_WFDMA0_INT_TX_PRI` | TX interrupt priority select |
| `0x7c02420c` | `MT_WFDMA0_RST_DTX_PTR` | reset TX DMA index |
| `0x7c024280` | `MT_WFDMA0_RST_DRX_PTR` | reset RX DMA index |
| `0x7c0242f0` | `MT_WFDMA0_PRI_DLY_INT_CFG0` | delay interrupt config |
| `0x7c024600..0x7c024644` | `MT_WFDMA0_TX_RING*_EXT_CTRL` | TX ring ext ctrl |
| `0x7c024680..0x7c02469c` | `MT_WFDMA0_RX_RING*_EXT_CTRL` | RX ring ext ctrl |
| `0x7c027030` | `MT_WFDMA_HOST_CONFIG` | WFDMA host config |
| `0x7c0270f0..0x7c0270fc` | (not defined in mt792x_regs.h) | MSI_INT_CFG0..3 (Windows writes fixed values) |
| `0x7c027044/050/078/07c` | (not defined in mt792x_regs.h) | WFDMA HIF busy / SLPPROT registers (Windows reads) |
| `0x740311a8` | (no symbol in mt792x_regs.h) | AXI_PCIE_IF_CTRL (Windows reads) |

---

## 2) mt76 (Linux) DMA enable sequence (mt792x)

From `mt76/mt792x_dma.c`:

1) **Prefetch config**
- Writes ring ext ctrl registers (`MT_WFDMA0_RX_RING*_EXT_CTRL`, `MT_WFDMA0_TX_RING*_EXT_CTRL`)
  with `PREFETCH(...)` values.

2) **Reset DMA indices**
- `MT_WFDMA0_RST_DTX_PTR = ~0`
- `MT_WFDMA0_RST_DRX_PTR = ~0` (mt7925 only)

3) **Delay interrupt config**
- `MT_WFDMA0_PRI_DLY_INT_CFG0 = 0`

4) **Set WFDMA0 GLO_CFG features**
- `MT_WFDMA0_GLO_CFG` ORs:
  - `TX_WB_DDONE`, `FIFO_LITTLE_ENDIAN`, `CLK_GAT_DIS`
  - `OMIT_TX_INFO`, `OMIT_RX_INFO_PFET2`
  - `DMA_SIZE=3`, `FIFO_DIS_CHECK`, `RX_WB_DDONE`
  - `CSR_DISP_BASE_PTR_CHAIN_EN`

5) **Enable TX/RX DMA**
- `MT_WFDMA0_GLO_CFG` ORs `TX_DMA_EN` + `RX_DMA_EN`

6) **mt7925‑specific extras**
- `MT_UWFDMA0_GLO_CFG_EXT1` ORs `BIT(28)`
- `MT_WFDMA0_INT_RX_PRI |= 0x0F00`
- `MT_WFDMA0_INT_TX_PRI |= 0x7F00`

---

## 3) Windows mt7927 writes (from mtkwecx.sys)

Key Windows sequence extracted from `AsicConnac3x/MT7925WpdmaConfig`:

- `0x7c024208 (WPDMA_GLO_CFG) |= 0x4000005`
- `0x7c0242b4 (WPDMA_GLO_CFG_EXT1) |= 0x10000000`
- `0x7c027030 (WFDMA_HOST_CONFIG)` read/write back
- `0x7c0270f0 = 0x660077`
- `0x7c0270f4 = 0x1100`
- `0x7c0270f8 = 0x30004f`
- `0x7c0270fc = 0x542200`
- `0x7c024298 = 0x0F00`, `0x7c02429c = 0x7F00`

Also observed ring‑init style writes:
- `0x7c024600..0x7c024641` and `0x7c024680..0x7c02468d`
- `0x7c02420c = 0xffffffff`, `0x7c024280 = 0xffffffff`

---

## 4) Direct comparison (mt76 vs Windows)

### A) Shared / matching behavior

1) **INT priority registers**
- Linux: `MT_WFDMA0_INT_RX_PRI = 0x0F00`, `MT_WFDMA0_INT_TX_PRI = 0x7F00`
- Windows: same values at `0x7c024298/0x7c02429c`

2) **GLO_CFG_EXT1 bit 28**
- Linux (mt7925): `MT_UWFDMA0_GLO_CFG_EXT1 |= BIT(28)`
- Windows: `0x7c0242b4 |= 0x10000000`

3) **Ring ext ctrl / reset index**
- Linux: writes TX/RX ring ext ctrl and `RST_DTX_PTR/RST_DRX_PTR`
- Windows: matches the same address ranges and resets to `0xffffffff`

### B) Differences that likely matter for mt7927 DMA unlock

1) **Windows sets an extra high bit in WPDMA_GLO_CFG**
- Windows: `WPDMA_GLO_CFG |= 0x4000005` (bit 22 set)
- Linux: sets feature bits (DDONE/omit/chain) + `TX_DMA_EN`/`RX_DMA_EN`
- `mt792x_regs.h` has **no definition for bit 22**, so Linux never sets it.

2) **Windows writes MSI_INT_CFG0..3 (0x7c0270f0..0x7c0270fc)**
- Linux mt76 (mt792x) **does not write these registers**.
- If mt7927 requires MSI registers pre‑programmed, this is a critical gap.

3) **Windows touches WFDMA_HOST_CONFIG (`0x7c027030`)**
- Linux mt76 defines `MT_WFDMA_HOST_CONFIG` but does **not** write it in mt792x code.

4) **Windows reads SLPPROT and HIF busy regs**
- Windows reads `0x7c027044/0x7c027050/0x7c027078/0x7c02707c`.
- Linux mt76 does not reference these addresses in mt792x code.

---

## 5) Hypothesis for mt7927 DMA lock

Based on the comparison, the following are the top suspects for why mt7927 DMA remains locked in the
Linux adaptation:

1) **Missing WPDMA_GLO_CFG bit 22** (`0x4000000`)
   - Windows enables it explicitly.
   - mt76 does not define or set it for mt792x.

2) **Missing MSI_INT_CFG programming** (`0x7c0270f0..0x7c0270fc`)
   - Windows writes fixed values, Linux doesn’t touch them.

3) **Missing WFDMA_HOST_CONFIG write** (`0x7c027030`)
   - Windows does a read/write cycle; Linux does not.

---

## 6) Files to inspect (mt76 side)

Key Linux files for deeper comparison:
- `mt76/mt792x_dma.c` — DMA enable/disable sequence and ring setup
- `mt76/mt792x_regs.h` — register addresses and bit definitions
- `mt76/dma.c` and `mt76/dma.h` — generic DMA helpers used by mt792x

---

## 7) Actionable next steps

If you want to validate this quickly on mt7927:
1. **Add a test write** for `MT_WFDMA_HOST_CONFIG` and `MSI_INT_CFG0..3` using the Windows values.
2. **Try OR‑ing `0x4000000` into `MT_UWFDMA0_GLO_CFG`** (bit 22), in addition to existing Linux bits.
3. Log whether WPDMA_GLO_CFG becomes writable and whether FW DMA progresses.

