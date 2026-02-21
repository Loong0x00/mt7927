# Windows RE: Auth Frame TXD DW2/DW5/DW6/DW7 — Binary-Verified Values

**Date**: 2026-02-17
**Source**: Ghidra decompilation of `mtkwecx.sys` — `XmitWriteTxDv1` (0x1401a2ca4) + `MlmeHardTransmit` (0x140053a0c)
**Method**: Full control flow trace through decompiled C and x86-64 disassembly

---

## TL;DR — Auth Frame Precise TXD Values

| DWord | Value | Notes |
|-------|-------|-------|
| **DW2** | **0xA000000B** | Fixed rate bits[31,29] + SUB_TYPE=0xB |
| **DW5** | **0x00000600 \| PID** | TX_STATUS_2_HOST + TX_STATUS_FMT + PID byte |
| **DW6** | **0x004B0000** | OFDM 6Mbps (rate index 0x4B) |
| **DW7** | **0x00000000** | TXD_LEN=0, bit 30 explicitly cleared |

### Which Document Was Correct?

| Field | Doc1 (full_txd_dma_path) | Doc2 (dw0_dw1_precise) | **Verified** |
|-------|-------------------------|------------------------|--------------|
| DW2 | 0xA000000B | 0x00000000 | **Doc1 correct** |
| DW5 | 0x00000000 | 0x00000000 | **BOTH WRONG** — should be 0x600+PID |
| DW6 | 0x004B0000 | 0x00000000 | **Doc1 correct** |
| DW7 | 0x00000000 | 0x00000000 | **Both correct** |

---

## Detailed Control Flow Analysis

### 1. TX Info Structure (param_3) — Built by MlmeHardTransmit

MlmeHardTransmit (FUN_140053a0c) builds a 0x71-byte TX info structure (`local_c8`) on the stack, then passes `&local_c8` as `param_3` to XmitWriteTxDv1.

Key fields set for auth frames:

| Stack Var | Offset from &local_c8 | XmitWriteTxDv1 Access | Value for Auth | Meaning |
|-----------|----------------------|----------------------|----------------|---------|
| local_c8 | 0x00 | `*param_3` | frame_length | Frame payload size |
| local_c6 | 0x02 | `*(byte*)(param_3+1)` | 0x18 (24) | 802.11 header length |
| local_c4 | 0x04 | `(char)param_3[2]` | 0x01 (modified from 0x04) | Queue class low byte |
| local_c1 | 0x07 | `*(byte*)(param_3+0x07)` | WLAN_IDX | STA index |
| local_c0 | 0x08 | `(char)param_3[4]` | 1 | ACK flag (1=need ACK) |
| local_be | 0x0A | `*(byte*)(param_3+0x05)` via short* | BSS flag | From BSS context |
| local_bd | 0x0B | `*(char*)(param_3+0x0b)` | protect | BSS protection |
| local_b9 | **0x0F** | `*(char*)(param_3+0x0f)` | **seq_num (1-99, never 0)** | **PID / priority** |
| local_9d | **0x2B** | `*(short*)(param_3+0x2b)` | **0 (management)** | **FRAME_TYPE** |
| local_9b | **0x2D** | `*(short*)(param_3+0x2d)` | **0xB (auth)** | **SUB_TYPE** |
| local_99 | 0x2F | `*(param_3+0x2f)` | BSS context ptr | For STA lookup |
| local_8f | 0x39 | `*(byte*)(param_3+0x39)` | 0 (zeroed) | Duration flag |
| local_7c | **0x4C** | `(char)param_3[0x26]` | **1** | **fixed_rate flag** |
| (unnamed) | 0x53 | `*(char*)(param_3+0x53)` | 0 (zeroed) | Secondary fixed rate |
| (unnamed) | 0x3D | `*(char*)(param_3+0x3d)` | 0 (zeroed) | Tertiary fixed rate |

#### Critical: `local_7c = 1` is set unconditionally

From MlmeHardTransmit decompilation (line ~723 in Ghidra output):
```c
local_7c = 1;   // offset 0x4C = param_3[0x26] = fixed_rate flag
```
This is set for ALL management frames processed by MlmeHardTransmit, including auth.

#### Critical: `local_b9 = seq_num` is always non-zero

From MlmeHardTransmit (line ~727):
```c
local_b9 = uVar9;  // uVar9 = (byte)param_3 = seq_num from FUN_14009a46c
```

`FUN_14009a46c(adapter, 3, 1)` returns values 1-99 (wraps at 100, explicitly avoids 0 for non-mgmt queues). So `local_b9 >= 1` always.

---

### 2. DW2 Trace — Result: 0xA000000B

#### Step 1: Frame Type/SubType encoding (XmitWriteTxDv1 lines ~104-113)

```c
// param_3[0x26] = local_7c = 1 ≠ 0 → ELSE branch (fixed-rate path)
uVar4 = (uint)*(ushort *)((longlong)param_3 + 0x2b) << 4;  // FRAME_TYPE << 4 = 0 << 4 = 0

// ELSE branch: encode into DW2 (not DW7)
uVar4 = (uVar4 ^ param_2[2]) & 0x30 ^ param_2[2];  // = 0 (DW2 was zeroed)
param_2[2] = uVar4;  // DW2 = 0

param_2[2] = (*(ushort *)((longlong)param_3 + 0x2d) ^ uVar4) & 0xf ^ uVar4;
// = (0xB ^ 0) & 0xf ^ 0 = 0xB
// DW2 = 0x0000000B
```

**Assembly confirmation** (0x1401a2e15-0x1401a2e2d):
```asm
MOV EAX, [RBX + 0x8]         ; EAX = DW2 (currently 0)
XOR EDX, EAX                  ; EDX = FRAME_TYPE<<4 ^ DW2
AND EDX, 0x30                 ; mask bits[5:4]
XOR EDX, EAX                  ; merge into DW2
MOV [RBX + 0x8], EDX          ; DW2[5:4] = FRAME_TYPE = 0
MOVZX EAX, word ptr [RDI+0x2d]; EAX = SUB_TYPE = 0xB
XOR EAX, EDX
AND EAX, 0xf                  ; mask bits[3:0]
XOR EAX, EDX                  ; merge into DW2
MOV [RBX + 0x8], EAX          ; DW2[3:0] = 0xB → DW2 = 0x0000000B
```

#### Step 2: Duration flag (lines ~114-115)

```c
param_2[2] = (((uint)*(byte *)((longlong)param_3 + 0x39) << 10 ^ param_2[2]) & 0x400 ^ param_2[2])
             & 0xffffefff;
// param_3+0x39 = 0 (duration flag zeroed) → no change, also clears bit 12
// DW2 remains 0x0000000B
```

#### Step 3: Fixed Rate Block (lines ~147-152)

```c
if ((((char)param_3[0x26] != '\0') ||        // local_7c = 1 → TRUE!
     (*(char *)((longlong)param_3 + 0x53) != '\0') ||  // 0 → FALSE
     (*(char *)((longlong)param_3 + 0x3d) != '\0'))) {  // 0 → FALSE
    // Condition TRUE because param_3[0x26] = 1

    param_2[7] = param_2[7] & 0xbfffffff;   // Clear DW7 bit 30
    param_2[2] = param_2[2] | 0xa0000000;   // DW2 |= 0xa0000000
    param_2[6] = param_2[6] & 0x7e00ffff | 0x4b0000;  // DW6 = 0x004B0000
}
// DW2 = 0x0000000B | 0xa0000000 = 0xA000000B
```

**Assembly confirmation** (0x1401a2f0f-0x1401a2f3a):
```asm
CMP byte ptr [RDI + 0x4c], R15B  ; param_3+0x4c vs 0 (local_7c = 1)
JNZ 0x1401a2f21                   ; → TAKEN (1 ≠ 0)
...
1401a2f21: BTR dword ptr [RBX+0x1c], 0x1e  ; Clear DW7 bit 30
1401a2f26: OR  dword ptr [RBX+0x8], 0xa0000000  ; DW2 |= 0xa0000000
1401a2f2d: MOV EAX, [RBX+0x18]             ; load DW6
1401a2f30: AND EAX, 0x7e00ffff              ; clear rate bits
1401a2f35: OR  EAX, 0x4b0000               ; set rate = 0x4B
1401a2f3a: MOV [RBX+0x18], EAX             ; store DW6
```

#### Step 4: Post-processing (lines ~153-156)

```c
// Only for data frames (FRAME_TYPE == 2):
if (*(short *)((longlong)param_3 + 0x2b) == 2) {
    param_2[2] = param_2[2] & 0xdfffffff;  // Would clear bit 29
}
// Auth: FRAME_TYPE = 0 ≠ 2 → NOT TAKEN → bit 29 preserved
```

#### Step 5: Rate override (line ~157+)

```c
if (*(char *)(*(longlong *)(param_1 + 0x14c0) + 0x2e9db4) == '\0') {
    return;  // Early return if no rate override
}
// If rate override enabled:
param_2[2] = param_2[2] | 0x80000000;  // bit 31 already set → no change
```

**DW2 Final = 0xA000000B regardless of rate override path.**

---

### 3. DW5 Trace — Result: 0x00000600 | PID

#### The key branch (XmitWriteTxDv1 lines ~131-146)

```c
if (*(char *)((longlong)param_3 + 0xf) == '\0') {
    // param_3+0x0f = local_b9 = seq_num (1-99) → NEVER '\0' for auth!
    // This branch NOT taken
    ...
}
else if ((*(short *)((longlong)param_3 + 0x2b) == 0) &&    // FRAME_TYPE=0 (mgmt) → TRUE
        (((*(short *)((longlong)param_3 + 0x2d) - 4U & 0xfffa) != 0 ||  // (0xB-4)&0xfffa=2≠0 → TRUE
         (*(short *)((longlong)param_3 + 0x2d) == 9)))) {

    // THIS BRANCH TAKEN for auth frames!

    param_2[5] = param_2[5] | 0x600;       // DW5 bits[10:9] set
    *(undefined1 *)(param_2 + 5) = *(undefined1 *)((longlong)param_3 + 0xf);
    // DW5 byte[0] = local_b9 = PID value
}
```

**Assembly confirmation** (0x1401a2eba-0x1401a2f0c):
```asm
CMP byte ptr [RDI+0xf], R15B    ; param_3+0x0f vs 0
JNZ 0x1401a2ee6                  ; → TAKEN (seq_num ≠ 0)
...
1401a2ee6: TEST AX, AX           ; FRAME_TYPE == 0?
1401a2ee9: JNZ 0x1401a2f0f       ; not taken (FRAME_TYPE=0)
1401a2eeb: MOVZX ECX, word ptr [RDI+0x2d]  ; SUB_TYPE = 0xB
1401a2eef: MOV EDX, 0xfffa
1401a2ef4: LEA EAX, [RCX-0x4]    ; 0xB - 4 = 7
1401a2ef7: TEST DX, AX           ; 7 & 0xfffa = 2 ≠ 0
1401a2efa: JNZ 0x1401a2f02       ; → TAKEN
1401a2f02: OR  dword ptr [RBX+0x14], 0x600  ; DW5 |= 0x600
1401a2f09: MOV AL, byte ptr [RDI+0xf]       ; AL = PID
1401a2f0c: MOV byte ptr [RBX+0x14], AL      ; DW5[7:0] = PID
```

**DW5 bit breakdown:**
```
bits[7:0]  = PID (from FUN_14009a46c, typically 1-99)
bit 9      = 1 (TX_STATUS_FMT)
bit 10     = 1 (TX_STATUS_2_HOST — request TX completion to host!)
bits[13:12]= 0 (cleared by subsequent param_2[5] &= 0xffffcfff)
```

**DW5 = 0x00000600 | PID_byte** (e.g., 0x00000601 for first auth attempt)

#### Subtype filter — which mgmt subtypes get DW5 status request

The condition `(subtype - 4) & 0xfffa != 0 || subtype == 9` **excludes** only:
- Subtype 4 (Probe Request) — no TX status needed (broadcast)
- Subtype 5 (Probe Response) — no TX status needed

**All other management subtypes including Auth (0xB) get TX status feedback.** This makes sense — auth needs ACK confirmation.

---

### 4. DW6 Trace — Result: 0x004B0000 (default) or adapter-specific

#### Default path (rate override disabled)

From the fixed rate block (same as DW2 Step 3):
```c
param_2[6] = param_2[6] & 0x7e00ffff | 0x4b0000;
// DW6 = 0x004B0000
```

**Rate 0x4B decoding (CONNAC3 TX_RATE format):**
```
0x4B = 0b 0100 1011
bits[5:0]  = 0x0B = MCS index 11 → OFDM 6Mbps
bits[9:6]  = 0x01 = TX_RATE_MODE_OFDM
bits[13:10]= 0x00 = 1 spatial stream
```

#### Rate override path (if adapter flag at +0x2e9db4 is set)

If the rate override flag is enabled, additional processing occurs:

```c
param_2[2] |= 0x80000000;    // DW2 bit 31 (already set, no change)
param_2[6] &= 0x7fffffff;    // Clear DW6 bit 31

// PHY mode → DW6 bits[24:22]
bVar1 = FUN_1401fd9f0(param_1, lVar3);
param_2[6] = ((bVar1 << 0x16) ^ param_2[6]) & 0x1c00000 ^ param_2[6];

// Rate index → DW6 bits[21:16] (may override the 0x4B set earlier!)
// For PHY mode 1 (OFDM): uses lookup table {0xb,0xf,0xa,0xe,0x9,0xd,0x8,0xc}
param_2[6] = ((bVar1 << 0x10) ^ param_2[6]) & 0x3f0000 ^ param_2[6];

// Additional fields:
// DW6 bits[2:0] = NSS from FUN_1401fd9b0
// DW6 bit 30 = from FUN_1401fda28
// DW6 bit 27 = from FUN_1401fda40
// DW6 bits[26:25] = from FUN_1401fda0c
// DW6 bit 29 = from FUN_1401fd9d0
```

**For typical auth frames, the rate override is unlikely to change the result significantly** — the OFDM 6Mbps rate is standard for management frames. But the exact DW6 value depends on adapter configuration.

**Conservative conclusion: DW6 = 0x004B0000 as the baseline, with possible adapter-specific rate override.**

---

### 5. DW7 Trace — Result: 0x00000000

#### Path analysis

1. **TXD zeroed** at start: DW7 = 0
2. **Frame type branch**: For fixed-rate frames (param_3[0x26] != 0), frame type goes into DW2 not DW7. DW7 unchanged.
3. **DW5/DW7 conditional block**: Only for data frames (FRAME_TYPE==2) with param_3+0x0f==0. Neither condition met for auth. DW7 unchanged.
4. **Fixed rate block**: `param_2[7] &= 0xbfffffff` clears bit 30 (already 0). DW7 = 0.
5. **Rate override path**: Does not modify DW7.

**DW7 = 0x00000000 confirmed.**

**Assembly** (0x1401a2f21):
```asm
BTR dword ptr [RBX+0x1c], 0x1e  ; Clear DW7 bit 30 (TXD_LEN)
; Already 0, no effect
```

---

## Summary of Document Errors

### Doc1 (`win_re_full_txd_dma_path.md`) — Mostly correct

| Field | Doc1 Claim | Verified | Status |
|-------|-----------|----------|--------|
| DW2 | 0xA000000B | 0xA000000B | ✅ Correct |
| DW5 | 0x00000000 | 0x00000600+PID | **❌ Wrong** — missed TX status request |
| DW6 | 0x004B0000 | 0x004B0000 | ✅ Correct |
| DW7 | 0x00000000 | 0x00000000 | ✅ Correct |

**Doc1 error**: The analysis of DW5 concluded "no PID tracking, no status request" but missed that auth frames take the `else if` branch where `param_3+0x0f != 0` AND `FRAME_TYPE == 0` AND `SUB_TYPE == 0xB` → DW5 gets 0x600 | PID.

### Doc2 (`win_re_txd_dw0_dw1_precise.md`) — Mostly wrong for DW2/DW6

| Field | Doc2 Claim | Verified | Status |
|-------|-----------|----------|--------|
| DW2 | 0x00000000 ("unless forced") | 0xA000000B | **❌ Wrong** — auth IS forced rate |
| DW5 | 0x00000000 | 0x00000600+PID | **❌ Wrong** |
| DW6 | 0x00000000 ("Windows DW6=0") | 0x004B0000 | **❌ Wrong** — fixed rate = 0x4B |
| DW7 | 0x00000000 | 0x00000000 | ✅ Correct |

**Doc2 error**: Claimed auth frames don't use fixed rate. But `local_7c = 1` is set **unconditionally** in MlmeHardTransmit for all management frames, so `param_3[0x26] != 0` is ALWAYS true → the fixed rate block ALWAYS executes for management frames.

---

## Impact on Our Driver

### What we must set for auth TXD:

```c
// DW2: Fixed rate + management auth frame type
txd->dw2 = 0xA000000B;
// bit 31 = FIXED_RATE indicator
// bit 29 = BM/SW_POWER_MGMT
// bits[5:4] = 0 (FRAME_TYPE = management)
// bits[3:0] = 0xB (SUB_TYPE = authentication)

// DW5: TX status request with PID
txd->dw5 = FIELD_PREP(MT_TXD5_TX_STATUS_HOST, 1) |  // bit 10
            FIELD_PREP(MT_TXD5_TX_STATUS_MCU, 1) |    // bit 9
            FIELD_PREP(MT_TXD5_PID, pid);              // bits[7:0]
// This tells firmware to report TX completion! Without this, firmware
// may not generate TX_DONE/TXFREE events.

// DW6: OFDM 6Mbps fixed rate
txd->dw6 = 0x004B0000;
// bits[22:16] = 0x4B = OFDM 6Mbps

// DW7: No TXD extension
txd->dw7 = 0;
```

### **DW5 is likely the key missing piece!**

Our driver currently sets DW5 = 0, meaning:
- **TX_STATUS_2_HOST = 0** → Firmware does NOT report TX completion to host
- **TX_STATUS_FMT = 0** → No status format specified
- **PID = 0** → No packet ID for matching

This could explain why firmware is "completely silent" after consuming the DMA descriptor — **we never asked for TX status feedback!** The firmware processes the frame but has no reason to notify the host about the result.

---

## Appendix: Subtype Filter for DW5 Status Request

The condition `(subtype - 4) & 0xfffa != 0 || subtype == 9` determines which management subtypes get TX status:

| Subtype | Name | (sub-4)&0xfffa | ==9? | Gets DW5? |
|---------|------|----------------|------|-----------|
| 0 | Assoc Req | 0xfffc | no | **YES** |
| 1 | Assoc Resp | 0xfffd→0xfff8 | no | **YES** |
| 2 | Reassoc Req | 0xfffe→0xfffa | no | **YES** |
| 3 | Reassoc Resp | 0xffff→0xfffa | no | **YES** |
| 4 | Probe Req | 0 | no | NO |
| 5 | Probe Resp | 1→0 | no | NO |
| 6 | Timing Adv | 2 | no | **YES** |
| 7 | Reserved | 3→2 | no | **YES** |
| 8 | Beacon | 4 | no | **YES** |
| 9 | ATIM | 5→0 | **yes** | **YES** |
| 0xA | Disassoc | 6 | no | **YES** |
| **0xB** | **Auth** | **7→2** | **no** | **YES** |
| 0xC | Deauth | 8 | no | **YES** |
| 0xD | Action | 9→8 | no | **YES** |

Only Probe Request (4) and Probe Response (5) are excluded — they're typically broadcast/unsolicited and don't need ACK confirmation.
