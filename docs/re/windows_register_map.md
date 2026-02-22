# Windows Driver Register Map (mtkwecx.sys)

This document summarizes register addresses observed in the Windows driver `mtkwecx.sys` from the driver bundle
`DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R`.

Sources (local analysis outputs):
- `/tmp/mtkwecx_fw_dma_strings.txt`
- `/tmp/mtkwecx_fw_dma_consts.txt`

Notes:
- Addresses below are the **absolute** values used in the Windows driver (e.g. `0x7c024208`).
- These appear to be **BAR2-relative** in Linux terms, likely `BAR2_BASE + offset`, where `BAR2_BASE` is
  `0x7c024000` or `0x7c027000` in the Windows mapping. You must confirm the Linux BAR mapping before using them.
- Function names are Ghidra-generated unless noted by debug strings (e.g. `MT7925WpdmaConfig`).

---

## 1) WPDMA/WFDMA config and MSI setup

### MT7925 WPDMA config (from debug string `MT7925WpdmaConfig`)
Function: `FUN_1401e6550`

Observed register access:
- `0x7c024208` (read/modify/write)  
  - If `param_2 != 0`: `WPDMA_GLO_CFG |= 0x4000005`
- `0x7c0242b4` (read/modify/write)  
  - `WPDMA_GLO_CFG_EXT1 |= 0x10000000`

When `param_2 != 0` and a runtime flag is set (`*(param_1 + 0x14682a5) != 0`), the driver writes:
- `0x7c027030` (`WFDMA_HOST_CONFIG`) read then write back
- `0x7c0270f0 = 0x660077`
- `0x7c0270f4 = 0x1100`
- `0x7c0270f8 = 0x30004f`
- `0x7c0270fc = 0x542200`

### MT6639 WPDMA config (for comparison)
Function: `FUN_1401d8290` (string `MT6639WpdmaConfig`)

Observed register access:
- `0x7c024208` (read/modify/write)  
  - If `param_2 != 0`: `WPDMA_GLO_CFG |= 0x5`
- `0x7c0242b4` (read/modify/write)  
  - `WPDMA_GLO_CFG_EXT1 |= 0x10000000`
- Same `WFDMA_HOST_CONFIG` and `0x7c0270f0/0xf4/0xf8/0xfc` writes as MT7925 when enabled

**Key difference:** MT7925 sets `0x4000000` in `WPDMA_GLO_CFG` (bit 22). MT6639 does not.

---

## 2) WPDMA ring/init style writes

Function: `FUN_1401e6430`

Observed register access:
- `0x7c024208` (read, clear bit 15): `val & 0xffff7fff` then write back
- Burst writes to ranges:
  - `0x7c024680 .. 0x7c02468c` (step 4)
  - `0x7c024600 .. 0x7c02460c` (step 4)
  - `0x7c02463c .. 0x7c024640` (step 4)
- Other writes:
  - `0x7c024298 = 0xf00`
  - `0x7c02429c = 0x7f00`
  - `0x7c02420c = 0xffffffff`
  - `0x7c024280 = 0xffffffff`

This looks like ring base/index reset and interrupt mask setup.

---

## 3) WFDMA/WPDMA/PCIe diagnostic reads (debug dump)

Function: `FUN_1401e7630` (string `connac3x_show_pcie_wfdma_info`)

Registers read (no writes seen in this function):
- `0x7c024204` HOST_INT_ENA
- `0x7c024100` CONN_HIF_RST
- `0x7c024200` HOST_INT_STA
- `0x7c024238` HOST_INT_STA_EXT
- `0x7c02423c` HOST_INT_ENA_EXT
- `0x7c024284` WPDMA_INFO
- `0x7c024288` WPDMA_INFO_EXT
- `0x7c0242b0` WPDMA_GLO_CFG_EXT0
- `0x7c0242b4` WPDMA_GLO_CFG_EXT1
- `0x7c0242b8` WPDMA_GLO_CFG_EXT2
- `0x7c0242e8` HOST_PER_DLY_INT_CFG
- `0x7c0242f0` WPDMA_PRI_DLY_INT_CFG0
- `0x7c0242f4` WPDMA_PRI_DLY_INT_CFG1
- `0x7c0242f8` WPDMA_PRI_DLY_INT_CFG2
- `0x7c0242fc` WPDMA_PRI_DLY_INT_CFG3
- `0x7c024150` HOST_TX_INT_PCIE_SEL
- `0x7c024154` HOST_RX_INT_PCIE_SEL
- `0x7c024298` WPDMA_INT_RX_PRI_SEL
- `0x7c02429c` WPDMA_INT_TX_PRI_SEL
- `0x7c02702c` WFDMA_MSI_CONFIG
- `0x7c027030` WFDMA_HOST_CONFIG
- `0x7c0270ac` WFDMA_MD_INT_LUMP_SEL
- `0x7c0270e8` WFDMA_DLY_IDX_CFG_0
- `0x7c0270ec` WFDMA_DLY_IDX_CFG_1
- `0x7c0270f0` MSI_INT_CFG0
- `0x7c0270f4` MSI_INT_CFG1
- `0x7c0270f8` MSI_INT_CFG2
- `0x7c0270fc` MSI_INT_CFG3
- `0x740311a8` AXI_PCIE_IF_CTRL

---

## 4) WFDMA SLPPROT / HIF busy reads

Function: `FUN_1401e183c`
- `0x7c027044` WFDMA_HIF_BUSY
- `0x7c027050` WFDMA_AXI_SLPPROT_CTRL

Function: `FUN_1401ee90c`
- `0x7c027044` WFDMA_HIF_BUSY
- `0x7c027050` WFDMA_AXI_SLPPROT_CTRL
- `0x7c027078` WFDMA_AXI_SLPPROT0_CTRL
- `0x7c02707c` WFDMA_AXI_SLPPROT1_CTRL

---

## 5) Misc/interrupt debug

Function: `FUN_1401dcadc` (string `connac3x_show_wfdma_interrupt_info_mt7925`)

Registers read:
- `0x7c027010` Global INT STA
- `0x7c027014` (paired with INT STA)
- Per-DMA INT STA computed from `0x7c024000 + 0x200` increments

Also writes/read a number of PCIe debug registers:
- `0x74030168`, `0x74030164`, `0x7403002c`
- `0x74030188 .. 0x7403018c`
- `0x740310e0 .. 0x740310f4`

---

## 6) Firmware pre-download init

Function: `FUN_1401e5d90` (string `MT7925PreFirmwareDownloadInit`)

No obvious direct register writes in the decompiled snippet, but it calls other functions that may do
hardware setup or state gating prior to firmware download.

---

## Summary of key WPDMA/WFDMA addresses

WPDMA core (likely BAR2 + 0x0200 range):
- `0x7c024208` WPDMA_GLO_CFG
- `0x7c0242b4` WPDMA_GLO_CFG_EXT1
- `0x7c0242b0` WPDMA_GLO_CFG_EXT0
- `0x7c0242b8` WPDMA_GLO_CFG_EXT2
- `0x7c024298` WPDMA_INT_RX_PRI_SEL
- `0x7c02429c` WPDMA_INT_TX_PRI_SEL
- `0x7c0242f0..0x7c0242fc` WPDMA_PRI_DLY_INT_CFG0..3
- `0x7c024150` HOST_TX_INT_PCIE_SEL
- `0x7c024154` HOST_RX_INT_PCIE_SEL

WFDMA / MSI / host config (likely BAR2 + 0x7000 range):
- `0x7c02702c` WFDMA_MSI_CONFIG
- `0x7c027030` WFDMA_HOST_CONFIG
- `0x7c0270f0..0x7c0270fc` MSI_INT_CFG0..3
- `0x7c0270e8..0x7c0270ec` WFDMA_DLY_IDX_CFG_0..1
- `0x7c0270ac` WFDMA_MD_INT_LUMP_SEL
- `0x7c027044` WFDMA_HIF_BUSY
- `0x7c027050` WFDMA_AXI_SLPPROT_CTRL
- `0x7c027078` WFDMA_AXI_SLPPROT0_CTRL
- `0x7c02707c` WFDMA_AXI_SLPPROT1_CTRL

PCIe/AXI debug:
- `0x740311a8` AXI_PCIE_IF_CTRL
- `0x74030164`, `0x74030168`, `0x7403002c`
- `0x74030188..0x7403018c`
- `0x740310e0..0x740310f4`

---

## How to use this document

1. Determine the Linux BAR2 base for MT7927 (from `lspci -vv` or your driver mapping).
2. Convert absolute addresses to offsets, likely:
   - `offset = address - 0x7c024000` for WPDMA region
   - `offset = address - 0x7c027000` for WFDMA region
3. Compare with your Linux test module to verify which writes are failing (WPDMA_GLO_CFG, WPDMA_GLO_CFG_EXT1,
   WFDMA_HOST_CONFIG, MSI_INT_CFGx).

