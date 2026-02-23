# MCU_RX0 BASE Polling Diagnostic Results (Task #3)

**Date**: 2026-02-15
**Module**: mt7927_init_dma.ko reinit_mode=11
**Kernel**: 6.18.9-arch1-2

## Summary

**MCU_RX0 BASE stays at 0x00000000 for the entire 2-second observation window after FW boot.** The running firmware does NOT configure MCU_RX0 as part of its boot sequence. MCU_RX0 configuration must be triggered by a specific host-side action that we have not yet discovered.

## Test 1: 11 iterations (early-exit on fw_sync=0x3)

```
[rx0-poll] START: MCU_RX0_BASE=0x00000000 MCU_RX1_BASE=0x00000000 MCU_DMA0_GLO=0x10703875 fw_sync=0x00000003 FSM_CMD=0x01010202 DUMMY=0xffff0003
[rx0-poll] END after 11 iterations, 0 changes
```

Key: fw_sync was ALREADY 0x3 at first read. FW boots during FW_START ACK processing (inside mt7927_mcu_send_cmd 2000ms wait).

## Test 2: Full 2000 iterations (2 seconds, no early exit)

```
[rx0-poll] START: MCU_RX0_BASE=0x00000000 MCU_RX1_BASE=0x00000000 MCU_DMA0_GLO=0x10703875 fw_sync=0x00000003 FSM_CMD=0x01010202 DUMMY=0xffff0003
[rx0-poll] END after 2000 iterations, 0 changes. MCU_RX0=0x00000000 MCU_RX1=0x00000000 GLO=0x10703875 fw_sync=0x00000003 FSM=0x01010202
FW ready: fw_sync=0x00000003 after 0ms MCU_RX0=0x00000000
```

**ZERO changes across 2000 iterations (2 full seconds)**. All registers remained perfectly stable:

| Register | Value (stable) | Meaning |
|----------|---------------|---------|
| MCU_RX0 BASE (0x02500) | 0x00000000 | Never configured |
| MCU_RX1 BASE (0x02510) | 0x00000000 | Never configured |
| MCU_DMA0 GLO_CFG (0x02208) | 0x10703875 | MCU DMA running, unchanged |
| fw_sync (CONN_ON_MISC) | 0x00000003 | FW ready, stable |
| FSM_CMD | 0x01010202 | Post-boot FSM, stable |
| DUMMY_CR (0x02120) | 0xffff0003 | fw_sync=3 reflected |

## Analysis

1. **FW boot is instant**: fw_sync=0x3 is already set when we first read after FW_START returns. The FW_START MCU command takes ~100ms (DMA kick→ACK), and by the time the ACK is processed, FW has already booted.

2. **MCU_RX0 is NOT auto-configured by FW during boot**: 2000ms of 1ms-interval polling shows zero changes. The FW boot sequence does not include configuring MCU_RX0/RX1 receive rings.

3. **MCU_RX0 must be triggered by the host**: In the upstream mt7925 driver, the FW configures MCU_RX0 in response to some host action. Candidates:
   - A specific MCU command (NIC_CAPABILITY itself? But that needs MCU_RX0 to work...)
   - A register write sequence (power management related?)
   - A second SET_OWN/CLR_OWN cycle after fw_sync=0x3 (but we tested this and NEED_REINIT was already consumed)
   - An interrupt or doorbell the host sends

4. **Chicken-and-egg problem**: If MCU_RX0 is configured in response to a MCU command, but MCU commands need MCU_RX0 to route replies, then there must be a bootstrap path. The FWDL path uses MCU_RX2/RX3 (which ROM configures). Maybe the first post-boot command must also use MCU_RX2/RX3?

## Implications

The search should focus on:
- What register writes or MCU commands the upstream mt7925 driver sends between fw_sync=0x3 and the first NIC_CAPABILITY
- Whether there's a "WFDMA init" MCU command that triggers MCU_RX0 configuration
- Whether the host needs to write MCU_RX0 BASE addresses directly (i.e., HOST configures them, not FW)
