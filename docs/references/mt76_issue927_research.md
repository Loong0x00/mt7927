# MT76 Issue #927 Community Research: MT7927 Linux WiFi Driver

**Research Date**: 2026-02-15
**Issue URL**: https://github.com/openwrt/mt76/issues/927
**Status**: Open (74+ comments since 2024-10-31)
**Created by**: markmann447

---

## Executive Summary

The MT7927 is confirmed to be an MT6639 mobile chip in PCIe packaging. There is NO official Linux WiFi driver. Multiple community members have attempted various approaches:

1. **zouyonghao** got the furthest on WiFi -- achieved HW init, WFSYS reset, MCU_IDLE (0x1D1E), firmware loading (MAC address from EFUSE), but could NOT get MCU commands working (no interrupts received)
2. **Loong0x00** did extensive Ghidra RE of Windows driver v5705275, identified critical DMA/register differences vs mt76/mt7925
3. **nvaert1986** made significant progress on Bluetooth, firmware loading works but BT function enable (WMT opcode 0x06) times out
4. **clemenscodes / jfmarliere** -- Bluetooth WORKS via simple kernel patch (btusb/btmtk changes)
5. **hmtheboy154** maintains gen4-mt7902 (similar vendor-module approach), suggested comparing mt7925 vs mt6639 in gen4m
6. **dubhater** offered to write the driver for payment; suggested firmware rename trick (MT6639 -> MT7925)
7. **Our project** is at fw_sync=0x3 with MCU_RX0 BASE=0 blocker

**Key finding**: zouyonghao reached a very similar state to ours -- FW loads, MCU_IDLE achieved, but MCU commands time out. This validates our hypothesis that the issue is in post-FWDL initialization, not firmware download itself.

---

## Timeline of Key Events

| Date | Who | What |
|------|-----|------|
| 2024-10-31 | markmann447 | Issue opened |
| 2025-08-05 | dubhater | Found MTK_modules repo with MT6639 PCIe code |
| 2025-09-01 | marcin-fm | Extracted MT6639 firmware from Windows driver, wrote unmtk.rb |
| 2025-10-30 | dubhater | Suggested firmware rename trick (MT6639->MT7925) |
| 2025-10-31 | zouyonghao | **First HW init success** -- CBInfra remap, WFSYS reset, MCU_IDLE |
| 2025-10-31 | zouyonghao | **Firmware loaded** -- MAC from EFUSE working |
| 2025-11-01 | nvaert1986 | BT firmware loaded, stuck at WMT FUNC_CTRL (opcode 0x06) |
| 2025-11-05 | nvaert1986 | Published btmtk_usb_mt6639.c driver attempt |
| 2025-11-07 | zouyonghao | "Spent hours trying WLAN, failed -- don't know wlanSendInitSetQueryCmd" |
| 2025-11-09 | zouyonghao | Switched to gen4m vendor module approach (also doesn't work) |
| 2025-11-11 | hmtheboy154 | Suggested comparing mt7925.c vs mt6639.c in gen4m |
| 2025-12-07 | marcin-fm | Found MT7995 card has subsystem ID 0x6639 (puzzling) |
| 2026-02-12 | Loong0x00 | Published extensive Windows driver RE analysis (14 documents) |
| 2026-02-13 | seebeen | Posted jfmarliere's BT driver repo |
| 2026-02-14 | clemenscodes | **Bluetooth WORKING** via simple kernel patch |
| 2026-02-14 | lupusbytes | Confirmed BT patch works on Gentoo |

---

## Approach 1: zouyonghao's mt7925e-based Driver (MOST RELEVANT TO US)

**Repo**: https://github.com/zouyonghao/mt7927
**Status**: HW init works, FW loads, MCU commands fail

### What Works
- CBInfra remap registers configured
- WFSYS reset via CB_INFRA_RGU (same as our approach!)
- MCU_IDLE state achieved (ROMCODE_INDEX = 0x1D1E)
- Firmware loaded successfully
- MAC address read from EFUSE (34:50:02:70:01:00)
- Antenna config detected (2x2, 2G+5G)

### What Fails
- **MCU command timeout**: "Message 00000010 (seq 1) timeout" during patch semaphore acquisition
- No interrupts received from MCU
- Cannot get past firmware download phase initially, then after fixes:
- Cannot get WiFi scan or any MCU-driven functionality working

### Key dmesg Output
```
mt7925e: MT7927 detected, initializing CBInfra remap registers
MT7927: CBTOP_PCIE_REMAP_WF    = 0x74037001
MT7927: CBTOP_PCIE_REMAP_WF_BT = 0x70007000
MT7927: Setting PCIE2AP remap
MT7927: PCIE2AP_REMAP_WF_1_BA = 0x18051803
ASIC revision: 720072
MT7927: CONN_INFRA VERSION = 0x03010002 - OK
MT7927: Performing WF/BT subsystem reset
MT7927: WF/BT subsystem reset completed
MT7927: Reinitializing WPDMA after subsystem reset
MT7927: MCU IDLE (0x00001d1e)
MT7927: Hardware initialization completed
MT7927: Loading firmware
Message 00000010 (seq 1) timeout    <-- FAILURE POINT
Failed to get patch semaphore
```

### Critical Observation
zouyonghao later got firmware fully loaded (MAC from EFUSE visible), meaning the DMA path for FWDL works. But MCU commands (via MCU_RX0/RX1 rings) never get responses. This is EXACTLY our blocker (MCU_RX0 BASE=0).

---

## Approach 2: Loong0x00's Windows Driver RE (v5705275)

**Published**: 2026-02-12
**14 analysis documents attached to issue comment 3893582694**

### Key Findings: Register Map Differences

#### WPDMA_GLO_CFG (0x7c024208)
- **Windows MT6639**: Sets `0x5` (TX_DMA_EN | RX_DMA_EN only)
- **Windows MT7925**: Sets `0x4000005` (includes bit 22)
- **Linux mt76**: Uses symbolic constants, unclear if bit 22 is set
- **NOTE**: MT6639 does NOT set bit 22! This may be important.

#### MSI Configuration (CRITICAL -- MISSING FROM LINUX)
Windows writes these BEFORE any DMA operations:
```
0x7c0270f0 = 0x00660077  (MSI_INT_CFG0)
0x7c0270f4 = 0x00001100  (MSI_INT_CFG1)
0x7c0270f8 = 0x0030004f  (MSI_INT_CFG2)
0x7c0270fc = 0x00542200  (MSI_INT_CFG3)
```
These registers are NEVER written by the Linux mt76 driver.

#### WPDMA_GLO_CFG_EXT1 (0x7c0242b4)
- Windows: `|= 0x10000000` (bit 28)
- Linux: No corresponding write found in mt7925 code

#### Interrupt Priority Registers
```
0x7c024298 = 0x0F00  (INT_RX_PRI_SEL)
0x7c02429c = 0x7F00  (INT_TX_PRI_SEL)
```

#### WFDMA_HOST_CONFIG (0x7c027030)
- Windows: Read-modify-write during init
- Linux: Defined but never written

### Key Findings: DMA Ring Architecture

#### TX/CMD Ring Addresses
```
TX/CMD rings: 0x7c024300 - 0x7c03000c
FWDL/Event rings: 0x7c024500 - 0x7c05050c
```

#### Ring Configuration from gen4m (Motorola)
**TX Rings (17 total):**
- Ring 0-3: AP data (primary, secondary, priority, altx)
- Ring 8-11: MD data paths
- Ring 14: MD command
- Ring 15: AP command
- **Ring 16: Firmware download**

**RX Rings (12 total):**
- Ring 4-5: AP data bands 0-1
- **Ring 6: AP events/transmit done** (this is where MCU responses come!)
- Ring 7: ICS logging
- Ring 8-11: MD data/events

### Key Findings: Firmware Download State Machine
- Windows polls `0x7c0600f0` for state value `3` (ready)
- Up to 500 iterations at 1ms intervals
- Our equivalent: fw_sync register

### Key Findings: MCU Command Architecture
- Command type byte `0xed` used as target
- Scatter packages (CID=0xee) get special encoding: `hdr + 0x24 = 0xa000`
- 58 command entries in dispatch table at 0x1402507e0
- 13-byte stride per entry
- Token-based tracking with event synchronization
- **Queue model with 20 slots (stride 0x60), NOT simple ring doorbell**

### Key Findings: Initialization Sequence (v5603998/v5705275)
```
1. MT6639PreFirmwareDownloadInit
2. AsicConnac3xWfdmaWaitIdle (4 register polls)
3. AsicConnac3xWpdmaInitRing (6 iterations for multiple DMA channels)
4. Firmware loading (AsicConnac3xLoadFirmware / LoadRomPatch)
5. MT6639InitTxRxRing (enables operational rings)
6. MT6639WpdmaConfig (runtime DMA settings)
7. MT6639ConfigIntMask (interrupt enable)
```

### Key Findings: MCU Send Backend Selection
```
if (dev_id == 0x6639 || dev_id == 0x7927 || dev_id == 0x7925) {
    if (flag_at_0x146cde9 == 0x01)
        FUN_14014e644();  // New path
    else
        MtCmdSendSetQueryCmdAdv();  // Legacy path
}
```

The legacy path (`MtCmdSendSetQueryCmdAdv`) handles FW download commands: `0x01, 0x02, 0x03, 0x05, 0x07, 0x10, 0x11, 0xee, 0xef`

### MCU Command Header Format (from MtCmdSendSetQueryCmdAdv)
```
Offset +0x20: length = payload_len + 0x20
Offset +0x24: set/query dimension
Offset +0x25: 0xa0 (fixed marker)
Offset +0x27: token/seq from FUN_14009a46c()
Offset +0x40+: payload data
```

For scatter (CID=0xee):
```
Offset +0x24: 0xa000
Offset +0x27: 0
```

### Analysis Documents (All from comment 3893582694)
1. `mt76_vs_windows_mt7927.md` -- Register differences mt76 vs Windows
2. `mt7925_vs_windows_mt7927.md` -- Register differences mt7925 vs Windows
3. `TEST_RESULTS_SUMMARY.md` -- 23 test modules, all fail at FW_STATUS=0xffff10f1
4. `windows_register_map.md` -- Complete WPDMA/WFDMA/MSI register map
5. `windows_v5705275_deep_reverse.md` -- Key functions and addresses
6. `windows_v5705275_initial_findings.md`
7. `windows_v5705275_mcu_cmd_backend.md` -- MCU command routing
8. `win_v5603998_fw_flow.md` -- Older driver FW flow
9. `win_v5705275_core_funcs.md`
10. `win_v5705275_dma_enqueue.md` -- DMA enqueue with 0x60-byte descriptors
11. `win_v5705275_dma_lowlevel.md`
12. `win_v5705275_fw_flow.md`
13. `win_v5705275_fw_proto_funcs.md`
14. `win_v5705275_helpers.md`
15. `win_v5705275_mcu_dma_submit.md` -- MCU DMA submission details
16. `win_v5705275_mcu_send_backends.md`
17. `win_v5705275_mcu_send_core.md`

---

## Approach 3: Bluetooth Driver (SOLVED)

### Working Solution
**Gitlab repo**: https://gitlab.com/jfmarliere/linux-driver-mediatek-mt7927-bluetooth
**NixOS module**: https://github.com/clemenscodes/linux-mediatek-mt6639-bluetooth-kernel-module
**Patch**: https://github.com/clemenscodes/linux-mediatek-mt6639-bluetooth-kernel-module/blob/main/patches/mt6639-bt-6.19.patch
**BT setup guide**: https://gist.github.com/max-prtsr/2e19d74e421b60fbad30b6932772e76e

### Patch Details (mt6639-bt-6.19.patch)
Changes to 3 files:
1. **btmtk.c**: Add `case 0x6639` for firmware path using `"mediatek/mt%04x/BT_RAM_CODE_MT%04x_2_%x_hdr.bin"`, plus section filtering (only dlmode=0x01 sections, skip WiFi sections to prevent hang)
2. **btmtk.h**: Add `FIRMWARE_MT7927` pointing to `"mediatek/mt6639/BT_RAM_CODE_MT6639_2_1_hdr.bin"`
3. **btusb.c**: Add USB device `{ USB_DEVICE(0x0489, 0xe13a) }` with MEDIATEK+WIDEBAND_SPEECH flags

### Critical BT Detail: Section Filtering
The BT firmware container includes WiFi sections. Sending these via BT causes chip hang. Must filter by dlmode byte (only process sections where dlmode == 0x01).

### BT Firmware Protocol
```
1. 0xFCAA -- Device reset
2. 0xFCC0 -- Power on subsystem
3. 0xFC77 -- Query firmware version
4. 0xFC6F -- Download section 0 (64KB at 0xE0900000)
5. 0xFC6F -- Download section 1 (54KB at 0x00900000)
6. 0xFC77 -- Verify load
7. 0xFCAA -- Reset to activate firmware
```

MT6639 loads only 2 sections (118KB total), vs MT7925 which loads 6 sections (556KB).

### Confirmed Working By
- clemenscodes (NixOS, ASUS motherboard)
- syabro (using Claude Code to apply)
- lupusbytes (Gentoo, ASUS ProArt X870E)

---

## Approach 4: gen4m Vendor Module (zouyonghao + hmtheboy154)

**gen4-mt7902 repo**: https://github.com/hmtheboy154/gen4-mt7902
**zouyonghao's gen4m fork**: https://github.com/zouyonghao/mt7927 (test branch)

### Strategy
Compile MediaTek's own vendor driver source (from Android BSPs) for MT6639.

### Sources for gen4m Code
1. **Fede2782/MTK_modules**: https://github.com/Fede2782/MTK_modules (Xiaomi Rodin BSP)
   - Original: https://github.com/MiCode/MTK_kernel_modules
2. **Motorola gen4m** (NEWER, has commit history): https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-wlan-core-gen4m
3. **MT6639 BT chip code**: https://github.com/Fede2782/MTK_modules/blob/bsp-rodin-v-oss/connectivity/bt/linux_v2/chip/btmtk_chip_6639.c

### PCI Device IDs from gen4m
```c
#define NIC6639_PCIe_DEVICE_ID1 0x3107
#define NIC6639_PCIe_DEVICE_ID2 0x6639
// Also: NIC7927_PCIe_DEVICE_ID (linked to mt66xx_driver_data_mt6639)
```

### gen4m Ring Configuration (Motorola MT6639)
```
TX Rings: 17 total
  0-3: AP data, 8-11: MD data, 14: MD cmd, 15: AP cmd, 16: FWDL
RX Rings: 12 total
  4-5: AP data bands, 6: AP events, 7: ICS, 8-11: MD
```

### Status
- Compiles but doesn't work correctly
- Device identified but fails to initialize properly
- "It can't turn on properly on the first try" (hmtheboy154's diagnosis)

---

## Approach 5: Firmware Rename Trick (dubhater's suggestion)

### Idea
Rename MT6639 firmware to MT7925 filenames, use existing mt7925e driver:
```
WIFI_RAM_CODE_MT6639_2_1.bin -> WIFI_RAM_CODE_MT7925_1_1.bin
WIFI_MT6639_PATCH_MCU_2_1_hdr.bin -> WIFI_MT7925_PATCH_MCU_1_1_hdr.bin
echo "14c3 6639" > /sys/bus/pci/drivers/mt7925e/new_id
```

### Result
Not extensively tested for WiFi. The MT7925 driver lacks MT6639-specific initialization (CBInfra remap, different WFSYS reset, different register programming).

---

## Key Reference Repositories

| Repo | What | Status |
|------|------|--------|
| https://github.com/zouyonghao/mt7927 | mt7925e-based WiFi driver | HW init + FW load work, MCU cmds fail |
| https://github.com/nvaert1986/btmtk-temp | BT driver + analysis docs | FW loads, function enable fails |
| https://gitlab.com/jfmarliere/linux-driver-mediatek-mt7927-bluetooth | BT kernel patch | WORKING |
| https://github.com/clemenscodes/linux-mediatek-mt6639-bluetooth-kernel-module | NixOS BT module | WORKING |
| https://github.com/hmtheboy154/gen4-mt7902 | gen4-mt79xx vendor module | Template for MT7927 |
| https://github.com/Fede2782/MTK_modules | MTK vendor modules (Xiaomi) | Reference code |
| https://github.com/MotorolaMobilityLLC/vendor-mediatek-kernel_modules-connectivity-wlan-core-gen4m | Motorola gen4m | Latest vendor code |
| https://github.com/MiCode/MTK_kernel_modules | Original MTK source | Xiaomi BSP |
| https://gist.github.com/marcin-fm/e53840817dea293f4ebe9176578ded9a | unmtk.rb firmware extractor | Working tool |
| https://gist.github.com/max-prtsr/2e19d74e421b60fbad30b6932772e76e | BT setup guide | Working guide |

---

## Implications for Our MCU_RX0 BASE=0 Blocker

### What the Community Confirms
1. **zouyonghao hit the EXACT same wall**: FW loads, MCU_IDLE achieved, but MCU commands time out. This is NOT unique to our approach.
2. **Multiple people confirmed**: The chip IS MT6639, NOT a variant of MT7925. Different register programming needed.
3. **Windows driver uses completely different post-FWDL init**: MT6639PreFirmwareDownloadInit, MT6639InitTxRxRing, MT6639WpdmaConfig, MT6639ConfigIntMask are all separate from MT7925 code paths.

### NEW Information from Loong0x00's RE

#### Missing MSI Configuration (HIGH PRIORITY)
Windows writes these 4 MSI registers that we never touch:
```
BAR0+0x100f0 -> 0x00660077
BAR0+0x100f4 -> 0x00001100
BAR0+0x100f8 -> 0x0030004f
BAR0+0x100fc -> 0x00542200
```
(Bus addresses: 0x7c0270f0 - 0x7c0270fc)

Without MSI configuration, the chip may never generate interrupts, which would explain:
- MCU_RX0 never getting configured by FW (FW can't signal completion)
- MCU commands timing out (no interrupt to deliver response)

#### WFDMA_HOST_CONFIG (0x7c027030) Never Written
Windows does read-modify-write on this. We never touch it.

#### GLO_CFG_EXT1 bit 28 (0x7c0242b4)
Windows explicitly sets `|= 0x10000000`. We may be missing this.

#### Firmware Sync Register
Windows polls `0x7c0600f0` for value 3. Need to verify this is the same as our fw_sync.

### NEW Information from gen4m (Motorola)

#### Ring 6 = AP Events
In the gen4m architecture, **RX Ring 6 is the AP event ring** (MCU responses). This may correspond to what mt76 calls MCU_RX0. The gen4m has a completely different ring numbering scheme.

#### Manual Prefetch Configuration
gen4m configures prefetch via EXT_CTRL registers with specific base addresses:
```
0x00400000, 0x01000000, 0x00800000
```
This may be needed for proper DMA operation.

#### DMASHDL
The gen4m driver has extensive DMASHDL (DMA Scheduler) configuration that we're largely missing. Our only DMASHDL write is `0xd6060 |= 0x10101`.

### Hypothesis Updates

1. **MSI configuration is likely critical**: Without it, the chip may not route interrupts, preventing FW from signaling DMA ring configuration completion
2. **Post-FWDL ring init is a separate phase**: Windows has `MT6639InitTxRxRing` as distinct from the FWDL ring setup. We may need to explicitly configure operational rings AFTER firmware boots.
3. **The "cannot receive interrupts" problem is universal**: zouyonghao, Loong0x00, and we all hit this. It's the core blocker for the entire community.
4. **Command encapsulation matters**: Windows uses `MtCmdSendSetQueryCmdAdv` with specific header format, not bare DMA writes. The 0xa0 marker at offset +0x25 and token at +0x27 may be important.

---

## Recommended Next Steps

1. **Try MSI register configuration** (0x7c0270f0-fc) BEFORE any MCU commands
2. **Write WFDMA_HOST_CONFIG** (0x7c027030) during init
3. **Set GLO_CFG_EXT1 bit 28** (0x7c0242b4 |= 0x10000000)
4. **Compare our gen4m ring layout** vs what we're programming
5. **Study Motorola gen4m mt6639.c** for the complete initialization sequence
6. **Contact zouyonghao** -- they got the furthest and may have unpublished findings
7. **Download and study Loong0x00's full analysis files** -- 14 documents with extensive RE data
8. **Try the gen4m vendor module approach** as an alternative path

---

## Notable Quotes

> "I tried analyzing it from another angle... The difference between the 7927 and 7925 is definitely not just the addition of 320MB; the underlying registers have been significantly modified, and almost all operations use DMA without consuming memory." -- Loong0x00

> "Spend several hours trying to make wlan work but failed... It seems we didn't know many functions in the Android repo, e.g., `wlanSendInitSetQueryCmd`" -- zouyonghao

> "with the fact that mt76 team ignored any request on mailing list or a direct email, it seems like we have to work on these on our own" -- hmtheboy154

> "I kept getting stuck on DMA issues. The community won't be able to create a viable alternative until this problem is solved." -- Loong0x00

> "Current observations align with missing Windows-required structures: Ring16 consumption stalls during scatter phases, HOST_INT_STA remains 0x00000000 with no MCU_CMD feedback" -- Loong0x00's analysis

---

## Windows Driver Versions Analyzed by Community

| Version | Analyzed By | Notes |
|---------|-------------|-------|
| v5603998 | Our project (Ghidra) | Older version, our primary RE target |
| v5705275 | Loong0x00 (Codex/Ghidra) | Newer version, 14 analysis docs published |
| ASUS ROG Forum driver | marcin-fm | Firmware extraction |

---

## Appendix: Complete Windows Register Map (from Loong0x00)

### WPDMA Core (BAR2 + 0x0200)
```
0x7c024100  CONN_HIF_RST
0x7c024150  HOST_TX_INT_PCIE_SEL
0x7c024154  HOST_RX_INT_PCIE_SEL
0x7c024200  HOST_INT_STA
0x7c024204  HOST_INT_ENA
0x7c024208  WPDMA_GLO_CFG           (MT6639: 0x5, MT7925: 0x4000005)
0x7c02420c  (Ring base)             Written 0xffffffff
0x7c024238  HOST_INT_STA_EXT
0x7c02423c  HOST_INT_ENA_EXT
0x7c024280  (Ring mask)             Written 0xffffffff
0x7c024284  WPDMA_INFO
0x7c024288  WPDMA_INFO_EXT
0x7c024298  INT_RX_PRI_SEL          Written 0x0F00
0x7c02429c  INT_TX_PRI_SEL          Written 0x7F00
0x7c0242b0  GLO_CFG_EXT0
0x7c0242b4  GLO_CFG_EXT1            |= 0x10000000
0x7c0242b8  GLO_CFG_EXT2
0x7c0242e8  HOST_PER_DLY_INT_CFG
0x7c0242f0  PRI_DLY_INT_CFG0
0x7c0242f4  PRI_DLY_INT_CFG1
0x7c0242f8  PRI_DLY_INT_CFG2
0x7c0242fc  PRI_DLY_INT_CFG3
```

### WFDMA/MSI (BAR2 + 0x7000)
```
0x7c027010  WFDMA_GLOBAL_INT_STA
0x7c027014  WFDMA_GLOBAL_INT_STA_1
0x7c02702c  WFDMA_MSI_CONFIG
0x7c027030  WFDMA_HOST_CONFIG       (read-modify-write in Windows)
0x7c027044  WFDMA_HIF_BUSY
0x7c027050  WFDMA_AXI_SLPPROT_CTRL
0x7c027078  WFDMA_AXI_SLPPROT0_CTRL
0x7c02707c  WFDMA_AXI_SLPPROT1_CTRL
0x7c0270ac  WFDMA_MD_INT_LUMP_SEL
0x7c0270e8  WFDMA_DLY_IDX_CFG_0
0x7c0270ec  WFDMA_DLY_IDX_CFG_1
0x7c0270f0  MSI_INT_CFG0            = 0x00660077
0x7c0270f4  MSI_INT_CFG1            = 0x00001100
0x7c0270f8  MSI_INT_CFG2            = 0x0030004f
0x7c0270fc  MSI_INT_CFG3            = 0x00542200
```

### Ring Setup Burst Ranges
```
0x7c024600 - 0x7c02460c
0x7c02463c - 0x7c024640
0x7c024680 - 0x7c02468c
```

### PCIe/AXI Debug
```
0x740311a8  AXI_PCIE_IF_CTRL
0x7403002c  PCIe debug
0x74030164  PCIe debug pair
0x74030168  PCIe debug pair
0x74030188 - 0x7403018c  PCIe debug range
0x740310e0 - 0x740310f4  PCIe debug range
```

### Firmware Status
```
0x7c0600f0  FW sync status (poll for value 3)
```

---

## Appendix: gen4m MT6639 Interrupt Configuration

From Motorola gen4m mt6639.c:

### MSI Mode Options
- **Single MSI**: Unified interrupt processing
- **8 MSI mode**: TX_DATA0, TX_DATA1, TX_FREE_DONE, RX_DATA0, RX_DATA1, EVENT, CMD, LUMP
- Auto-clear for MSI 2-5
- Deassert timer: 0x40 ticks

### Interrupt Masks
```
RX done: rings 4, 5, 6, 7
TX done: rings 0, 1, 2, 3, 15, 16
MCU SW int: CONNAC_MCU_SW_INT
Subsystem int: CONNAC_SUBSYS_INT
```

### Firmware Names
```
Primary: mt6639_wifi.bin
Flavor:  mt6639_wifi_[flavor].bin
RAM:     WIFI_RAM_CODE_MT6639_[flavor]_[version].bin
Patch:   WIFI_MT6639_PATCH_MCU_[flavor]_[version]_hdr.bin
```
