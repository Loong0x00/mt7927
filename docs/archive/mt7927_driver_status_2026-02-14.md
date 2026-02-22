# MT7927 PCIe WiFi 7 Linux Driver - Project Status

**Date**: 2026-02-14
**Device**: MediaTek MT6639 (PCI ID: 14c3:6639, CONNAC3X)
**Driver file**: `tests/04_risky_ops/mt7927_init_dma.c` (~3330 lines)
**Status**: Firmware boot successful, post-boot MCU init not yet implemented

---

## Part 1: Implemented Features

### 1.1 PCI Driver Framework

| Item | Status | Notes |
|------|--------|-------|
| PCI probe/remove | Done | Module auto-binds to 14c3:6639 |
| BAR0 MMIO mapping | Done | Full BAR0 mapped, bounds-checked read/write |
| DMA mask (32-bit) | Done | `dma_set_mask(0xFFFFFFFF)` |
| Bus master enable | Done | `pci_set_master()` |
| L1 remap window | Done | Access 0x18xxxxxx registers via 0x155024+0x130000 |
| Module parameters | Done | 28 tunable params for runtime experimentation |

### 1.2 Power Management / Ownership

| Item | Status | Notes |
|------|--------|-------|
| SET_OWN (FW_OWN) | Done | Write BIT(0) to LPCTL, poll BIT(2) OWN_SYNC |
| CLR_OWN (DRV_OWN) | Done | Write BIT(1) to LPCTL, poll BIT(2) clear |
| WAKEPU_TOP / WAKEPU_WF | Done | Force CONNINFRA + WF wakeup before reset |

### 1.3 MCU / WFSYS Reset

| Item | Status | Notes |
|------|--------|-------|
| WFSYS_SW_RST (upstream method) | Done | Clear/set BIT(0) at 0x7c000140, poll INIT_DONE |
| CB_INFRA_RGU reset (alt path) | Done | Assert/deassert BIT(4), alternative reset sequence |
| Sleep protection disable | Done | Disable WF_SLP_CTRL and WFDMA_SLP_CTRL during reset |
| EMI sleep protection | Done | EMI_CTL BIT(1) via L1 remap |
| cbinfra PCIe remap | Done | PCIE_REMAP_WF=0x74037001 |
| MCU ROM idle wait | Done | Poll ROMCODE_INDEX for 0x1D1E |
| MCIF remap | Done | Configurable MCU->host DMA address translation |
| SLP_STS monitoring | Done | Extensive timing/transition logging |

### 1.4 WFDMA DMA Engine

| Item | Status | Notes |
|------|--------|-------|
| DMA disable/reset | Done | GLO_CFG TX/RX disable, logic reset, pointer reset |
| TX ring allocation (q15 WM) | Done | 256 descriptors, MCU command ring |
| TX ring allocation (q16 FWDL) | Done | 128 descriptors, firmware download ring |
| RX ring allocation (q6 EVT) | Done | 128 descriptors + 2KB buffers, event ring |
| Dummy RX rings (q4-7 or q0-3) | Done | Satisfy prefetch config requirements |
| Manual prefetch config | Done | TX/RX_RING_EXT_CTRL for CONNAC3X |
| GLO_CFG phased enable | Done | Phase 1 (no DMA_EN) -> Phase 2 (enable) |
| GLO_CFG_EXT0 / EXT1 | Done | DMASHDL disable, EXT1=0x10000000 |
| HOST_INT_ENA | Done | TX15/16 done + RX event interrupts |
| MSI config (CFG0-3) | Done | Ring->MSI mapping |
| DMASHDL bypass | Done | SW_CONTROL BIT(28) bypass enable |
| MCU_WRAP GLO enable | Done | MCU-side DMA wrapper |
| WFDMA_DUMMY_CR NEED_REINIT | Done | Signal MCU to reinit WFDMA |
| MCU DMA0 RX CIDX fix | Done | Set CIDX=CNT-1 for WFDMA write space |

### 1.5 MCU Command Interface

| Item | Status | Notes |
|------|--------|-------|
| TXD header construction | Done | Upstream format: Q_IDX=0x20, HDR_FORMAT=CMD |
| Command send (q15 WM ring) | Done | With seq tracking, DMA kick, completion poll |
| Event receive (q6 RX ring) | Done | Poll DMA_DONE, extract event data, advance CIDX |
| Event hex dump | Done | Full 44-byte event data logged |
| Configurable timeout | Done | Per-command timeout (200ms default, 2000ms for FW_START) |
| Scatter send (q16 FWDL ring) | Done | Raw data without TXD, matching upstream |
| Scatter send (q15 WM alt path) | Done | TXD-wrapped with CID=0xEE, Windows driver compat |

### 1.6 Firmware Download

| Item | Status | Notes |
|------|--------|-------|
| Patch firmware loading | Done | `WIFI_MT6639_PATCH_MCU_2_1_hdr.bin` |
| Patch semaphore get/release | Done | CID 0x10 (SEM_CONTROL) |
| Patch init_download | Done | CID 0x05 (PATCH_START) with addr/len/mode |
| Patch scatter transfer | Done | Chunked transfer, max_len=2048 (patch) |
| Patch finish | Done | CID 0x07 (PATCH_FINISH) |
| Patch encryption handling | Done | AES / scramble / key_idx from sec_info |
| RAM firmware loading | Done | `WIFI_RAM_CODE_MT6639_2_1.bin` |
| RAM init_download per region | Done | CID 0x01 (TARGET_ADDRESS_LEN) with addr/len/mode |
| RAM scatter transfer | Done | 4096-byte chunks (upstream PCIe size) |
| RAM encryption handling | Done | FW_FEATURE_SET_ENCRYPT, ENCRY_MODE, KEY_IDX |
| FW_START_OVERRIDE (CID 0x02) | Done | option=1, addr=override_addr, 2000ms timeout |
| fw_sync polling | Done | Poll CONN_ON_MISC bits[1:0]=3, 1500ms timeout |
| Region feature parsing | Done | NON_DL skip, OVERRIDE_ADDR capture |

**Firmware boot result**: `fw_sync=0x00000003` (FW N9 READY)

### 1.7 Diagnostics & Debug

| Item | Status | Notes |
|------|--------|-------|
| Register read/write bounds check | Done | Prevents out-of-BAR0 access |
| Bridge state dump | Done | SLP_STS, FSM, HIF_BUSY, CTRL0, SLPCTRL |
| DMA ring state dump | Done | Per-ring BASE/CNT/CIDX/DIDX |
| MCU DMA0/DMA1 dump | Done | MCU-side ring state, EXT_CTRL |
| MCU command hex dump | Done | TXD + payload per command |
| MCU event hex dump | Done | Full RX event data |
| SLP_STS transition timing | Done | Microsecond-resolution after INIT_DONE |
| Windows key register dump | Done | Matching values from Ghidra RE |
| MIB counter dump | Done | TX/RX DMAD counters |
| DMASHDL state dump | Done | Group/queue mapping, page config |
| Error state diagnostics | Done | GLO_CFG2, ERR_INT_STA, TIMEOUT_CFG on timeout |
| RX ring DIDX/CIDX dump all | Done | All 8 RX rings on event timeout |

### 1.8 Module Cleanup

| Item | Status | Notes |
|------|--------|-------|
| DMA ring free | Done | All allocated rings freed |
| WFSYS reset on remove | Done | Clean state for next load |
| BAR0 unmap | Done | `pci_iounmap()` |
| PCI region release | Done | `pci_release_regions()` |
| PCI device disable | Done | `pci_disable_device()` |

---

## Part 2: Unimplemented Features

### 2.1 Post-Boot MCU Initialization (CRITICAL - Next Step)

The firmware is running, but the driver hasn't sent any post-boot MCU commands.
Upstream `mt7925_run_firmware()` does the following after firmware boot:

| Step | Upstream Function | MCU Command | Purpose |
|------|-------------------|-------------|---------|
| 1 | `mt7925_mcu_get_nic_capability()` | `MCU_UNI_CMD(CHIP_CONFIG)` | Query antenna config, band caps, MAC address |
| 2 | `mt7925_load_clc()` | `MCU_UNI_CMD(PATCH_FINISH_REQ)` variant | Load country/region calibration data |
| 3 | `mt7925_mcu_fw_log_2_host()` | `MCU_UNI_CMD(WSYS_CONFIG)` | Enable firmware debug logging |
| 4 | `mt7925_mcu_set_eeprom()` | `MCU_UNI_CMD(EFUSE_CTRL)` | Configure EEPROM/eFuse access mode |
| 5 | `mt7925_mac_init()` | Direct register writes | RX buffer size, WTBL clear, band init, rate table |

**Difficulty**: Medium-High. Need to implement UNI_CMD message format (different from download-phase commands). Upstream uses `mt76_mcu_send_and_get_msg_v2()` with structured TLV payloads.

### 2.2 UNI_CMD Message Format

Current MCU command interface only supports the download-phase format (CID-based, simple payload). Post-boot firmware uses a different format:

| Feature | Download Phase (Done) | Post-Boot (TODO) |
|---------|----------------------|-------------------|
| Command format | CID + flat payload | UNI_CMD + TLV tags |
| TXD header | 48-byte connac2_mcu_txd | Same, but with UNI_CMD wrapper |
| Response format | Simple 12-byte event | Variable-length TLV response |
| Sequence tracking | 4-bit seq | 4-bit seq + async event handling |
| Direction | H2N only | H2N + N2H (unsolicited events) |

**Need to implement**:
- `struct mt7925_uni_txd` header format
- TLV (Tag-Length-Value) payload builder
- Response TLV parser
- UNI_CMD ID mapping (upstream `mt76_connac_mcu.h` lines 1097+)

### 2.3 MAC Layer Initialization

Upstream `mt7925_mac_init()` (`mt7925/init.c:78`):

| Step | Register/Action | Purpose |
|------|----------------|---------|
| 1 | `MT_MDP_BNRCFR0/1` | Configure de-AMSDU, band receiver |
| 2 | `MT_SEC_SCR_MAP0` | Set security translation table |
| 3 | WTBL clear | Zero out wireless transmit block table |
| 4 | Band config (x2) | Initialize 2.4G and 5G/6G bands |
| 5 | Rate table | Set basic rate configuration |

### 2.4 Data Path TX/RX Rings

Current driver only has MCU rings. Data path needs:

| Ring | Type | Purpose | Status |
|------|------|---------|--------|
| TX q0 (BAND0) | TX data | 802.11 frame transmission | Not allocated |
| RX q0 (MCU_WM) | RX MCU | MCU WM event reception | Dummy only |
| RX q2 (BAND0) | RX data | 802.11 frame reception | Not allocated |
| TX NAPI | Polling | TX completion processing | Not implemented |
| RX NAPI | Polling | RX data processing | Not implemented |

### 2.5 Interrupt Handling

| Item | Status | Notes |
|------|--------|-------|
| IRQ handler registration | Not done | Need `devm_request_irq()` |
| IRQ tasklet | Not done | Bottom-half processing for DMA completion |
| NAPI poll | Not done | Network packet processing |
| MSI-X support | Not done | Multi-queue interrupt routing |

Currently all DMA completion is poll-based in the probe path. Real operation requires interrupt-driven processing.

### 2.6 ieee80211 / mac80211 Integration

| Item | Status | Notes |
|------|--------|-------|
| `ieee80211_alloc_hw()` | Not done | Allocate wireless hardware struct |
| `ieee80211_register_hw()` | Not done | Register with kernel wireless stack |
| `wiphy` configuration | Not done | Bands, rates, HT/VHT/HE/EHT caps |
| `ieee80211_ops` callbacks | Not done | add_interface, config, tx, etc. |
| MAC address assignment | Not done | From EEPROM/eFuse via MCU |
| Regulatory domain | Not done | Country code, channel list |
| HE/EHT capability setup | Not done | WiFi 6E/7 feature advertisement |
| MLO (Multi-Link Operation) | Not done | WiFi 7 specific |

### 2.7 Station Mode Operations

| Item | Status | Notes |
|------|--------|-------|
| Scan | Not done | MCU-assisted channel scan |
| Authentication | Not done | Open/WPA/WPA2/WPA3 |
| Association | Not done | BSS join/leave |
| Key management | Not done | PTK/GTK install via MCU |
| Power save | Not done | PS-Poll, U-APSD |
| Roaming | Not done | Fast BSS transition |

### 2.8 TX/RX Data Path

| Item | Status | Notes |
|------|--------|-------|
| TX frame formatting | Not done | 802.11 header, AMSDU, QoS |
| TX DMA submission | Not done | Ring buffer management |
| TX completion | Not done | Free buffers, update stats |
| RX DMA processing | Not done | Ring buffer consumption |
| RX frame parsing | Not done | 802.11 header, decap |
| RX reordering | Not done | Block-ACK reorder buffer |

### 2.9 Power Management

| Item | Status | Notes |
|------|--------|-------|
| Runtime PM | Not done | `CONFIG_PM` support |
| Deep sleep | Not done | MCU_UNI_CMD deep sleep config |
| WoWLAN | Not done | Wake-on-Wireless-LAN patterns |
| ASPM L0s disable | Not done | Upstream does this in pci_mcu.c |
| Suspend/Resume | Not done | System sleep integration |

### 2.10 Miscellaneous

| Item | Status | Notes |
|------|--------|-------|
| Thermal monitoring | Not done | Temperature sensor, throttling |
| debugfs interface | Not done | Runtime debug filesystem |
| LED control | Not done | Activity LED |
| EEPROM/eFuse | Not done | Calibration data access |
| Coexistence (BT) | Not done | WiFi/BT coex management |
| Firmware log relay | Not done | MCU debug log forwarding |

---

## Part 3: Architecture & Key Decisions

### 3.1 Solved Problems (Historical)

| Problem | Root Cause | Solution |
|---------|-----------|----------|
| HIF_BUSY deadlock | q16 DMA probe kick puts data MCU can't process | `skip_dma_probe=1` (skip probe entirely) |
| MCU crash at FW_START (0xdead1234) | `ram_dl_mode` missing encryption flags | Added DL_MODE_ENCRYPT + DL_MODE_RESET_SEC_IV |
| dmesg buffer overflow | Per-chunk kick dumps (5 lines x 911 chunks) | Suppress dumps for qid==16 |
| Event length extraction | Used `ctrl & 0xffff` (wrong bits) | `FIELD_GET(MT_DMA_CTL_SD_LEN0, ctrl)` |
| CLR_OWN failure | OWN_SYNC (BIT(2)) never clears in some paths | Retry with 2ms delay, up to 100 retries |

### 3.2 Hardware Constraints

| Constraint | Impact |
|-----------|--------|
| PCIe FLR puts device in D3cold | Never use `pcie_flr()`, need PCI rescan to recover |
| `pci_reset_function()` deadlocks in probe | Cannot use standard PCI reset during probe |
| VDNR register (0xfE06C) returns 0x87654321 | Dead on PCIe variant, don't rely on it |
| SLP_STS=0x07770313 re-appears ~26us after INIT_DONE | Does NOT block DMA (proven), can be ignored for now |
| CONN_HW_VER reads 0x80000000 (expect 0x20010000) | Register mapping differs from SoC variant |

### 3.3 Proven Working Configuration

```
insmod mt7927_init_dma.ko \
    skip_dma_probe=1 \
    use_upstream_txd=1 \
    use_mt6639_init=1 \
    use_wfsys_reset=1 \
    force_wf_reset=1 \
    evt_ring_qid=6 \
    fix_mcu_rx_cidx=1 \
    disable_ext0_dmashdl=1 \
    use_emi_slpprot=1 \
    wait_mcu_event=1 \
    scatter_via_wm=0
```

### 3.4 Reference Code Sources

| Source | Path | Usage |
|--------|------|-------|
| Upstream mt76/mt7925 | `mt76/mt7925/` | Primary reference for register sequences |
| Upstream mt76 common | `mt76/mt76_connac_mcu.c/h` | MCU command format, FW download |
| Vendor mobile driver | `mt6639/` (if available) | CONNAC3X-specific register maps |
| Windows driver (Ghidra) | `WiFi_AMD-MediaTek_v5.7.0.5275/` | Register values, init sequences |

---

## Part 4: Recommended Next Steps (Priority Order)

### Step 1: UNI_CMD Infrastructure
Implement the post-boot MCU command format (UNI_CMD + TLV). This is the gateway to all further initialization. Reference: `mt76/mt7925/mcu.c` message construction functions.

### Step 2: NIC Capability Query
Send `MCU_UNI_CMD(CHIP_CONFIG)` to get MAC address, antenna config, band capabilities. This validates the MCU command interface works post-boot.

### Step 3: EEPROM & MAC Init
Configure eFuse access and initialize MAC registers. This gets the hardware into a state where it can process frames.

### Step 4: ieee80211 Registration
Register with mac80211 to create a wireless interface (`wlan0`). Even without full TX/RX, this makes the device visible to userspace.

### Step 5: Data Path & Scan
Implement TX/RX data rings, NAPI polling, and scan capability. This is where the device becomes usable for connecting to networks.
