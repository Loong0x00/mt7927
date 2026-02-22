# TXP (TX Payload) Descriptor Format Research
**Date**: 2026-02-22
**Agent**: txp-verify
**Team**: tx-debug
**Source**: Windows RE from Ghidra analysis + existing driver code

---

## Executive Summary

**Status**: TXP format **appears correct** but **UNVERIFIED against Windows binary**.

Our current TXP implementation matches expected scatter-gather patterns, but Windows RE documentation **lacks detailed analysis of N6PciUpdateAppendTxD (FUN_14005d6d8)** — the function that builds the TXP structure for data frames.

**Current Evidence**:
- ✅ dmesg output shows sensible TXP values (token|VALID, payload DMA, length with LAST bit)
- ✅ Structure layout matches expected scatter-gather descriptor pattern
- ❌ No assembly-level verification of field encodings from Windows binary
- ❌ LAST bit format unknown (is it BIT(15) of len0, or something else?)

---

## 1. Current Driver Implementation

### struct mt7927_hw_txp (32 bytes)
```c
struct mt7927_hw_txp {
    __le16 msdu_id[4];           // [0x00..0x07]: 8 bytes
                                  // [0] = token | MT_MSDU_ID_VALID (BIT(15))
                                  // [1-3] = 0 (unused)

    struct mt7927_txp_ptr ptr[2]; // [0x08..0x1f]: 24 bytes (2 scatter-gather entries)
} __packed __aligned(4);

struct mt7927_txp_ptr {           // 12 bytes per entry
    __le32 buf0;                  // [+0x00] DMA address low 32 bits
    __le16 len0;                  // [+0x04] length | LAST bit (BIT(15)?)
    __le16 len1;                  // [+0x06] (unknown purpose — unused?)
    __le32 buf1;                  // [+0x08] DMA address (for 2nd buffer?)
} __packed __aligned(4);
```

**Total**: TXD (32) + TXP (32) = 64 bytes per frame = `MT7927_TXWI_SIZE`

### How TXP Is Currently Built (src/mt7927_mac.c)
```c
// Single scatter-gather entry for payload
txp->ptr[0].buf0 = cpu_to_le32(lower_32_bits(payload_dma));
txp->ptr[0].len0 = cpu_to_le16(skb->len | MT_TXP_LEN_LAST);
txp->ptr[0].len1 = 0;  // unused
txp->ptr[0].buf1 = 0;  // unused (only one payload buffer)
```

### dmesg Output Example
```
TXP: 00000000: 00008000 00000000 0b147000 0000801e
     ^^^^^^^^^^^^^^
     msdu_id[0,1] = 0x8000, 0x0000
                   ^^^^^^^^
                   token=0 | VALID=BIT(15)

                         ptr[0].buf0 = 0x0b147000 (payload DMA addr)
                                       ^^^^^^^^
                                       len0=0x801e, len1=0x0000
                                       ^^^^=len, ^^=LAST bit?
```

**Decoding**:
- `msdu_id[0]` = 0x8000 = token(0) | VALID(BIT(15)) ✅
- `msdu_id[1]` = 0x0000 ✅
- `ptr[0].buf0` = 0x0b147000 (physical DMA address) ✅
- `ptr[0].len0` = 0x801e = 30 bytes payload | LAST bit set
  - If LAST=BIT(15): len=0x001e=30, LAST=BIT(15) set ✅
  - Payload length matches frame size ✅

---

## 2. Windows RE Analysis

### 2.1 Documentation Structure (from docs/)

| Document | Content | TXP Coverage |
|----------|---------|-------------|
| `win_re_dma_descriptor_format.md` | N6PciTxSendPkt function analysis | ❌ None (discusses 16-byte DMA descriptors, not TXP) |
| `win_re_full_txd_dma_path.md` | Complete TX path (TXD + DMA submission) | ⚠️ Mentions N6PciUpdateAppendTxD but no decompilation |
| `win_re_ring2_analysis.md` | Management frame Ring 2 path | ❌ None (mgmt uses SF mode, no TXP) |

### 2.2 Key Findings About CT vs SF Mode

**Management Frames (Ring 2, SF mode):**
```c
// From win_re_full_txd_dma_path.md
└─ FUN_140053a0c (NdisCommonHifPciMlmeHardTransmit)
   ├─ Build TX info struct (113 bytes)
   ├─ Copy frame to buffer at offset 0x20 (after TXD)
   │  buf[0x00..0x1F] = TXD (32 bytes)
   │  buf[0x20..end]  = 802.11 frame (inline)
   └─ FUN_14005d1a4(HIF, buf_node, 2)  // Ring 2, param_3=2
      └─ NO call to FUN_14005d6d8 (N6PciUpdateAppendTxD)
         Management frames: TXD + frame **inline**, NO TXP!
```

**Data Frames (Ring 0/1, CT mode):**
```c
// From win_re_full_txd_dma_path.md
└─ FUN_140053a0c (similar path)
   ├─ Build TX info struct
   ├─ TXP scatter-gather setup (at buf+0x20)
   ├─ FUN_14005d6d8(adapter, buf)  // N6PciUpdateAppendTxD
   │  └─ **FUNCTION NOT DECOMPILED IN DOCS**
   └─ FUN_14005d1a4(HIF, buf_node, <2)  // Ring 0/1, param_3 < 2
      └─ DMA submission (16-byte descriptor)
```

**Critical Documentation Gap**:
```
Section 7: "FUN_14005d6d8 (N6PciUpdateAppendTxD) — 仅数据帧使用"
           (only used for data frames)

⚠️  No actual decompilation or assembly analysis provided!
```

### 2.3 DMA Descriptor Format (16 bytes, NOT TXP)

```c
// Written by N6PciTxSendPkt (0x14005d1a4)
struct wfdma_tx_descriptor {
    __le32 buf0;      // [0x00] Physical address low 32 bits
    __le32 ctrl;      // [0x04] Length[29:16] | OWN(BIT30) | flags
    __le32 buf_ext;   // [0x08] Always 0
    __le32 buf_addr_h;// [0x0c] Physical address high 16 bits
};
```

This is the **DMA ring descriptor** (written to the ring), NOT the TXP payload header!

---

## 3. Critical Questions (Unanswered)

### 3.1 TXP Structure Verification

**Q1**: Does Windows actually use a 32-byte TXP structure?
- Current code: Yes, 8 bytes msdu_id[4] + 24 bytes ptr[2]
- Windows RE: **NOT VERIFIED** (no N6PciUpdateAppendTxD decompilation)

**Q2**: What is the actual field layout inside N6PciUpdateAppendTxD?
- Current assumptions:
  - ptr[0].buf0 = payload DMA addr
  - ptr[0].len0 = payload length | LAST bit
  - ptr[1] = unused (single buffer case)
- Windows RE: **UNKNOWN** (function never analyzed)

**Q3**: How many scatter-gather entries per frame?
- Current code: Up to 2 (ptr[0] and ptr[1])
- Windows RE: **UNKNOWN**

### 3.2 Field Encoding Details

**Q4**: LAST bit format?
- Current code: `len0 |= MT_TXP_LEN_LAST` = `len0 |= BIT(15)`
- Windows RE: **UNVERIFIED**
  - Could be BIT(15) ✓ likely
  - Could be BIT(14) or other bit
  - Could be different encoding altogether

**Q5**: len1 field purpose?
- Current code: Always 0
- Windows RE: **UNKNOWN**
- Possibilities:
  - Padding/alignment
  - Scatter-gather entry count
  - Control flags
  - Unused (then why include in struct?)

**Q6**: buf1 field purpose?
- Current code: Always 0 (only single payload buffer used)
- Windows RE: **UNKNOWN**
- Possibilities:
  - 2nd payload buffer for AMSDU
  - Control/status field
  - Unused in single-frame case

---

## 4. Comparison with MT76 Reference Code

From `src/mt7927_pci.h`:
```c
/* Likely imported from mt76/mt76_connac.h or similar */
struct mt7927_hw_txp {
    __le16 msdu_id[4];
    struct mt7927_txp_ptr ptr[MT7927_TXP_MAX_BUF_NUM / 2];
};
```

**Analysis**:
- ✅ MT76 upstream also uses scatter-gather for CONNAC chips
- ✅ Similar structure (msdu_id array + ptr array)
- ❌ **But**: MT76 is for MT7925 (different chip!), not MT6639/MT7927
- ❌ User instruction (CLAUDE.md): "mt76 is NOT trusted for firmware logic"

---

## 5. What We Know For Certain (Windows RE Verified)

✅ **Confirmed from Windows binary**:
1. Data frames use scatter-gather (CT mode) on Ring 0/1
2. Function `N6PciUpdateAppendTxD` exists at 0x14005d6d8
3. This function is called after XmitWriteTxDv1 (TXD construction)
4. Management frames DON'T use TXP (SF mode on Ring 2)
5. DMA descriptor format matches our understanding (16 bytes)

❌ **NOT verified from Windows binary**:
1. Exact TXP field layout and encodings
2. How many scatter-gather entries per frame
3. LAST bit position and encoding
4. Purpose of len1 and buf1 fields

---

## 6. Current dmesg Sanity Check

Given dmesg output:
```
TXP: 00000000: 00008000 00000000 0b147000 0000801e
```

**Interpretations**:
| Field | Value | Assessment |
|-------|-------|-----------|
| msdu_id[0] | 0x8000 | token=0, VALID=BIT(15) set ✅ makes sense |
| msdu_id[1] | 0x0000 | unused ✅ |
| ptr[0].buf0 | 0x0b147000 | Physical DMA address ✅ reasonable |
| ptr[0].len0 | 0x801e | len=30 (0x1e) bytes, BIT(15) set ✅ payload size matches |
| ptr[0].len1 | 0x0000 | unused ✅ |
| ptr[0].buf1 | 0x??? | (would be next dword, not shown) |

**Conclusion**: Current values look **sensible and consistent** but cannot confirm **correctness** without Windows binary verification.

---

## 7. Methodology Limitations

### What Can't Be Verified Without Deeper RE:
1. **Function F UN_14005d6d8 assembly code** — need Ghidra decompilation
   - Shows exactly how TXP fields are constructed
   - Confirms field offsets, sizes, and encodings
   - Would answer Questions 1-6 above

2. **Usage examples in Windows** — need trace of real TX frames
   - Shows actual dmesg-equivalent output from Windows
   - Confirms field values in real scenarios

3. **Hardware TXP format spec** — need MediaTek firmware/PHY docs
   - Would explain why certain bits are used
   - Could clarify len1/buf1 purposes

---

## 8. Recommendations

### For Immediate Use
1. **Current TXP implementation**: ✅ ACCEPTABLE
   - Structure layout is reasonable
   - Output values are sensible
   - Likely correct, but unverified

2. **If TX is still failing**:
   - Problem is likely NOT TXP format (structure output looks right)
   - Look instead at:
     - TXD field values (DW0-DW7 encoding)
     - Ring selection and configuration
     - DMA descriptor format (16-byte to ring)
     - Firmware/MCU command setup

### For Future Verification
1. **Ghidra RE of FUN_14005d6d8**:
   - Extract from mtkwecx.sys v5705275
   - Import to existing Ghidra project at `tmp/ghidra_project/mt7927_re`
   - Document assembly-level field assignments

2. **Cross-check with MT6639 Android driver**:
   - If available, compare TXP usage in similar context
   - But remember: Android may differ from Windows

3. **Test with known-good firmware**:
   - If we can get TX working at all, inspect actual TXP values
   - Confirm against this research

---

## 9. References

| Document | Purpose | Coverage |
|----------|---------|----------|
| `docs/win_re_dma_descriptor_format.md` | N6PciTxSendPkt analysis | DMA ring descriptors only |
| `docs/win_re_full_txd_dma_path.md` | Full TX path | mentions N6PciUpdateAppendTxD but no decompilation |
| `docs/win_re_ring2_analysis.md` | Ring 2 mgmt frames | SF mode (no TXP) analysis |
| `src/mt7927_pci.h` | Current driver structs | TXP definitions |
| `src/mt7927_mac.c` | TXP construction | `txp->ptr[0].buf0/len0` assignment |

---

## 10. Conclusion

**TXP Format Status**: 🟡 **LIKELY CORRECT, UNVERIFIED**

- Current implementation: ✅ Sensible structure and output
- Windows RE docs: ⚠️ Incomplete (missing N6PciUpdateAppendTxD analysis)
- Recommendation: ✅ Can use current format with caution
- If TX fails: 🔴 Problem is probably elsewhere (not TXP format)

**Key Blockers for Full Verification**:
1. No Ghidra decompilation of FUN_14005d6d8 in existing docs
2. No assembly-level field assignment verification
3. No access to firmware/hardware spec

**Next Steps**:
- Priority should be other TX issues (TXD encoding, ring config, MCU commands)
- Only revisit TXP if other fixes don't resolve TX problems
