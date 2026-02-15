# MT7927 QA Tool Technical Analysis

**Document Source**: MT7927 Test-Mode Software Application Note Part-1: QA-Tool User Guideline v0.3 (2022-07-08)

**Analysis Date**: 2026-02-15

**Purpose**: Extract hardware/driver-relevant technical information from MediaTek's official QA tool documentation.

---

## Executive Summary

This PDF is primarily a **QA/testing tool user guide** rather than a hardware register manual. It provides limited low-level technical details but contains valuable information about:

1. **Hardware specifications** (chip capabilities, frequencies, modulation)
2. **Interface types** supported (USB, SDIO, PCIe)
3. **E-fuse/EEPROM access** procedures
4. **RF configuration** parameters
5. **Integration requirements** (M.2 2230 PCIe form factor)

**CRITICAL**: This document does NOT contain:
- Register address maps
- DMA configuration details
- Firmware loading procedures
- MCU command/response formats
- WFDMA initialization sequences
- Bus addresses (0x7c/0x70/0x81 ranges)

For driver development, this document has **limited utility** but confirms key hardware facts.

---

## 1. Hardware Specifications

### 1.1 Product Overview
- **Model**: MT7927
- **Type**: 2TX 11be (WiFi7) BW320 + BT/BLE Combo Card
- **Power Supply**: 3.3Vdc from host equipment
- **Operating Temperature**: -10°C to 70°C
- **Form Factor**: M.2 2230 ("NGFF" Next Generation Form Factor)
- **Interface**: M.2 2230 PCIe slot

### 1.2 Supported Interfaces
MT7927 supports three interface types (context: for QA tool access):
1. **USB interface** (Bluetooth via "Generic Bluetooth Adapter" + WiFi_If device)
2. **SDIO interface**
3. **PCIe interface** ← Primary interface for WiFi

**Driver Implication**: The QA tool can operate over all three buses, but production WiFi uses PCIe.

### 1.3 Wireless Capabilities

#### WiFi Modulation Types
- **2.4GHz**: GFSK, π/4-DQPSK, 8DPSK, CCK, DQPSK, DBPSK for OFDM
- **5/6GHz**: 64QAM, 16QAM, QPSK, BPSK for OFDM
- **WiFi 6E (11ax)**: 256QAM for OFDM in VHT mode, 1024QAM for OFDM in 11ax mode
- **WiFi 7 (11be)**: 4096QAM for OFDM in 11be mode

#### Bluetooth Modulation
- **BT EDR**: FHSS
- **BT LE**: GFSK
- **WLAN**: DSSS, OFDM, OFDMA

#### Transfer Rates
**Bluetooth**:
- BT EDR: up to 3 Mbps
- BT LE: up to 2 Mbps

**WiFi 2.4GHz**:
- 802.11b: up to 11 Mbps
- 802.11g: up to 54 Mbps
- 802.11n: up to 300 Mbps
- 802.11ax: up to 573.5 Mbps
- 802.11be: up to 688.2 Mbps

**WiFi 5GHz**:
- 802.11a: up to 54 Mbps
- 802.11n: up to 300 Mbps
- 802.11ac: up to 1733.3 Mbps
- 802.11ax: up to 2401.9Mbps
- 802.11be: up to 1441.2 Mbps

**WiFi 6GHz**:
- 802.11a: up to 54 Mbps
- 802.11ax: up to 1201.0 Mbps
- 802.11be: up to 5764.8 Mbps

### 1.4 Operating Frequencies
- **BT EDR**: 2402MHz - 2480MHz
- **BT LE**: 2402MHz - 2480MHz
- **2.4GHz WiFi**: 2.412 - 2.472GHz
- **5GHz**: 5.18-5.25GHz, 5.25-5.32GHz, 5.5-5.72GHz, 5745-5825GHz
- **6GHz**: 5.955-6.425GHz, 6.435-6.525GHz, 6.525-6.875GHz, 6.875-7.115GHz

### 1.5 Channel Information
**BT EDR**: 79 channels
**BT LE**: 40 channels

**2.4GHz WiFi**: 802.11b, 802.11g, 802.11n (HT20), VHT20, 802.11ax (HE20), 802.11be (EHT20): 13 channels

**5GHz/6GHz**: 802.11n (HT40), VHT40, 802.11ax (HE40), 802.11be (EHT40): 9 channels

### 1.6 U-NII Band Support
Detailed U-NII-1, U-NII-2A, U-NII-2C, U-NII-3 band definitions with specific channels (see page 28 of PDF for complete tables).

### 1.7 Transmit Power Levels
**Example (5.955-6.425GHz)**:
- 1TX: 65.163 mW (EIRP: 22.9 dBm / 194.984 mW)
- 2TX: 65.66 mW (EIRP: 22.93 dBm / 196.336 mW)

(Full power tables available on pages 27-28)

---

## 2. E-fuse / EEPROM Access

**CRITICAL FOR CALIBRATION DATA**

### 2.1 EEPROM Modes
The QA tool supports two modes:
1. **BIN-file mode**: Reads/writes to `eeprom.bin` file (offline)
2. **E-fuse mode**: Direct hardware access to on-chip E-fuse

### 2.2 E-fuse Access via QA Tool

#### Reading E-fuse Values
1. Launch QA tool in E-fuse mode (no `eeprom.bin` file present)
2. EEPROM sheet shows "EEPROM Type: E-fuse"
3. Select "Single Read/Write Mode" → "READ"
4. Enter offset in "Offset" field (e.g., 0x0000)
5. Click "R/W" button
6. Value appears in "Value" text box

**Example**: Reading offset 0x0001 returns value 0x00

#### Writing E-fuse Values
1. E-fuse mode, select "WRITE"
2. Set offset and new value in "Offset" and "Value" boxes
3. Click "R/W" button
4. Click "Read ALL" to update e-fuse table and verify

**Example**: Writing 0x01 to offset 0x0055 and verifying

### 2.3 Driver Implications
- Driver must be able to read E-fuse for calibration data (MAC address, TX power tables, frequency offsets)
- E-fuse access likely via specific register interface (not documented in this PDF)
- Windows QA tool uses proprietary driver "QA-Tool Windows driver" to access hardware

---

## 3. RF Configuration Parameters

### 3.1 TX/RX Configuration
The QA tool shows various RF parameters:

- **RF Type**: Should show "MT7927 : 2 T 2 R" for normal operation
- **Band selection**: Band0 (G band) / Band1 (A band)
- **Channel/Mode/Rate**: 802.11b CCK, 802.11g OFDM, 802.11n HT Mix, 802.11ac VHT, 802.11ax HESU/HETB/EHTTB/RU modes
- **Bandwidth**: Pre-Packet BW (System BW)
- **TX streams**: TX0, TX1, or both
- **Packet length**: Variable (0 = infinite packets)
- **Power adjustment**: ±dB offset controls
- **Frequency offset**: ±Hz controls

### 3.2 11ax/11be Specific Settings
- **HE TB (trigger based)** modes require RU (Resource Unit) page settings
- **RU index settings** with category tables:
  - RU26: category 26*9
  - RU52: category 52*4
  - RU106: category 106+106
  - RU242: category 242*1
  - RU484: category 484*1
  - RU996: category 996*1
  - RU996*2: category 996*2

- **Stream index/length** configuration via lookup tables (page 18)
- **MU NSS/LDPC** settings per stream

### 3.3 VSA (Vector Signal Analyzer) Settings
Integration with Litepoint MW Web page:
- LTF Type, LDPC Extra Sym, PE Disamb alignment to QAtool settings
- MCS rate, Stream setting, RU idx alignment
- Coding options (LDPC vs BCC)

### 3.4 Homologation Test Settings
**Normal test**: Packet length = 512 Byte
**SAR test**: HWTX mode, adjust packet lengths for 85% duty cycle, packet length = 512 Byte

---

## 4. What This Document DOES NOT Contain

### 4.1 Missing Critical Driver Information
This QA tool manual **does not** provide:

1. **No register addresses** or memory maps
2. **No DMA/WFDMA configuration** details
3. **No firmware download** procedures or formats
4. **No MCU command** structures (class=0x8a, 0x02, 0xc0, 0xed, etc.)
5. **No bus addresses** (0x7c024208, 0x70028600, etc.)
6. **No initialization sequences** (CLR_OWN, SET_OWN, etc.)
7. **No ring configuration** (MCU_RX0/RX1/RX2/RX3, HOST RX)
8. **No interrupt configuration** details
9. **No power management** register details
10. **No ROMCODE_INDEX** or boot status registers

### 4.2 QA Tool Driver Architecture
The document mentions a **"QA-Tool Windows driver"** that must be installed:
- This is a proprietary test driver (not production driver)
- It provides direct hardware access for E-fuse, RF testing
- Installation requires disabling Windows driver integrity checks on Win10
- Driver package: `MediaTek MT6639 QAtool PCIe Driver` (note: references MT6639!)

**CONFIRMATION**: The QA tool driver is labeled **MT6639**, confirming MT7927 = MT6639 in PCIe package.

---

## 5. Useful Confirmations for Driver Development

### 5.1 MT7927 = MT6639 Confirmation
Page 11 (driver installation screenshot) shows:
- Installing driver for WiFi device labeled **"WiFi_If"**
- Driver package name: **"MediaTek MT6639 QAtool PCIe Driver"**

**This confirms**: MT7927 uses MT6639 silicon, supporting our use of MT6639 Android driver as reference.

### 5.2 Interface Confirmation
- **PCIe is the primary WiFi interface** (M.2 2230 form factor)
- **USB is for Bluetooth** ("Generic Bluetooth Adapter" device)
- Dual-interface design: WiFi over PCIe, BT over USB

### 5.3 E-fuse Structure Hints
The screenshots show E-fuse data structured as:
- Hex dump format (16 bytes per line)
- Offset addressing (0x0000, 0x0010, 0x0020, etc.)
- Standard EEPROM-style layout

Driver should expect calibration data in similar format.

### 5.4 RF Type Check
The tool verifies "RF Type: MT7927 : 2 T 2 R" on launch.
- Suggests a hardware ID register exists
- Driver should verify chip identity similarly

---

## 6. Integration Requirements (Section 3.3-3.5)

### 6.1 Physical Integration
- **Form factor**: M.2 2230 PCIe slot ("NGFF")
- **Power**: 3.3Vdc from host
- **Temperature**: -10°C to 70°C operational range

### 6.2 Antenna Requirements
Three certified antenna sets documented (page 29):
- **Set 1**: PSA RFMTA340718EMLB302 (2.4-2.4835GHz, 5.15-5.85GHz)
- **Set 2**: PSA RFMTA311020EMMB301 (2.4-2.4835GHz, 5.925-6.425GHz, 6.425-6.525GHz, 6.525-6.875GHz, 6.875-7.125GHz)
- **Set 3**: PSA RFMTA421208MMB701 (5.925-7.125GHz)

Connector types: I-pex(MHF) or IPEX

**IMPORTANT**: Final host product must have integral (non-removable) antenna.

### 6.3 FCC Regulatory Notes
- **FCC ID**: RAS-MT7927 (confirmed as grantee's FCC ID)
- **Module type**: Low Power Indoor Client
- **KDB 996369 D03 OEM Manual v01** compliance required
- **Co-location restrictions**: Cannot co-locate with other transmitters without separate certification
- **RF exposure**: Minimum 20cm distance required
- **Prohibited use**: No control of unmanned aircraft systems (drones)

---

## 7. Recommendations for Driver Development

### 7.1 Use This Document For:
1. **Chip specification validation** (WiFi 7, 2T2R, frequency ranges)
2. **E-fuse access planning** (understand data structure/offsets)
3. **Confirming MT7927 = MT6639** (driver package naming)
4. **RF parameter ranges** (for TX power, channel validation)

### 7.2 Do NOT Rely On This For:
1. Register programming sequences
2. DMA/WFDMA configuration
3. Firmware download protocols
4. MCU command formats
5. Initialization procedures
6. Interrupt handling
7. Ring buffer management

### 7.3 Next Steps
Continue using primary references:
- **mt6639/** Android driver (PRIMARY - same silicon)
- **mt76/mt7925/** upstream driver (SECONDARY - similar architecture)
- **Ghidra RE of Windows driver** (mtkwecx.sys - actual register sequences)
- **Vendor datasheets** (if/when available)

---

## 8. Technical Gaps vs. Current Driver Needs

### 8.1 Current Blocker: MCU_RX0 Ring Not Configured
**What we need**:
- How firmware decides which rings to configure
- Post-firmware-boot initialization sequence
- DMASHDL enable procedure (0xd6060 |= 0x10101)
- PostFwDownloadInit MCU commands

**What this PDF provides**:
- None of the above

### 8.2 WFSYS Reset Procedure
**What we need**:
- CB_INFRA_RGU register programming sequence
- Post-reset polling requirements
- Register state expectations

**What this PDF provides**:
- None of the above

### 8.3 TXD Format
**What we need**:
- Q_IDX routing rules
- LONG_FORMAT vs legacy TXD structure
- Header format variations

**What this PDF provides**:
- None of the above (QA tool handles this internally)

---

## 9. Conclusion

The **MT7927 QA Tool User Guide** is a **test/validation tool manual**, not a hardware programming guide. It confirms key architectural facts (MT7927 = MT6639, PCIe interface, WiFi 7 capabilities) but provides **no register-level details** needed for driver development.

**Value**: Medium (for specification validation, E-fuse understanding)
**Utility for current blocker**: Low (no DMA/WFDMA/MCU command details)

**Primary references remain**:
1. MT6639 Android kernel driver (same chip)
2. Windows driver reverse engineering (Ghidra analysis)
3. MT76/MT7925 upstream code (architecture patterns)

---

## Appendix A: RF Type Verification

From page 12 screenshot, the QA tool UI shows:
```
RF Type
MT7927 : 2 T 2 R
```

This suggests a hardware register returning chip ID. Cross-reference with:
- MT6639 datasheet (if available)
- Windows driver chip detection code
- Existing `mt7927_init_dma.c` chip ID reads

---

## Appendix B: E-fuse Offset Examples

From screenshots (pages 23-24):
- Offset 0x0000: contains factory data
- Offset 0x0001: value 0x00
- Offset 0x0055: writable test location

**Driver TODO**:
- Map E-fuse offsets to calibration parameters
- Implement E-fuse read/write functions
- Parse MAC address, TX power tables from E-fuse

---

## Appendix C: Document Metadata

- **Title**: MT7927 Test-Mode Software Application Note Part-1: QA-Tool User Guideline
- **Version**: V0.3
- **Release Date**: 2022-07-08
- **Authors**: Jack Pan (initial), Leon Hsu (RX updates), Jack Pan (modifications)
- **Classification**: MediaTek Confidential B
- **Copyright**: © 2021-2022 MediaTek Inc.
- **Pages**: 35 total

---

*End of Analysis*
