# Windows RE: PLE/PSE Register Access and DMASHDL Init Analysis

**Date**: 2026-02-21  
**Binary**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (2,546,112 bytes)  
**Analysis Method**: Python + Capstone disassembly  

---

## Executive Summary

**Critical Finding**: Windows does NOT initialize PLE/PSE registers during PostFwDownloadInit. PLE/PSE (0x820c0xxx/0x820c8xxx) are accessed **read-only in diagnostic/debug dump functions only**. There is no PLE/PSE init sequence to replicate.

**DMASHDL finding confirmed**: Windows PostFwDownloadInit does only one DMASHDL operation:
```c
readl(MT_HIF_DMASHDL_QUEUE_MAP0);   // BAR0 + 0xd6060
writel(val | 0x10101, MT_HIF_DMASHDL_QUEUE_MAP0);
```

---

## Section 1: Windows Register Address Space Mapping

### Address Space Translation

Windows driver uses `0x7c0XXXXX` as abstract register addresses. The relationship to Linux BAR0 offsets is:

```
Linux_BAR0_offset = (Windows_addr - 0x7c000000) + 0xb0000
Windows_addr = 0x7c000000 + (Linux_BAR0_offset - 0xb0000)
```

**Verified mapping** (all confirmed present in binary):

| Register Name | Linux BAR0 | Windows Address | Binary Hits |
|---|---|---|---|
| MT_WFDMA0_BASE | 0xd4000 | 0x7c024000 | 8 refs |
| MT_WFDMA0_GLO_CFG | 0xd4208 | 0x7c024208 | 16 refs |
| MT_WFDMA0_TX_RING0_BASE | 0xd4300 | 0x7c024300 | 4 refs |
| MT_WFDMA0_TX_RING2_BASE | 0xd4320 | 0x7c024320 | 2 refs |
| MT_WFDMA0_RX_RING4_BASE | 0xd4500 | 0x7c024500 | 3 refs |
| MT_WFDMA0_RX_RING6_BASE | 0xd4560 | 0x7c024560 | 1 ref |
| MT_WFDMA0_HOST_INT_ENA | 0xd4200 | 0x7c024200 | 6 refs |
| MT_WFDMA0_HOST_INT_STA | 0xd4204 | 0x7c024204 | 5 refs |
| MT_HIF_DMASHDL_QUEUE_MAP0 | 0xd6060 | 0x7c026060 | 6 refs |
| MT_HIF_DMASHDL_QUEUE_MAP1 | 0xd6064 | 0x7c026064 | 2 refs |

### Register Access Mechanism

The Windows driver uses an **indirect register access** layer:

1. **For small addresses (0-0x17ffffff) and 0x7c0xxxxx range**:
   - Add `0x9c000000` (32-bit wraparound): `0x7c026060 + 0x9c000000 = 0x18026060`
   - Calls `FUN_140052cdc` (window-based access)
   - This uses a remap window struct with: `[control_reg, mask, shift, data_reg]`

2. **For 0x820c0xxx/0x820c8xxx (PLE/PSE)**:
   - Calls `FUN_140052f94` (L1 remap path)
   - Uses hardware L1 remap window to access bus addresses > BAR0 size

3. **Core read API**: `FUN_1400099ac(obj, reg_addr, &output)` → calls `FUN_140054ee4`
4. **Core write API**: `FUN_140009a18(obj, reg_addr, value)` → calls `FUN_140057d48`

---

## Section 2: DMASHDL Initialization

### Function Location

**Function VA**: `0x1401d7738`  
**Size**: ~64 bytes  
**Callers**:
- `0x1401cf5ee` (primary PostFwDownloadInit path)
- `0x1401d4eab` (secondary init path)

### Exact Assembly (Confirmed Binary)

```asm
; FUN_1401d7738: DMASHDL_init
0x1401d7738:  push  rbx
0x1401d773a:  sub   rsp, 0x20
0x1401d773e:  and   [rsp+0x38], 0        ; clear output buffer
0x1401d7743:  lea   r8, [rsp+0x38]       ; output ptr
0x1401d7748:  mov   edx, 0x7c026060      ; MT_HIF_DMASHDL_QUEUE_MAP0
0x1401d774d:  mov   rbx, rcx             ; save obj
0x1401d7750:  call  FUN_1400099ac        ; read register → [rsp+0x38]
0x1401d7755:  mov   r8d, [rsp+0x38]      ; load read value
0x1401d775a:  mov   edx, 0x7c026060      ; MT_HIF_DMASHDL_QUEUE_MAP0
0x1401d775f:  or    r8d, 0x10101         ; set bits 0, 8, 16
0x1401d7766:  mov   rcx, rbx
0x1401d776e:  call  FUN_140009a18        ; write back
0x1401d7773:  add   rsp, 0x20
0x1401d7777:  pop   rbx
0x1401d7778:  ret
```

### Linux Equivalent

```c
/* EXACT Windows DMASHDL init - only these 2 operations */
static void mt7927_dmashdl_minimal_init(struct mt7927_dev *dev)
{
    u32 val;
    
    /* Windows ONLY does this single OR operation on MAP0 */
    /* Address: BAR0 + 0xd6060 = MT_HIF_DMASHDL_QUEUE_MAP0 */
    val = readl(dev->base + 0xd6060);
    writel(val | 0x10101, dev->base + 0xd6060);
    
    /* That's it. No other DMASHDL writes during PostFwDownloadInit. */
}
```

**IMPORTANT**: Our current driver does a full DMASHDL reconfiguration (15+ register writes copied from MT6639 Android). This is WRONG. Windows only does the single OR operation above.

### What 0x10101 Sets

`0x10101 = BIT(0) | BIT(8) | BIT(16)`

In MT_HIF_DMASHDL_QUEUE_MAP0 format, these bits likely enable/configure queues 0, 1, and 2 in their respective nibble fields. The exact semantics require further register definition research, but Windows only sets these 3 bits without disturbing other firmware-configured values.

---

## Section 3: PLE/PSE Register Access Analysis

### Addresses Found in Binary

**PLE registers accessed** (via L1 remap, read-only):
| Register | Address | Binary Hits |
|---|---|---|
| PLE_CTRL | 0x820c0000 | 2 (data only) |
| PLE_FREEPG_CNT | 0x820c0100 | 2 |
| PLE_FREEPG_HEAD_TAIL | 0x820c0104 | 1 |
| PLE_QUEUE_EMPTY | 0x820c0360 | 2 (code) + 2 (data) |
| PLE_txq (various) | 0x820c0304, 0x820c0308, etc. | many |

**PSE registers accessed** (via L1 remap, read-only):
| Register | Address | Binary Hits |
|---|---|---|
| PSE_CTRL | 0x820c8000 | 2 (data only) |
| PSE_FREEPG_CNT | 0x820c8100 | multiple |
| PSE_QUEUE_EMPTY | 0x820c80b0 | 2 (code) + 2 (data) |
| PSE_QUEUE_EMPTY_2 | 0x820c80bc | accessed |

**Other MAC addresses**:
| Register | Address | Binary Hits |
|---|---|---|
| HIF_SYS_BASE area | 0x820f0000 | 891 hits (internal pattern matching) |
| MAC_AX_TOP | 0x820e0000 | 2 (data only) |

### PLE/PSE Access Pattern

All PLE/PSE code accesses follow this pattern:

```asm
; Read PLE_QUEUE_EMPTY (0x820c0360)
lea   r8, [rbp + offset]          ; output buffer
mov   edx, 0x820c0360             ; register address
mov   rcx, rbx                     ; object ptr  
call  FUN_1400099ac               ; read register
```

**Calling convention**: `read_reg(obj, reg_addr_32bit, *output_u32)`

### PLE/PSE Are Diagnostic-Only

The two code functions containing PLE/PSE reads:

1. **FUN_1401e7290** (VA 0x1401e7290): Large diagnostic dump function
   - Reads: PLE_CTRL, PLE_FREEPG_CNT, PLE_FREEPG_HEAD_TAIL, PLE_QUEUE_EMPTY, PLE_txq registers, PLE pkt counters
   - Reads: PSE_CTRL, PSE_FREEPG_CNT, PSE_QUEUE_EMPTY, PSE queues
   - **Has 0 callers found in text section** → called indirectly (function pointer / error handler)

2. **FUN_1401f5f40** (VA 0x1401f5f40): Second diagnostic dump
   - Same pattern as above
   - **Has 0 callers found in text section** → error handler path

**Conclusion**: PLE/PSE reads occur ONLY in diagnostic/crash dump handlers, not during normal init or TX path. There is no PLE/PSE initialization code in Windows driver.

### What PLE/PSE Registers Contain

The diagnostic dump reads these specific registers in sequence:

**PLE reads** (function FUN_1401e7290):
```
0x820c0304, 0x820c0308, 0x820c0004,  // PLE queue status
0x820c0360,                           // PLE_QUEUE_EMPTY
0x820c0600, 0x820c0700, 0x820c0800, 0x820c0900,  // PLE TXQ
0x820c03a0, 0x820c03a4,              // PLE queue stats
0x820c000c,                           // PLE_CTRL bits
```

**PSE reads** (function FUN_1401e80xx):
```
0x820c8034, 0x820c8038,              // PSE queue info
0x820c8004,                           // PSE_CTRL
0x820c80b0,                           // PSE_QUEUE_EMPTY
0x820c80bc, 0x820c80b4,              // PSE queue empty ext
```

---

## Section 4: L1 Remap Mechanism

### How 0x820c Addresses Are Accessed

Windows uses function `FUN_140052f94` for 0x820c addresses. Key flow:

```c
// Simplified pseudo-code of FUN_140052f94
NTSTATUS remap_read(obj, reg_addr, *output) {
    remap_cfg = obj->vtable[0x146515f];  // get remap config struct
    
    if (!remap_cfg) {
        // Error: L1 remap not configured
        return ERROR;
    }
    
    // Lock
    mutex_lock(&obj->remap_lock);
    
    // Read current window value (to save/restore)
    old_win = read_reg(remap_cfg->window_ctrl_reg);
    
    // Compute window select bits from reg_addr
    // Mask address bits, shift, OR into window register
    new_win = compute_window(reg_addr, remap_cfg);
    write_reg(remap_cfg->window_ctrl_reg, new_win);
    
    // Read from window data register (BAR0 mapped window)  
    window_offset = reg_addr & remap_cfg->data_mask;
    *output = read_reg(remap_cfg->data_reg + window_offset);
    
    // Restore window
    write_reg(remap_cfg->window_ctrl_reg, old_win);
    
    mutex_unlock(&obj->remap_lock);
}
```

The remap config structs seen in `.data`:
```
VA=0x140254f30: [0x00100000, 0x00010000, 0x70020000, 0x001f0000]
VA=0x1402575c4: [0x001e0000, 0x00009000, 0x7c090000, 0x00150000]
```
Format: `[window_ctrl_reg, mask, ctrl_value_shift, data_base_reg]`

**Key insight**: For our Linux driver, we do NOT need to implement L1 remap for PLE/PSE access during TX/init because:
1. Windows doesn't read PLE/PSE during normal operation
2. Firmware manages PLE/PSE internally
3. PLE/PSE reads are only for diagnostics

---

## Section 5: PostFwDownloadInit Sequence Context

### DMASHDL Init Call Context (Caller 2, `0x1401d4eab`)

After the DMASHDL single OR write, the sequence continues with:

1. `call 0x1401d7738` — DMASHDL init (the OR operation)
2. `call 0x1401d5f08` — NIC_CAP query (reads 0x88000004 register, queries capabilities)
3. On success: continues with further init steps

The NIC_CAP register at 0x88000004 is accessed:
```asm
0x1401d605a:  mov   edx, 0x88000004    ; NIC_CAP register  
0x1401d605f:  mov   r8, rdi
0x1401d6062:  call  FUN_1400099ac      ; read caps
```

### Full Confirmed PostFwDownloadInit Step Order (from Session 17 docs + this analysis)

1. **DMASHDL init**: `readl(0xd6060) |= 0x10101` (the ONLY DMASHDL operation)
2. **NIC_CAP**: Query at 0x88000004
3. **UniCmd sequence**: Config/DBDC/ScanConfig etc.
4. **No PLE/PSE init** — firmware handles these internally

---

## Section 6: PLE/PSE Diagnostic Usage (Post-TX Debug)

Since PLE/PSE are read-only diagnostic, here is the Linux equivalent for debugging TX failures:

```c
/* DEBUG ONLY: Read PLE queue status after failed TX */
static void mt7927_debug_ple_pse(struct mt7927_dev *dev)
{
    u32 ple_empty, pse_empty;
    u32 ple_freepg, pse_freepg;
    
    /* PLE: Packet Loss Engine */
    ple_freepg = mt7927_l1_read(dev, 0x820c0100);  /* PLE_FREEPG_CNT */
    ple_empty  = mt7927_l1_read(dev, 0x820c0360);  /* PLE_QUEUE_EMPTY */
    
    /* PSE: Packet Store Engine */
    pse_freepg = mt7927_l1_read(dev, 0x820c8100);  /* PSE_FREEPG_CNT */
    pse_empty  = mt7927_l1_read(dev, 0x820c80b0);  /* PSE_QUEUE_EMPTY */
    
    dev_info(dev->dev,
        "PLE: freepg=%u, queue_empty=0x%08x\n"
        "PSE: freepg=%u, queue_empty=0x%08x\n",
        ple_freepg & 0xfff, ple_empty,
        pse_freepg & 0xfff, pse_empty);
}
```

Note: `mt7927_l1_read()` requires implementing the L1 remap window mechanism. For immediate debugging, these registers may be accessible via the MCU command interface or may show 0xFFFFFFFF if not L1-remapped.

---

## Section 7: Action Items for TX Fix

Based on this analysis:

### Priority 1: Fix DMASHDL Init (HIGH - confirmed root cause candidate)

**Current code** (WRONG — full reconfiguration from MT6639 Android):
```c
// REMOVE all of this:
writel(0x..., base + 0xd6000);  // DMASHDL_BASE
writel(0x..., base + 0xd6004);
// ... 15+ register writes
```

**Correct Windows behavior**:
```c
/* Replace entire DMASHDL init with just this: */
u32 val = readl(base + 0xd6060);   /* MT_HIF_DMASHDL_QUEUE_MAP0 */
writel(val | 0x10101, base + 0xd6060);
```

### Priority 2: Add PLE/PSE Diagnostic Reads After TX Failure

After submitting a TX frame and waiting for firmware response:
- Read `0x820c0360` (PLE_QUEUE_EMPTY) — if bits set for relevant queue, frame is stuck in PLE
- Read `0x820c80b0` (PSE_QUEUE_EMPTY) — if bits set, frame is stuck in PSE
- These require L1 remap window access

### Priority 3: No PLE/PSE Init Needed

Do NOT add PLE/PSE initialization code. Windows driver confirms firmware manages these registers. Adding init code may interfere with firmware operation.

---

## Appendix: Key Function Addresses (mtkwecx.sys AMD v5.7.0.5275)

| Function | VA | Description |
|---|---|---|
| `ReadRegister` | 0x1400099ac | Read hardware register into buffer |
| `WriteRegister` | 0x140009a18 | Write value to hardware register |
| `CoreReadReg` | 0x140054ee4 | Core read implementation |
| `CoreWriteReg` | 0x140057d48 | Core write implementation |
| `RemapRead_SmallAddr` | 0x140052cdc | Handles 0x7c0XXXXX addresses |
| `RemapRead_LargeAddr` | 0x140052f94 | Handles 0x820cXXXX addresses (L1 remap) |
| `DMASHDL_init` | 0x1401d7738 | DMASHDL MAP0 OR init |
| `PLE_dump_1` | 0x1401e7290 | PLE/PSE diagnostic dump (function 1) |
| `PLE_dump_2` | 0x1401f5f40 | PLE/PSE diagnostic dump (function 2) |

