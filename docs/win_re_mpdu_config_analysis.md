# Windows Driver MPDU Configuration Analysis — MPDU_ERROR Root Cause Investigation

**Analysis Date**: 2026-02-17
**Context**: TX_DONE status=3 (MPDU_ERROR), 30 retries, AP doesn't ACK auth frame
**Analyst**: reverser agent (Ghidra RE + MT6639 cross-reference)
**Sources**: Existing RE docs + MT6639 reference code + Windows mtkwecx.sys

---

## Executive Summary

The MPDU_ERROR means the frame **reaches RF and is transmitted** but the AP **doesn't ACK** it. After analyzing the Windows driver, MT6639 reference code, and our driver side-by-side, I found **5 critical configuration gaps** that could cause this:

| Priority | Issue | Impact |
|----------|-------|--------|
| **P0** | RATE TLV missing `u2OperationalRateSet` | Firmware may lack valid rate table |
| **P0** | Missing IFS_TIME TLV (tag=0x17) | Wrong slot/SIFS timing → IFS violations |
| **P0** | Missing PROTECT TLV (tag=3) | Wrong protection mode → frame timing errors |
| **P1** | Missing SEC TLV (tag=0x10) | Firmware may not handle auth correctly |
| **P1** | 8 missing BSS_INFO/STA_REC TLVs | Incomplete firmware state |

---

## 1. TX Rate Decoding: u2TxRate=0x004b

### CONNAC3X Rate Format

```
u2TxRate = 0x004b = 0b0000_0000_0100_1011

bits[0:5]  = MCS  = 0x0b (11) → OFDM 6Mbps
bits[6:9]  = MODE = 0x01      → TX_RATE_MODE_OFDM
bits[10:13]= NSS  = 0x00      → 1 spatial stream
bit[14]    = STBC = 0         → no STBC
```

### OFDM Rate Index Mapping (from MT6639 nic_rate.h)

```
Mode  | Values
------+------------------------------------------------
  0   | TX_RATE_MODE_CCK   (1/2/5.5/11 Mbps)
  1   | TX_RATE_MODE_OFDM  (6/9/12/18/24/36/48/54 Mbps)
  2   | TX_RATE_MODE_HTMIX
  4   | TX_RATE_MODE_VHT
  8   | TX_RATE_MODE_HE_SU
 12   | TX_RATE_MODE_EHT
```

OFDM MCS index 0x0b → **6 Mbps** (mandatory basic rate for 5GHz).

**Conclusion**: The TX rate 0x004b = OFDM 6Mbps is **correct and valid** for auth frames. The rate itself is NOT the problem.

---

## 2. CRITICAL: RATE TLV (tag=0x0B) — Missing OperationalRateSet

### MT6639 Structure (reference)

```c
// Source: mt6639/include/nic_uni_cmd_event.h line 429
struct UNI_CMD_BSSINFO_RATE {
    uint16_t u2Tag;                // 0x0B
    uint16_t u2Length;             // 16 bytes total
    uint16_t u2OperationalRateSet; // ← ALL supported rates bitmap
    uint16_t u2BSSBasicRateSet;    // ← mandatory basic rates bitmap
    uint16_t u2BcRate;             // broadcast rate
    uint16_t u2McRate;             // multicast rate
    uint8_t  ucPreambleMode;       // 0=long, 1=short
    uint8_t  aucPadding[3];
};
```

### Our Structure (mt7927_pci.h)

```c
struct bss_rate_tlv {
    __le16 tag;
    __le16 len;
    u8 __rsv1[2];        // ← BUG: This IS u2OperationalRateSet — always 0!
    __le16 basic_rate;    // ← u2BSSBasicRateSet — correctly set
    __le16 bc_trans;      // u2BcRate — always 0
    __le16 mc_trans;      // u2McRate — always 0
    u8 short_preamble;    // ucPreambleMode — correctly set
    u8 bc_fixed_rate;     // padding in MT6639
    u8 mc_fixed_rate;     // padding in MT6639
    u8 __rsv2;            // padding
};
```

### The Bug

**Offset 4-5 (`__rsv1[2]`) is `u2OperationalRateSet` — it's always 0!**

MT6639 code sets both:
```c
// Source: mt6639/nic/nic_uni_cmd_event.c line 1694
tag->u2OperationalRateSet = cmd->u2OperationalRateSet;
tag->u2BSSBasicRateSet = cmd->u2BSSBasicRateSet;
```

### Required Fix

```c
// Rename __rsv1 to operational_rate and set it:

struct bss_rate_tlv {
    __le16 tag;
    __le16 len;
    __le16 operational_rate;  // Was __rsv1[2] — supported rates bitmap
    __le16 basic_rate;
    ...
};

// In mt7927_mcu_add_bss_info():
if (chan->band == NL80211_BAND_2GHZ) {
    req.rate.operational_rate = cpu_to_le16(0x3FFF); // all HR-DSSS + OFDM
    req.rate.basic_rate = cpu_to_le16(HR_DSSS_ERP_BASIC_RATE);
} else {
    req.rate.operational_rate = cpu_to_le16(0x3FC0); // all OFDM rates
    req.rate.basic_rate = cpu_to_le16(OFDM_BASIC_RATE);
}
```

### Rate Set Bitmap (from MT6639 wlan_def.h)

```
Bit  | Rate       | Index Name
-----|------------|------------------
 0   | 1 Mbps     | RATE_1M_SW_INDEX
 1   | 2 Mbps     | RATE_2M_SW_INDEX
 2   | 5.5 Mbps   | RATE_5_5M_SW_INDEX
 3   | 11 Mbps    | RATE_11M_SW_INDEX
 4   | 22 Mbps    | RATE_22M_SW_INDEX
 5   | 33 Mbps    | RATE_33M_SW_INDEX
 6   | 6 Mbps     | RATE_6M_SW_INDEX
 7   | 9 Mbps     | RATE_9M_SW_INDEX
 8   | 12 Mbps    | RATE_12M_SW_INDEX
 9   | 18 Mbps    | RATE_18M_SW_INDEX
10   | 24 Mbps    | RATE_24M_SW_INDEX
11   | 36 Mbps    | RATE_36M_SW_INDEX
12   | 48 Mbps    | RATE_48M_SW_INDEX
13   | 54 Mbps    | RATE_54M_SW_INDEX

Predefined Sets:
  HR_DSSS        = 0x000F  (bits 0-3: 1/2/5.5/11 Mbps)
  OFDM           = 0x3FC0  (bits 6-13: 6-54 Mbps)
  ERP (2.4G all) = 0x3FFF  (bits 0-13: all rates)
```

---

## 3. CRITICAL: Missing IFS_TIME TLV (tag=0x17)

### Why This Matters

IFS (Inter-Frame Spacing) timing is critical for frame TX. Without IFS_TIME:
- Firmware uses default (possibly wrong) slot time
- On 5GHz, **short slot time (9μs) is mandatory** — long slot (20μs) violates spec
- Wrong SIFS/EIFS causes **frame timing violations** — AP sees corrupted timing

### MT6639 Structure

```c
// Source: mt6639/include/nic_uni_cmd_event.h line 562
struct UNI_CMD_BSSINFO_IFS_TIME {  // Tag = 0x17
    uint16_t u2Tag;
    uint16_t u2Length;
    uint8_t  fgSlotValid;      // 1 = use slot time below
    uint8_t  fgSifsValid;      // 1 = use SIFS time below
    uint8_t  fgRifsValid;
    uint8_t  fgEifsValid;
    uint16_t u2SlotTime;       // 9 for 5GHz, 20 for 2.4GHz non-short-slot
    uint16_t u2SifsTime;
    uint16_t u2RifsTime;
    uint16_t u2EifsTime;
    uint8_t  fgEifsCckValid;
    uint8_t  aucPadding[1];
    uint16_t u2EifsCckTime;
};
// Total: 20 bytes
```

### MT6639 Sets It

```c
// Source: mt6639/nic/nic_uni_cmd_event.c line 1668-1676
ifs_tag->u2Tag = UNI_CMD_BSSINFO_TAG_IFS_TIME;
ifs_tag->u2Length = sizeof(*ifs_tag);
ifs_tag->fgSlotValid = TRUE;
if (cmd->ucUseShortSlotTime)
    ifs_tag->u2SlotTime = 9;   // short slot (5GHz mandatory)
else
    ifs_tag->u2SlotTime = 20;  // long slot (2.4GHz fallback)
```

### Required Fix

Add to BSS_INFO command:
```c
struct bss_ifs_time_tlv {
    __le16 tag;          // 0x17
    __le16 len;          // sizeof = 20
    u8 slot_valid;       // 1
    u8 sifs_valid;       // 0 (use default)
    u8 rifs_valid;       // 0
    u8 eifs_valid;       // 0
    __le16 slot_time;    // 9 for 5GHz, 20 for 2.4GHz
    __le16 sifs_time;    // 0
    __le16 rifs_time;    // 0
    __le16 eifs_time;    // 0
    u8 eifs_cck_valid;   // 0
    u8 pad;
    __le16 eifs_cck_time;// 0
} __packed;
```

---

## 4. CRITICAL: Missing PROTECT TLV (tag=3)

### MT6639 Structure

```c
// Source: mt6639/include/nic_uni_cmd_event.h line 350
struct UNI_CMD_BSSINFO_PROTECT {  // Tag = 0x03
    uint16_t u2Tag;
    uint16_t u2Length;
    uint32_t u4ProtectMode;  // Bitmask of protection modes
};
// Total: 8 bytes

// Protection mode flags:
enum ENUM_PROTECTION_MODE_T {
    HT_NON_MEMBER_PROTECT     = BIT(1),
    HT_BW20_PROTECT           = BIT(2),
    HT_NON_HT_MIXMODE_PROTECT = BIT(3),
    LEGACY_ERP_PROTECT         = BIT(5),
    VEND_LONG_NAV_PROTECT      = BIT(6),
    VEND_GREEN_FIELD_PROTECT   = BIT(7),
    VEND_RIFS_PROTECT          = BIT(8),
};
```

### Required Fix

For auth frame (pre-association), protection mode should be 0 (no protection):
```c
struct bss_protect_tlv {
    __le16 tag;            // 0x03
    __le16 len;            // 8
    __le32 protect_mode;   // 0 for initial auth
} __packed;
```

---

## 5. Missing SEC TLV (tag=0x10)

### MT6639 Structure

```c
// Source: mt6639/include/nic_uni_cmd_event.h line 474
struct UNI_CMD_BSSINFO_SEC {  // Tag = 0x10
    uint16_t u2Tag;
    uint16_t u2Length;
    uint8_t  ucAuthMode;    // See auth mode table below
    uint8_t  ucEncStatus;   // Encryption status
    uint8_t  ucCipherSuit;  // WA cipher suite
    uint8_t  aucPadding[1];
};
// Total: 8 bytes

// Auth modes:
// AUTH_MODE_OPEN        = 0
// AUTH_MODE_SHARED      = 1
// AUTH_MODE_WPA         = 3
// AUTH_MODE_WPA_PSK     = 4
// AUTH_MODE_WPA2        = 6
// AUTH_MODE_WPA2_PSK    = 7
// AUTH_MODE_WPA3_SAE    = 11
```

### Required Fix

For WPA2-PSK (our AP uses WPA2):
```c
struct bss_sec_tlv {
    __le16 tag;          // 0x10
    __le16 len;          // 8
    u8 auth_mode;        // 7 = AUTH_MODE_WPA2_PSK
    u8 enc_status;       // 0 initially
    u8 cipher_suit;      // 0
    u8 pad;
} __packed;
```

---

## 6. Channel Privilege Configuration

### UNI_CMD_CNM_CH_PRIVILEGE_REQ Structure

```c
// Source: mt6639/include/nic_uni_cmd_event.h line 1793
// CID: UNI_CMD_ID_CNM = 0x0C (not ROC!)
struct UNI_CMD_CNM_CH_PRIVILEGE_REQ {  // Tag = 0
    uint16_t u2Tag;
    uint16_t u2Length;
    uint8_t  ucBssIndex;
    uint8_t  ucTokenID;            // ← unique token for this request
    uint8_t  ucPrimaryChannel;
    uint8_t  ucRfSco;              // ← secondary channel offset
    uint8_t  ucRfBand;             // 1=2.4G, 2=5G, 3=6G
    uint8_t  ucRfChannelWidth;     // 0=20MHz, 1=40MHz, 2=80MHz
    uint8_t  ucRfCenterFreqSeg1;   // center channel for 80/160
    uint8_t  ucRfCenterFreqSeg2;   // center channel for 160
    uint8_t  ucRfChannelWidthFromAP;
    uint8_t  ucRfCenterFreqSeg1FromAP;
    uint8_t  ucRfCenterFreqSeg2FromAP;
    uint8_t  ucReqType;            // ENUM_CH_REQ_TYPE_T
    uint32_t u4MaxInterval;        // timeout in ms
    uint8_t  ucDBDCBand;
    uint8_t  aucReserved[3];
};
// Total: 24 bytes
```

### vs Our ROC Implementation

Our driver uses `mt7927_mcu_roc_acquire()` which may use different CID/structure. Should verify we're sending:
- Correct CID (`UNI_CMD_ID_CNM` = 0x0C)
- Correct fields (especially `ucRfSco`, `ucRfBand`, `ucRfChannelWidth`)
- The `ucReqType` field

**Grant Event**: Firmware responds with `UNI_EVENT_CNM_CH_PRIVILEGE_GRANT` (tag=0, event CID matches).

---

## 7. BSS_INFO TLV Gap Analysis

### MT6639 Sends 12 TLVs (arSetBssInfoTable[])

| # | TLV | Tag | Size | Our Driver | Status |
|---|-----|-----|------|------------|--------|
| 1 | BASIC | 0x00 | 32B | ✅ Have | OK |
| 2 | RLM+PROTECT+IFS_TIME | 0x02+0x03+0x17 | 12+8+20=40B | ⚠️ RLM only | **Missing PROTECT + IFS_TIME** |
| 3 | RATE | 0x0B | 16B | ⚠️ Partial | **Missing OperationalRateSet** |
| 4 | SEC | 0x10 | 8B | ❌ Missing | **Need to add** |
| 5 | QBSS | 0x0F | 8B | ❌ Missing | Nice-to-have |
| 6 | SAP | 0x0D | 40B | ❌ Missing | Not needed for STA |
| 7 | P2P | 0x0E | 8B | ❌ Missing | Not needed for STA |
| 8 | HE | 0x05 | 16B | ❌ Missing | Nice-to-have |
| 9 | BSS_COLOR | 0x04 | 8B | ❌ Missing | Nice-to-have |
| 10 | 11V_MBSSID | 0x06 | 8B | ❌ Missing | Not needed |
| 11 | WAPI | 0x0C | 8B | ❌ Missing | Not needed |
| 12 | MLD | 0x1A | 20B | ✅ Have | OK |

### Critical Missing for Auth TX

1. **PROTECT (tag=3)** — P0: firmware needs protection mode
2. **IFS_TIME (tag=0x17)** — P0: firmware needs slot time
3. **RATE OperationalRateSet** — P0: firmware needs rate table
4. **SEC (tag=0x10)** — P1: firmware needs auth/encryption mode

---

## 8. STA_REC TLV Gap Analysis

### Windows Driver Sends 13 TLVs

| # | TLV | Tag | Our Driver | Status |
|---|-----|-----|------------|--------|
| 1 | BASIC | 0x00 | ✅ Have | OK |
| 2 | RA | 0x01 | ✅ Have | OK |
| 3 | STATE_CHANGED | 0x07 | ✅ Have | OK |
| 4 | HT_BASIC | 0x09 | ❌ Missing | Nice-to-have |
| 5 | VHT_BASIC | 0x0a | ❌ Missing | Nice-to-have |
| 6 | PHY_INFO | 0x15 | ✅ Have | OK |
| 7 | BA_OFFLOAD | 0x16 | ❌ Missing | Not needed for auth |
| 8 | HE_BASIC | 0x19 | ❌ Missing | Nice-to-have |
| 9 | MLD_SETUP | 0x20 | ❌ Missing | Nice-to-have |
| 10 | EHT_MLD | 0x21 | ❌ Missing | Not needed |
| 11 | EHT_BASIC | 0x22 | ❌ Missing | Not needed |
| 12 | UAPSD | 0x24 | ❌ Missing | Not needed |
| 13 | HDR_TRANS | 0x2B | ✅ Have | OK (custom) |

STA_REC gaps are less likely to cause MPDU_ERROR. Focus on BSS_INFO gaps first.

---

## 9. DMASHDL Configuration

Our DMASHDL config matches MT6639 closely:
- Group quotas: 0-2 active (0xfff/0x10), matches MT6639
- Queue mapping: Q0→G0, Q16(ALTX)→G0, matches MT6639
- BYPASS=1: Kept on because CMD ring Q_IDX=0x20 exceeds DMASHDL range (0-31)

**DMASHDL is NOT the issue** — configuration is correct.

---

## 10. BSS_INFO BASIC Connection Type

Our value: `CONNECTION_INFRA_STA = STA_TYPE_STA | NETWORK_INFRA = 0x10001`

Windows reversed value: `0x10001` (Infrastructure Client)

**Connection type is correct** — matches both Windows and MT6639.

---

## 11. Recommended Fix Order

### Phase 1: Most Likely MPDU_ERROR Fixes (implement all 3 together)

1. **Fix RATE TLV**: Rename `__rsv1` → `operational_rate`, set to `0x3FC0` (5GHz) or `0x3FFF` (2.4GHz)

2. **Add IFS_TIME TLV (tag=0x17)**: Set `fgSlotValid=1`, `u2SlotTime=9` (5GHz) / `20` (2.4GHz)

3. **Add PROTECT TLV (tag=3)**: Set `u4ProtectMode=0` (no protection for initial auth)

### Phase 2: Should Add

4. **Add SEC TLV (tag=0x10)**: Set `ucAuthMode=7` (WPA2_PSK), `ucEncStatus=0`

5. **Add QBSS TLV (tag=0xF)**: Set `ucIsQBSS=0`

### Phase 3: Optional

6. Add HE TLV (tag=5), BSS_COLOR (tag=4)
7. Add HT_BASIC, VHT_BASIC to STA_REC

---

## 12. IFS_TIME Theory: Why This Causes MPDU_ERROR

The 802.11 standard requires specific inter-frame spacing:

```
DIFS = SIFS + 2 × SlotTime

5GHz:  DIFS = 16 + 2×9  = 34μs  (correct)
       DIFS = 16 + 2×20 = 56μs  (WRONG if using long slot)
```

If firmware uses wrong slot time (20μs instead of 9μs on 5GHz):
- Our frame transmits with **wrong DIFS** (56μs instead of 34μs)
- AP may **not respond** because the timing doesn't match its expectation
- Frame appears valid but **NAV/timing is inconsistent**
- AP ignores the frame → no ACK → 30 retries → MPDU_ERROR

This is the most plausible explanation for:
- Frame reaching RF ✅
- Valid rate (OFDM 6Mbps) ✅
- AP not ACKing ❌
- Same behavior on both 5GHz and 2.4GHz ❌ (both could use wrong slot time)

---

## Files Referenced

| File | Purpose |
|------|---------|
| `mt6639/include/nic_uni_cmd_event.h` | TLV structure definitions |
| `mt6639/nic/nic_uni_cmd_event.c` | TLV building functions |
| `mt6639/include/nic_cmd_event.h` | Connection type defines |
| `mt6639/include/nic/wlan_def.h` | Rate set bitmaps |
| `mt6639/include/chips/cmm_asic_connac3x.h` | TX rate format macros |
| `mt6639/include/nic/nic_rate.h` | Rate mode constants |
| `src/mt7927_pci.c` line 2363 | Our BSS_INFO function |
| `src/mt7927_pci.h` line 1571 | Our bss_rate_tlv struct |
| `docs/win_bss_info_tlv_analysis.md` | Previous BSS_INFO RE |
| `docs/win_re_sta_rec_tlvs.md` | Previous STA_REC RE |
| `docs/win_re_tx_mgmt_path.md` | Previous TX path RE |
