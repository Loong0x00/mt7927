# Mode 34 Results: Full Upstream mt7925 Init Sequence

**Date:** 2026-02-15
**Two tests run:**
1. Test A: Bare second CLR_OWN (no WFSYS_RST in pre-FWDL)
2. Test B: WFSYS_RST → clean DMA re-init → bare CLR_OWN (skip all extra init steps)

---

## Test A: Bare Second CLR_OWN (without WFSYS_RST)

**Hypothesis:** Following upstream mt7925 order — bare SET_OWN→CLR_OWN after DMA_INIT + L0s disable.

### Result: BARE CLR_OWN IS A NO-OP ON MT7927

| Observation | Mode 11 (heavy CLR_OWN) | Mode 34A (bare CLR_OWN) |
|---|---|---|
| NEED_REINIT consumed | YES (0xffff0002 → 0xffff0000) | **NO** (stays 0xffff0002) |
| HOST rings wiped | YES (all BASE → 0) | **NO** (all survived) |
| HOST GLO_CFG reset | YES (→ 0x1010b870) | **NO** (stays 0x5410ba75) |
| MCU_RX2/RX3 re-configured | YES | **NO** (unchanged) |
| R2A FSM state | 0x03030101 (reset) | **0x00000202** (new!) |
| CLR_OWN time | 10ms | 10ms |

FWDL **succeeded** (bare CLR_OWN was no-op, HOST rings survived). NIC_CAPABILITY: **-110**.

---

## Test B: WFSYS_RST → Clean DMA → Bare CLR_OWN (Upstream Clean Path)

**Hypothesis:** WFSYS_RST resets ROM to fresh state. Combined with clean DMA re-init (no extra steps between WFSYS_RST and DMA), bare CLR_OWN should work.

### Result: WFSYS_RST WORKS BUT BREAKS BARE CLR_OWN + FWDL

**WFSYS_RST succeeded — cleaned EVERYTHING:**
```
Post-RST: DUMMY_CR=0xffff0000 GLO=0x1010b870 FSM=0x00000000
```
- DUMMY_CR reset to 0xffff0000 (NEED_REINIT cleared by reset)
- GLO_CFG reset to 0x1010b870 (DMA disabled, defaults)
- R2A FSM completely zeroed
- MCU_RX2/RX3 BASE=0x00000000 (ROM-configured FWDL rings GONE)
- MCU_DMA0_GLO=0x1010b870 (MCU DMA in reset state)

**DMA re-init succeeded:**
```
GLO=0x5430ba75 DUMMY_CR=0xffff0002 TX15=0x0a3f5000
```
HOST rings reprogrammed, NEED_REINIT set, DMA re-enabled.

**Bare CLR_OWN TIMED OUT (5 seconds):**
```
CLR_OWN: LPCTL=0x00000004 OWN_SYNC=1 after 5000ms FAIL
```
OWN_SYNC stuck at 1. ROM cannot process CLR_OWN after WFSYS_RST without extra init.

**FWDL FAILED: -110**
```
ring15 not consumed: cpu_idx=0x1 dma_idx=0x0
ROMCODE=0x00001d1e  (NOT 0xdead1234 = MCU_IDLE!)
INT_ENA=0x00000000 MCU_CMD=0x00000000
```
- ROM is NOT in IDLE state (ROMCODE=0x00001d1e)
- DMA not functional (TX15 DIDX stays 0, command not consumed)
- No interrupts enabled

### ROOT CAUSE: WFSYS_RST Puts ROM in Boot State, Extra Init IS Required

After WFSYS_RST, ROM needs to re-boot. The "extra steps" we skip are **NOT optional corruption** — they are **REQUIRED** for ROM to reach IDLE state:
1. **Sleep protection disable** — ROM can't access WFDMA without this
2. **Wait for ROMCODE=MCU_IDLE** — ROM needs time to boot (0x00001d1e → 0xdead1234)
3. **MCU_OWN** — needed for ROM to access hardware
4. **MCIF remap** — needed for ROM's memory-mapped access

Without these steps, ROM is stuck at ROMCODE=0x00001d1e and cannot:
- Process CLR_OWN (OWN_SYNC times out)
- Configure MCU_RX2/RX3 (FWDL rings)
- Process DMA commands (DIDX stays 0)

---

## Combined Conclusions

1. **Bare CLR_OWN is fundamentally different on MT7927 vs MT7925:**
   - MT7925: bare CLR_OWN triggers ROM processing (upstream works)
   - MT7927: bare CLR_OWN is a no-op (no NEED_REINIT processing, no ring wipe)

2. **WFSYS_RST cleans state but requires full re-init:**
   - Resets DUMMY_CR, GLO_CFG, FSM, MCU_RX rings — all back to zero/defaults
   - But ROM falls back to boot state (ROMCODE=0x00001d1e)
   - Extra init steps (slpprot, ROM idle wait, MCU_OWN, MCIF remap) are NECESSARY

3. **The "extra steps" ARE required on MT7927:**
   - Not corruption — they're hardware initialization
   - MT7927 has different boot requirements than MT7925
   - Can't skip them even with WFSYS_RST

4. **What this means for the approach:**
   - Simply copying upstream mt7925 sequence won't work
   - MT7927 needs its own init sequence
   - The HEAVY CLR_OWN (mode 11 style) is the only way to get ROM to process NEED_REINIT
   - The problem isn't init order — it's that ROM only configures MCU_RX2/RX3, never MCU_RX0/RX1

## What's Ruled Out
- Upstream init order as a fix
- WFSYS_RST + clean path as a fix
- Bare CLR_OWN as substitute for heavy CLR_OWN
- "Extra steps" as the cause of MCU_RX0 failure
- L0s disable as missing ingredient
