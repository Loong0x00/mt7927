# Mode 33 Results: Pure Vendor mt6639 Prefetch (RX4-7 Only)

**Date:** 2026-02-15
**Hypothesis:** Our hybrid HOST RX prefetch config (RX0-3 + RX4-7) confuses FW's WFDMA init. Pure vendor mt6639 prefetch (RX4-7 only) should let FW configure MCU_RX0 itself.

## Configuration
- `reinit_mode=33`
- Pure vendor mt6639WfdmaManualPrefetch: RX4-7 only, all depth=4, sequential base_ptr
- HOST RX0 NOT allocated (BASE explicitly zeroed)
- RX0-3 EXT_CTRL = 0 (no prefetch)
- Pre-FWDL: standard mode 11 wakeup + CLR_OWN

## Result: HYPOTHESIS REJECTED

**MCU_RX0 BASE=0x00000000** — FW did NOT configure MCU_RX0 with pure vendor prefetch.

### Key Observations

1. **Prefetch correctly applied:**
   - HOST_RX EXT: 0=0x00000000, 1=0x00000000, 2=0x00000000, 3=0x00000000
   - HOST_RX EXT: 4=0x00000004, 5=0x00400004, 6=0x00800004, 7=0x00c00004
   - Exact match to vendor mt6639WfdmaManualPrefetch()

2. **FW boot succeeded:** fw_sync=0x3 (FW ready), WFSYS_INIT_DONE received immediately

3. **MCU ring state (unchanged):**
   - MCU_RX0: BASE=0x00000000, CNT=512 (NOT configured)
   - MCU_RX1: BASE=0x00000000, CNT=512 (NOT configured)
   - MCU_RX2: BASE=0x0226ca00, CNT=40 (ROM-configured, FWDL ring)
   - MCU_RX3: BASE=0x0226cc80, CNT=40 (ROM-configured, FWDL ring)
   - MCU_TX0-3: ALL BASE=0x00000000 (NOT configured)

4. **rx0-poll during boot:** 2000 iterations, ZERO changes. MCU_RX0 never transitions.

5. **NIC_CAPABILITY: -110** (timeout) — same as all modes

6. **DUMMY_CR=0xffff0003** — NEED_REINIT still set (not consumed by running FW)

7. **FSM_CMD=0x01010202** — post-boot R2A FSM state, same as all other modes

8. **MCU_DMA0_GLO=0x10703875** — unchanged from ROM config

## Conclusion

Pure vendor prefetch (RX4-7 only) makes NO difference to MCU_RX0/MCU_TX configuration.
The prefetch config (hybrid vs pure vendor) is NOT the root cause of the MCU DMA issue.

The running FW simply does not configure MCU_RX0/RX1 or MCU_TX rings regardless of
HOST-side prefetch settings. Something else must trigger this configuration — likely a
specific MCU command or interrupt sequence that we haven't found yet.

## What This Rules Out
- Hybrid prefetch confusing FW WFDMA init
- HOST RX0 allocation interfering with FW MCU_RX0 setup
- Prefetch depth differences (8 vs 4) affecting FW behavior
