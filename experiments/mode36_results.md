# Mode 36 Results: DUMMY_CR BIT(0)|BIT(1) Before CLR_OWN

**Date:** 2026-02-15
**Hypothesis:** BIT(0) in DUMMY_CR = "NEED_INIT" (full init incl MCU_RX0/RX1) vs BIT(1) = "NEED_REINIT" (partial, FWDL rings MCU_RX2/RX3 only).

## Configuration
- `reinit_mode=36`
- Set BIT(0)|BIT(1) in DUMMY_CR before mode11 heavy CLR_OWN
- After CLR_OWN, check if ROM configured MCU_RX0

## Result: HYPOTHESIS REJECTED — BIT(0) Already Set By ROM

**MCU_RX0_BASE=0x00000000** — BIT(0) had NO effect on MCU_RX0 configuration.
**NIC_CAPABILITY: -110**

### Key Observations

1. **DUMMY_CR was ALREADY 0xffff0003** — ROM sets BIT(0) naturally. We weren't adding anything new.
2. **ROM consumed BOTH bits** during CLR_OWN: 0xffff0003 → 0xffff0000. Both BIT(0) and BIT(1) cleared.
3. **MCU_RX0 BASE=0** after CLR_OWN — same as mode 11 (only MCU_RX2/RX3 configured).
4. **Mode11 step 10h re-sets only BIT(1)** → DUMMY_CR=0xffff0002.
5. **Post-boot: DUMMY_CR=0xffff0003** — FW re-sets both bits during boot.
6. **Post-boot MCU_RX0 still zero** — FW boot doesn't configure it either.

### What This Means

BIT(0) in DUMMY_CR is NOT a "NEED_INIT" flag. It was already set by ROM at power-on and ROM already sees it during every CLR_OWN. ROM consumes both bits but STILL only configures MCU_RX2/RX3 (FWDL rings). BIT(0) appears to be a generic status bit with no bearing on MCU_RX0 initialization.

### What's Ruled Out
- DUMMY_CR BIT(0) as MCU_RX0 configuration trigger
- Any DUMMY_CR bit combination as the missing ingredient
- ROM distinguishing between "init" vs "reinit" via DUMMY_CR bits
