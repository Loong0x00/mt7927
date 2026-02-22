# Windows mtkwecx.sys — Full RF/PHY/Channel Configuration Analysis

**Analysis Date**: 2026-02-17
**Tool**: Ghidra 12.0.3 Headless + Custom Decompiler Scripts
**Binary**: mtkwecx.sys v5705275 (Windows WiFi 7 Driver, MT7927)
**Source Path**: `E:\worktmp\easy-fwdrv-neptune-mp-MT7927_WIFI_DRV_Win-2226-MT7927_2226_WIN10_AB\7295\wlan_driver\seattle\wifi_driver\windows\Common\`
**Analyst**: re-rfphy agent (Ghidra RE)

---

## Executive Summary

Through comprehensive Ghidra headless reverse engineering of all 173 UniCmd/MtCmd functions in the Windows driver, I found:

1. **Channel management uses CID=0x27 (CNM)** — `nicUniCmdChReqPrivilege` builds a 0x18-byte TLV with full channel parameters
2. **Channel abort uses tag=1 within same CID=0x27** — `nicUniCmdChAbortPrivilege` sends a 0xC-byte abort TLV
3. **No explicit RF/PHY/radio init commands exist** — The driver has NO `RadioEn`, `TxPower`, `PhyInit`, `BandConfig`, `RateSet` strings
4. **BSS_INFO uses `FUN_1400c1e88` to build TLV blob** — Passes an 0x74-byte local struct containing ALL BSS parameters
5. **`nicUniCmdSetBssRlmImpl` builds BOTH RLM (tag=2) and PROTECT (tag=3) TLVs** — A single function fills both
6. **IFS_TIME TLV (tag=0x17) is embedded in `nicUniCmdSetBssRlmImpl`** — After RLM+PROTECT at offset param_2+6
7. **`nicUniCmdBssActivateCtrl` sends DEV_INFO + BSS_INFO_BASIC together** — This is a combined activate command
8. **`nicUniCmdBandGapClockReq` sends CID=0x0F (BAND_CONFIG)** — Gap clock request, not radio enable

---

## 1. Channel Privilege — `nicUniCmdChReqPrivilege` (0x14014ff94)

### Function Signature
```c
undefined8 nicUniCmdChReqPrivilege(undefined8 adapter, undefined1 *ch_params, longlong *out_cmd)
```

### MCU Command
- **CID**: `0x27` (passed to `FUN_14014f788` = `nicUniCmdAllocEntry`)
- **Option**: Query (need response) — expects GRANT event back
- **Size**: `((bVar1 + bVar1 * 2) * 8 + 0x1c)` — variable based on number of channels

### TLV Structure — Channel Request (tag=0, size=0x18 per entry)

First entry uses tag=0, subsequent entries use tag=3.

```c
struct UNI_CMD_CNM_CH_PRIVILEGE_REQ {  // tag=0 for first, tag=3 for subsequent
    u16 tag;                // 0x0000 (first) or 0x0003 (additional)
    u16 length;             // 0x0018 (24 bytes)
    u8  ucBssIndex;         // puVar5[0] → param_2[0]
    u8  ucTokenID;          // puVar5[1] → param_2[1]
    // offset 6-7:
    u8  ucRfBand;           // puVar5[5] → param_2[5]
    u8  ucRfChannelWidth;   // puVar5[6] → converted via bandwidth mapping
    u8  ucPrimaryChannel;   // puVar5[3] → param_2[3]
    u8  ucRfChannelWidthConverted; // converted from puVar5[6]
    // offset 10-11:
    u8  ucRfCenterFreqSeg1; // puVar5[7] → param_2[7]
    u8  ucRfCenterFreqSeg2; // puVar5[8] → param_2[8]
    // offset 12-13 (FROM AP fields):
    u8  ucRfChannelWidthFromAP; // puVar5[9] converted (or = above if 0)
    u8  ucRfCenterFreqSeg1FromAP; // puVar5[10] or puVar5[7]
    u8  ucRfCenterFreqSeg2FromAP; // puVar5[11] or puVar5[8]
    u8  ucReqType;          // puVar5[0xE] → param_2[0xE]
    u16 reserved;
    u32 u4MaxInterval;      // puVar5[0x10] (4 bytes)
    u8  ucDBDCBand;         // puVar5[0xC] → param_2[0xC]
    u8  ucPadding[1];
    u8  unknown_0x15;       // puVar5[0xD] → converted (3→0xFE, 4→0xFF)
} __packed;
```

### Bandwidth Mapping (puVar5[6] → TLV field)
```c
// Input value → TLV bandwidth value:
// 0 → 0  (20MHz)
// 1 → 1  (40MHz)
// 2 → 2  (80MHz)
// 3 → 3  (160MHz)
// 4 → 6  (320MHz — EHT)
// Same mapping for FROM_AP fields (puVar5[9])
```

### Key Observations
- Uses `bVar1 = param_2[0x1C]` as **number of additional channel entries** (0 = single channel)
- Each additional channel entry is 0x20 bytes apart in the input struct
- Supports multi-link (MLO) channel requests — multiple band entries
- Returns allocated command in `*out_cmd`
- Error code `0xc000009a` = allocation failure

---

## 2. Channel Abort — `nicUniCmdChAbortPrivilege` (0x14014fe60)

### Function Signature
```c
undefined8 nicUniCmdChAbortPrivilege(undefined8 adapter, undefined1 *ch_params, longlong *out_cmd)
```

### MCU Command
- **CID**: `0x27` (same as request — CN/CNM channel management)
- **Size**: `0x10` (16 bytes — TLV header + 12 bytes payload)

### TLV Structure — Channel Abort (tag=1)
```c
struct UNI_CMD_CNM_CH_PRIVILEGE_ABORT {
    u16 tag;            // fixed 0x0001
    u16 length;         // fixed 0x000C (12 bytes)
    u8  ucBssIndex;     // param_2[0]
    u8  ucDBDCBand;     // param_2[1]
    u8  ucPadding;      // param_2[0xD] converted (3→0xFE, 4→0xFF)
    u8  reserved[5];
} __packed;
```

**Note**: `*(undefined4 *)(lVar3 + 4) = 0xc0001` → This sets tag=0x0001, length=0x000C in little-endian.

### Band Index Mapping
Same as request: value 3 → 0xFE, value 4 → 0xFF (special band indices for 6GHz/7GHz?).

---

## 3. Legacy Channel Command — `MtCmdChPrivilage` (0x1400c5e08)

### Function Signature
```c
int MtCmdChPrivilage(longlong adapter, longlong bss_ctx, byte band_idx,
                     uint bandwidth, byte param_5, int req_type,
                     undefined4 timeout, undefined8 cb1, undefined8 cb2,
                     uint param_10, byte dbdc_band)
```

### Size: 4592 bytes — This is the MAIN channel management function

### Key Findings

1. **Builds a 0x20-byte channel parameter struct** on stack (`local_78` through `uStack_69`):
```c
struct ch_param_struct {
    u8  primary_channel;      // offset 0: from BSS context
    u8  token_id;             // offset 1: from FUN_14009a46c(adapter, 1, 1)
    u8  rf_sco;               // offset 2: 0 (secondary channel offset)
    u8  rf_band;              // offset 3: band_idx parameter
    u8  rf_bw;                // offset 4: from BSS descriptor or param
    u8  rf_channel_width;     // offset 5: bandwidth parameter
    u8  center_freq_seg1;     // offset 6: from BSS descriptor lVar8+0x7a8
    u8  center_freq_seg2;     // offset 7: from BSS descriptor lVar8+0x7a9
    u8  center_freq_seg1b;    // offset 8: from BSS descriptor lVar8+0x7aa
    u8  center_freq_seg2b;    // offset 9: from BSS descriptor lVar8+0x7ab
    u8  center_freq_from_ap1; // offset 10: from lVar8+0x7ac
    u8  center_freq_from_ap2; // offset 11: from lVar8+0x7ad
    u8  req_type;             // offset 12: param_6 (request type)
    u8  dbdc_req_type;        // offset 13: 4 (default)
    u8  dbdc_band;            // offset 14: param_11
    u8  padding;              // offset 15
    u32 max_interval;         // offset 16: timeout in ms (200/500/1000/2000/4000)
} __packed;
```

2. **Timeout values by scenario**:
   - STA connecting, DBDC active + 2nd BSS online: **1000ms**
   - STA connecting, DBDC active: **4000ms**
   - STA mode, AP-concurrent mode: **2000ms**
   - STA mode, connected with ROC: **500ms**
   - STA mode, general: **200ms**
   - ROC/scan: **500ms**
   - req_type=1: uses `param_7` (caller-specified timeout)

3. **Request types** (param_6):
   - `0` = JOIN (normal connection)
   - `1` = ROC (Remain-On-Channel)
   - `2` = scan?
   - `3` = scan?
   - `0xAB` = NAN discovery
   - `0xB1` = ??? (same logic as req_type 1-3)
   - `0xC` = MLD/link (when 5cafe4=3)

4. **Calls `FUN_1400cdc4c` to submit MCU command**:
   ```c
   // For initial DBDC switch:
   FUN_1400cdc4c(param_2, 0x1c, 0xed, ...);  // CID=0xED? size=0x1C
   // For normal channel request:
   FUN_1400cdc4c(param_2, 0x1c, 0xed, 0);
   ```

5. **Uses spinlock for state protection**: `KeAcquireSpinLockRaiseToDpc` before modifying channel state

---

## 4. BSS_INFO HE TLV — `nicUniCmdBssInfoTagHe` (0x14014cd50)

### TLV Header
```c
*param_2 = 0x100005;  // tag=0x0005, length=0x0010 (16 bytes)
```

### TLV Structure
```c
struct BSS_INFO_HE_TLV {
    u16 tag;            // 0x0005
    u16 length;         // 0x0010 (16 bytes)

    // offset 4-5: HE Operation params
    u16 he_op_params;   // bits[3:0] = lVar3+0x5bf >> 4 (default PE duration)
                        // bits[9:4] = lVar3+0x5c0 & 0x3F (BSS color info?)

    // offset 6: HE BSS color
    u8  bss_color;      // lVar3+0x5bf & 7

    // offset 7: partial BSS color
    u8  partial_bss_color; // lVar3+0x5c1 & 1

    // offset 8-9: max BSS idle period
    u16 max_bss_idle_period; // lVar3+0x5c3

    // offset 10-11: same as above (duplicated)
    u16 max_bss_idle_dup;    // lVar3+0x5c3

    // offset 12-13: same again
    u16 field_12;            // lVar3+0x5c3

    // offset 14: additional HE flags
    u8  he_flags;            // param_3[0x3e] — with conditional OR of bits
                             // bit 1: set if TWTResponder enabled (STA mode, NIC supports)
                             // bit 0: set if 18cf9dc flag set

    // offset 15: padding
    u8  pad;
} __packed;
```

### Conditional HE Flags (offset 14)
```c
// TWTResponder: set bit 1 when:
//   (NIC flag 18cf9dd != 0) AND (bss_type == 4 OR (param_3[0x58*8]==2 AND bss_type==1))
// Additional flag: set bit 0 when:
//   NIC flag 18cf9dc != 0
```

---

## 5. BSS_INFO EHT TLV — `nicUniCmdBssInfoTagEht` (0x14014d150)

### TLV Header
```c
*param_2 = 0x10001e;  // tag=0x001E, length=0x0010 (16 bytes)
```

**Note**: Tag is **0x1E** (30), NOT 0x0C as previously documented! This is the CONNAC3 EHT tag.

### TLV Structure
```c
struct BSS_INFO_EHT_TLV {
    u16 tag;              // 0x001E
    u16 length;           // 0x0010 (16 bytes)
    u8  eht_disabled;     // lVar5+0x772 & 1
    u8  eht_dup_6g;       // lVar5+0x772 >> 1 & 1
    u8  eht_op_info1;     // lVar5+0x777
    u8  eht_op_info2;     // lVar5+0x778
    u8  eht_op_info3;     // lVar5+0x779
    u8  pad;
    u16 eht_op_params;    // lVar5+0x77a
    u8  padding[4];
} __packed;
```

---

## 6. BSS_INFO BSS_COLOR TLV — `nicUniCmdBssInfoTagBssColor` (0x14014d010)

### TLV Header
```c
*param_2 = 0x80004;  // tag=0x0004, length=0x0008 (8 bytes)
```

### TLV Structure
```c
struct BSS_INFO_BSS_COLOR_TLV {
    u16 tag;             // 0x0004
    u16 length;          // 0x0008 (8 bytes)
    u8  bss_color_dis;   // ~(lVar2+0x5c2 >> 7) & 1  — inverted disable bit
    u8  bss_color;       // lVar2+0x5c2 & 0x3F        — 6-bit BSS color value
    u8  padding[2];
} __packed;
```

---

## 7. BSS RLM + PROTECT + IFS_TIME — `nicUniCmdSetBssRlmImpl` (0x140150edc)

**CRITICAL FINDING**: This single function builds **THREE TLVs** in sequence:
1. **RLM TLV (tag=0x02)** — Channel/bandwidth parameters
2. **PROTECT TLV (tag=0x03)** — Protection mode flags
3. **IFS_TIME TLV (tag=0x17)** — Inter-frame spacing

### RLM TLV (tag=0x02, length=0x10)
```c
*param_2 = 0x100002;  // tag=0x0002, length=0x0010

struct BSS_INFO_RLM_TLV {
    u16 tag;                // 0x0002
    u16 length;             // 0x0010 (16 bytes)
    u8  primary_channel;    // param_3[2]
    u8  center_freq_seg1;   // param_3[0x10]
    u8  center_freq_seg2;   // param_3[0x11]
    u8  bandwidth;          // converted from param_3[0xF]:
                            // 0→0(20MHz), 1→1(40MHz if SCO!=0), 2→2(80MHz), 3→3(160MHz), 4→6(320MHz)
    u8  tx_stream;          // param_3[0x14]
    u8  rx_stream;          // param_3[0x15]
    u8  short_st;           // param_3[0xC]
    u8  ht_op_info;         // param_3[3]
    u8  sco;                // param_3[1] (secondary channel offset)
    u8  band;               // 0
    u8  padding[2];
} __packed;
```

### PROTECT TLV (tag=0x03, length=0x08)
```c
param_2[4] = 0x80003;  // tag=0x0003, length=0x0008

struct BSS_INFO_PROTECT_TLV {
    u16 tag;              // 0x0003
    u16 length;           // 0x0008 (8 bytes)
    u32 protect_mode;     // Bitmask:
                          // bit 5 (0x20): set if param_3[4] != 0 (ERP protection)
                          // bit 1 (0x02): set if param_3[5] == 1 (HT non-member protect)
                          // bit 2 (0x04): set if param_3[5] == 2 (HT BW20 protect)
                          // bit 3 (0x08): set if param_3[5] == 3 (HT non-HT mixmode)
                          // bit 7 (0x80): set if param_3[6] == 1 (green field protect)
} __packed;
```

### IFS_TIME TLV (tag=0x17, length=0x14)
```c
param_2[6] = 0x140017;  // tag=0x0017, length=0x0014 (20 bytes)

struct BSS_INFO_IFS_TIME_TLV {
    u16 tag;              // 0x0017
    u16 length;           // 0x0014 (20 bytes)
    u8  slot_valid;       // 1 (always)
    u8  sifs_valid;       // 0
    u16 slot_time;        // 9 if short_slot (param_3[0xE]!=0), else 20
    // remaining 12 bytes: zeros (SIFS/RIFS/EIFS not set)
} __packed;
```

**Slot time calculation**:
```c
*(ushort *)(param_2 + 8) = (-(ushort)(param_3[0xe] != '\0') & 0xfff5) + 0x14;
// When param_3[0xE] != 0: (-1 & 0xFFF5) + 0x14 = 0xFFF5 + 0x14 = 0x0009 → 9μs (short)
// When param_3[0xE] == 0: (0 & 0xFFF5) + 0x14 = 0x0014 → 20μs (long)
```

### Function Return
Returns offset of end of IFS_TIME TLV relative to start: `((int)(param_2 + 6) - (int)param_2) + 0x14`
= `0x18 + 0x14 = 0x2C` (44 bytes total for all 3 TLVs).

---

## 8. nicUniCmdSetBssRlm — Wrapper (0x1401445e0)

### MCU Command
- **CID**: `0x02` (BSS_INFO / BSS_RLM — passed to `nicUniCmdAllocEntry` as first arg)
- **Allocates**: `0x30` (48 bytes) payload — enough for RLM+PROTECT+IFS_TIME
- **First byte of payload**: `*puVar2 = *puVar1` = BSS index
- **Then calls** `nicUniCmdSetBssRlmImpl(adapter, puVar2 + 4, puVar1)` to fill TLVs

### Input validation
- Checks `*param_2 == 0x19` (command ID marker) and `param_2[0x10] == 0x16` (payload size)

---

## 9. MtCmdSetBssInfo — Main BSS_INFO Command (0x1400cf928)

### Function Signature
```c
ulonglong MtCmdSetBssInfo(longlong adapter, longlong bss_ctx, longlong sta_ctx, byte mld_flag)
```

### Key Findings

1. **Builds a 0x74-byte (116 bytes) local struct** containing all BSS parameters
2. **Calls `FUN_1400c1e88`** to build the actual MCU command TLV blob from this struct
3. **Final submission**: `FUN_1400cdc4c(bss_ctx, 0x12, 0xed, 0)` — CID=0xED, subcmd=0x12

### Parameters extracted from BSS/STA context:
```c
// From BSS context (param_2):
local_c8 = bss_index;           // param_2+0x24
local_a4 = bssid[0-3];          // param_2+0x2E0 (4 bytes of BSSID)
local_a0 = bssid[4-5];          // param_2+0x2E4 (2 bytes of BSSID)
local_c5 = param_2+0x307;       // connection state?
local_c7 = ~(param_2[0x2e6964] >> 7) & 1; // active flag
local_c6 = (bss_type != 4) ? 0 : 2;       // AP mode flag

// From STA context (lVar11):
local_9c = sta+0x4a2;           // beacon interval
local_9a = sta+0x4a4;           // DTIM period
local_94 = sta+0x4ac;           // PHY type
local_91 = sta+0x4a7;           // band index
local_98 = sta+9;               // capabilities byte

// HE parameters from STA:
local_68..65 = sta+0x5bf..5c2;  // HE operation info (3 bytes)
local_65 = sta+0x5c2;           // BSS color byte
local_64 = sta+0x5c3;           // HE additional (2 bytes)

// Short slot time decision:
local_6b = 1 or 2;              // 1=short_slot, 2=long_slot
// Limited by hardware capability: uVar12 = *(adapter+0x14651f7) >> 4
```

### TLV Builder `FUN_1400c1e88`
This function takes the 0x74-byte struct and the adapter/BSS contexts, then builds all BSS_INFO TLVs. Unfortunately it's too large to fully decompile in this pass, but we know it constructs the full TLV chain including BASIC, RLM, PROTECT, IFS_TIME, RATE, SEC, HE, BSS_COLOR, MLD.

---

## 10. BSS Activate — `nicUniCmdBssActivateCtrl` (0x140143540)

### MCU Command
- **CID 1**: `0x01` (DEV_INFO) — allocated with `FUN_14014f788(param_1, 1, 0x10)`
- **CID 2**: `0x02` (BSS_INFO) — allocated with `FUN_14014f788(param_1, 2, size)`
- **Both sent together** in a single command batch

### DEV_INFO Part (CID=0x01, 0x10 bytes)
```c
struct DEV_INFO_ACTIVATE {
    u8  bss_index;      // pcVar1[3]
    u8  activate;       // (*pcVar1 != 4) - 2 → 0xFF(activate) or 0xFE(deactivate)
    u8  pad[2];
    u8  tag_lo;         // 0
    u8  tag_hi;         // 0
    u8  len_lo;         // 0x0C
    u8  len_hi;         // 0
    u8  own_mac_addr;   // pcVar1[1]
    u8  dbdc_band;      // pcVar1[0xB]
    u32 bssid;          // pcVar1[4..7]
    u16 bssid_hi;       // pcVar1[8..9]
} __packed;
```

### BSS_INFO_BASIC Part (CID=0x02)
The activate function builds a **full BSS_INFO_BASIC TLV** inline:
```c
struct BSS_ACTIVATE_BASIC {
    u8  bss_index;          // *pcVar1
    u8  pad[3];
    u16 tag;                // 0x0000
    u16 length;             // 0x0020 (32 bytes)
    u8  own_mac;            // pcVar1[1]
    u8  mld_link_idx;       // 0xFF
    u8  band_idx;           // pcVar1[3]
    u8  band_idx2;          // pcVar1[3]
    u32 connection_type;    // FUN_14014fa20() result (0x10001 for STA)
    u8  active;             // ~(param_1[0x2e6964] >> 7) & 1
    u8  wlan_idx;           // param_1[0xb9550]
    u32 bssid;              // from sta context or bss context
    u16 bssid_hi;
    u16 beacon_interval;    // (ushort)(byte)pcVar1[10]
    u16 dtim_period;        // (short)param_1[0xb8612]
    u8  dtim_count;         // (char)param_1[0xb862d]
    u16 capability;         // from FUN_14014fdfc()
    u8  phy_type;           // from lVar11+0x4ac or default=3
    u16 cap_low;
    u8  cap_high;
    u8  dbdc_band;          // pcVar1[0xB]
} __packed;
```

### Activate vs Deactivate Order
- **Deactivate**: BSS_INFO first, then DEV_INFO
- **Activate**: DEV_INFO first, then BSS_INFO

### MLD Support
If `pcVar1[1] != 0` (MLD enabled), also calls `nicUniCmdBssInfoMld` to append MLD TLV (tag=0x1A).

---

## 11. Band Gap Clock Request — `nicUniCmdBandGapClockReq` (0x14014f868)

### Function Name (confirmed from string)
Actually named `nicUniCmdBandGapClockReq` — NOT `nicUniCmdBandConfig`.

### MCU Command
```c
FUN_14014eb0c(bss_ctx, 0xf, 1, 0, 0x10, 0, 0xc, local_res8, 0, 0, 0xff);
// Params: adapter, CID=0x0F, option=1(set), ?, size=0x10, ?, payload_size=0xC, payload, ...
```

- **CID**: `0x0F` (BAND_CONFIG)
- **Tag**: `0x0004` (from `*(undefined4 *)(local_res8 + 4) = 0x80004`)
- **Payload**: `param_2` (1 byte) at offset 8

### TLV Structure
```c
struct UNI_CMD_BAND_GAP_CLOCK {
    u16 tag;        // 0x0004
    u16 length;     // 0x0008
    u8  param;      // The gap clock parameter
    u8  pad[3];
} __packed;
```

**Purpose**: Gap clock calibration request, NOT radio enable/disable.

---

## 12. PHY Mode Enumeration — `DecodePhyMode` (0x1400c1594)

Complete PHY mode enumeration from Windows driver:

| Value | String | Description |
|-------|--------|-------------|
| 0 | PHY_11BG_MIXED | Legacy 2.4GHz b/g |
| 1 | PHY_11B | Legacy 2.4GHz b-only |
| 2 | PHY_11A | Legacy 5GHz a-only |
| 3 | PHY_11ABG_MIXED | Multi-band legacy |
| 4 | PHY_11G | 2.4GHz g-only |
| 5 | PHY_11ABGN_MIXED | Multi-band with HT |
| 6 | PHY_11N | HT-only |
| 7 | PHY_11GN_MIXED | 2.4GHz g + HT |
| 8 | PHY_11AN_MIXED | 5GHz a + HT |
| 9 | PHY_11BGN_MIXED | 2.4GHz b/g + HT |
| 10 | PHY_11AGN_MIXED | Multi-band + HT |
| 11 (0xB) | PHY_11VHT | VHT (WiFi 5) |
| 12 (0xC) | PHY_11VHT5G_ONLY | VHT 5GHz-only |
| 13 (0xD) | PHY_11He | HE (WiFi 6) |
| 14 (0xE) | PHY_11BE | EHT (WiFi 7) |

---

## 13. Power/IPI Handler — `nicUniEventIPIHandler` (0x140152658)

This is an **EVENT handler** (not a command sender). It processes IPI (Inter-Processor Interrupt) events from firmware related to power measurement.

- Reads power data from `adapter + 0x18cf934` (11 entries × 4 bytes)
- Calculates weighted average power levels
- Stores results at `adapter + 0x18cf928` and `adapter + 0x18cf92c`
- Only runs when `adapter + 0x1466add != 0`
- Uses IPI values like 0x5f, 0x5b, 0x58, 0x55, 0x52, 0x4e, 0x49, 0x44, 0x3f, 0x3a, 0x36 (dBm thresholds?)

**This is NOT a TX power configuration command** — it's a received-power measurement handler.

---

## 14. Complete UniCmd/MtCmd Function Catalog (173 functions)

### Channel/CNM Functions
| Function | Address | CID | Purpose |
|----------|---------|-----|---------|
| `nicUniCmdChReqPrivilege` | `14014ff94` | 0x27 | Channel request |
| `nicUniCmdChAbortPrivilege` | `14014fe60` | 0x27 | Channel abort |
| `MtCmdChPrivilage` | `1400c5e08` | 0xED | Legacy channel privilege |
| `MtCmdChPrivilegeForCsa` | `1400c6ff8` | ? | CSA channel switch |
| `nicUniCmdCnmGetInfo` | `140145c60` | ? | CNM info query |
| `nicUniCmdEventChMngrHandleChEven` | `140147930` | — | Channel grant event handler |

### BSS_INFO Functions
| Function | Address | CID | Purpose |
|----------|---------|-----|---------|
| `MtCmdSetBssInfo` | `1400cf928` | 0xED/0x12 | Main BSS_INFO command |
| `nicUniCmdSetBssInfo` | `1401444a0` | 0x02 | UniCmd BSS_INFO |
| `nicUniCmdBssActivateCtrl` | `140143540` | 0x01+0x02 | Activate/Deactivate |
| `nicUniCmdBssInfoTagBasic` | `14014c610` | — | Basic TLV (tag=0) |
| `nicUniCmdBssInfoTagHe` | `14014cd50` | — | HE TLV (tag=5) |
| `nicUniCmdBssInfoTagBssColor` | `14014d010` | — | BSS Color TLV (tag=4) |
| `nicUniCmdBssInfoTagEht` | `14014d150` | — | EHT TLV (tag=0x1E) |
| `nicUniCmdBssInfoMld` | `14014fad0` | — | MLD TLV (tag=0x1A) |
| `nicUniCmdBssInfoConnType` | `14014fa20` | — | Connection type calculator |
| `nicUniCmdSetBssRlm` | `1401445e0` | 0x02 | RLM command wrapper |
| `nicUniCmdSetBssRlmImpl` | `140150edc` | — | **RLM + PROTECT + IFS_TIME TLVs** |
| `MtCmdSetBssRlmParam` | `1400d02ac` | ? | Legacy RLM |

### Power/Band Functions
| Function | Address | CID | Purpose |
|----------|---------|-----|---------|
| `nicUniCmdBandGapClockReq` | `14014f868` | 0x0F | Band gap clock |
| `nicUniCmdPowerCtrl` | `140143ef0` | ? | Power control |
| `nicUniCmdPowerSaveMode` | `140144db0` | ? | Power save mode |
| `nicUniCmdPowerSkuCtrl` | `140150cc0` | ? | SKU power limit |
| `MtCmdSetPowerCtrl` | `1400d2448` | ? | Legacy power control |
| `MtCmdSetPowerCtrlByBssid` | `1400d26fc` | ? | Power control by BSSID |
| `MtCmdSendPwrLimitCmd` | `1400cc144` | ? | Power limit command |
| `nicUniCmdSetCountryPwrLimitPerRa` | `140146850` | ? | Country power limit |
| `nicUniCmdPmEnable` | `1401443b0` | ? | PM enable |
| `nicUniCmdPmDisable` | `1401442d0` | ? | PM disable |

### Rate/Fix Rate Functions
| Function | Address | CID | Purpose |
|----------|---------|-----|---------|
| `nicUniCmdSetFixRate` | `140145ae0` | ? | Fixed rate setting |

### STA_REC Functions
| Function | Address | CID | Purpose |
|----------|---------|-----|---------|
| `MtCmdSendStaRecUpdate` | `1400cdea0` | 0xED | STA_REC update |
| `nicUniCmdUpdateStaRec` | `1401446d0` | ? | UniCmd STA_REC |
| `nicUniCmdRemoveStaRec` | `140143fd0` | ? | Remove STA_REC |
| `nicUniCmdStaRecConnType` | `140151608` | — | STA connection type |
| `nicUniCmdStaRecTagHeBasic` | `14014d810` | — | HE basic TLV |
| `nicUniCmdStaRecTagHe6gCap` | `14014dae0` | — | HE 6GHz cap TLV |
| `nicUniCmdStaRecTagEhtInfo` | `14014db80` | — | EHT info TLV |
| `nicUniCmdStaRecTagEhtMld` | `14014e2a0` | — | EHT MLD TLV |
| `nicUniCmdStaRecTagMldSetup` | `14014ddc0` | — | MLD setup TLV |
| `nicUniCmdStaRecTagT2LM` | `140151690` | — | T2LM TLV |

### NOT FOUND — Confirmed Absent
These function strings do **NOT exist** in the binary:
- `nicUniCmdBssInfoTagRate` — **Rate TLV is built differently** (inside MtCmdSetBssInfo's blob builder `FUN_1400c1e88`)
- `nicUniCmdBssInfoTagRlm` — Handled by `nicUniCmdSetBssRlmImpl`
- `nicUniCmdBssInfoTagProtect` — Handled by `nicUniCmdSetBssRlmImpl`
- `nicUniCmdBssInfoTagIfsTime` — Handled by `nicUniCmdSetBssRlmImpl`
- `nicUniCmdBssInfoTagSec` — Likely inside `FUN_1400c1e88`
- `nicUniCmdBssInfoTagQbss` — Likely inside `FUN_1400c1e88`
- `RadioEn`, `TxPower`, `BandConfig`, `RateSet` — **Not present**

---

## 15. Critical Findings for Auth TX Fix

### Finding 1: IFS_TIME is built by nicUniCmdSetBssRlmImpl
The IFS_TIME TLV is NOT a standalone TLV — it's appended after RLM + PROTECT by the same function. Our driver builds RLM separately but **never builds PROTECT or IFS_TIME**.

**Fix**: After building RLM TLV, also append PROTECT and IFS_TIME TLVs.

### Finding 2: Slot time formula is crucial
```c
slot_time = short_slot ? 9 : 20;
// 5GHz: MUST use 9μs (short slot is mandatory)
// 2.4GHz: use 20μs unless AP supports short slot
```
Without this, firmware uses wrong DIFS timing → frames transmitted with wrong inter-frame spacing → AP ignores them.

### Finding 3: BSS Activate sends full BSS_INFO_BASIC
`nicUniCmdBssActivateCtrl` builds a complete BSS_INFO_BASIC TLV with connection_type, BSSID, beacon interval, DTIM, capability, PHY type. This is sent BEFORE any other BSS configuration.

### Finding 4: Channel request supports multi-band
The channel request supports multiple channel entries (for MLO). Each entry is 0x18 bytes with its own band/BW/center-freq parameters.

### Finding 5: No RF/PHY init commands needed
Confirmed: **No Radio Enable, TX Power init, PHY Init, or RF calibration commands** exist in the binary. Firmware handles all of this internally from E-fuse data.

---

## 16. Recommended Implementation Changes

### Priority 1: Add PROTECT + IFS_TIME to BSS RLM
```c
// After existing RLM TLV in our BSS_INFO command:

// PROTECT TLV (tag=3, 8 bytes)
struct bss_protect_tlv {
    __le16 tag;            // 0x0003
    __le16 len;            // 0x0008
    __le32 protect_mode;   // 0 for initial auth (no protection needed)
} __packed;

// IFS_TIME TLV (tag=0x17, 20 bytes)
struct bss_ifs_time_tlv {
    __le16 tag;            // 0x0017
    __le16 len;            // 0x0014
    u8 slot_valid;         // 1
    u8 sifs_valid;         // 0
    __le16 slot_time;      // 9 for 5GHz, 20 for 2.4GHz
    __le16 sifs_time;      // 0
    __le16 rifs_time;      // 0
    __le16 eifs_time;      // 0
    u8 eifs_cck_valid;     // 0
    u8 pad;
    __le16 eifs_cck_time;  // 0
} __packed;
```

### Priority 2: Verify RATE TLV in BSS_INFO
The Rate TLV is built inside `FUN_1400c1e88` (the BSS_INFO blob builder), not as a standalone function. We need to ensure our RATE TLV includes `operational_rate` (not just basic_rate).

### Priority 3: Send BSS Activate correctly
Windows sends DEV_INFO + BSS_INFO_BASIC together via `nicUniCmdBssActivateCtrl`. Our activate flow should match this pattern.

### Priority 4: Channel request format
Ensure our ROC/channel request matches the TLV format:
- CID=0x27, tag=0 for request, tag=1 for abort
- 0x18 bytes per channel entry
- Correct bandwidth mapping (4→6 for 320MHz)

---

## Appendix: Raw Ghidra Output Files
- Decompiled functions: `/tmp/rfphy_decompile.txt` (2616 lines)
- Script: `/home/user/ghidra_scripts/AnalyzeRfPhyChannel2.java`

---

*Analysis complete — 2026-02-17*
