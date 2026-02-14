# mt7925 (Linux) vs Windows mt7927 (mtkwecx.sys) register analysis

This document compares the Linux `mt7925` driver code in this repo with the Windows driver observations
from `mtkwecx.sys` (mt7927). It focuses on WFDMA/WPDMA/PCIe registers and init sequences relevant to DMA
bring-up.

Sources:
- Linux mt7925 code: `mt7925/`
- Windows register map: `docs/windows_register_map.md`

Limitations:
- `mt792x_regs.h` is not present in this workspace, so some Linux register addresses are symbolic only
  (e.g. `MT_WFDMA0_GLO_CFG`). When possible, I infer the Windows absolute addresses from the mapping in
  `mt7925/pci.c` and the Windows dump.

---

## 1) Linux mt7925: what the code actually touches

### WFDMA host IRQ and rings (from `mt7925/regs.h`)
- `MT_WFDMA0_HOST_INT_ENA = MT_WFDMA0(0x204)`
- `MT_WFDMA0_HOST_INT_DIS = MT_WFDMA0(0x22c)`
- `MT_RX_DATA_RING_BASE = MT_WFDMA0(0x500)`

### WFDMA GLO config + DMA enable/disable (from `mt7925/pci.c`)
- Poll busy bits:
  - `mt76_poll(dev, MT_WFDMA0_GLO_CFG, MT_WFDMA0_GLO_CFG_TX_DMA_BUSY | MT_WFDMA0_GLO_CFG_RX_DMA_BUSY, 0, 1000)`
- Disable DMA:
  - `mt76_clear(dev, MT_WFDMA0_GLO_CFG, MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN)`
- Enable DMA:
  - `mt76_set(dev, MT_WFDMA0_GLO_CFG, MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN)`

### PCIe interrupt enable (from `mt7925/pci.c`, `mt7925/pci_mac.c`)
- `mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0x0)`
- `mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0xff)`

### Sleep protect (from `mt7925/pci.c`)
- `mt76_rmw_field(dev, MT_HW_EMI_CTL, MT_HW_EMI_CTL_SLPPROT_EN, 1)`

### Register mapping (from `mt7925/pci.c`)
The Linux driver remaps physical address ranges into bus offsets. Relevant ranges:
- `0x7c020000` -> map `0x0d0000` (label: "CONN_INFRA, wfdma")
- `0x74030000` -> map `0x010000` (label: "PCIe MAC")

This mapping is consistent with Windows absolute addresses like `0x7c024208`
(i.e. `0x7c020000 + 0x4208`).

---

## 2) Windows mt7927: registers and writes observed

See `docs/windows_register_map.md` for the full map.
Key observations (highly relevant to DMA unlock):

- `WPDMA_GLO_CFG` at `0x7c024208` is written with `|= 0x4000005` in MT7925 path
- `WPDMA_GLO_CFG_EXT1` at `0x7c0242b4` is written with `|= 0x10000000`
- `WFDMA_HOST_CONFIG` at `0x7c027030` is read/then written
- `MSI_INT_CFGx` are explicitly written:
  - `0x7c0270f0 = 0x660077`
  - `0x7c0270f4 = 0x1100`
  - `0x7c0270f8 = 0x30004f`
  - `0x7c0270fc = 0x542200`
- WFDMA SLPPROT registers are read:
  - `0x7c027050`, `0x7c027078`, `0x7c02707c`

---

## 3) Address-level comparison (Windows absolute vs Linux symbols)

The following table maps Windows absolute addresses to inferred Linux symbols based on `mt7925/regs.h`
(and the remap table in `mt7925/pci.c`).

| Windows absolute | Inferred Linux symbol | Notes |
|---|---|---|
| `0x7c024204` | `MT_WFDMA0_HOST_INT_ENA` | Linux uses as host IRQ enable (`regs.h`). |
| `0x7c02422c` | `MT_WFDMA0_HOST_INT_DIS` | Linux uses as host IRQ disable (`regs.h`). |
| `0x7c024208` | `MT_WFDMA0_GLO_CFG` | Linux uses for DMA enable/disable; Windows sets `|= 0x4000005`. |
| `0x7c0242b4` | `MT_WFDMA0_GLO_CFG_EXT1` | Windows sets `|= 0x10000000`; Linux does not touch in mt7925 folder. |
| `0x7c024298` | `MT_WFDMA0_INT_RX_PRI_SEL` | Windows ring init writes; Linux not explicit in mt7925 folder. |
| `0x7c02429c` | `MT_WFDMA0_INT_TX_PRI_SEL` | Windows ring init writes; Linux not explicit in mt7925 folder. |
| `0x7c027030` | `WFDMA_HOST_CONFIG` | Windows writes; Linux not explicit in mt7925 folder. |
| `0x7c0270f0..0x7c0270fc` | `MSI_INT_CFG0..3` | Windows writes; Linux not explicit in mt7925 folder. |
| `0x740311a8` | `AXI_PCIE_IF_CTRL` | Windows debug dump reads; Linux has PCIe MAC remap but no explicit read in mt7925 folder. |

---

## 4) Behavioral differences that matter for DMA bring-up

### A) WPDMA_GLO_CFG bits
- **Windows mt7925 path:** `WPDMA_GLO_CFG |= 0x4000005`
  - This includes bit 22 (`0x4000000`) plus bits 0 and 2.
- **Linux mt7925 path:** uses `MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN`
  - The exact bit positions depend on `mt792x_regs.h` (not present).

**Implication:** Windows explicitly sets an extra high bit that Linux may not, which could be a key
unlock/enable bit for mt7927.

### B) WPDMA_GLO_CFG_EXT1
- **Windows:** `WPDMA_GLO_CFG_EXT1 |= 0x10000000`
- **Linux:** No explicit write in `mt7925/` folder.

**Implication:** There may be a missing EXT1 enable step in the Linux path.

### C) MSI / WFDMA_HOST_CONFIG
- **Windows:** writes `WFDMA_HOST_CONFIG` and four MSI_INT_CFG registers with fixed values.
- **Linux mt7925 folder:** no explicit write to these registers.

**Implication:** Windows configures WFDMA MSI paths earlier than Linux (or Linux does so in shared
`mt76` code not copied into this workspace). If mt7927 requires explicit MSI config to unlock DMA,
this difference is significant.

### D) SLPPROT path
- **Linux:** sets `MT_HW_EMI_CTL_SLPPROT_EN` (EMI sleep protect enable).
- **Windows:** reads WFDMA SLPPROT registers (`0x7c027050/0x78/0x7c`) but no visible writes in the
  snippet. Still, the presence of SLPPROT in debug output suggests it is part of the bring-up context.

---

## 5) What the Linux code does NOT show (in this workspace)

Because only `mt7925/` was copied, the following may exist in shared mt76/mt792x code but are not
visible here:
- Exact bit definitions for `MT_WFDMA0_GLO_CFG_*`
- Low-level WFDMA register programming in mt76 DMA helpers
- MSI setup and host config writes (if any)

If you can also copy `mt792x_regs.h` and any shared `mt76` DMA source (`../dma.h`, `../dma.c`),
I can extend the comparison and confirm whether Linux already programs the same registers.

---

## 6) Summary (actionable differences)

1. **Windows explicitly writes `WPDMA_GLO_CFG` with `0x4000005`**; Linux may be missing bit 22.
2. **Windows explicitly writes `WPDMA_GLO_CFG_EXT1 |= 0x10000000`**; Linux does not in mt7925 folder.
3. **Windows writes MSI config registers (`0x7c0270f0..0x7c0270fc`) and touches `WFDMA_HOST_CONFIG`.**
   Linux mt7925 folder doesn’t show these writes.

These three differences are the most likely to explain why Linux-side DMA enable fails on mt7927.

