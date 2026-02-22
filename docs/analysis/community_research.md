# MT7927/MT6639 Community Research — Linux WiFi Driver Efforts

**Date**: 2026-02-15
**Analyst**: agent3

## Summary

No working Linux WiFi driver for MT7927/MT6639 exists in the community. Our project
is the most advanced effort, having achieved FWDL with fw_sync=0x3. All other efforts
are stuck at much earlier stages.

## 1. Community Projects Analyzed

### ehausig/mt7927 (GitHub)
- **URL**: https://github.com/ehausig/mt7927
- **Status**: ON-HOLD INDEFINITELY (as of 2025-08-18)
- **Stage**: Stuck at firmware loading (never loaded FW)
- **Key issues**:
  - Uses PCI ID 0x7927 (not 0x6639) — wrong device ID
  - Uses MT7925 firmware (doesn't work on MT6639)
  - Uses BAR2 offsets (FW_STATUS at 0x0200) — wrong register mapping
  - No CONNAC3X, no CB_INFRA_RGU, no WFDMA architecture knowledge
  - 23 test modules, all firmware-dependent tests failed
  - Confirmed `pci_reset_function()` hangs (matches our finding)
  - FW_STATUS = 0xffff10f1 ("waiting for firmware") — never progressed past this
- **Usefulness to us**: NONE — much earlier stage than our driver

### openwrt/mt76 Issue #927
- **URL**: https://github.com/openwrt/mt76/issues/927
- **Status**: Open (11+ months, 74+ comments, 42+ thumbs up)
- **Key contributor**: marcin-fm (firmware extraction)
- **Technical content**: Minimal — mostly "+1" support requests
- **Key finding by marcin-fm** (2025-09-01):
  - MT7927 = variant of MT6639 (mobile chip)
  - Extracted firmware files from Windows driver using `unmtk.rb` tool
  - Firmware files: `WIFI_RAM_CODE_MT6639_2_1.bin`, `BT_RAM_CODE_MT6639_2_1_hdr.bin`
  - Gist: https://gist.github.com/marcin-fm/e53840817dea293f4ebe9176578ded9a
- **dubhater** (2025-08-05): Referenced "MTK_modules" PCIE code — couldn't find the repo
- **Usefulness to us**: Confirmed MT6639 identity (we already knew this)

### Widespread "MT7927 not working on Linux" reports
- Linux Mint Forums, Manjaro Forums, Garuda Forums, CachyOS GitHub, Arch Forums
- Users with ASUS ROG X870E, Lenovo 16IAX10H, etc. all stuck
- Common advice: "Wait for MediaTek" or "Swap WiFi card to MT7925"

## 2. Wrong Information in Community

**CRITICAL**: Multiple sources claim "MT7927 is architecturally identical to MT7925
except for 320MHz channel width." **This is WRONG.** Our research proves:

- MT7927 = MT6639 (mobile SoC chip in PCIe package)
- MT7925 = CONNAC2/mt76 family chip
- Completely different register addresses, bus2chip mappings, init sequences
- MT7925 firmware doesn't work on MT7927
- Upstream mt7925e driver can't read chip ID on MT7927 (returns 0)
- MT6639 firmware (from vendor mobile driver) works: `WIFI_RAM_CODE_MT6639_2_1.bin`

## 3. Firmware Extraction Tool

marcin-fm created a Ruby script to extract firmware from Windows driver archives:

**File format**: TAR-like with "MTK-" magic header
- 16-byte header: magic(4) + nitems(2) + unk1(2) + file_size(4) + zero(4)
- Per-file entry (76 bytes): filename(48) + date(16) + offset(4) + size(4) + zero(4)
- Files: `mtkwlan.dat` and `mtkbt.dat` are the containers

**We already have these firmware files** extracted from Windows driver v5.7.0.5275.

## 4. Upstream MT7925 Driver Init Sequence Analysis

From `torvalds/linux` mt7925/pci.c — the probe sequence for comparison:

```
mt7925_pci_probe:
  1. pcim_enable_device()
  2. pci_set_master()
  3. mt76_alloc_device()
  4. mt76_mmio_init() — register accessors with bus2chip remapping
  5. __mt792x_mcu_fw_pmctrl() — SET_OWN
  6. __mt792xe_mcu_drv_pmctrl() — CLR_OWN
  7. mt792x_wfsys_reset() — WFSYS reset at 0x7c000140
  8. mt7925_dma_init():
     - mt76_dma_attach()
     - mt792x_dma_disable()
     - mt76_connac_init_tx_queues() — HOST data TX
     - mt76_init_mcu_queue(MT_MCUQ_WM) — HOST MCU WM TX ring
     - mt76_init_mcu_queue(MT_MCUQ_FWDL) — HOST FWDL TX ring
     - mt76_queue_alloc(MT_RXQ_MCU) — HOST MCU event RX ring
     - mt76_queue_alloc(MT_RXQ_MAIN) — HOST data RX ring
     - mt792x_dma_enable()
  9. devm_request_irq()
 10. mt7925_register_device() → mt7925_init_hardware() → mt7925_mcu_init()
     → firmware download + MCU command setup
```

**Key observation**: The HOST-side queues (TX/RX rings) are allocated in `mt7925_dma_init()`.
The MCU-side DMA rings (MCU_DMA0 TX/RX) are configured by the firmware/ROM,
NOT by the host driver. The host driver only configures HOST WFDMA rings.

**Implication for our blocker**: After FWDL, the running FW is supposed to configure
MCU_DMA0 RX0/RX1 (for receiving post-boot MCU commands from host) and MCU_DMA0
TX0/TX1 (for sending events to host). Our FW does NOT do this. This is the blocker.

## 5. Key Question Not Answered by Community

**What triggers the FW to configure MCU DMA0 RX0/TX0 after boot?**

No community source answers this. The upstream mt7925 driver doesn't explicitly
configure MCU-side DMA — it relies on the FW/ROM to do it automatically. For MT6639,
this automatic configuration doesn't happen.

Possible answers (require Ghidra RE of Windows driver):
1. A specific MCU command after FW_START
2. A register write that signals "host ready"
3. A specific init sequence in AsicConnac3xPostFwDownloadInit
4. The NEED_REINIT mechanism working differently than we think

## 6. Windows Driver Versions Available

| Version | Source | Notes |
|---------|--------|-------|
| 5.7.0.5275 | ASUS/MediaTek | Latest, supports MT6639 + MT7927 |
| 5.6.0.4303 | station-drivers | Earlier version |
| 5.3.0.1498 | station-drivers | Oldest available |

All use `mtkwecx.sys` as the main driver binary. Ghidra RE of
`AsicConnac3xPostFwDownloadInit` in this binary is Task #6 (in progress).
