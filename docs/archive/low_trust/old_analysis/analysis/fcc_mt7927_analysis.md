# FCC MT7927 Document Analysis

**Date**: 2026-02-15
**FCC ID**: RAS-MT7927
**Applicant**: MediaTek Inc., Hsinchu, Taiwan
**Product**: 2TX 11be (WiFi7) BW320 + BT/BLE Combo Card

## Summary

Two URLs were investigated:
1. https://fcc.report/FCC-ID/RAS-MT7927 -- FCC filing listing page
2. https://manuals.plus/m/5d003150a0ca826ebc6bcdf30c621a3c8ab4a0f0a893fc59167a5d801c088717 -- **403 Forbidden** (blocked by anti-crawl)

### Key Finding: Document Cross-Contamination

The fcc.report third-party aggregator serves **wrong documents** for many RAS-MT7927 entries:
- "Internal Photos" (6256367.pdf) = photos of wireless earbuds (TWS case), NOT MT7927
- "Modular Approval" (5780647.pdf) = Reolink IP Camera test report (FCC ID 2A4AS-2109A)
- "User Manual" (5780653.pdf) = LTE frequency stability test data for wrong device
- "User Manual" (7567627.pdf) = Prowise Touchscreen Ten G3 manual (touchscreen display)
- "Test Setup Photos" (7567626.pdf) = Prowise Touchscreen Ten G3 manual again

The fccid.io mirror has the **correct** documents. The genuine MT7927 document retrieved:
- `Users-Manual-revised-1109-6211524.pdf` = **MediaTek MT7927 Test-Mode Software Application Note (QA-Tool User Guideline) V0.3, 2022-07-08**

## Genuine MT7927 Technical Information Extracted

### 1. General Description (from MediaTek QA-Tool AN)

> "MT7927 chip is highly integrated single chip which have built in 2x2 dual-band wireless LAN and Bluetooth combo radio."

- **Input Rating**: 3.3Vdc (from host equipment)
- **Operating Temperature**: -10 to 70 C
- **Configuration**: 2T2R (2 transmit, 2 receive)
- **Classification**: MediaTek Confidential B

### 2. Supported Interfaces

MT7927 supports **three** bus interfaces:
- **USB**
- **SDIO**
- **PCIe**

This confirms the chip has a PCIe interface path. Our driver uses PCIe (M.2 2230 form factor).

### 3. Physical Form Factor

- **M.2 2230** NGFF (Next Generation Form Factor)
- **PCIe Bus** interface
- **Antenna connector**: i-pex (MHF) / IPEX

### 4. Modulation Types

| Technology | Modulation |
|-----------|-----------|
| BT EDR | GFSK, pi/4-DQPSK, 8DPSK |
| BT LE | GFSK |
| 802.11b | CCK, DQPSK, DBPSK (DSSS) |
| 802.11a/g | BPSK, QPSK, 16QAM, 64QAM (OFDM) |
| 802.11ac (VHT) | up to 256QAM (OFDM) |
| 802.11ax (HE) | up to 1024QAM (OFDM/OFDMA) |
| 802.11be (EHT) | up to 4096QAM (OFDM/OFDMA) |

### 5. Transfer Rates

| Band | Standard | Max Rate |
|------|----------|----------|
| 2.4GHz | 802.11b | 11 Mbps |
| 2.4GHz | 802.11g | 54 Mbps |
| 2.4GHz | 802.11n | 300 Mbps |
| 2.4GHz | VHT | 400 Mbps |
| 2.4GHz | 802.11ax | 573.5 Mbps |
| 2.4GHz | 802.11be | 688.2 Mbps |
| 5GHz | 802.11a | 54 Mbps |
| 5GHz | 802.11n | 300 Mbps |
| 5GHz | 802.11ac | 1733.3 Mbps |
| 5GHz | 802.11ax | 2401.9 Mbps |
| 5GHz | 802.11be | 1441.2 Mbps |
| 6GHz | 802.11a | 54 Mbps |
| 6GHz | 802.11ax | 1201.0 Mbps |
| 6GHz | 802.11be | **5764.8 Mbps** |

### 6. Operating Frequencies

| Band | Frequency Range |
|------|----------------|
| BT EDR | 2402 - 2480 MHz |
| BT LE | 2402 - 2480 MHz |
| 2.4GHz WiFi | 2.412 - 2.472 GHz |
| 5GHz WiFi | 5.18-5.25, 5.25-5.32, 5.5-5.72, 5.745-5.825 GHz |
| 6GHz WiFi | 5.955-6.425, 6.435-6.525, 6.525-6.875, 6.875-7.115 GHz |

### 7. Antenna Configuration

Three antenna sets are certified:

| Set | Model | Type | Connector | Gain (dBi) | Freq (GHz) | Cable |
|-----|-------|------|-----------|------------|------------|-------|
| 1 | RFMTA340718EMLB302 | PIFA | i-pex(MHF) | 3.18/4.92 | 2.4/5GHz | 200mm |
| 2 | RFMTA311020EMMB301 | PIFA | i-pex(MHF) | 1.71-4.82 | 2.4/5/6GHz | 200mm |
| 3 | RFMTA421208IMMB701 | PIFA | IPEX | -4.99 | 5.925-7.125 | 300mm |

Antenna vendor: **Walsin Technology Corp.** (PSA brand)
- Contact: Andrew Lin, andrewlin@passivecomponent.com
- Tel: +886-3-475-8711 #8172

### 8. Channel Configuration

- BT EDR: 79 channels
- BT LE: 40 channels
- 2.4GHz 20MHz: 13 channels
- 2.4GHz 40MHz: 9 channels
- 5/6GHz: multiple channel plans per bandwidth

### 9. WiFi Modes Confirmed Working in QA-Tool

- 802.11b CCK
- 802.11g OFDM
- 802.11n HT Mix Mode (HT20, HT40)
- 802.11ac VHT (VHT20, VHT40, VHT80, VHT160)
- 802.11ax HESU (HE SU)
- 802.11ax HETB (HE Trigger-Based, RU mode)
- **802.11be EHTSU** (EHT SU)
- **802.11be EHTTB** (EHT Trigger-Based, RU mode)

### 10. Band Selection

- **Band0** = G band (2.4 GHz)
- **Band1** = A band (5 GHz / 6 GHz)

### 11. E-fuse / EEPROM

The MT7927 has an on-chip E-fuse that stores calibration data. The QA-Tool can:
- Read E-fuse values by address offset
- Write E-fuse values by address offset
- Load from eeprom.bin file (BIN-file mode)

### 12. Test Configurations

- Normal test: 512 byte packet length
- SAR test: HWTX mode, duty cycle ~85%
- RU index mapping follows standard 802.11ax/be RU allocation tables
- Litepoint IQxel used as VSA (Vector Signal Analyzer) for test

### 13. RF Exposure

- Minimum separation distance: **20cm** (mobile use)
- SAR/Power Density evaluation required for portable use
- Body-worn minimum: 5mm separation

## FCC Filing Summary (from fcc.report listing page)

### Approved Frequency Bands
- 2.4 GHz: 2402.0-2480.0 MHz, 2412.0-2472.0 MHz
- 5 GHz: 5180-5240, 5260-5320, 5500-5720, 5745-5825 MHz
- 5.9 GHz: 5815, 5835, 5845, 5855, 5865-5885 MHz
- 6 GHz: 5955-7115, 5955-6425, 6525-6875 MHz

### Document Count (fcc.report)
- Test Reports: 23 documents
- RF Exposure: 15 documents
- Photos: 10 documents
- User Manuals: 3 documents
- Administrative: 18+ documents

## What Was NOT Found (Driver-Relevant)

The FCC filings contain **NO information** about:
- Register maps or memory-mapped I/O addresses
- DMA ring configuration or WFDMA architecture
- Firmware download procedures or protocols
- MCU command/response message formats
- WFSYS reset sequences
- Host-to-MCU / MCU-to-Host communication protocols
- PCIe BAR layout or configuration space details
- Power management (LPCTL/SET_OWN/CLR_OWN) registers
- TXD/RXD descriptor formats

This is expected -- FCC filings are regulatory compliance documents focused on RF emissions, not silicon-level programming interfaces. MediaTek's register-level documentation is under NDA and not part of public FCC filings.

## Relevance to Driver Development

### Confirmed facts useful for our driver:
1. **MT7927 = 2T2R** -- matches our driver's assumption
2. **PCIe interface** -- confirmed (M.2 2230)
3. **3.3V power** -- from host PCIe slot
4. **Tri-band** -- 2.4/5/6GHz, all via same chip
5. **USB interface also exists** -- BT uses USB path (matches our btusb experience)
6. **802.11be (WiFi 7)** -- confirmed, with 4096-QAM and OFDMA

### Not useful for our current blocker:
The FCC documents provide zero insight into the MCU_RX0 configuration issue, WFSYS reset sequence, or PostFwDownloadInit MCU commands. These require continued Ghidra RE of the Windows driver (mtkwecx.sys).

## Access Instructions for Manual Download

If the user wants to manually download the correct MT7927 documents:

1. **Use fccid.io** (NOT fcc.report) -- fcc.report has cross-contaminated documents
2. **Direct URL for QA-Tool manual**: https://fccid.io/RAS-MT7927/User-Manual/Users-Manual-revised-1109-6211524.pdf
3. **manuals.plus** URL returns 403 Forbidden -- requires browser access or different user-agent
4. **FCC.gov OETS** (apps.fcc.gov) is very slow but has authoritative documents

Recommended fccid.io URLs to try manually:
- https://fccid.io/RAS-MT7927/Internal-Photos/ (internal photos of actual MT7927 module)
- https://fccid.io/RAS-MT7927/Test-Report/ (RF test reports)
- https://fccid.io/RAS-MT7927/User-Manual/Users-Manual-7241524 (2024 user manual)

## Web Search Additional Findings

From web search results:
- **Filogic 380** is the marketing name for MT7927
- **Windows only**: MT7927 driver officially supports only Windows 11 64-bit 23H2+
- **No Linux driver**: Confirmed -- "currently no drivers for the MT7927 chipset for Linux systems" (as of search results)
- **MT7925 vs MT7927**: MT7925 has Linux support (mt7925 in mt76 tree), MT7927 does NOT
- **PCI ID 14c3:6639**: Confirmed MT7927 = MT6639 silicon
- **Max throughput**: Up to 6500 Mbps with MLO, 5000 Mbps single channel (6GHz)

Sources:
- https://fccid.io/RAS-MT7927
- https://fcc.report/FCC-ID/RAS-MT7927
- https://zfishtek.com/index.php/product/mediatek-mt7927-wireless-lan-card/
- https://www.hcx.com.hk/product/wifi-7-bluetooth-5-3-pcie-x1-wireless-network-adapter-card-with-mediatek-6500mbps-mt7927-chipset/
- https://github.com/morrownr/USB-WiFi/issues/431

---

## Update: manuals.plus URL Access Attempt (2026-02-15)

### Attempted URL
https://manuals.plus/m/5d003150a0ca826ebc6bcdf30c621a3c8ab4a0f0a893fc59167a5d801c088717

### Result
**403 Forbidden** - Anti-crawl protection blocks automated access (WebFetch tool)

### Analysis
The `manuals.plus` website implements server-side protection that rejects non-browser HTTP requests. This is a common anti-scraping measure that requires:
- Browser user-agent headers
- JavaScript execution capability
- Cookie/session handling
- Potentially CAPTCHA solving

### Manual Download Required
To access this document, the user must:
1. **Open URL in web browser** (Chrome/Firefox/Edge)
2. **Manually download or view** the document content
3. **Extract technical details** if any register-level information exists

### Expected Content Type
Based on the URL pattern and FCC context, this is likely:
- Another MT7927 user manual or application note
- RF test report (regulatory compliance)
- Installation guide or product specification sheet

**NOT expected to contain driver-level technical details** such as:
- Register maps (WFDMA, MCU, WFSYS)
- DMA ring configuration procedures
- Firmware download protocol specifications
- PCIe BAR memory layout
- TXD/RXD descriptor formats
- MCU command class definitions

### Why Driver Details Won't Be There
FCC filings are **regulatory compliance documents** focused on:
- RF emissions and safety
- Antenna characteristics
- Channel/frequency usage
- Power levels and SAR exposure

Silicon programming interfaces (registers, DMA, firmware protocols) are:
- **Covered by NDA** (Non-Disclosure Agreement)
- **Not required for FCC certification**
- **Never published in public regulatory filings**

MediaTek's register-level documentation requires vendor partnership or reverse engineering (Ghidra analysis of Windows driver binaries).

### Recommendation
**Skip this URL** - the document (if accessible) will not advance MCU_RX0 investigation. Continue with:
1. **Windows driver RE** (mtkwecx.sys v5603998 in Ghidra)
2. **CB_INFRA_RGU reset testing** (Mode 40 branch)
3. **PostFwDownloadInit sequence** implementation (DMASHDL + 9 MCU commands)
