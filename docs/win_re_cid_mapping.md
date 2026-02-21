# Windows RE: UniCmd CID Dispatch Table — Complete Mapping

## Analysis Date: 2026-02-21
## Source: mtkwecx.sys (AMD-MediaTek v5.7.0.5275, x64)
## Method: Binary disassembly (Capstone), dispatch table extraction

---

## 1. Architecture Overview

### Key Functions

| VA | Role |
|----|------|
| `0x1400cdc4c` | **nicUniCmdAllocEntry** — top-level UniCmd dispatch stub.<br>Checks chip ID (0x6639/0x7927/0x7925/0x738/0x717) and UniCmd feature flag.<br>**EDX = outer tag, R8 = option byte (0xed=fire-and-forget, 0x07=query)**.<br>If UniCmd supported: recursively calls self with DL=0x14 then jumps to `0x14014e644`.<br>If legacy: jumps to `0x1400cd2a8`. |
| `0x14014e644` | **nicUniCmdDispatch** — UniCmd path, calls `0x14014f720` (tag→index lookup). |
| `0x14014f720` | **Tag→Index Converter** — searches dispatch table at `0x1402507e0`, returns table index. |
| `0x14014f788` | **nicUniCmdBufAlloc** — allocates UniCmd buffer for a given inner CID and size.<br>Called as `nicUniCmdBufAlloc(adapter, inner_CID, size)` (DL=inner_CID, R8=payload_size). |
| `0x1400cd2a8` | Legacy (non-UniCmd) command path — not used on MT6639. |

### Dispatch Table Location

```
VA:   0x1402507e0
Size: 58 entries × 13 bytes = 754 bytes
```

**Entry format (13 bytes)**:
```
[+0x00] u16 outer_tag     — EDX value passed to nicUniCmdAllocEntry
[+0x02] u16 inner_CID     — CID written into UniCmd inner header
[+0x04] u8  option_filter — 0x00 = no filter; nonzero = match only if option==this
[+0x05] u64 handler_va    — firmware-facing handler function pointer
```

**Special outer_tag values**:
- `0xed` = multi-match entries, discriminated by `option_filter` (byte at +4)
- `0xa5` = wildcard inner_CID (legacy CID reuse)

---

## 2. Complete Dispatch Table (all 58 entries)

```
[Idx]  outer_tag  inner_CID  option  handler_va          Command
----------------------------------------------------------------------
[ 0]   0x8a       0x0e       0x00    0x0140144b20        NIC_CAP
[ 1]   0x02       0x0b       0x00    0x0140144be0        CLASS_02 (PostInit cmd)
[ 2]   0xc0       0x0d       0x00    0x0140145670        WM_UNI_CMD (generic event/notify)
[ 3]   0x28       0x28       0x00    0x0140143db0        DBDC
[ 4]   0xca       0x0e       0x00    0x0140143a70        NIC_CAP_V2 / extension
[ 5]   0xc5       0x0b       0x00    0x0140144890        ? (same inner as CLASS_02)
[ 6]   0x0f       0x15       0x00    0x0140143fc0        MU_MU_SET
[ 7]   0x5d       0x2c       0x00    0x0140146850        HIF_CTRL (suspend/resume only)
[ 8]   0x70       0x0e       0x00    0x0140143950        NIC_CAP extension
[ 9]   0x11       0x01       0x00    0x0140143540        DEV_INFO ✓
[10]   0x03       0x16       0x00    0x0140143020        SCAN_REQ ✓
[11]   0x1b       0x16       0x00    0x00000001401431d0  SCAN_CANCEL
[12]   0x1c       0x27       0x00    0x0140144820        CH_PRIVILEGE (ROC)
[13]   0x05       0x02       0x00    0x0140144db0        BSS_INFO_BASIC (outer wrapper)
[14]   0x17       0x02       0x00    0x00000001401442d0  BSS_INFO_SEC (Security)
[15]   0x12       0x02       0x00    0x00000001401444a0  BSS_INFO_RATE
[16]   0x16       0x02       0x00    0x00000001401443b0  BSS_INFO_RLM
[17]   0x19       0x02       0x00    0x00000001401445e0  BSS_INFO_IFS_TIME
[18]   0x18       0x02       0x00    0x0000000140144110  BSS_INFO_QBSS_LOAD
[19]   0x1e       0x02       0x00    0x0140144e80        BSS_INFO_MLD
[20]   0x15       0xa5       0x00    0x0140142fc0        PM_DISABLE (legacy wildcard)
[21]   0x14       0x03       0x00    0x0140143fd0        UniCmd_WRAPPER (outer container)
[22]   0x13       0x03       0x00    0x00000001401446d0  TLV_INFO (chip cfg?)
[23]   0x07       0x03       0x00    0x0140145d30        SET_DOMAIN (regulatory)
[24]   0x08       0x03       0x00    0x0140145f70        WFDMA_CFG / HIF_CTRL_EXT
[25]   0xfd       0x03       0x00    0x0140146060        ?
[26]   0x1d       0x04       0x00    0x0140145550        ?
[27]   0x0a       0x08       0x00    0x0140143cd0        BSS_INFO_HE (or ext type 8)
[28]   0x81       0x23       0x00    0x0140146790        ?
[29]   0xce       0x22       0x00    0x00000001401463e0  ?
[30]   0x85       0x23       0x00    0x0140146620        ?
[31]   0x61       0x16       0x00    0x00000001401432b0  SCAN (extension)
[32]   0x62       0x16       0x00    0x0000000140143390  SCAN (extension 2)
[33]   0xc4       0x0e       0x00    0x0000000140143bf0  NIC_CAP extension
[34]   0x58       0x05       0x00    0x0140144cd0        EFUSE_CTRL
[35]   0x04       0x0f       0x00    0x0000000140143ef0  ?
[36]   0xfc       0x24       0x00    0x00000001401459e0  ?
[37]   0x2a       0x2f       0x00    0x00000001401450d0 (was 0x5ae0) ?  
[38]   0x2b       0x38       0x00    0x0140145ba0        ?
[39]   0x79       0x27       0x00    0x0140145c60        CH_PRIVILEGE extension
[40]   0xb1       0x25       0xa8    0x00000001401458d0  STA_REC (option filter=0xa8)
[41]   0xb0       0x43       0x00    0x00000001401457c0  STA_REC_HW?
[42]   0xf6       0x07       0x00    0x00000001401461c0  ? (inner=0x07)
[43]   0xc1       0x0a       0x00    0x0140144970        ?
[44]   0xed       0x2d       0x21    0x0140146d60        TXPOWER (option=0x21)
[45]   0xed       0x14       0x94    0x0140144f80        DBDC ext (option=0x94)
[46]   0xed       0x33       0x1e    0x00000001401450d0  ? (option=0x1e)
[47]   0xed       0x13       0x81    0x0140145230        BAND_CONFIG (option=0x81)
[48]   0xed       0x1a       0x3c    0x0140145460        ? (option=0x3c)
[49]   0xed       0x25       0xa8    0x0140146ec0        STA_REC v2 (option=0xa8)
[50]   0xed       0x7b       0xbf    0x0000000140147010  ? (option=0xbf)
[51]   0xed       0xa4       0xc0    0x0000000140147160  ? (option=0xc0)
[52]   0x93       0x49       0x00    0x0140146950        BAND_CONFIG
[53]   0x4c       0x4a       0x00    0x0140146a50        BSS_INFO_PROTECT
[54]   0x8f       0x19       0x00    0x0140146290        ?
[55]   0x7e       0x44       0x00    0x0140147330        ?
[56]   0xed       0x2d       0x01    0x0140147410        TXPOWER (option=0x01)
[57]   0xed       0x2d       0x4f    0x0000000140147550  TXPOWER (option=0x4f)
```

---

## 3. Confirmed CID Mappings (Cross-Verified with CLAUDE.md)

| Command | outer_tag | inner_CID | Notes |
|---------|-----------|-----------|-------|
| **NIC_CAP** | `0x8a` | `0x0e` | ✓ matches CLAUDE.md |
| **DEV_INFO** | `0x11` | `0x01` | ✓ matches CLAUDE.md |
| **BSS_INFO** | `0x05` | `0x02` | ✓ matches CLAUDE.md |
| **STA_REC** | `0xb1` | `0x25` | ✓ matches CLAUDE.md |
| **SCAN_REQ** | `0x03` | `0x16` | ✓ matches CLAUDE.md |
| **CH_PRIVILEGE** (ROC) | `0x1c` | `0x27` | Verified |
| **DBDC** | `0x28` | `0x28` | outer=inner |
| **BAND_CONFIG** | `0x93` | `0x49` | New discovery |
| **BSS_INFO_PROTECT** | `0x4c` | `0x4a` | Separate inner CID! |
| **EFUSE_CTRL** | `0x58` | `0x05` | — |
| **SET_DOMAIN** (regulatory) | `0x07` | `0x03` | — |
| **WFDMA_CFG/HIF_CTRL_EXT** | `0x08` | `0x03` | Same inner CID as SET_DOMAIN |
| **HIF_CTRL** (power mgmt) | `0x5d` | `0x2c` | NOT used in probe |
| **MU_MU_SET** | `0x0f` | `0x15` | — |
| **BSS_INFO_HE** | `0x0a` | `0x08` | — |
| **SCAN_CANCEL** | `0x1b` | `0x16` | Same inner as SCAN_REQ |
| **CLASS_02 cmd** | `0x02` | `0x0b` | PostFwDownloadInit step3 |

---

## 4. BSS_INFO Sub-TLV Outer Tags (all map to inner_CID=0x02)

All of these use the same inner CID `0x02`, distinguished only by the outer TLV tag:

| outer_tag | Sub-TLV Name | Handler VA | Payload size |
|-----------|-------------|------------|--------------|
| `0x05` | BSS_INFO_BASIC | `0x140144db0` | 0x0c (12 bytes alloc, checks size=4) |
| `0x12` | BSS_INFO_RATE | `0x1401444a0` | variable (checks size=0x74) |
| `0x16` | BSS_INFO_RLM | `0x1401443b0` | — |
| `0x17` | BSS_INFO_SEC | `0x1401442d0` | — |
| `0x18` | BSS_INFO_QBSS_LOAD | `0x140144110` | — |
| `0x19` | BSS_INFO_IFS_TIME | `0x1401445e0` | 0x30 (48 bytes, checks size=0x16) |
| `0x1e` | BSS_INFO_MLD | `0x140144e80` | 0x14 (checks size=4) |
| `0x4c` | BSS_INFO_PROTECT | `0x140146a50` | — (inner=**0x4a**, NOT 0x02!) |
| `0x0a` | BSS_INFO_HE | `0x140143cd0` | — (inner=**0x08**, NOT 0x02!) |

**Critical**: BSS_INFO_PROTECT (`0x4c`) uses inner CID `0x4a`, not `0x02`. BSS_INFO_HE (`0x0a`) uses inner CID `0x08`.

---

## 5. BSS_INFO_BASIC Payload Structure

From handler at `0x140144db0`, payload (at `rax+0x18` after alloc, 12 bytes allocated):
```
TLV value payload:
[+0x00]  u8  bss_idx       = input[0]
[+0x01..3] padding
[+0x04]  u32 packed_flags  = 0x00080015 (hardcoded)
           byte+4 = 0x15 (likely: bss_type=infra(2)+conn_type flags?)
           byte+6 = 0x08 (band indicator?)
[+0x08]  u8  hw_bss_idx    = input[1]
```

Note: `0x00080015` packed field: `0x15 = 0b00010101`, `0x08` could be band bits.

---

## 6. BSS_INFO_MLD Payload Structure

From handler at `0x140144e80` (immediately follows BASIC in memory), 0x14 bytes allocated:
```
TLV value payload:
[+0x00]  u8  bss_idx          = input[0]
[+0x04]  u32 packed1          = 0x00080013
[+0x08]  u8  mld_addr_hi      = input[1] >> 4
[+0x09]  u8  mld_addr_lo      = input[1] & 0x0f
[+0x0c]  u32 packed2          = 0x00080014
[+0x10]  u8  field_from_in2   = input[2]
[+0x11]  u8  field_from_in3   = input[3]
```

---

## 7. BSS_INFO_IFS_TIME Payload Structure

From handler at `0x1401445e0`, checks size=0x16 (22 bytes), allocates 0x30 (48 bytes):
```
TLV value payload (0x30 bytes):
[+0x00]  u8  bss_idx / band
Copies from input struct starting at +0, offset by 4 words.
```

---

## 8. STA_REC_BASIC Payload Structure

From handler at `0x1401458d0`, checks size=0x3c, allocates 0x44 (68 bytes), inner CID=0x25:
```
TLV value payload (0x44 bytes):
[+0x00]  u8  = 0xff (WCID = invalid/broadcast)
[+0x01..3] reserved
[+0x04]  u32 packed = 0x00400087
           byte+4 = 0x87 = ?
           byte+6 = 0x40 = ?
[+0x08]  u8  from_input[0]   (sta_type/conn_type?)
[+0x0a]  u16 from_input[2]   (WCID?)
[+0x0c]  u8  from_input[4]   (addr[0])
[+0x0d]  u8  from_input[5]   (addr[1])
[+0x0e]  u8  from_input[6]   (addr[2])
[+0x0f]  u8  from_input[7]   (addr[3])
[+0x10]  u8  from_input[8]   (addr[4])
[+0x14]  u32 from_input[0xc]
[+0x18]  u32 from_input[0x10]
[+0x1c]  u32 from_input[0x14]
[+0x20]  u32 from_input[0x18]
```

`0x400087` packed: bit7=1 (some flag), bits2:0=7, bit30=0, bits22:16=4. This is likely sta_type/conn_type bitmask from the Windows driver's STA_REC_BASIC_T structure.

---

## 9. CH_PRIVILEGE (ROC) TLV Structure

From handler at `0x140144820` + acquire function `0x14014fe60`:

**Acquire TLV** (inner CID=0x27, 0x10 bytes allocated):
```
[+0x04]  u32 = 0x000c0001
           byte+4 = 0x01 (acquire action)
           byte+6 = 0x0c (?)
[+0x08]  u8  token_id     = rdi[0]
[+0x09]  u8  channel      = rdi[1]
[+0x0a]  u8  band_mapped:
           input[0xd]==4 → 0xff (6GHz?)
           input[0xd]==3 → 0xfe (5GHz?)
           else → input[0xd] as-is
```

**Release TLV** (inner CID=0x27, variable size):
```
Size = 0x1c + (input[0x1c] * 3 * 8)   [from: lea r8d,[rbx+rbx*2]; lea r8d,[r8*8+0x1c]]
[+0x00..] = per-channel slots
```

---

## 10. Inner CID Reference Table

| inner_CID | Command | Notes |
|-----------|---------|-------|
| `0x01` | DEV_INFO | outer=0x11 |
| `0x02` | BSS_INFO | outer=0x05/0x12/0x16/0x17/0x18/0x19/0x1e |
| `0x03` | UniCmd_WRAPPER / SET_DOMAIN / WFDMA_CFG | outer=0x14/0x07/0x08 |
| `0x04` | unknown | outer=0x1d |
| `0x05` | EFUSE_CTRL | outer=0x58 |
| `0x07` | unknown | outer=0xf6 |
| `0x08` | BSS_INFO_HE or ext | outer=0x0a |
| `0x0b` | CLASS_02 / PostInit cmd | outer=0x02/0xc5 |
| `0x0d` | WM_UNI_CMD | outer=0xc0 |
| `0x0e` | NIC_CAP | outer=0x8a/0xca/0x70/0xc4 |
| `0x0f` | unknown | outer=0x04 |
| `0x13` | BAND_CONFIG (also 0xed/0x81) | — |
| `0x14` | DBDC ext | outer=0xed (option=0x94) |
| `0x15` | MU_MU_SET | outer=0x0f |
| `0x16` | SCAN_REQ / SCAN_CANCEL | outer=0x03/0x1b/0x61/0x62 |
| `0x19` | unknown | outer=0x8f |
| `0x1a` | unknown | outer=0xed (option=0x3c) |
| `0x22` | unknown | outer=0xce |
| `0x23` | unknown | outer=0x81/0x85 |
| `0x24` | unknown | outer=0xfc |
| `0x25` | STA_REC | outer=0xb1 (option=0xa8) |
| `0x27` | CH_PRIVILEGE (ROC) | outer=0x1c/0x79 |
| `0x28` | DBDC | outer=0x28 |
| `0x2c` | HIF_CTRL (power) | outer=0x5d |
| `0x2d` | TXPOWER | outer=0xed (options=0x01/0x21/0x4f) |
| `0x2f` | unknown | outer=0x2a |
| `0x33` | unknown | outer=0xed (option=0x1e) |
| `0x38` | unknown | outer=0x2b |
| `0x43` | STA_REC_HW? | outer=0xb0 |
| `0x44` | unknown | outer=0x7e |
| `0x49` | BAND_CONFIG | outer=0x93 |
| `0x4a` | BSS_INFO_PROTECT | outer=0x4c |
| `0x7b` | unknown | outer=0xed (option=0xbf) |
| `0xa4` | unknown | outer=0xed (option=0xc0) |
| `0xa5` | wildcard (legacy) | outer=0x15 |

---

## 11. PostFwDownloadInit CLASS_02 Command

The mysterious `class=0x02` command from PostFwDownloadInit corresponds to:
- **outer_tag = 0x02** → **inner_CID = 0x0b**
- Handler at `0x140144be0`
- Handler checks: `byte[rdx]==0x02`, size==0x0c (12 bytes)
- Allocates 0x10 bytes with DL=0x0b

This is distinct from BSS_INFO (outer=0x05, inner=0x02). The outer_tag=0x02 is a standalone command, NOT a BSS_INFO sub-TLV.

**From Windows PostFwDownloadInit**: payload = `{0x01, 0x00, 0x70000}` in 12 bytes.

---

## 12. Implications for Driver

### Current Issues

1. **BSS_INFO CID in header**: Correct. inner_CID=0x02 goes in the UniCmd header for all BSS_INFO sub-TLVs (except PROTECT=0x4a and HE=0x08).

2. **BSS_INFO_PROTECT**: Currently sent with outer=0x4c → handler uses inner=0x4a (NOT 0x02). Our driver currently sends this with the wrong inner CID — need to verify.

3. **CLASS_02 (outer=0x02, inner=0x0b)**: We skip this command in PostFwDownloadInit. It is a valid UniCmd command. The payload `{1, 0, 0x70000}` = 12 bytes should work with the handler.

4. **STA_REC option=0xa8**: The dispatch table entry [40] has option_filter=0xa8. This means the dispatcher also matches option=0xa8 (not just 0x00). Our current option=0x06 (set) would NOT match entry [40] if the option filter is active. However, looking at the search logic in `0x14014f720`: option filter is only enforced when outer_tag=0xed. For outer_tag=0xb1, option_filter=0xa8 appears to be stored as metadata (R8=0xa8 passed when calling the dispatch function), not a filter criterion. **Verify**: ensure we pass R8=0xa8 for STA_REC updates.

5. **NIC_CAP has 4 outer tags** (0x8a, 0xca, 0x70, 0xc4) all mapping to inner=0x0e. Different capabilities query types.

---

## 13. Binary Evidence Summary

```
VA of dispatch table:     0x1402507e0
Entry stride:             13 bytes (0xd)
Table search (0x14014f720):
  lea rdx, [rip + 0x101097]   ; at 0x14014f742 → table = 0x14014f749+0x101097 = 0x1402507e0
  cmp ecx, 0x3a               ; 0x3a = 58 entries max
```

All 58 entries were read and decoded at analysis time. The outer_tag/inner_CID pairs were cross-verified against:
- CLAUDE.md known values (NIC_CAP, DEV_INFO, BSS_INFO, STA_REC, SCAN_REQ — all confirmed)
- Handler function prologues (each handler verifies `cmp byte ptr [rdx], outer_tag` and checks expected payload size before calling `nicUniCmdBufAlloc` with the inner CID in DL)

