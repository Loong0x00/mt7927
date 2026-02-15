# Mode 37 Results: Direct MCU_RX0 BASE Write

**Date:** 2026-02-15
**Hypothesis:** MCU_RX0 BASE=0 is the root cause. Writing a valid SRAM address to MCU_RX0 will let WFDMA route HOST TX15 data to MCU, enabling NIC_CAPABILITY.

## Configuration
- `reinit_mode=37`
- Pre-FWDL: standard mode 11 (heavy CLR_OWN for FWDL)
- Post-boot: Write MCU_RX0/RX1 with SRAM addresses, then send NIC_CAPABILITY

## Result: HYPOTHESIS REJECTED — MCU_RX0 BASE Is NOT The Root Cause

**MCU_RX0 write SUCCEEDED** — BASE=0x0226cf00 took effect, register IS writable.
**MCU_RX0 CIDX=0 DIDX=0 after command** — WFDMA did NOT DMA any data there.
**NIC_CAPABILITY: -110** — still fails.

### Key Observations

1. **MCU_RX0 registers ARE writable from HOST:**
   - MCU_RX0: BASE=0x0226cf00, CNT=128, CIDX=0, DIDX=0
   - MCU_RX1: BASE=0x0226d700, CNT=128, CIDX=0, DIDX=0
   - Verified: reads back correctly.

2. **SRAM address computed from ROM-configured rings:**
   - MCU_RX2=0x0226ca00, MCU_RX3=0x0226cc80 (cnt=40)
   - MCU_RX0 placed at 0x0226cf00 (after MCU_RX3 end)
   - MCU_RX1 placed at 0x0226d700 (after MCU_RX0 end)

3. **WFDMA did NOT route data to MCU_RX0:**
   - CIDX and DIDX stayed 0 after NIC_CAPABILITY command
   - HOST TX15 DMA completed (DIDX advanced) but data never reached MCU

4. **FSM_CMD=0x03030101** — pre-boot FSM state (different from usual 0x01010202, confirms FSM state is irrelevant)

5. **R2A_STS=0x07770313** — unchanged, sleep status doesn't block DMA

### CRITICAL CONCLUSION

**MCU_RX0 BASE=0 is NOT the root cause.** Even with a valid SRAM address:
- WFDMA does not DMA HOST TX15 data to MCU_RX0
- The R2A bridge drops/doesn't route HOST→MCU traffic
- MCU_RX0 BASE being zero was a SYMPTOM, not the CAUSE

The actual blocker is the WFDMA's internal routing/bridge configuration, which is controlled by something we haven't identified — NOT by ring BASE addresses.

### What This Changes About Our Understanding

1. **MCU_RX0 BASE=0 theory DEAD**: Writing valid BASE doesn't fix routing
2. **R2A bridge controls routing**: Bridge decides WHERE data goes, not ring BASEs
3. **Ring configuration is a FW-side concern**: FW configures its OWN MCU_RX rings in SRAM. HOST writing them is meaningless without FW participation.
4. **The problem is upstream of MCU_RX0**: Data never even gets from WFDMA TX→MCU_RX path

### What's Ruled Out
- MCU_RX0 BASE=0 as root cause
- HOST-side MCU_RX ring configuration as fix
- SRAM address calculation issues
- Register write access issues (registers ARE writable)

### What This Points To
The WFDMA's internal dispatch/routing between HOST TX and MCU RX is the real blocker. This is likely configured by FW during its initialization, not by HOST register writes. We need to find what MCU command or initialization step triggers the FW to set up this internal routing.
