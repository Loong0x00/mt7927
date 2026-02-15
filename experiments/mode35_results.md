# Mode 35 Results: Post-boot HEAVY CLR_OWN

**Date:** 2026-02-15
**Hypothesis:** A HEAVY CLR_OWN (wakeup + sleep protection disable) post-boot will trigger the running FW to process NEED_REINIT and configure MCU_RX0/RX1. All previous post-boot CLR_OWN tests used BARE CLR_OWN (proven no-op in Mode 34).

## Configuration
- `reinit_mode=35`
- Pre-FWDL: standard mode 11 heavy CLR_OWN (so FWDL works normally)
- Post-boot (after fw_sync=0x3): HEAVY CLR_OWN with full wakeup + slpprot disable

## Result: HYPOTHESIS REJECTED

**MCU_RX0_BASE=0x00000000** — Running FW does NOT configure MCU_RX0.
**NIC_CAPABILITY: -110** — same failure.

### Key Observations

1. **DUMMY_CR=0xffff0003 before heavy CLR_OWN** — FW DID set NEED_REINIT (BIT(1))
2. **Heavy CLR_OWN succeeded** — completed in 10ms ✓
3. **DUMMY_CR=0xffff0003 AFTER heavy CLR_OWN** — NEED_REINIT **NOT consumed!**
4. **HOST rings SURVIVED** — TX15 and RX6 BASEs intact, GLO=0x5410ba75
5. **FW still alive** — fw_sync=0x3, MCU_CMD=0x00008000
6. **MCU_RX0-3 unchanged** — same state as before heavy CLR_OWN
7. **MCU_TX0-3 all BASE=0** — unchanged
8. **FSM=0x01010202** — post-boot state, not reset

### CRITICAL Discovery: Running FW Does NOT Process NEED_REINIT

| Observation | ROM heavy CLR_OWN (mode 11) | FW heavy CLR_OWN (mode 35) |
|---|---|---|
| NEED_REINIT consumed | **YES** (0xffff0002 → 0xffff0000) | **NO** (stays 0xffff0003) |
| HOST rings wiped | **YES** (full WFDMA reset) | **NO** (all survive) |
| HOST GLO_CFG reset | **YES** (→ 0x1010b870) | **NO** (stays 0x5410ba75) |
| MCU_RX2/RX3 configured | **YES** | **NO** (unchanged) |
| MCU_RX0 configured | No | **No** |

**NEED_REINIT processing is a ROM-ONLY behavior**, not a FW behavior.
- ROM's CLR_OWN handler: WFDMA reinit + ring config (boot sequence logic)
- FW's CLR_OWN handler: runtime power management wake-up only (no WFDMA reinit)

This applies to BOTH bare AND heavy CLR_OWN post-boot. The heavy vs bare distinction only matters for ROM, not for running FW.

### What This Means

The running FW simply does NOT have NEED_REINIT processing in its CLR_OWN wake handler. The FW set NEED_REINIT (BIT(1) in DUMMY_CR) during boot, but this appears to be a STATUS flag from FW perspective ("I need the host to complete reinit"), not an ACTION flag that FW itself processes.

### What's Definitively Ruled Out
- Post-boot heavy CLR_OWN as MCU_RX0 configuration trigger
- Post-boot bare CLR_OWN (Mode 34 already proved this)
- NEED_REINIT as an FW-processed flag (it's ROM-only)
- Running FW configuring MCU_RX0 via any CLR_OWN mechanism

### Remaining Question
What triggers MCU_RX0 configuration? It's not:
- ROM during CLR_OWN (ROM only does MCU_RX2/RX3)
- Running FW during CLR_OWN (doesn't process NEED_REINIT)
- Running FW during boot (MCU_RX0 stays 0 after fw_sync=0x3)

Must be something else entirely — possibly a specific MCU command or initialization sequence that the upstream mt7925 driver sends that we haven't identified yet.
