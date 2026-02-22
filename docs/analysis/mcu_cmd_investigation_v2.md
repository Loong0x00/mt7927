# MCU CMD Investigation v2 — Post-FW GAP Analysis

## Context
Team-lead corrected initial hypothesis: DMASHDL at 0xd6060 IS already enabled (0x10101) in Mode 40
before NIC_CAP, but MCU_CMD=0x8000 and no DMA response. Investigation focuses on the GAP between
FW boot and first MCU command.

## KEY FINDING #1: HAL_ENABLE_FWDL is a NO-OP for MT6639

In the vendor Android driver (`mt6639/chips/mt6639/mt6639.c:387`):
```c
.asicEnableFWDownload = NULL,
```

The `HAL_ENABLE_FWDL()` macro calls `halEnableFWDownload()` which delegates to
`prChipInfo->asicEnableFWDownload`. Since this is NULL for MT6639, it does **NOTHING**.

**Implication**: The vendor MT6639 driver NEVER sets `FW_DWLD_BYPASS_DMASHDL` (BIT(9) in GLO_CFG)
or any other FWDL-specific bit. But OUR driver DOES set it at line 2577 and never clears it!

## KEY FINDING #2: No Register Writes Between FW Boot and First MCU Command

**Vendor Android driver** (`mt6639/common/wlan_lib.c:1204-1272`):
```
wlanDownloadFW()              → FW download
  → HAL_ENABLE_FWDL(FALSE)   → NO-OP (asicEnableFWDownload=NULL)
wlanCheckWifiFunc()           → poll fw_sync (register reads only)
wlanQueryNicResourceInformation() → nicTxResetResource() (local, no MMIO)
wlanQueryNicCapabilityV2()    → FIRST MCU COMMAND
```

**Windows driver** (ghidra_post_fw_init.md line 507):
> "WFDMA enable (0xd6060 |= 0x10101) is the ONLY pre-MCU register write.
> No other register writes happen between FW boot and the first MCU command."

**Conclusion**: Both vendor Android and Windows drivers confirm the GAP is EMPTY (or just DMASHDL enable).
There is NO missing register write between FW boot and first MCU command.

## KEY FINDING #3: Mode 40 Has Residual FWDL State

After FW download completes (fw_sync=0x3), our Mode 40 code has these FWDL artifacts still active:

| Register | Bit | Set At | Purpose | Should Clear After FWDL? |
|----------|-----|--------|---------|--------------------------|
| GLO_CFG (0xd4208) | BIT(9) FW_DWLD_BYPASS_DMASHDL | line 2577 | Bypass DMA scheduler during FWDL | **YES** — vendor never sets it! |
| DMASHDL_SW_CONTROL (0xd6004) | BIT(28) DMASHDL_BYPASS | line 2592 | Bypass DMASHDL quota | **Maybe** — upstream mt7925 leaves it set, but it works for mt7925 |

**The vendor MT6639 NEVER sets either of these.** Our code sets them for FWDL compatibility with
upstream mt7925 flow, but MT6639/MT7927 may behave differently with bypass active.

## KEY FINDING #4: GLO_CFG_EXT1 BIT(28) Missing from Mode 40

Windows writes `0x7c0242b4 |= 0x10000000` (GLO_CFG_EXT1 |= BIT(28) = `TX_FCTRL_MODE`)
during WpdmaConfig, BEFORE FW download. Our main DMA init function (`mt7927_dma_init`)
writes this at line 1898-1900, but Mode 40's inline DMA setup does NOT include it.

From mt7996: `WF_WFDMA0_GLO_CFG_EXT1_TX_FCTRL_MODE BIT(28)` — TX flow control mode.

**NOTE**: `MT_UWFDMA0_GLO_CFG_EXT1` (upstream 0x7c0242b4) = `MT_WPDMA_GLO_CFG_EXT1` (our 0xd42b4).
Same register, different macro names. Upstream mt7925 also writes BIT(28).

## KEY FINDING #5: Vendor Init Order Differs Fundamentally

**Vendor MT6639 Android (AXI):**
```
1. CLR_OWN                           ← driver ownership
2. nicInitializeAdapter              ← chip ID, MCR, HIF init
3. halHifSwInfoInit                  ← ring alloc, DMA enable, DMASHDL, NEED_REINIT
4. HAL_ENABLE_FWDL(TRUE)            ← NO-OP
5. WFSYS RESET (CB_INFRA_RGU)       ← assert + poll INIT_DONE + de-assert
6. ECO version
7. FW download
8. fw_sync poll
9. MCU commands (NIC_CAP)
```

**Our Mode 40:**
```
1. WFSYS RESET (CB_INFRA_RGU)       ← assert + de-assert
2. CLR_OWN                           ← wipes ALL HOST ring BASEs!
3. DMA reprogram                     ← restore ring BASEs, prefetch, GLO_CFG
4. FW download
5. DMASHDL enable (0xd6060 |= 0x10101)
6. NIC_CAP
```

**Critical difference**: In vendor flow, rings are set up BEFORE WFSYS reset. WFSYS reset only resets
WF subsystem, NOT WFDMA (which is in CONN_INFRA). So HOST ring BASEs survive the reset.
After reset, ROM boots, sees valid rings + NEED_REINIT, and configures MCU rings.

In our flow, CLR_OWN comes AFTER WFSYS reset and wipes everything. We reprogram rings but the
ROM has already processed NEED_REINIT during CLR_OWN (with rings=0).

## KEY FINDING #6: Vendor WFSYS Reset Includes INIT_DONE Polling

Vendor does (`wlan_lib.c:1178-1181`):
```c
mt6639HalCbInfraRguWfRst(prAdapter, TRUE);   // Assert reset
mt6639HalPollWfsysSwInitDone(prAdapter);      // Poll for state change
mt6639HalCbInfraRguWfRst(prAdapter, FALSE);   // De-assert reset
```

Our Mode 40 does:
```c
mt7927_wr(dev, CB_INFRA_RGU, val | BIT(4));  // Assert
usleep_range(100, 200);                        // Fixed 100us delay
mt7927_wr(dev, CB_INFRA_RGU, val & ~BIT(4)); // De-assert
// Poll ROMCODE_INDEX for MCU_IDLE
```

We skip `PollWfsysSwInitDone` and use a fixed 100us delay instead. This could cause a timing issue.

## RECOMMENDED EXPERIMENTS (Priority Order)

### Experiment A: Clear FWDL bypass bits after FW download
After FW download succeeds (before NIC_CAP):
```c
// Clear FW_DWLD_BYPASS_DMASHDL from GLO_CFG
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
val &= ~MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;  // clear BIT(9)
mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

// Clear DMASHDL_BYPASS
val = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
val &= ~MT_HIF_DMASHDL_BYPASS_EN;             // clear BIT(28)
mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, val);
```
**Rationale**: Vendor MT6639 never sets these. Having them active after FW boot may block DMA routing.

### Experiment B: Add GLO_CFG_EXT1 BIT(28) to Mode 40
```c
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
val |= 0x10000000;  // TX_FCTRL_MODE
mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, val);
```
**Rationale**: Both Windows and upstream mt7925 write this. Mode 40 skips it.

### Experiment C: Vendor-style init order (no CLR_OWN in Mode 40)
Skip CLR_OWN in Mode 40. Instead:
1. Use initial probe's CLR_OWN + ring setup (already done)
2. WFSYS reset via CB_INFRA_RGU (WFDMA/rings survive since it's in CONN_INFRA)
3. FW download directly (no ring reprogram needed)
4. DMASHDL enable
5. NIC_CAP

**Rationale**: Matches vendor flow. HOST rings survive WFSYS reset, ROM/FW sees valid rings.

### Experiment D: Poll WFSYS_SW_INIT_DONE during reset
Match vendor behavior:
```c
// Assert reset
mt7927_wr(dev, CB_INFRA_RGU, val | BIT(4));

// Poll for WFSYS state change (vendor: mt6639HalPollWfsysSwInitDone)
// May need to poll 0xe0010 (WFSYS_SW_INIT_DONE) for BIT(4) clear
for (i = 0; i < 100; i++) {
    if (!(mt7927_rr(dev, 0xe0010) & BIT(4)))
        break;
    usleep_range(1000, 2000);
}

// De-assert reset
mt7927_wr(dev, CB_INFRA_RGU, val & ~BIT(4));
```

## BIT(15) in MCU2HOST_SW_INT_STA Interpretation

BIT(15) = 0x8000 is within the valid MCU2HOST software interrupt mask (BITS(0,15) = 0xFFFF).
Vendor ISR at `cmm_asic_connac3x.c:772-790` would log "undefined SER status" for this bit.
It is NOT any standard error/control bit (ERROR_DETECT_MASK covers only bits 2-5).

**Possible interpretations**:
1. MCU "I tried to respond but DMA path is broken" notification
2. MCU command parsing error (invalid TXD format?)
3. MCU-specific status for MT6639 not documented in available headers

## Summary

The root cause is likely NOT a missing register write between FW boot and MCU commands.
Both vendor and Windows drivers have minimal/no writes in that gap.

The more likely root causes are:
1. **Residual FWDL state** (FW_DWLD_BYPASS_DMASHDL) that blocks normal MCU response routing
2. **Init order** (CLR_OWN wipes rings before they can be used by ROM/FW)
3. **Missing WFSYS_SW_INIT_DONE polling** during CB_INFRA_RGU reset
4. **FW boot state** — FW boots but doesn't complete MCU_RX0 configuration due to chip-specific behavior
