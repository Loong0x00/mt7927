# WFSYS_SW_INIT_DONE & INT_STA Research — Session 9

## Executive Summary

Three critical findings that change our understanding of the Mode 42 results:

1. **0xe0010 = 0 is EXPECTED** — it's a reset control register, NOT a status register
2. **INT_STA BIT(14) = RX done on EVENT ring (hw ring 6)** — FW IS putting data on the event ring
3. **TX WFDMA stall is the PRIMARY blocker** — NIC_CAP descriptor never consumed by DMA engine

---

## Finding 1: WFSYS_SW_INIT_DONE at 0xe0010 Is a MISIDENTIFICATION

### What 0xe0010 Actually Is

The register at BAR0+0xe0010 (bus 0x7c060010) is **CONN_INFRA_WFSYS_SW_RST** — a reset control
register used in Windows ToggleWfsysRst steps 16-18:

```
Step 16: WRITE 0x7c060010 = BIT(0)    — Assert CONN_INFRA reset
Step 17: Poll  0x7c060010 BIT(2)       — Wait for reset done
Step 18: WRITE 0x7c060010 = BIT(1)    — Deassert CONN_INFRA reset
```

Source: `docs/references/ghidra_post_fw_init.md` lines 96-98

### It Is NOT "WFSYS SW Init Done"

The REAL "WFSYS SW Init Done" check for MT6639 is:
- **Register**: ROMCODE_INDEX at 0x81021604 (BAR0+0xc1604)
- **Expected value**: 0x1D1E (MCU_IDLE)
- **Our driver already does this and it SUCCEEDS**

Source: `mt6639/chips/mt6639/hal_wfsys_reset_mt6639.c:124-160`
```c
u_int8_t mt6639HalPollWfsysSwInitDone(struct ADAPTER *prAdapter)
{
    // Polls WF_TOP_CFG_ON_ROMCODE_INDEX_REMAP_ADDR (0x81021604)
    // Expects MCU_IDLE = 0x1D1E
    // Up to 200ms timeout (2 * 100ms sleep)
}
```

### MT7961 vs MT6639 — Different Registers

The confusion arose from MT7961, which has a separate register:
- **MT7961**: `CONN_INFRA_RGU_WFSYS_SW_RST_B_ADDR` at 0x7C000140 with `WFSYS_SW_INIT_DONE` BIT(4)
  (`mt6639/include/chips/hal_wfsys_reset_mt7961.h:102-107`)
- **MT6639**: Uses ROMCODE_INDEX at 0x81021604 for MCU_IDLE detection — completely different mechanism

### CONN_INFRA Reset (Steps 16-18) — Only in Windows Driver

The vendor Android driver for MT6639 (`hal_wfsys_reset_mt6639.c`) does NOT perform the
CONN_INFRA reset at 0x7c060010. The Windows driver's ToggleWfsysRst has extra steps the Android
vendor skips. Reading 0xe0010 = 0 is EXPECTED because we never write to it.

**CONCLUSION: 0xe0010 = 0 is NOT a problem. WFSYS init done already confirmed via ROMCODE_INDEX = 0x1D1E.**

---

## Finding 2: INT_STA BIT(14) = RX Done on AP EVENT Ring

### Bit Layout (MT6639 HOST_INT_STA at BAR0+0xd4200)

From `mt6639/include/chips/coda/mt6639/wf_wfdma_host_dma0.h`:

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | rx_done_int_sts_0 | RX ring 0 (data) |
| 1 | rx_done_int_sts_1 | RX ring 1 (data) |
| 2 | rx_done_int_sts_2 | RX ring 2 |
| 3 | rx_done_int_sts_3 | RX ring 3 |
| 4-9 | tx_done_int_sts_0-5 | TX rings 0-5 |
| 12 | rx_done_int_sts_4 | RX ring 4 (AP DATA0) |
| 13 | rx_done_int_sts_5 | RX ring 5 (AP DATA1) |
| **14** | **rx_done_int_sts_6** | **RX ring 6 (AP EVENT)** |
| 15 | rx_done_int_sts_7 | RX ring 7 (AP TDONE0) |
| 29 | mcu2host_sw_int_sts | MCU2HOST SW interrupt |

### Ring 6 = AP EVENT Ring (MCU Responses)

From `mt6639/chips/mt6639/mt6639.c`:
```c
struct wfdma_group_info mt6639_wfmda_host_rx_group[] = {
    {"P0R4:AP DATA0", ..._RX_RING4_...},
    {"P0R6:AP EVENT", ..._RX_RING6_...},   // ← THIS ONE
    {"P0R5:AP DATA1", ..._RX_RING5_...},
    {"P0R7:AP TDONE0", ..._RX_RING7_...},
};
```

Software `RX_RING_EVT_IDX_1` maps to hardware ring 6 (offset = 6 * MT_RINGREG_DIFF).

In the interrupt handler (`mt6639.c:566-568`):
```c
if ((u4Sta | WF_WFDMA_HOST_DMA0_HOST_INT_STA_rx_done_int_sts_6_MASK) ||
    (prAdapter->u4NoMoreRfb & BIT(RX_RING_EVT_IDX_1)))
    halRxReceiveRFBs(prAdapter, RX_RING_EVT_IDX_1, FALSE);
```

### Interpretation of BIT(14) Being Set

**There IS data on the MCU event ring (hw RX ring 6)**. This could be:
1. FW startup notification after FW_START
2. FWDL completion event
3. Residual from FWDL scatter acknowledge

This means the WFDMA RX path (MCU → HOST) is at least partially working — the FW has placed
something on the event ring. This is actually a GOOD sign.

### Vendor INT_STA Clearing Pattern

From `mt6639/os/linux/hif/common/kal_pdma.c:508-522`:
```c
HAL_MCR_RD(prAdapter, WPDMA_INT_STA, &u4RegValue);
// ... process interrupts ...
HAL_MCR_WR(prAdapter, WPDMA_INT_STA, u4RegValue);  // Write-1-to-clear
```

INT_STA is **write-1-to-clear** — writing a bit value of 1 clears that interrupt status bit.
The vendor reads, processes, then writes back the read value to clear all pending bits.

### Does Pending INT_STA Block DMA?

**Generally NO** — WFDMA should handle TX and RX independently per ring. A pending RX interrupt
on ring 6 should not block TX DMA on the TX CMD ring. However, DMASHDL flow control might
couple them if host RX buffer exhaustion triggers back-pressure.

---

## Finding 3: TX WFDMA Stall Analysis

### The Key Observation

Mode 42 showed: cpu_idx=0xd, dma_idx=0xc on the TX ring
- HOST wrote NIC_CAP descriptor at index 0xd
- WFDMA only consumed up to 0xc (FWDL descriptors)
- **WFDMA TX engine stopped processing after FWDL**

### GLO_CFG DMA Control Bits (MT6639)

From `wf_wfdma_host_dma0.h`:
- **TX_DMA_EN** = BIT(0) in GLO_CFG — enables TX DMA engine
- **TX_DMA_BUSY** = BIT(1) in GLO_CFG — read-only, indicates TX DMA busy
- **RX_DMA_EN** = BIT(2) in GLO_CFG
- **RX_DMA_BUSY** = BIT(3) in GLO_CFG
- **TX_DMA_EN_2ND** = BIT(0) in GLO_CFG2 (0xd425C) — second TX DMA for rings with attribute=1
- **TX_DMA_BUSY** = BIT(1) in GLO_CFG2

### WPDMA_RST_DTX_PTR — TX Pointer Reset

Register: `WF_WFDMA_HOST_DMA0_WPDMA_RST_DTX_PTR_ADDR` = BASE+0x20C (BAR0+0xd420C)
- Can reset TX DMA read pointer for specific rings
- Used by `asicConnac3xWfdmaIntRstDtxPtrAddrGet()`

### Ring Assignment Difference

**Vendor uses SEPARATE TX rings**:
- TX ring 16 (`CONNAC3X_FWDL_TX_RING_IDX`) — FWDL only
- TX ring 15 (`tx_ring_cmd_idx = 15`) — MCU commands
- TX ring 0-1 — data

If our driver uses the SAME ring for FWDL and MCU commands:
- FWDL fills the ring (descriptors 0-12, dma_idx advances to 0xc)
- NIC_CAP added at index 0xd
- But the WFDMA may have stopped servicing this ring after FWDL ended

### GLO_CFG_EXT1 BIT(28) = TX_FCTRL_MODE

From vendor `MT6639WpdmaConfig()`:
```c
READ(ctx, 0x7c0242b4, &val);  // GLO_CFG_EXT1
val |= 0x10000000;             // BIT(28) = TX_FCTRL_MODE
WRITE(ctx, 0x7c0242b4, val);
```

This is TX flow control mode — affects how DMASHDL manages TX scheduling.
Mode 42 added this, but TX still stalled.

---

## Finding 4: Vendor Init Order (Complete)

From `mt6639/common/wlan_lib.c:1080-1272`, the full MT6639 PCIe init sequence:

```
1.  nicpmSetDriverOwn()          — CLR_OWN (get driver ownership)
2.  nicInitializeAdapter()       — chip init, MCR config, HIF init
3.  wlanWakeUpWiFi()             — wake WiFi subsystem
4.  halHifSwInfoInit()           — ring alloc, DMA enable, DMASHDL, NEED_REINIT
5.  HAL_ENABLE_FWDL(TRUE)       — NO-OP for MT6639 (asicEnableFWDownload=NULL)
6.  nicDisableInterrupt()        — DISABLE ALL INTERRUPTS (before FWDL!)
7.  nicTxInitResetResource()     — reset TX resources
8.  mt6639HalCbInfraRguWfRst(TRUE)   — assert WFSYS reset (CB_INFRA_RGU BIT(4))
9.  mt6639HalPollWfsysSwInitDone()   — poll ROMCODE_INDEX = 0x1D1E (200ms timeout)
10. mt6639HalCbInfraRguWfRst(FALSE)  — deassert WFSYS reset
11. wlanDownloadFW()             — FW download
12. wlanCheckWifiFunc()          — poll fw_sync
13. wlanQueryNicCapabilityV2()   — FIRST MCU COMMAND (NIC_CAP)
```

### Critical Differences from Our Mode 40/42

| Aspect | Vendor | Our Mode 40/42 |
|--------|--------|----------------|
| **Ring setup** | BEFORE reset (step 4) | AFTER CLR_OWN (wiped) |
| **CLR_OWN** | BEFORE reset (step 1) | AFTER reset |
| **Interrupts** | Disabled before FWDL (step 6) | Not explicitly disabled |
| **HAL_ENABLE_FWDL** | NO-OP for MT6639 | Sets BIT(9) BYPASS_DMASHDL! |
| **TX ring for FWDL** | Ring 16 (separate) | Same ring as MCU? |
| **TX ring for MCU** | Ring 15 (separate) | Same ring as FWDL? |
| **WFSYS reset poll** | Between assert & deassert | After deassert |

### Ring Survival Across WFSYS Reset

WFSYS reset only resets the WF subsystem, NOT WFDMA (which is in CONN_INFRA). So:
- HOST ring BASEs in WFDMA survive WFSYS reset
- The vendor sets up rings (step 4) BEFORE reset (step 8) — rings remain valid
- Our Mode 40: reset first, then CLR_OWN (wipes ALL ring BASEs), then reprogram
- After CLR_OWN, ROM processes NEED_REINIT with rings=0 → broken MCU ring config

---

## Hypotheses for TX WFDMA Stall (Priority Order)

### H1: FWDL and MCU commands on same TX ring — Ring State Corruption
If our driver uses the same physical TX ring for FWDL (TX ring 16 in vendor) and MCU commands
(TX ring 15 in vendor), the FWDL completion may put the ring in a state where new descriptors
aren't processed. The WFDMA ring FSM may be in a "completed" state.

**Test**: Read TX ring dma_idx/cpu_idx BEFORE and AFTER adding NIC_CAP descriptor. Also read
GLO_CFG to verify TX_DMA_EN is set and TX_DMA_BUSY status.

### H2: DMASHDL Flow Control Blocking
DMASHDL controls DMA scheduling between TX queues. After clearing BYPASS bits (Mode 42), the
DMASHDL may now be ACTIVELY managing flow control — but without proper quota configuration,
it blocks all TX.

DMASHDL needs quotas configured per-queue. The vendor's `halHifSwInfoInit()` configures these
during step 4. We may be missing this configuration entirely.

**Test**: Read DMASHDL status registers at BAR0+0xd6000 area.

### H3: Pending RX Event Causing Back-pressure
The pending event on RX ring 6 (BIT(14) in INT_STA) could cause DMASHDL back-pressure if the
host RX buffers are exhausted. DMASHDL may refuse to schedule TX until host consumes RX.

**Test**: Clear INT_STA by writing 0xFFFFFFFF to BAR0+0xd4200 before NIC_CAP. Also consume
the pending RX event on ring 6.

### H4: Ring Index Mismatch
If FWDL uses TX ring 16 and NIC_CAP uses TX ring 15 (like vendor), but our DMA pointers
(cpu_idx/dma_idx) are being read from the WRONG ring, we'd see the FWDL ring's stale state.

**Test**: Verify which TX ring the NIC_CAP descriptor is actually written to. Read BOTH
TX ring 15 and TX ring 16 pointers.

### H5: GLO_CFG TX_DMA_EN Lost After FWDL
The GLO_CFG two-phase init may not properly set TX_DMA_EN. Or FWDL completion may clear it.

**Test**: Read GLO_CFG (0xd4208) immediately before NIC_CAP. Verify BIT(0) is set.

---

## Recommended Next Steps

1. **Diagnostic dump before NIC_CAP**: Read and log these registers JUST BEFORE sending NIC_CAP:
   - GLO_CFG (0xd4208) — check TX_DMA_EN, TX_DMA_BUSY
   - INT_STA (0xd4200) — current interrupt status
   - DMASHDL status regs (0xd6000-0xd6010)
   - TX ring 15 and 16 cpu_idx/dma_idx
   - RX ring 6 cpu_idx/dma_idx — check if there's pending RX data

2. **Clear INT_STA before NIC_CAP**: Write 0xFFFFFFFF to 0xd4200

3. **Use separate TX rings**: FWDL on ring 16, MCU commands on ring 15 (matching vendor)

4. **Consume pending RX event**: Read data from RX ring 6 before sending NIC_CAP

5. **Match vendor init order**: Set up rings BEFORE WFSYS reset, skip CLR_OWN after reset
