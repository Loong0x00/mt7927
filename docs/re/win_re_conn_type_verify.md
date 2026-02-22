# Windows RE: conn_type / packed fields verification

**Source**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys`  
**Date**: 2026-02-21  
**Purpose**: Verify BSS_INFO_BASIC and STA_REC_BASIC conn_type/conn_state fields

---

## Architecture Overview (critical context)

`mtkwecx.sys` is a **host-side** Windows driver. The handlers analyzed below run on the
HOST CPU and build MCU TLV command buffers sent to MT6639 firmware. They are NOT
firmware code.

### Dispatch Table Structure

The driver uses a unified dispatch table at `0x1402507e0` with **58 entries x 13 bytes each**:

```
Entry format (13 bytes):
  [+0x00] u8   outer_tag      (TLV tag value sent on wire)
  [+0x01] u8   unknown
  [+0x02] u16  inner_cid      (class: 0x02=BSS_INFO, 0x25=STA_REC)
  [+0x04] u8   option_filter  (for tag=0xed multi-handler dispatch)
  [+0x05] u64  fn_ptr         (handler function pointer)
```

Lookup function at `0x14014f720`:
- For `tag != 0xed`: match by `word[entry+0] == tag` → option_filter NOT used
- For `tag == 0xed`: match by `byte[entry+4] == option_byte` → enables multiple handlers per class

### Node Allocator (`0x14014f788`)

Allocates pool of `payload_size + 0x28` bytes:

```
Node structure:
  [+0x00..+0x07]  linked list prev/next pointers
  [+0x10]         class byte (0x02=BSS_INFO, 0x25=STA_REC)
  [+0x14]         payload_size (dword)
  [+0x18]         payload_ptr → node+0x28
  [+0x20]         payload_size (word)
  [+0x28..]       payload data ← THIS IS WHAT FIRMWARE RECEIVES
```

The dispatch serializes `node->payload_ptr` → MCU TLV body (wire).

---

## Task 1: BSS_INFO_BASIC conn_type analysis

### Handler: `0x140144db0` (dispatch table: `tag=0x05, inner_cid=0x02`)

#### Full Disassembly (relevant sections)

```
0x140144db0:  cmp  byte ptr [rdx], 5        ; tag must = 0x05
0x140144dc6:  cmp  dword ptr [rdx+0x10], 4  ; payload_len (or version) must = 4
0x140144dd0:  mov  rdi, qword ptr [rdx+0x18]; rdi = payload ptr from NDIS input
0x140144dd4:  mov  r8d, 0xc                 ; allocate 12 bytes
0x140144dda:  mov  dl, 2                    ; class = BSS_INFO
0x140144ddc:  call 0x14014f788              ; allocate node
; → on success:
0x140144e3c:  mov  rcx, qword ptr [rax+0x18]; rcx = node->payload (at node+0x28)
0x140144e40:  mov  al, byte ptr [rdi]       ; al = NDIS input[0]
0x140144e42:  mov  byte ptr [rcx], al       ; payload[+0x00] = NDIS[0]
0x140144e44:  mov  dword ptr [rcx+4], 0x80015 ; payload[+0x04] = 0x00080015 ← HARDCODED
0x140144e4b:  mov  al, byte ptr [rdi+1]     ; al = NDIS input[1]
0x140144e4e:  mov  byte ptr [rcx+8], al     ; payload[+0x08] = NDIS[1]
```

#### Wire TLV Payload (12 bytes — what firmware receives)

| Offset | Size | Value | Source |
|--------|------|-------|--------|
| +0x00  | u8   | BSS_INDEX | from NDIS input[0] |
| +0x01  | u8   | 0x00 | zero-init (not written) |
| +0x02  | u8   | 0x00 | zero-init |
| +0x03  | u8   | 0x00 | zero-init |
| +0x04  | u32  | **0x00080015** | **HARDCODED** |
| +0x08  | u8   | param | from NDIS input[1] |
| +0x09  | u8   | 0x00 | zero-init |
| +0x0a  | u8   | 0x00 | zero-init |
| +0x0b  | u8   | 0x00 | zero-init |

**Wire bytes**: `[bss_idx][00][00][00][15][00][08][00][param][00][00][00]`

#### conn_type Question

**Q: Is `conn_type` explicitly in this packed field?**  
**A: YES — but it is HARDCODED to `0x00080015`, never read from host input.**

The Windows driver does NOT pass `conn_type` from the NDIS/user layer to firmware.
`0x00080015` is hard-coded for **all** BSS_INFO_BASIC TLVs regardless of connection type.

**Q: What does `0x15` at byte[+4] encode?**  
In MT6639 context, `CONNECTION_INFRA_STA = BIT(0)|BIT(16) = 0x00010001`.  
`0x00080015` is **completely different** from `0x10001`. The encoding scheme differs:

```
0x00080015 bit analysis:
  BIT(0)  = 1  (infra flag?)
  BIT(2)  = 1  (STA mode?)
  BIT(4)  = 1  (active/connected?)
  BIT(19) = 1  (upper 16-bit flag?)

Byte breakdown (LE):
  byte[4] = 0x15  (conn_type or mode flags)
  byte[5] = 0x00
  byte[6] = 0x08  (additional flag BIT(3))
  byte[7] = 0x00
```

The `0x00080015` is a **firmware-specific packed value** for Windows MT6639 STA connection.
It does NOT correspond to the MT6639 Android `CONNECTION_INFRA_STA = 0x10001`.

**Q: Does the handler read `conn_type` from input and write it to output?**  
**A: NO.** The handler reads only `NDIS[0]` (BSS index) and `NDIS[1]` (unknown param).
All other fields including `conn_type` are hardcoded.

#### Our Driver's BSS_INFO_BASIC — Format Mismatch

Our driver (MT6639 Android-derived struct) sends:
```c
struct bss_info_basic {
    // Large struct with conn_type=0x10001 at some MT6639 offset
    __le32 conn_type;  // = 0x00010001 (CONNECTION_INFRA_STA)
    ...
};
```

Windows firmware expects at wire offset [+4]: `0x00080015`  
Our driver sends at MT6639 struct's conn_type offset: `0x00010001`

**This is a payload format mismatch.** The MT6639 Android BSS_INFO_BASIC struct layout
is incompatible with what the Windows-side firmware expects.

---

## Task 2: STA_REC_BASIC conn_state analysis

### Handler: `0x1401458d0` (dispatch table: `tag=0xb1, inner_cid=0x25, option_filter=0xa8`)

#### Full Disassembly (key section)

```
0x1401458d0:  cmp  byte ptr [rdx], 0xb1     ; tag must = 0xb1
0x1401458e6:  cmp  dword ptr [rdx+0x10], 0x3c ; payload_len must = 60 bytes
0x1401458f0:  mov  rdi, qword ptr [rdx+0x18]; rdi = our TLV payload ptr
0x1401458f4:  mov  r8d, 0x44               ; allocate 0x44 = 68 bytes output
0x1401458fa:  mov  dl, 0x25                ; class = STA_REC
0x1401458fc:  call 0x14014f788             ; allocate node
; → on success:
0x14014595c:  mov  rcx, qword ptr [rax+0x18]; rcx = node payload
0x140145960:  mov  byte ptr [rcx], 0xff    ; payload[+0x00] = 0xff ← HARDCODED
0x140145963:  mov  dword ptr [rcx+4], 0x400087 ; payload[+0x04] = 0x00400087 ← HARDCODED
0x14014596a:  mov  al, byte ptr [rdi]      ; al = input[0]
0x14014596c:  mov  byte ptr [rcx+8], al    ; payload[+0x08] = conn_state ← FROM INPUT
0x14014596f:  movzx eax, word ptr [rdi+2]  ; ax = input[2:4]
0x140145973:  mov  word ptr [rcx+0xa], ax  ; payload[+0x0a] = AID
0x140145977:  mov  al, byte ptr [rdi+4]    ; mac[0]
0x14014597a:  mov  byte ptr [rcx+0xc], al  ; payload[+0x0c] = mac[0]
0x14014597d:  mov  al, byte ptr [rdi+5]    ; mac[1]
0x140145980:  mov  byte ptr [rcx+0xd], al  ; payload[+0x0d] = mac[1]
0x140145983:  mov  al, byte ptr [rdi+6]    ; mac[2]
0x140145986:  mov  byte ptr [rcx+0xe], al  ; payload[+0x0e] = mac[2]
0x140145989:  mov  al, byte ptr [rdi+7]    ; mac[3]
0x14014598c:  mov  byte ptr [rcx+0xf], al  ; payload[+0x0f] = mac[3]
0x14014598f:  mov  al, byte ptr [rdi+8]    ; mac[4]
0x140145992:  mov  byte ptr [rcx+0x10], al ; payload[+0x10] = mac[4]
0x140145995:  mov  eax, dword ptr [rdi+0xc]; dword at input[12]
0x140145998:  mov  dword ptr [rcx+0x14], eax
0x14014599b:  mov  eax, dword ptr [rdi+0x10]; dword at input[16]
0x14014599e:  mov  dword ptr [rcx+0x18], eax
0x00000001401459a1:  mov  eax, dword ptr [rdi+0x14]; dword at input[20]
0x00000001401459a4:  mov  dword ptr [rcx+0x1c], eax
0x00000001401459a7:  mov  eax, dword ptr [rdi+0x18]; dword at input[24]
0x00000001401459aa:  mov  dword ptr [rcx+0x20], eax
```

#### Wire TLV Payload (0x44 = 68 bytes — what firmware receives)

| Output Offset | Size | Value | Source |
|----------------|------|-------|--------|
| +0x00 | u8   | **0xFF** | **HARDCODED** |
| +0x01..+0x03 | - | 0x00 | zero-init |
| +0x04 | u32  | **0x00400087** | **HARDCODED** |
| +0x08 | u8   | **conn_state** | from our input[0] ← KEY FIELD |
| +0x09 | u8   | 0x00 | zero-init |
| +0x0a | u16  | AID | from our input[2:4] |
| +0x0c | u8   | mac[0] | from our input[4] |
| +0x0d | u8   | mac[1] | from our input[5] |
| +0x0e | u8   | mac[2] | from our input[6] |
| +0x0f | u8   | mac[3] | from our input[7] |
| +0x10 | u8   | mac[4] | from our input[8] |
| +0x11 | u8   | mac[5]? | zero-init (not copied!) |
| +0x14 | u32  | field_c | from our input[12] |
| +0x18 | u32  | field_10 | from our input[16] |
| +0x1c | u32  | field_14 | from our input[20] |
| +0x20 | u32  | field_18 | from our input[24] |

**Q: What is `from_input[0]`?**  
**A: `conn_state`** — the connection state byte, placed at firmware output `[+0x08]`.

**Q: Does the handler copy state/type fields from input?**  
**A: YES** — it copies `input[0]` (conn_state) and MAC/AID from the input payload.

**Q: What does `0x87` (low byte of `0x00400087`) encode?**  
```
0x00400087 bit analysis:
  BIT(0) = 1
  BIT(1) = 1
  BIT(2) = 1
  BIT(7) = 1  (0x80)
  BIT(22) = 1 (0x400000)
```
This is a hardcoded STA_REC packed flags field (capabilities/features bitmask).

#### conn_state Values

```c
// STA_REC uses:
STATE_DISCONNECT = 0
STATE_CONNECTED  = 1
```

Our driver sends `conn_state = 1` at our TLV `input[0]` → firmware receives it at `output[+0x08]`.  
**This is CORRECT.**

#### option_filter = 0xa8 Analysis

`0xa8 = 10101000b = BIT(3)|BIT(5)|BIT(7)`

For tag `0xb1` (not `0xed`), the lookup function at `0x14014f720` matches by **tag only**,
NOT by option_filter. The `option_filter=0xa8` field in the dispatch table is metadata
(possibly indicating which operations include STA_REC_BASIC: connect/update/disconnect).

**Our option=0x06 (set) does NOT affect whether this handler is invoked for tag=0xb1.**

#### STA_REC TLV Payload Expected Size

The handler validates: `dword [rdx+0x10] == 0x3c` (60 bytes)  
Our STA_REC_BASIC input payload **must be 60 bytes**.

Input payload layout (what WE send, 60 bytes):
```
[+0x00] u8  conn_state      (0=disconnect, 1=connected)
[+0x01] u8  (unused by handler — but must be present)
[+0x02] u16 AID
[+0x04] u8  mac[0]
[+0x05] u8  mac[1]
[+0x06] u8  mac[2]
[+0x07] u8  mac[3]
[+0x08] u8  mac[4]
[+0x09] u8  mac[5] (note: handler skips this when copying, output[+0x11] stays 0)
[+0x0c] u32 field  → firmware[+0x14]
[+0x10] u32 field  → firmware[+0x18]
[+0x14] u32 field  → firmware[+0x1c]
[+0x18] u32 field  → firmware[+0x20]
[+0x1c..+0x3b] remaining 32 bytes (handler doesn't read these)
```

---

## Task 3: Cross-check Connect Flow

### BSS_INFO conn_type for STA connect

Windows writes `conn_type = 0x00080015` (HARDCODED) at BSS_INFO_BASIC wire payload `[+4]`.

**This is NOT the same as `CONNECTION_INFRA_STA = 0x10001` from MT6639 Android.**

Our current driver code writes `conn_type = 0x10001` at the MT6639 struct's conn_type offset,
which maps to a DIFFERENT wire offset than `[+4]`. **Format mismatch.**

### STA_REC state for STA connect

Windows sends `conn_state = 1` (STATE_CONNECTED) at STA_REC input payload `[+0]`.

**Our driver sending `conn_state = 1` is CORRECT.**

---

## Critical Findings & Driver Impact

### Finding 1: BSS_INFO_BASIC TLV format is WRONG

Our driver uses the MT6639 Android `bss_info_basic` struct which has:
- `conn_type = 0x10001` at its struct offset
- A large payload (much larger than 12 bytes)

Windows firmware expects a **compact 12-byte payload** with:
- `0x00080015` hardcoded at offset `[+4]`
- BSS index at offset `[+0]`
- One param byte at offset `[+8]`

**Action needed**: Replace our BSS_INFO_BASIC TLV payload with the correct 12-byte format.

### Finding 2: BSS_INFO TLV Tags may be Wrong

Windows uses these TLV tag values in BSS_INFO UniCmd:

| Tag  | Handler VA     | Function | Payload size |
|------|----------------|----------|--------------|
| 0x05 | 0x140144db0   | BASIC    | 12 bytes     |
| 0x17 | 0x1401442d0   | RLM      | 8 bytes (payload_len=4) |
| 0x12 | 0x1401444a0   | PROTECT  | 116+ bytes   |
| 0x16 | 0x1401443b0   | IFS_TIME | payload_len=12 |
| 0x19 | 0x1401445e0   | RATE     | 22 bytes     |
| 0x18 | 0x140144110   | SEC      | (no len check) |
| 0x1e | 0x140144e80   | MLD      | 4 bytes      |

MT6639 Android uses different tag values (0x15=BASIC, 0x16=RLM, 0x18=PROTECT, etc.).
**The Windows firmware uses different tag numbers than MT6639 Android source.**

### Finding 3: STA_REC_BASIC conn_state is CORRECT

Our `conn_state = 1` (STATE_CONNECTED) at payload `[+0]` is correct.  
Windows also hardcodes `0xff` at output `[+0]` and `0x00400087` at output `[+4]` — these
are post-processing additions, not wire values we send.

### Finding 4: STA_REC TLV payload size = 60 bytes

The handler validates `payload_len == 0x3c = 60 bytes`. Our driver must send exactly 60 bytes.

---

## Summary Table

| Question | Answer |
|----------|--------|
| BSS_INFO_BASIC conn_type from input? | **NO** — hardcoded 0x00080015 |
| conn_type = 0x10001 valid? | **NO** — completely different encoding |
| STA_REC_BASIC conn_state from input? | **YES** — input[0] → output[+8] |
| conn_state = 1 (CONNECTED) correct? | **YES** |
| BSS_INFO_BASIC payload size | **12 bytes** (not large MT6639 struct) |
| STA_REC_BASIC payload size | **60 bytes** |
| Windows BSS_INFO wire value at [+4] | **0x00080015** |
| MT6639 Android conn_type | **0x10001** (incompatible) |

