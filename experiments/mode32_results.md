# Mode 32 Results: WFDMA HIF_SW_RST (0xd703C)

**Date**: 2026-02-15
**Module**: mt7927_init_dma.ko reinit_mode=32
**Kernel**: 6.18.9-arch1-2

## Summary

**Mode 32 FAILED catastrophically.** HIF_SW_RST (0xd703C BIT(0)) before FWDL kills the WFDMA DMA engine entirely. TX ring 15 DIDX never advances after kick, so patch download times out (-110). Post-boot phase never reached.

## Execution Flow

### Phase 1: Mode 11 CLR_OWN (baseline, ran first)
- SET_OWN/CLR_OWN cycle completed normally
- NEED_REINIT consumed: DUMMY_CR 0xffff0002 -> 0xffff0000
- Post-CLR_OWN MCU_RX state (as expected):
  - MCU_RX0: BASE=0x00000000 (not configured by ROM)
  - MCU_RX1: BASE=0x00000000 (not configured by ROM)
  - MCU_RX2: BASE=0x0226ca00 (ROM configured - FWDL)
  - MCU_RX3: BASE=0x0226cc80 (ROM configured - FWDL)
- HOST rings zeroed by CLR_OWN -> reprogrammed successfully
- After restore: GLO=0x5410ba75, TX15=0x0b021000, DUMMY=0xffff0002

### Phase 2: Mode 32 Pre-FWDL HIF_SW_RST
- SW_RST before write: 0x00000000
- Wrote BIT(0) to 0xd703C
- SW_RST after 3ms: 0x00000000 (auto-cleared OK)
- FSM_CMD after reset: 0x00000000 (was 0x00000000)
- **HOST rings zeroed AGAIN** by HIF_SW_RST -> reprogrammed
- After reprogram: GLO=0x1010b87d (TX_EN=1, RX_EN=1)
- NEED_REINIT set back to 1: DUMMY_CR=0xffff0002

### Phase 3: FWDL Attempt (FAILED)
- Patch download: `ring15 not consumed: cpu_idx=0x1 dma_idx=0x0`
- **TX ring 15 CIDX kicked to 1, but DIDX stayed at 0** = DMA engine not consuming descriptors
- HOST_INT_STA=0x00000000 (no interrupt fired)
- fw_sync stayed at 0x00000001 (never progressed past ROM)
- ROMCODE=0x00001d1e
- **Result: -110 (timeout)**

### Phase 4: Post-boot (NEVER REACHED)
- Firmware flow failed at patch download, device kept bound but post-boot mode32 code never executed.

## Key Register States Post-Failure

| Register | Value | Note |
|----------|-------|------|
| GLO_CFG | 0x1010b877 | TX/RX EN set but DMA dead |
| INT_ENA | 0x00000000 | No interrupts enabled |
| INT_STA | 0x00000000 | No interrupts pending |
| MCU_CMD | 0x00000000 | |
| SW_INT_ENA | 0x00000000 | |
| GLO_CFG2 | 0x00000002 | |
| DUMMY_CR | 0xffff0002 | NEED_REINIT=1 |
| MCU_DMA0_GLO | 0x10703875 | MCU DMA running but pointless |
| fw_sync | 0x00000001 | Stuck at ROM stage |

## MCU Ring State at Failure

| Ring | BASE | CNT | CIDX | DIDX |
|------|------|-----|------|------|
| MCU_TX0 | 0x00000000 | 512 | 0 | 0 |
| MCU_TX1 | 0x00000000 | 512 | 0 | 0 |
| MCU_TX2 | 0x00000000 | 512 | 0 | 0 |
| MCU_TX3 | 0x00000000 | 512 | 0 | 0 |
| MCU_RX0 | 0x00000000 | 512 | - | - |
| MCU_RX1 | 0x00000000 | 512 | - | - |
| MCU_RX2 | 0x0226ca00 | 40 | - | - |
| MCU_RX3 | 0x0226cc80 | 40 | - | - |

## Analysis

1. **HIF_SW_RST is more destructive than CLR_OWN**: Both zero HOST ring bases, but HIF_SW_RST additionally breaks the WFDMA DMA engine's ability to process TX descriptors. Even after ring reprogramming and GLO_CFG TX_EN re-enable, DIDX never advances.

2. **Auto-clear works but damage is done**: The BIT(0) in 0xd703C auto-clears within 3ms, but the internal WFDMA state is corrupted beyond what ring reprogramming can fix.

3. **Cannot test post-boot behavior**: Since HIF_SW_RST before FWDL kills the DMA pipeline, we never reach the post-boot phase where HIF_SW_RST might have been useful for resetting the R2A bridge.

4. **NEED_REINIT re-set**: After HIF_SW_RST, DUMMY_CR was 0xffff0002 again, suggesting the reset creates conditions where NEED_REINIT gets set or is preserved.

## Conclusion

**Mode 32 is a dead end.** HIF_SW_RST (0xd703C) cannot be used safely - it breaks the WFDMA DMA engine in a way that ring reprogramming cannot fix. The only way to recover from HIF_SW_RST would likely be a full WFSYS_SW_RST cycle, which defeats the purpose.

This confirms that the WFDMA hardware reset path available through registers is too aggressive for selective R2A bridge reset. The R2A bridge routing problem must be solved through FW-level configuration, not hardware resets.
