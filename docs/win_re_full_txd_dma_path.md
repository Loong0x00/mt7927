# Windows mtkwecx.sys — Complete TX Descriptor & DMA Submission Path

**RE Date**: 2026-02-17
**Tool**: Ghidra 12.0.3 headless decompilation
**Binary**: mtkwecx.sys v5705275 (from MT7927 WiFi driver)
**Focus**: Auth frame (management frame) TX path — TXD construction, DMA ring submission, buffer layout

---

## Table of Contents

1. [Executive Summary — Critical Differences from Our Driver](#1-executive-summary)
2. [Full Auth Frame TX Call Chain](#2-auth-frame-tx-call-chain)
3. [Buffer Allocation and Layout](#3-buffer-allocation-and-layout)
4. [TXD Construction — XmitWriteTxDv1 (All 8 DWORDs)](#4-txd-construction)
5. [DMA Ring Selection — Ring 2 for Management Frames](#5-dma-ring-selection)
6. [DMA Descriptor Format](#6-dma-descriptor-format)
7. [Ring Kick — CIDX Write](#7-ring-kick)
8. [Sequence Number Generator (FUN_14009a46c)](#8-sequence-number-generator)
9. [Retry Configuration](#9-retry-configuration)
10. [Comparison: Windows vs Our Driver](#10-comparison)
11. [Recommended Changes](#11-recommended-changes)

---

## 1. Executive Summary

### CRITICAL DIFFERENCES from our driver:

| # | Issue | Windows | Our Driver | Impact |
|---|-------|---------|------------|--------|
| **1** | **TX DMA Ring** | **Ring 2** (HW ring idx 2) | Ring 0 (data) or Ring 15 (MCU) | **CRITICAL** — wrong ring = wrong firmware path |
| **2** | **Buffer Format** | TXD+frame **inline** (SF mode) | TXD+TXP (CT mode) on Ring 0 | **CRITICAL** — firmware expects inline on ring 2 |
| **3** | **PKT_FMT** | 0 (TXD only, from DW0[24:23]) | 2 (CMD) on Ring 15 | Wrong format for mgmt |
| **4** | **DW1 HDR_FORMAT** | 0b10 = 802.11 via bit 15 | Same ✅ | OK |
| **5** | **DW1 FIXED_RATE** | Set via DW1 or path-dependent | BIT(31) ✅ | OK |
| **6** | **DW2 Fixed Rate Flags** | bits 31+29 set (0xa0000000) | Not all set | Minor |
| **7** | **DW3 REM_TX_COUNT** | 15 (bits [15:12] = 0xF) | 30 (bits [15:11]) | Different encoding |
| **8** | **DW6 TX_RATE** | 0x4b (bits [22:16]) | 0 (FIXED_RATE_IDX=0) | Different rate encoding |
| **9** | **DW7 TXD_LEN** | bit 30 = **0** for auth | 0 ✅ | OK — matches! |
| **10** | **Ring needs 11 free descriptors** | Queue check: param=0xb | We don't check | Minor |

### Most Likely Root Cause of MPDU_ERROR:

**We are sending management frames on the WRONG DMA ring.** The Windows driver uses **hardware TX Ring 2** for all management frames. Our driver uses Ring 0 (data ring with CT mode) or Ring 15 (MCU command ring with CMD format). The firmware likely has ring-specific processing paths — management frames on Ring 2 go through the LMAC TX path, while Ring 0 expects CT-mode data frames and Ring 15 expects MCU commands.

---

## 2. Auth Frame TX Call Chain

### Complete decompiled call chain:

```
MlmeAuthReqAction (0x14013f660)
  ├─ FUN_14013ff40() — Build auth frame header
  ├─ FUN_14009a46c(param_1, 3, 1) — Sequence number generator (NOT ring selector!)
  │   Returns: seq_num (byte, wraps at 100)
  │
  └─ FUN_14000cf90(HIF_handle, frame_buf, frame_len, seq_num)
     │   "AUTH_DIRECT_TX_SEND" — Lock wrapper
     │
     └─ FUN_140053714(HIF, frame_buf, frame_len, seq_num)
        │   === NdisCommonHifPciMiniportMMRequest ===
        │   (name from trace string at 0xb6d)
        │
        ├─ FUN_1400aa324(adapter, 0x1600b71, &buf_ptr)
        │   "MlmeAllocateMemoryEx" — Allocate 0x818-byte TX buffer node
        │
        └─ FUN_140053a0c(HIF, buf_node, seq_num)
           │   === NdisCommonHifPciMlmeHardTransmit ===
           │   (name from trace string at 0x9ed)
           │
           ├─ FUN_140035aec(adapter, 1) — "TxDmaqAlloc" DMA buffer (queue=1=mgmt)
           │   Returns: DMA-capable buffer with TXD+frame space
           │
           ├─ Build TX info struct (113 bytes, see §4)
           │   ├─ frame_length, hdr_len=24, queue_class=0x0104
           │   ├─ WLAN_IDX, OWN_MAC, band from STA record
           │   ├─ FRAME_TYPE=0 (mgmt), SUB_TYPE=0xB (auth)
           │   └─ fixed_rate=1, pkt_fmt, protect flags
           │
           ├─ Copy frame to buffer at offset 0x20 (after TXD):
           │   buf[0x00..0x1F] = TXD (32 bytes, written next)
           │   buf[0x20..end]  = 802.11 auth frame
           │
           ├─ FUN_1401a2c8c() → FUN_1401a2ca4()
           │   === XmitWriteTxDv1 === (see §4 for complete analysis)
           │   Fills TXD DW0-DW7 at buffer offset 0
           │
           ├─ FUN_14005d1a4(HIF, buf_node, **2**)
           │   === N6PciTxSendPkt ===
           │   param_3 = **2** → MANAGEMENT ring path (hardcoded!)
           │
           │   ├─ FUN_14000cc44(adapter, 2, flags, 0xb, ...)
           │   │   Queue check: ring=2, need 11 free descriptors
           │   │
           │   ├─ FUN_1400359cc() — Queue frame to mgmt TX list
           │   │
           │   ├─ Write DMA descriptor (16 bytes, see §6)
           │   │   DW0 = buf_phys_addr_low
           │   │   DW1 = (length << 16) | 0x40000000 (OWN bit)
           │   │   DW3[15:0] = buf_phys_addr_high
           │   │
           │   └─ FUN_140009a18(adapter, ring_reg, new_cidx)
           │       Ring kick: write CIDX to MMIO register (see §7)
           │
           └─ Return 0 (success)
```

### Key Address Map:

| Address | Function Name (from strings) | Role |
|---------|------------------------------|------|
| 0x14013f660 | MlmeAuthReqAction | Auth frame entry point |
| 0x14009a46c | (seq number gen) | Sequence counter, NOT ring selector |
| 0x14000cf90 | (lock wrapper) | Lock → NdisCommonHifPciMiniportMMRequest → Unlock |
| 0x140053714 | NdisCommonHifPciMiniportMMRequest | Alloc buffer, find BSS, call HardTransmit |
| 0x140053a0c | NdisCommonHifPciMlmeHardTransmit | Build TX info, write TXD, submit to DMA |
| 0x1401a2ca4 | XmitWriteTxDv1 | TXD construction (all 8 DWORDs) |
| 0x14005d1a4 | N6PciTxSendPkt | DMA ring submission (ring 2 for mgmt) |
| 0x140009a18 | (ring kick wrapper) | Write CIDX to kick DMA ring |
| 0x140057d48 | NdisCommonHifPciWriteReg | MMIO register write with address translation |

---

## 3. Buffer Allocation and Layout

### Buffer Structure for Auth Frames

```
Offset    Content
─────────────────────────────
0x00      TXD DW0 (4 bytes)
0x04      TXD DW1 (4 bytes)
0x08      TXD DW2 (4 bytes)
0x0C      TXD DW3 (4 bytes)
0x10      TXD DW4 (4 bytes) = 0
0x14      TXD DW5 (4 bytes)
0x18      TXD DW6 (4 bytes)
0x1C      TXD DW7 (4 bytes)
─────────────────────────────
0x20      802.11 Frame Control (2 bytes) = 0x00B0 (auth)
0x22      Duration (2 bytes)
0x24      Addr1/DA (6 bytes) = AP BSSID
0x2A      Addr2/SA (6 bytes) = STA MAC
0x30      Addr3/BSSID (6 bytes)
0x36      Seq Control (2 bytes)
0x38      Auth Algorithm (2 bytes) = 0 (Open)
0x3A      Auth Seq Number (2 bytes) = 1
0x3C      Status Code (2 bytes) = 0
─────────────────────────────
Total:    0x3E = 62 bytes (for open auth without vendor IEs)
```

**This is SF (Short Frame / inline) mode** — TXD and frame payload are contiguous in a single DMA buffer. NO TXP scatter-gather table.

### From decompiled code (NdisCommonHifPciMlmeHardTransmit):

```c
// Set initial size to 0x20 (TXD = 32 bytes)
*(uint *)(buf_node + 0x68) = 0x20;

// Copy frame data starting at buf + 0x20
FUN_140010118(*(longlong *)(buf_node + 0x60) + 0x20,  // dest = buf + 0x20
              frame_data,                                // src = 802.11 frame
              frame_len);                                // len

// Update total size
*(int *)(buf_node + 0x68) += frame_len;  // total = 0x20 + frame_len

// Write TXD at buf + 0x00
FUN_1401a2c8c(adapter, *(longlong *)(buf_node + 0x60), &tx_info);
```

### Buffer Allocation:

```c
// MlmeAllocateMemoryEx: allocates from a pre-allocated pool
// Pool entry size: 0x818 (2072) bytes — enough for TXD + max frame
// Parameter 0x1600b71 or 0xa00577 is just a debug tag, NOT size
FUN_1400aa324(adapter, 0x1600b71, &buf_ptr);
```

---

## 4. TXD Construction — XmitWriteTxDv1

### Function: `FUN_1401a2ca4` (XmitWriteTxDv1)

**Parameters:**
- `param_1`: Adapter context (longlong)
- `param_2`: TXD buffer pointer (uint *) — 8 DWORDs to fill
- `param_3`: TX info structure (short *) — 113 bytes of control data

### TX Info Structure (113 bytes, built in NdisCommonHifPciMlmeHardTransmit)

| Byte Offset | Variable | Value for Auth | TXD Field |
|-------------|----------|---------------|-----------|
| 0-1 | frame_length | auth payload size | DW0 TX_BYTE_CNT |
| 2 | hdr_len | 0x18 (24) | DW1 HDR_INFO |
| 4-5 | queue_class | 0x0104 | DW3 retry mode |
| 7 | WLAN_IDX | from STA record | DW1 WLAN_IDX |
| 8 | flags | 1 for auth | DW3 NO_ACK |
| 0x0A | BSS flag | from BSS info | DW0 bit 31 |
| 0x0B | protect | from BSS | DW3 PROTECT |
| 0x0F | priority | queue type | DW5 rate idx |
| 0x2B-0x2C | frame_type | 0 (management) | DW2/DW7 mode select |
| 0x2D-0x2E | sub_type | 0xB (auth) | DW2 SUB_TYPE |
| 0x39 | duration_flag | 0 | DW2 DURATION |
| 0x4C | fixed_rate | 1 (mgmt always) | Fixed rate path |

### Step-by-step TXD fill for auth frames:

#### Step 1: Clear TXD
```c
memset(param_2, 0, 0x20);  // Zero all 32 bytes
```

#### Step 2: DW0 — TX Byte Count + Format

```c
// DW0[15:0] = TX_BYTE_CNT = frame_length + 0x20 (TXD size)
*(short *)param_2 = frame_length + 0x20;

// DW0[30:26] = from queue_class byte (0x04)
// 0x04 << 26 = 0x10000000 → DW0[30:26] = 0b00100
// This maps to PKT_FMT + Q_IDX encoding
*param_2 = (((uint)queue_class_byte << 0x1a) ^ DW0) & 0x7c000000 ^ DW0;

// DW0[31] = BSS flag (typically 0 for auth)
*param_2 = (bss_flag << 31) | (DW0 & 0x7fffffff);
```

**DW0 for auth frame:**
```
Bit 31:    0 (BSS flag)
Bit 30:26: 0b00100 (= 4, but field interpretation depends on CONNAC3 spec)
           Our Q_IDX GENMASK(31,25) / PKT_FMT GENMASK(24,23) overlap here
Bit 15:0:  frame_length + 32
```

**NOTE**: The DW0 bit layout for CONNAC3 may differ from our macro definitions. The Windows driver uses bits [30:26] as a 5-bit field. With our definitions:
- MT_TXD0_PKT_FMT = GENMASK(24,23) = 2 bits
- MT_TXD0_Q_IDX = GENMASK(31,25) = 7 bits

Value 0x04 in bits[30:26] → PKT_FMT=0 (bits[24:23]=0), Q_IDX has bit 28 set. This needs verification against CONNAC3 spec.

#### Step 3: DW1 — WLAN_IDX, HDR_FORMAT, OWN_MAC

```c
// DW1[15] = HDR_FORMAT_V3 = 802.11 (sets bit 15 → GENMASK(15,14) = 0b10 = 2)
DW1 |= 0x8000;

// DW1[12:8] = HDR_INFO = hdr_len = 24 (for non-CT mode: shifted left by 8)
DW1[12:8] = 24;

// DW1[14] = 1 (HDR_FORMAT flag, always set)
DW1 |= 0x4000;

// DW1[13] = 0 (cleared)
DW1 &= ~0x2000;

// DW1[7:0] = WLAN_IDX (from STA record)
DW1[7:0] = wlan_idx;

// DW1[23:21] = OWN_MAC index
DW1[23:21] = own_mac_idx;

// DW1[31:26] = TGID/band from adapter config
DW1[31:26] = band_config;

// DW1[25] = 1 (set for non-CT mode mgmt frames)
DW1 |= 0x2000000;
```

**DW1 for auth frame:**
```
Bit 31:26: band/TGID config
Bit 25:    1 (non-CT mode flag)
Bit 24:21: (zeroed unless TID set)
Bit 20:16: HDR_INFO = 24 (0x18 = header length in bytes)
Bit 15:    1 } HDR_FORMAT_V3 = 0b10 = 802.11
Bit 14:    1 }
Bit 13:    0
Bit 12:    (from TGID)
Bit 11:0:  WLAN_IDX
```

**NOTE on DW1[25]**: Our driver's MT_TXD1_OWN_MAC = GENMASK(30,25) includes bit 25. The Windows driver uses bit 25 as a separate flag (non-CT-mode indicator). This means **OWN_MAC field is GENMASK(30,26)** not GENMASK(30,25) in practice, or bit 25 serves dual purpose.

#### Step 4: DW2 — Frame Type, SubType, Fixed Rate

For auth frames (fixed_rate flag = 1):

```c
// DW2[5:4] = FRAME_TYPE = 0 (management)
DW2[5:4] = 0;

// DW2[3:0] = SUB_TYPE = 0xB (authentication)
DW2[3:0] = 0xB;

// DW2[10] = duration flag (0 for auth)
DW2[10] = 0;

// DW2[12] = 0 (cleared)
DW2 &= ~0x1000;

// Fixed rate flags (set when fixed_rate=1):
DW2 |= 0xa0000000;  // Set bits 31 and 29
// DW2[31] = 1 — likely POWER_OFFSET MSB or FIXED_RATE indicator
// DW2[29] = 1 — likely SW_POWER_MGMT or BM flag
```

**DW2 for auth frame:**
```
Bit 31:    1 (fixed rate: POWER_OFFSET MSB or FIXED_RATE)
Bit 29:    1 (fixed rate flag)
Bit 12:    0
Bit 10:    0 (duration control)
Bit 5:4:   0b00 (FRAME_TYPE = management)
Bit 3:0:   0xB (SUB_TYPE = authentication)
All other bits: 0
= 0xA000000B
```

#### Step 5: DW3 — Retry Count, ACK, Protect

```c
// DW3[0] = NO_ACK: 0 if ACK required (auth needs ACK)
DW3[0] = (tx_info[4] == 0) ? 1 : 0;  // For auth: likely 0

// DW3[1] = PROTECT (from BSS protection mode)
DW3[1] = (protect_flag != 0) ? 1 : 0;

// Retry count — depends on queue_class:
// queue_class[4] = 0x04 (not 0x07), takes ELSE branch:
DW3 |= 0xf000;  // bits [15:12] = 0xF

// With our field definition MT_TXD3_REM_TX_COUNT = GENMASK(15,11):
// 0xf000 = bits[15:12]=0xF, bit[11]=0
// This gives REM_TX_COUNT = 0xF000 >> 11 = 0x1E = 30? No:
// 0xF000 in binary = 1111_0000_0000_0000
// GENMASK(15,11) extracts bits [15:11] = 11110 = 30
// So REM_TX_COUNT = 30! Same as our driver (just encoded differently)
```

Wait, let me recalculate:
- 0xF000 = 0b 1111 0000 0000 0000
- GENMASK(15,11) = 0b 1111 1000 0000 0000 = 0xF800
- Extracting bits [15:11] from 0xF000: shift right 11 → 0xF000 >> 11 = 0x1E = 30

**Actually REM_TX_COUNT = 30** — but wait, `if (char)param_3[2] == 7` branch sets 0x7800:
- 0x7800 >> 11 = 15

So for queue_class 7: retry=15, for others (including auth with class 4): retry=30.

Hmm, but auth queue_class = (char)0x0104 = 0x04, not 7. So: DW3 |= 0xF000 → REM_TX_COUNT = 30.

**DW3 for auth frame:**
```
Bit 15:11: REM_TX_COUNT = 30 (0xF000 encodes this)
Bit 1:     PROTECT (from BSS)
Bit 0:     NO_ACK = 0 (ACK required for auth)
= 0x0000F000 (base, plus NO_ACK/PROTECT flags)
```

#### Step 6: DW4 — Unused
```
DW4 = 0x00000000
```

#### Step 7: DW5 — Status, PID

```c
// DW5[13:12] = 0 (cleared)
DW5 &= ~0x3000;
```

**DW5 for auth frame:**
```
= 0x00000000 (no PID tracking, no status request)
```

#### Step 8: DW6 — TX Rate, MSDU Count

For fixed rate auth frames:
```c
// DW6 = (DW6 & 0x7e00ffff) | 0x4b0000
// Clears bits [24:23] and [22:16], then sets bits [22:16] = 0x4b
DW6[22:16] = 0x4b;  // Rate = OFDM 6Mbps
```

Decoding 0x4b as CONNAC3 rate:
```
0x4b = 0b 0100 1011
bits[5:0]  = MCS = 0x0b (11) = OFDM 6Mbps rate index
bits[9:6]  = MODE = 0x01 = TX_RATE_MODE_OFDM
bits[13:10]= NSS = 0x00 = 1 spatial stream
```

**DW6 for auth frame:**
```
Bit 22:16: 0x4B (OFDM 6Mbps, mode=1, mcs=11)
All other bits: 0
= 0x004B0000
```

**CRITICAL DIFFERENCE**: Our driver sets `MT_TXD6_TX_RATE = 0` (letting firmware decide), while Windows explicitly sets rate = 0x4b (OFDM 6Mbps). However, our driver also sets `MT_TXD6_MSDU_CNT = 1` and `MT_TXD6_DIS_MAT`, which Windows doesn't set here.

#### Step 9: DW7 — TXD Length

```c
// For fixed rate frames: DW7[30] is CLEARED
DW7 &= ~0x40000000;  // = param_2[7] & 0xbfffffff
```

Since TXD was zeroed initially and DW7[30] is explicitly cleared:

**DW7 for auth frame:**
```
Bit 30: 0 (TXD_LEN_1_PAGE NOT set)
= 0x00000000
```

**This matches our driver** — DW7 = 0 is correct for auth frames.

### Complete TXD for Auth Frame (Windows):

```
DW0: 0x1000003E  (example: TX_BYTE_CNT=62, PKT_FMT/Q_IDX=0x04)
     [15:0]  = frame_len + 32 = 30 + 32 = 62 (0x3E)
     [30:26] = 0x04 (from queue_class encoding)
     [31]    = 0

DW1: 0x0600C8XX  (example: WLAN_IDX=XX, HDR_FORMAT=802.11)
     [11:0]  = WLAN_IDX
     [14:13] = 0b10 (HDR_FORMAT_V3 = 802.11)
     [15]    = 1
     [20:16] = 0x18 (24 = hdr_len)
     [23:21] = OWN_MAC
     [25]    = 1 (non-CT mode flag)
     [31:26] = band/TGID

DW2: 0xA000000B  (FIXED_RATE flags + management auth)
     [3:0]   = 0xB (SUB_TYPE = auth)
     [5:4]   = 0 (FRAME_TYPE = management)
     [29]    = 1 (fixed rate flag)
     [31]    = 1 (fixed rate flag)

DW3: 0x0000F000  (30 retries, ACK required)
     [0]     = 0 (NO_ACK = false → ACK required)
     [1]     = 0 (PROTECT)
     [15:11] = REM_TX_COUNT = 30

DW4: 0x00000000  (unused)

DW5: 0x00000000  (no status tracking)

DW6: 0x004B0000  (OFDM 6Mbps)
     [22:16] = 0x4B (rate = OFDM 6Mbps)

DW7: 0x00000000  (TXD_LEN = 0)
```

---

## 5. DMA Ring Selection — Ring 2 for Management Frames

### Finding: Management frames use Hardware TX Ring 2

From `N6PciTxSendPkt` (FUN_14005d1a4):

```c
// Called with param_3 = 2 for management frames:
iVar10 = FUN_14005d1a4(*(undefined8 *)(lVar4 + 0x1f80), lVar11, 2);
```

The ring selection logic:
```c
if (param_3 < 2) {
    // DATA path (ring 0 or 1)
    FUN_14000cc44(adapter, ring_idx, flags, 1, ...);  // need 1 descriptor
    FUN_14005d6d8(adapter, buf);  // N6PciUpdateAppendTxD (CT mode TXP)
    FUN_140036d24(adapter, buf);  // Queue to data TX wait queue
} else {
    // MANAGEMENT path (ring 2+)
    FUN_14000cc44(adapter, ring_idx, flags, 0xb, ...);  // need 11 descriptors!
    FUN_1400359cc(adapter, buf);  // Queue to mgmt TX list
}
// Both paths then:
// 1. Write DMA descriptor (inline in N6PciTxSendPkt)
// 2. Kick ring via FUN_140009a18
```

### Ring Index Validation

From `NdisCommonHifPciFreeDescriptorRequest` (FUN_1400532e4):
```c
if (param_2 < 4) {
    // Valid rings: 0, 1, 2, 3
    // Check free descriptor count...
} else {
    return STATUS_INSUFFICIENT_RESOURCES;  // Invalid ring
}
```

**Windows supports 4 TX rings (0-3)**:
| Ring | Usage | Queue Check Count |
|------|-------|-------------------|
| 0 | Data TX (AC_BE/AC_BK) | 1 descriptor |
| 1 | Data TX (AC_VI/AC_VO) | 1 descriptor |
| **2** | **Management TX** | **11 descriptors** |
| 3 | (possibly beacon/other) | Unknown |

### Hardware Ring Registers:

```
Ring 0: BASE=0xd4300, CNT=0xd4304, CIDX=0xd4308, DIDX=0xd430c
Ring 1: BASE=0xd4310, CNT=0xd4314, CIDX=0xd4318, DIDX=0xd431c
Ring 2: BASE=0xd4320, CNT=0xd4324, CIDX=0xd4328, DIDX=0xd432c
Ring 3: BASE=0xd4330, CNT=0xd4334, CIDX=0xd4338, DIDX=0xd433c
```

**Our driver only initializes Ring 0 (data), Ring 15, and Ring 16 (MCU).**
**Ring 2 is never initialized!**

---

## 6. DMA Descriptor Format

### WFDMA TX DMA Descriptor (16 bytes / 4 DWORDs)

Written inline in `N6PciTxSendPkt`:

```c
// Clear 16 bytes
FUN_14001022c(dma_desc, 0x10);

// DW0: Buffer physical address (low 32 bits)
*dma_desc = *(uint *)(buf_node + 0x58);  // phys_addr_low

// DW3[15:0]: Buffer physical address (high 16 bits)
*(uint16 *)(dma_desc + 3) = *(uint16 *)(buf_node + 0x5c);  // phys_addr_high

// DW1: Length + OWN bit
// For management frames (param_3 >= 2):
uint len = *(int *)(buf_node + 0x68);  // total length (TXD + frame)
dma_desc[1] = ((len << 16) ^ dma_desc[1]) & 0x3fff0000 ^ dma_desc[1];
dma_desc[1] = (dma_desc[1] & 0x7fffffff) | 0x40000000;
```

### DMA Descriptor Layout:

```
DW0: [31:0]  = Buffer physical address (low 32 bits)

DW1: [15:0]  = (available for flags/padding)
     [29:16] = Buffer length (total: TXD + frame)
     [30]    = OWN = 1 (DMA engine owns this descriptor)
     [31]    = 0 (LS/last segment flag — cleared for mgmt)

DW2: [31:0]  = 0 (cleared)

DW3: [15:0]  = Buffer physical address (high 16 bits)
     [31:16] = 0 (cleared)
```

---

## 7. Ring Kick — CIDX Write

### Call chain:

```
FUN_140009a18(adapter, ring_base_reg, new_cidx)
  └─ FUN_140057d48(HIF_handle, register_offset, value)
       NdisCommonHifPciWriteReg
```

### FUN_140057d48 (NdisCommonHifPciWriteReg):

```c
undefined4 FUN_140057d48(longlong *param_1, ulonglong reg_offset, undefined4 value) {
    // Chip-specific address translation for MT6639/MT7927:
    short chip_id = *(short *)(*param_1 + 0x1f72);
    if (chip_id == 0x6639 || chip_id == 0x7927 || ...) {
        // Address range check and remapping:
        if (reg_offset in range [0x18800000, 0x18c00000)) {
            // Remap to BAR0 offset via FUN_140052e38
            FUN_140052e38(param_1, remapped_offset, value);
        } else if (reg_offset in range [0x7c000000, 0x7c400000)) {
            // Direct BAR0 access
            FUN_14005313c(param_1, reg_offset, value);
        } else {
            // Standard mapping
            FUN_140052e38(param_1, translated_offset, value);
        }
    }

    // Simple case (no translation needed):
    *(uint *)(reg_offset + param_1[5]) = value;  // MMIO write: bar0[offset] = value
}
```

For Ring 2 CIDX write:
```
Register: MT_WPDMA_TX_RING_CIDX(2) = 0xd4328
Value: new CPU index (incremented after writing descriptor)
```

---

## 8. Sequence Number Generator (FUN_14009a46c)

### CRITICAL CORRECTION: This is NOT a ring selector!

Previous analysis incorrectly assumed `FUN_14009a46c(param_1, 3, 1)` returned a ring index. It's actually a **per-queue sequence number generator**.

```c
char FUN_14009a46c(longlong adapter, int queue_id, char increment) {
    // Per-queue sequence counters at adapter + 0x146cde4
    // queue_id: 0=AC_BK, 1=AC_BE, 2=mgmt(special), 3=AC_VO?

    if (increment) {
        if (queue_id == 2) {
            // Mgmt queue: wraps at 127 (0x7f)
            counter[2] = (counter[2] + 1) & 0x7f;
        } else {
            // Other queues: wraps at 100
            counter[queue_id] = (counter[queue_id] + 1) % 100;
        }
    }

    byte val = counter[queue_id];

    // Special handling for non-mgmt queues: skip 0
    if (val == 0 && queue_id != 2) {
        counter[queue_id] = 1;
        return 1;
    }

    // Special handling for mgmt queue: avoid 0 and 1
    if (queue_id == 2 && (val == 1 || val == 0)) {
        counter[2] = 0x6f;  // = 111
        return 0x6f;
    }

    return val;
}
```

In MlmeAuthReqAction: `FUN_14009a46c(adapter, 3, 1)` generates a sequence number for queue 3, which is passed as the 4th parameter to `FUN_14000cf90`. This sequence number is used for frame ordering/dedup, **not for ring selection**.

The DMA ring (2) is **hardcoded** in `NdisCommonHifPciMlmeHardTransmit`:
```c
iVar10 = FUN_14005d1a4(HIF_handle, buf_node, 2);  // Ring 2 — HARDCODED!
```

---

## 9. Retry Configuration

### Windows Auth Frame Retries:

From XmitWriteTxDv1 with queue_class = 0x0104:
- `(char)queue_class = 0x04`, which is NOT equal to 7
- Takes the ELSE branch: `DW3 |= 0xF000`
- `0xF000` with `MT_TXD3_REM_TX_COUNT = GENMASK(15,11)`:
  - Bits [15:11] of 0xF000 = 0b11110 = **30 retries**

### Our Driver:
```c
val = FIELD_PREP(MT_TXD3_REM_TX_COUNT, 30);  // Same: 30 retries
```

**Retry count matches** (30). The different TXFREE count (15) we see might be firmware behavior or a different failure mode, not a TXD configuration issue.

---

## 10. Comparison: Windows vs Our Driver

### Side-by-Side TXD Comparison

| Field | Bit Position | Windows Auth | Our Driver Auth | Match? |
|-------|-------------|-------------|-----------------|--------|
| **DW0** | | | | |
| TX_BYTE_CNT | [15:0] | frame_len + 32 | skb->len + MT_TXD_SIZE ✅ | ✅ |
| PKT_FMT | [24:23] | 0 (from encoding) | 2 (MT_TX_TYPE_CMD) | **❌ WRONG** |
| Q_IDX | [31:25] | 0x04-ish (from encoding) | 0 (MCU_Q0) | **❌ WRONG** |
| **DW1** | | | | |
| WLAN_IDX | [11:0] | from STA record | from wcid->idx ✅ | ✅ |
| TGID | [13:12] | band_config | band_idx ✅ | ✅ |
| HDR_FORMAT_V3 | [15:14] | 0b10 (802.11) | 0b10 (802.11) ✅ | ✅ |
| HDR_INFO | [20:16] | 24 (bytes) | mac_hdr_len/2 = 12 | **❌ DIFF** |
| OWN_MAC | [30:25] | own_mac_idx | omac_idx | ✅ |
| FIXED_RATE | [31] | complex (via DW2) | BIT(31) set | **⚠️ CHECK** |
| Non-CT flag | [25] | 1 (set for SF mode) | not explicitly set | **❌ MISSING** |
| **DW2** | | | | |
| SUB_TYPE | [3:0] | 0xB (auth) | 0xB (auth) ✅ | ✅ |
| FRAME_TYPE | [5:4] | 0 (management) | 0 (management) ✅ | ✅ |
| HDR_PAD | [11:10] | 0 | 0 (no key) ✅ | ✅ |
| MAX_TX_TIME | [25:16] | 0 | 30 (units) | **❌ DIFF** |
| Fixed rate bits | [31]+[29] | 0xa0000000 | 0 | **❌ MISSING** |
| **DW3** | | | | |
| NO_ACK | [0] | 0 | 0 ✅ | ✅ |
| PROTECT | [1] | from BSS | 0 (no key) | ⚠️ |
| REM_TX_COUNT | [15:11] | 30 | 30 ✅ | ✅ |
| BA_DISABLE | [28] | 0 | 1 (set by our code) | **❌ DIFF** |
| **DW5** | | | | |
| PID | [7:0] | 0 | 0 ✅ | ✅ |
| TX_STATUS_HOST | [10] | 0 | 0 ✅ | ✅ |
| **DW6** | | | | |
| DIS_MAT | [3] | 0 | 1 | **❌ DIFF** |
| MSDU_CNT | [9:4] | 0 | 1 | **❌ DIFF** |
| TX_RATE | [21:16] | 0x4B (OFDM 6M) | 0 | **❌ DIFF** |
| **DW7** | | | | |
| TXD_LEN | [31:30] | 0 | 0 ✅ | ✅ |

### DMA Path Comparison

| Aspect | Windows | Our Driver | Match? |
|--------|---------|------------|--------|
| **DMA Ring** | Ring 2 (HW) | Ring 0 (data) or Ring 15 (MCU) | **❌ CRITICAL** |
| **Buffer Format** | TXD + frame inline (SF) | TXD+TXP (CT) on Ring 0; TXD+frame (SF) on Ring 15 | **❌ CRITICAL** |
| **Ring Descriptor Size** | 16 bytes per entry | 16 bytes ✅ | ✅ |
| **Ring Kick** | CIDX write to 0xd4328 | CIDX write to ring register ✅ | ✅ |
| **Descriptor OWN bit** | DW1[30] = 1 | DW1[30] = 1 ✅ | ✅ |

---

## 11. Recommended Changes

### Priority 1: TX Ring 2 for Management Frames (CRITICAL)

The Windows driver uses **hardware TX Ring 2** for management frames. Our driver must:

1. **Initialize TX Ring 2** during probe:
   - Allocate ring descriptor memory (same as Ring 0)
   - Write ring base address to `MT_WPDMA_TX_RING_BASE(2)` = BAR0+0xd4320
   - Write ring count to `MT_WPDMA_TX_RING_CNT(2)` = BAR0+0xd4324
   - Enable ring in prefetch configuration
   - Add TX Ring 2 interrupt enable

2. **Route management frames to Ring 2**:
   - In `mt7927_mac_write_txwi()`, set PKT_FMT=0 (not CMD) for mgmt
   - Use SF mode (TXD + frame inline) on Ring 2
   - Submit DMA descriptor to Ring 2 instead of Ring 15

### Priority 2: TXD Field Corrections

1. **DW0 PKT_FMT**: Change from 2 (CMD) to 0 (TXD only) for mgmt on Ring 2
2. **DW0 Q_IDX**: Change from 0 (MCU_Q0) to match Windows encoding
3. **DW1 HDR_INFO**: Windows uses hdr_len in bytes (24), we use hdr_len/2 (12). Check CONNAC3 spec.
4. **DW2**: Add fixed rate bits 0xa0000000 for mgmt
5. **DW6 TX_RATE**: Set to 0x4B (OFDM 6Mbps) instead of 0 (firmware-decided)
6. **DW6 MSDU_CNT/DIS_MAT**: Remove these for mgmt (Windows doesn't set them)

### Priority 3: Minor Fixes

1. **DW3 BA_DISABLE**: Don't set for auth frames (Windows doesn't)
2. **DW2 MAX_TX_TIME**: Set to 0 instead of 30 (Windows sets 0 for mgmt)
3. **DW1[25]**: Set non-CT-mode flag for SF frames

---

## Appendix A: Source Paths Used

| Source | Content |
|--------|---------|
| `/tmp/ghidra_full_tx_dma.txt` | Main decompilation output |
| `/tmp/ghidra_txdw_output.txt` | XmitWriteTxDv1 decompilation |
| `/tmp/ghidra_txd_output.txt` | N6PciTxSendPkt decompilation |
| `/tmp/ghidra_submit_output.txt` | NdisCommonHifPciMlmeHardTransmit |

## Appendix B: Windows Source Path (from strings)

The Windows driver source path revealed in trace strings:
```
e:\worktmp\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\
7295\wlan_driver\seattle\wifi_driver\windows\platform\ndiscommon\ndiscommonhifPCI.c
```
- `neptune-mp` = MT7927 project codename
- `7295` = build number
- `ndiscommonhifPCI.c` = HIF (Host Interface) PCIe abstraction layer
