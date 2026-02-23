# MT7927 Bluetooth Driver Analysis — WiFi Init Clues

**Date**: 2026-02-15
**Analyst**: agent3 (BT driver code analysis)

## Summary

Analyzed three BT driver packages for MT7927 to find CONN_INFRA register access patterns
applicable to WiFi PCIe driver development. **Key finding**: the BT driver uses
`MTK_BT_RESET_REG_CONNV3 = 0x70028610` for subsystem reset, which is in the same
CB_INFRA_RGU register block as the WiFi WFSYS reset at `0x70028600`. Our PCIe WiFi driver
already maps this as `MT_CB_INFRA_RGU_WF_SUBSYS_RST = BAR0 0x1f8600`.

## Source Material

| Driver | Type | Location | Key files |
|--------|------|----------|-----------|
| mt7927-bluetooth-main | Linux USB (patched btmtk) | `linux-driver-mediatek-mt7927-bluetooth-main/` | btmtk.h, btmtk.c, btusb.c, debug_mtk_bt_init.c |
| mt6639-bluetooth-module | Linux USB (patch file) | `linux-mediatek-mt6639-bluetooth-kernel-module-main/` | patches/mt6639-bt-6.19.patch |
| Windows BT driver | .inf + .sys binary | `DRV_Bluetooth_MTK_MT7925_27_TP_W11_64_V110430542_20250709R/` | mtkbtfilter.inf |

## 1. Register Definitions Found

### btmtk.h — UHW CR mapping (USB Hardware Control Register)

| Define | Chip Address | Purpose | BAR0 Offset (PCIe) |
|--------|-------------|---------|-------------------|
| MTK_BT_MISC | 0x70002510 | BT misc status | 0x1e2510 * |
| MTK_BT_SUBSYS_RST | 0x70002610 | BT subsystem reset (older) | 0x1e2610 * |
| MTK_BT_RESET_REG_CONNV3 | 0x70028610 | **CB_INFRA_RGU BT reset** | **0x1f8610** |
| MTK_BT_READ_DEV_ID | 0x70010200 | Device ID register | ** |
| MTK_UDMA_INT_STA_BT | 0x74000024 | USB DMA interrupt status | N/A (USB only) |
| MTK_UDMA_INT_STA_BT1 | 0x74000308 | USB DMA interrupt status 1 | N/A (USB only) |
| MTK_EP_RST_OPT | 0x74011890 | USB endpoint reset options | N/A (USB only) |
| MTK_BT_WDT_STATUS | 0x740003A0 | BT watchdog status | N/A (USB only) |
| MTK_BT_RST_DONE | 0x00000100 | BIT(8) in BT_MISC = reset done | — |

\* BAR0 mapping via bus2chip {0x70000000, 0x1e0000, 0x9000}
\*\* 0x70010200 is likely in a different bus2chip block (0x70010000 range)

### PCIe bus2chip for CB_INFRA region

From `mt7927_init_dma.c` line 80:
```
chip addr 0x70028000, bus2chip: {0x70020000, 0x1f0000, 0x10000}
```

Formula: `BAR0_offset = 0x1f0000 + (chip_addr - 0x70020000)`

| Chip Address | BAR0 Offset | Register |
|-------------|------------|----------|
| 0x70028600 | **0x1f8600** | CB_INFRA_RGU WF_SUBSYS_RST (WiFi) |
| 0x70028610 | **0x1f8610** | CB_INFRA_RGU BT reset (CONNV3) |
| 0x70026554 | 0x1f6554 | CB_INFRA_MISC0 PCIE_REMAP_WF |
| 0x70026558 | 0x1f6558 | CB_INFRA_MISC0 PCIE_REMAP_WF_BT |
| 0x70025030 | 0x1f5030 | CB_INFRA_SLP_CTRL MCU_OWN |
| 0x70025034 | 0x1f5034 | CB_INFRA_SLP_CTRL MCU_OWN_SET |

## 2. BT Subsystem Reset Sequence (for MT7927/dev_id 0x6639)

From `btmtk_usb_subsys_reset()` in btmtk.c lines 864-901:

```c
// For dev_id == 0x7925 || dev_id == 0x6639:
// Step 1: Read CB_INFRA_RGU BT reset register
err = btmtk_usb_uhw_reg_read(hdev, MTK_BT_RESET_REG_CONNV3, &val);
// val = 0x000123e0 (from USB capture)

// Step 2: Set BIT(5) — enable BT reset
val |= (1 << 5);
err = btmtk_usb_uhw_reg_write(hdev, MTK_BT_RESET_REG_CONNV3, val);

// Step 3: Configure reset — clear bits 8-15, set BIT(13)
err = btmtk_usb_uhw_reg_read(hdev, MTK_BT_RESET_REG_CONNV3, &val);
val &= 0xFFFF00FF;   // clear bits 8-15
val |= (1 << 13);    // set bit 13
err = btmtk_usb_uhw_reg_write(hdev, MTK_BT_RESET_REG_CONNV3, val);

// Step 4: USB endpoint reset (USB-specific, skip for PCIe)
err = btmtk_usb_uhw_reg_write(hdev, MTK_EP_RST_OPT, 0x00010001);

// Step 5: TRIGGER reset — set BIT(0)
err = btmtk_usb_uhw_reg_read(hdev, MTK_BT_RESET_REG_CONNV3, &val);
val |= (1 << 0);
err = btmtk_usb_uhw_reg_write(hdev, MTK_BT_RESET_REG_CONNV3, val);

// Step 6: Clear USB DMA interrupts (USB-specific, skip for PCIe)
err = btmtk_usb_uhw_reg_write(hdev, MTK_UDMA_INT_STA_BT, 0x000000FF);
err = btmtk_usb_uhw_reg_write(hdev, MTK_UDMA_INT_STA_BT1, 0x000000FF);
msleep(100);

// Step 7: Poll for reset done
err = readx_poll_timeout(btmtk_usb_reset_done, hdev, val,
    val & MTK_BT_RST_DONE, 20000, 1000000);
// btmtk_usb_reset_done reads MTK_BT_MISC (0x70002510), checks BIT(8)

// Step 8: Verify device ID
err = btmtk_usb_id_get(hdev, 0x70010200, &val);
```

### USB Capture Confirmation (debug_mtk_bt_init.c)

The Windows USB capture (cap2.pcap, Frames 79-95) confirms this sequence:

| Frame | bmRT | Register | Value (LE) | Operation |
|-------|------|----------|-----------|-----------|
| F79 | 0xDE (read) | 0x70028610 | e0 23 01 00 | Read BT_RESET_REG |
| F81 | 0x5E (write) | 0x70028610 | e0 23 01 00 | Write back (BIT(5) already set) |
| F83 | 0x5E (write) | 0x74011890 | 01 00 01 00 | EP_RST_OPT (USB) |
| F85 | 0xDE (read) | 0x70028610 | e0 23 01 00 | Verify |
| F87 | 0x5E (write) | 0x70028610 | e1 23 01 00 | **BIT(0) set = TRIGGER** |
| F89 | 0xDE (read) | 0x70002510 | 00 01 00 00 | Poll BT_MISC RST_DONE |
| F91 | 0x5E (write) | 0x74000024 | ff 00 00 00 | Clear UDMA ints (USB) |
| F93 | 0x5E (write) | 0x740003BC | 01 00 00 00 | Unknown UDMA reg |
| F95 | 0x5E (write) | 0x74000018 | 01 00 00 00 | Unknown UDMA reg |

Note: Value `0x000123e0` has BIT(5)=1, BIT(8)=1, BIT(13)=1 already set. The trigger is BIT(0).

## 3. BT USB Register Access Mechanism

The BT driver uses USB vendor control transfers (NOT PCIe BAR):

### UHW Register Read (btmtk_usb_uhw_reg_read)
```
bmRequestType = 0xDE (Device→Host, Vendor)
bRequest = 0x01
wValue = reg >> 16
wIndex = reg & 0xffff
```

### UHW Register Write (btmtk_usb_uhw_reg_write)
```
bmRequestType = 0x5E (Host→Device, Vendor)
bRequest = 0x02
wValue = reg >> 16
wIndex = reg & 0xffff
```

### Regular Register Read (btmtk_usb_reg_read)
```
bmRequestType = USB_TYPE_VENDOR | USB_DIR_IN (0xC0)
bRequest = 0x63
wValue = reg >> 16
wIndex = reg & 0xffff
```

The USB controller firmware internally translates these register addresses to
on-chip bus accesses. This mechanism is fundamentally different from PCIe BAR MMIO
and cannot be reused for WiFi.

## 4. BT Firmware Download (MT7927)

### Firmware Selection
- dev_id = 0x6639 (read from reg 0x70010200)
- FW name: `mediatek/mt6639/BT_RAM_CODE_MT6639_2_1_hdr.bin`
- Note: `_2_` format, NOT `_1_` like mt7925

### Section Filtering (CRITICAL)
The BT firmware has 9 sections but ONLY 5 are BT sections:
- Filter: `dlmodecrctype & 0xFF == 0x01` → BT section
- Sections 5-7 are WiFi sections — sending them **hangs the chip**
- This matches our WiFi FWDL which uses DL_MODE_ENCRYPT for RAM regions

### Download Protocol
- Uses WMT (Wireless MediaTek) vendor HCI commands (opcode 0xfc6f)
- WMT_PATCH_DWNLD with 250-byte blocks (flag: 1=first, 2=middle, 3=last)
- After all sections: 100ms wait for activation
- Then WMT_FUNC_CTRL(1) to enable BT protocol

### Post-FW Setup
```
1. Write MTK_EP_RST_OPT = 0x00010001 (USB endpoint reset)
2. WMT_FUNC_CTRL(op=6, data=1) → Enable BT protocol
3. Set MSFT opcode 0xFD30
4. Enable AOSP extensions
5. (Optional) ISO interface init for LE Audio
```

## 5. Comparison: BT Init vs WiFi Init

| Aspect | BT (USB) | WiFi (PCIe) |
|--------|----------|-------------|
| Bus | USB vendor control transfers | PCIe BAR0 MMIO |
| Reset register | 0x70028610 (CB_INFRA_RGU) | 0x7c000140 (CONN_INFRA_CFG) |
| Reset bit | BIT(0) at 0x70028610 | BIT(0) at 0xf0140 |
| FW download | WMT HCI commands | WFDMA DMA scatter |
| FW activation | WMT_FUNC_CTRL(1) | FW_START_OVERRIDE(option=1) |
| Post-FW MCU cmd | WMT HCI vendor events | MCU DMA (HOST TX → MCU RX) |
| MCU response | WMT HCI events via USB | MCU TX → HOST RX (DMA) |
| FWDL max chunk | 250 bytes | 4096 bytes |

### CRITICAL DIFFERENCE: Two different reset registers!

- **CONN_INFRA_CFG WFSYS_SW_RST** at chip 0x7c000140 (BAR0 0xf0140):
  - Used by upstream mt7925 driver
  - In CONN_INFRA_CFG address space
  - May be a "soft" reset

- **CB_INFRA_RGU WF_SUBSYS_RST** at chip 0x70028600 (BAR0 0x1f8600):
  - Used by vendor mt6639 mobile driver
  - In CB_INFRA_RGU address space
  - May be a "hard" hardware reset
  - BT equivalent at 0x70028610 is CONFIRMED WORKING

**Hypothesis**: The CB_INFRA_RGU reset (0x1f8600 BIT(4)) may properly reinitialize
the WFDMA subsystem including MCU_RX0, which the CONN_INFRA_CFG reset (0xf0140) does not.

## 6. Windows BT Driver (.inf analysis)

The mtkbtfilter.inf confirms:
- USB VID 0x0489 PID 0xE13A is our specific device (MTKBT_A2_2 section)
- Also matches USB VID 0x0E8D PID 0x6639 and 0x7927
- Uses mtkbtfilterx.sys (binary, no source available)
- Filter driver that sits below bthusb.sys in the USB stack
- Firmware stored in mtkbt.dat container

Registry settings of interest:
- `VsMsftOpCode = 0xFD30` (Microsoft vendor-specific HCI opcode)
- `FWBin = 1` (firmware binary download required)
- Various sideband audio settings (HFP, A2DP, LE Audio offload)

## 7. Shared BT/WiFi Architecture

### CONN_INFRA is shared
Both BT and WiFi share the CONN_INFRA subsystem. Key evidence:
- Both use registers in 0x70020000-0x7002FFFF (CB_INFRA block)
- BT reset at 0x70028610, WiFi reset at 0x70028600 — same register page
- Firmware contains both BT and WiFi sections (9 total, 5 BT + 4 WiFi)

### No BT/WiFi coordination in BT driver
The BT driver does NOT check if WiFi is initialized. It independently:
1. Resets BT subsystem via CB_INFRA_RGU
2. Downloads BT firmware sections
3. Enables BT protocol

### DMA architecture differs
- BT uses USB (UDMA) — no WFDMA rings
- WiFi uses WFDMA (PCIe DMA) — ring-based with MCU_RX0/RX1 for commands

## 8. Actionable Recommendations

1. **Try CB_INFRA_RGU WFSYS reset (Mode 40)**:
   - Read BAR0+0x1f8600, set BIT(4), wait, clear, then FWDL
   - This is what vendor mt6639 mobile driver does

2. **Probe BT reset register from PCIe**:
   - Read BAR0+0x1f8610 to verify bus2chip mapping works for this block
   - If readable, the entire CB_INFRA_RGU block is accessible

3. **Read BT_MISC from PCIe**:
   - BAR0+0x1e2510 should give MTK_BT_MISC value
   - Can check BIT(8) RST_DONE — confirms another bus2chip mapping

4. **Consider BT reset before WiFi**:
   - If BT subsystem state affects WiFi WFDMA, resetting BT first might help
   - The BT driver does this on every probe — our WiFi driver doesn't

5. **Verify 0x70010200 accessibility from PCIe**:
   - This is the device ID register (should read 0x6639)
   - Need to identify the bus2chip mapping for 0x70010000 range
