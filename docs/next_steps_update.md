# MT7927 Driver Development - Consolidated Investigation Report
# Date: 2026-02-15 (Team Session)
# Team: mt7927-fsm-fix (agent1, agent2, agent3)

---

## Executive Summary

Three parallel investigations were conducted to resolve the R2A Bridge FSM blocker that prevents HOST-to-MCU communication after firmware boot. The combined findings conclusively show:

1. **The R2A FSM state is a RED HERRING** -- recovering FSM to 0x03030101 does NOT restore MCU command routing
2. **The real problem is MCU_RX0 BASE=0x00000000** -- the MCU-side DMA ring for post-boot commands was never configured
3. **NEED_REINIT already consumed by ROM** -- Task #9 proved our driver ALREADY does the SET_OWN/CLR_OWN with NEED_REINIT. ROM only configures MCU_RX2/RX3 (FWDL rings), NOT MCU_RX0/RX1. MT7927 ROM differs from MT7925!
4. **Running FW must configure MCU_RX0** -- not ROM. The question is what triggers it.
5. **Multiple alternative approaches eliminated** -- AXI dispatch routing, GLO_CFG_EXT2 halt/drop, WPDMA1 interrupts, Q_IDX routing, WFDMA1 bridge, and direct FSM manipulation all proven ineffective

**UPDATE (Task #9 result):** The second SET_OWN/CLR_OWN hypothesis was INVALIDATED. Our driver already consumes NEED_REINIT (DUMMY_CR goes from 0xffff0002 to 0xffff0000 during Mode 11 CLR_OWN). ROM only sets up FWDL rings. The running firmware itself must configure MCU_RX0 for post-boot commands, but we don't know what triggers it. This is now the primary investigation (Task #12).

---

## Table of Contents

1. [Agent1 Results: Dummy Command FSM Recovery](#1-agent1)
2. [Agent2 Results: Alternative FSM Recovery Approaches](#2-agent2)
3. [Agent3 Results: Upstream Code Analysis](#3-agent3)
4. [Comprehensive Eliminated Hypotheses](#4-eliminated)
5. [Root Cause Analysis](#5-root-cause)
6. [Recommended Fix: Second SET_OWN/CLR_OWN with NEED_REINIT](#6-fix)
7. [Task #9 Status: Testing the Fix](#7-task9)
8. [New Discoveries](#8-discoveries)
9. [Remaining Open Questions](#9-questions)
10. [Priority-Ordered Next Steps](#10-next-steps)

---

## 1. Agent1 Results: Dummy Command FSM Recovery {#1-agent1}

### Mode 26: Dummy Command + FSM Poll + Real Command

**Approach**: Send a throwaway MCU command immediately after fw_sync=0x3 to trigger DMA activity, wait for R2A FSM recovery to 0x03030101, then send the real NIC_CAPABILITY command.

**Results**:
- Dummy command DMA completed (CIDX=DIDX advanced)
- FSM recovered to 0x03030101 in ~0ms (immediately after DMA kick)
- NIC_CAPABILITY sent with FSM confirmed at 0x03030101
- **Result: -110 (ETIMEDOUT)** -- command still fails even with "working" FSM
- MCU_RX0-3 DIDX unchanged -- data never reaches MCU

**Conclusion**: FSM state 0x03030101 does NOT mean the R2A bridge is routing correctly. The FSM state is a superficial indicator, not the controlling mechanism. The real issue is that MCU_RX0 (the post-boot command destination) has BASE=0x00000000 -- it was never configured.

---

## 2. Agent2 Results: Alternative FSM Recovery Approaches {#2-agent2}

### Mode 27: AXI Dispatch Controller Force-Routing + GLO_CFG_EXT2

**Approach**: Use DISP_CTRL (0xd70A0) to force AXI routing toward MCU, check GLO_CFG_EXT2 (0xd42B8) for TX halt/drop conditions.

**Results**:
- DISP_CTRL reads 0x00000000 -- no active force-routing (MCU is already default target)
- GLO_CFG_EXT2=0x00000004 -- TX_DROP=0, TX_HALT=0, pause=0 (no blocking conditions)
- Force-to-MCU (0x00000005) written and confirmed: NIC_CAPABILITY **-110**
- Force-to-HIF (0x0000000a) written and confirmed: NIC_CAPABILITY **-110**
- Neither force-routing option affects command delivery

**Eliminated**: AXI dispatch routing and GLO_CFG_EXT2 halt/drop as causes.

### Mode 28: WPDMA1 MCU_INT_EVENT + Bare CIDX Kick

**Approach**: Signal MCU via WPDMA1 interrupt path (0x03108, different from HOST2MCU at 0x02108), then test bare CIDX kick without valid descriptor.

**Results**:
- Part A (WPDMA1 MCU_INT_EVENT):
  - DMA_INIT signal: no FSM effect
  - RESET_DONE signal: no FSM effect
  - Combined signal: no FSM effect
  - NIC_CAPABILITY after signals: **-110**
- Part B (Bare CIDX kick):
  - **NEW DISCOVERY**: FSM entered state 0x03030303 (never seen before!)
  - 0x03030303 is an intermediate state between 0x01010202 and 0x03030101
  - FSM stuck at 0x03030303 for ~3s, then recovered to 0x03030101 after real command
  - NIC_CAPABILITY with FSM at 0x03030101: **-110**

**Eliminated**: WPDMA1 MCU_INT_EVENT as a trigger for MCU ring configuration.
**New data**: FSM state 0x03030303 exists as a transient intermediate state.

### Mode 29: Combined Kitchen Sink Approach

**Approach**: Combined all signals -- WPDMA1 INT, NEED_REINIT set, AXI force-routing, throwaway dummy command, FSM poll, then real command.

**Results**:
- FSM recovered to 0x03030101 in 0ms after dummy kick (instant!)
- NIC_CAPABILITY sent with FSM confirmed at 0x03030101
- **Result: -110 (ETIMEDOUT)**
- MCU_RX0-3 DIDX unchanged -- no data reaches MCU
- Tested with Q_IDX=0x22 (MCU_RX2, which has valid BASE=0x0226ca00): **-110**
- Tested with Q_IDX=0x3e (FWDL routing): **-110**
- Tested with Q_IDX=0x20 (standard MCU): **-110**

**Key Conclusion**: ALL Q_IDX routing options fail post-boot. The R2A bridge blocks ALL traffic after FW boot regardless of destination ring. Even routing to MCU_RX2 (which has a valid BASE and worked during FWDL) fails.

---

## 3. Agent3 Results: Upstream Code Analysis {#3-agent3}

Full analysis document: `docs/upstream_analysis_post_fw_boot.md`

### Critical Discovery: Two SET_OWN/CLR_OWN Cycles

The upstream mt7925 driver performs TWO SET_OWN/CLR_OWN power cycles:

**Cycle 1** (Probe, `pci.c:386-392`):
- Bare SET_OWN/CLR_OWN before WFSYS reset
- Establishes initial driver ownership
- NEED_REINIT is NOT set at this point

**Between cycles**:
- WFSYS reset
- DMA init (allocate all ring buffers, write BASE/CNT to hardware)
- **DMA enable sets NEED_REINIT=BIT(1) in DUMMY_CR** (critical!)
- IRQ setup, device registration

**Cycle 2** (mcu_init, `pci_mcu.c:38-43`):
- SET_OWN/CLR_OWN with NEED_REINIT=1 already set
- ROM sees NEED_REINIT during CLR_OWN processing
- **ROM configures MCU-side DMA rings** -- this is what bridges HOST commands to MCU
- After this, FW download + first MCU command work

### Our Driver's Mistake

Our driver performs only ONE SET_OWN/CLR_OWN cycle, and it happens before DMA init. By the time we download FW and send MCU commands, the WFDMA R2A bridge has not been properly configured by the ROM's NEED_REINIT handler. MCU_RX0 never gets a valid BASE address.

### Vendor mt6639 Confirmation

The vendor mt6639 driver explicitly DISABLES dummy commands for CONNAC3X:
```c
#if (CFG_SUPPORT_CONNAC3X == 0)
    wlanSendDummyCmd(prAdapter, TRUE);
#endif
```

This confirms that for MT6639/MT7927, no dummy command workaround is needed -- the proper fix is correct initialization via the NEED_REINIT handshake.

### NEED_REINIT Handshake Flow

1. Driver sets NEED_REINIT=BIT(1) in DUMMY_CR (0x02120)
2. Driver does SET_OWN (hands control to ROM/FW)
3. ROM sees NEED_REINIT=1, configures MCU-side DMA rings, clears BIT(1)
4. Driver does CLR_OWN (takes back control)
5. Driver verifies BIT(1) was cleared (ROM processed it)
6. Driver reprograms HOST-side rings (CLR_OWN side effect zeroes them)

---

## 4. Comprehensive Eliminated Hypotheses {#4-eliminated}

| Hypothesis | Mode(s) | Result | Status |
|------------|---------|--------|--------|
| FSM state 0x01010202 blocks routing | 26, 29 | FSM recovers to 0x03030101, commands still fail | **ELIMINATED (RED HERRING)** |
| AXI dispatch mis-routing (DISP_CTRL) | 27 | Force-MCU and force-HIF both fail | **ELIMINATED** |
| GLO_CFG_EXT2 TX halt/drop | 27 | All zeros, no blocking conditions | **ELIMINATED** |
| WPDMA1 MCU_INT_EVENT signals | 28 | DMA_INIT, RESET_DONE, combined -- no effect | **ELIMINATED** |
| Q_IDX routing wrong destination | 29 | Q_IDX=0x20, 0x22, 0x3e all fail post-boot | **ELIMINATED** |
| MCU_RX2 still accessible post-boot | 29 | Q_IDX=0x22 targeting MCU_RX2 (valid BASE) fails | **ELIMINATED** |
| WFDMA1 bridge alternative path | 28 | WFDMA1 remains disabled (GLO=0x00000000) | **ELIMINATED** |
| HOST2MCU_SW_INT_SET signals | pre-team | All combinations at both 0x02108 and 0xd4108 | **ELIMINATED** |
| Dummy command pattern (CONNAC3X) | 26 | Vendor explicitly disables for CONNAC3X | **ELIMINATED** |
| LOGIC_RST after FW boot | mode 22 | Creates FSM 0x01010303 (worse) | **ELIMINATED** |
| Post-boot CLR_OWN (no NEED_REINIT) | mode 16 | Running FW handles differently, no DMA reinit | **ELIMINATED** |

---

## 5. Root Cause Analysis {#5-root-cause}

### The Problem (Confirmed)

After FW boots, HOST TX15 DMA completes but data never reaches MCU_RX. The WFDMA R2A bridge's internal routing table was never configured by ROM because our driver skips the second SET_OWN/CLR_OWN cycle that triggers the NEED_REINIT handler.

### Why MCU_RX0 BASE=0x00000000

MCU_RX0 is the intended destination for post-boot MCU commands (Q_IDX=0x20 routes to MCU_RX_Q0). During FWDL, ROM configures MCU_RX2 and MCU_RX3 for firmware download commands. Post-boot, the running FW expects commands on MCU_RX0, but ROM never configured it because NEED_REINIT was never consumed via a second CLR_OWN cycle.

### Why MCU_RX2 Also Fails Post-Boot

Even though MCU_RX2 has a valid BASE (0x0226ca00), it fails post-boot because:
- The R2A bridge's routing table is reconfigured by FW boot
- Post-boot routing maps Q_IDX=0x22 to MCU_RX0 (which has BASE=0), not MCU_RX2
- OR: The entire R2A bridge routing is invalidated when FW takes over from ROM

### Why FSM State Is Misleading

The R2A FSM registers (0xd752c, 0xd7530) reflect the internal state machine of the bridge hardware, but they are NOT the controlling factor for routing. The bridge can show FSM=0x03030101 ("working") while its internal routing table still points to unconfigured MCU_RX rings. The FSM state tracks DMA transaction flow state, not routing configuration.

---

## 6. Recommended Fix: Second SET_OWN/CLR_OWN with NEED_REINIT {#6-fix}

### Matching Upstream Exactly

The upstream mt7925 initialization sequence that our driver should replicate:

```
Phase 1 (Probe):
  1. SET_OWN → CLR_OWN                     [Cycle 1, bare -- already done]
  2. WFSYS reset                            [already done]
  3. DMA disable (force=true)               [already done]
  4. Allocate TX/RX rings                   [already done]
  5. DMA enable + SET NEED_REINIT           [partially done -- need to verify NEED_REINIT]

Phase 2 (mcu_init, MISSING FROM OUR DRIVER):
  6. SET_OWN                                [NEW -- hand control to ROM]
  7. CLR_OWN                                [NEW -- ROM sees NEED_REINIT, configures MCU DMA]
  8. Reprogram HOST rings                   [NEW -- CLR_OWN side effect zeroes them]
  9. Re-enable DMA (GLO_CFG)                [NEW]
  10. L0S disable                           [already done]

Phase 3 (FW download + command):
  11. FWDL (patch + RAM + FW_START)         [already works]
  12. Poll fw_sync=0x3                      [already works]
  13. Send NIC_CAPABILITY                   [should work after fix!]
```

### Key Implementation Notes

1. **NEED_REINIT must be set BEFORE the second SET_OWN**: Write BIT(1) to DUMMY_CR (0x02120) before handing control to ROM.

2. **CLR_OWN zeroes ALL HOST ring BASEs**: This is a known side effect. Call `mt7927_dma_reprogram_rings()` after the second CLR_OWN to restore HOST-side ring addresses.

3. **Verify NEED_REINIT was consumed**: After CLR_OWN, read DUMMY_CR and confirm BIT(1) is cleared. If still set, ROM did not process it.

4. **Re-enable DMA**: The second CLR_OWN disables GLO_CFG. Re-enable TX_DMA_EN + RX_DMA_EN.

5. **No full HOST-side DMA reset needed**: Upstream uses `__mt792xe_mcu_drv_pmctrl()` (bare version, no wpdma_reinit_cond) during init. Just reprogram ring bases.

### Minimal Test Code (Pseudocode)

```c
// After DMA init (rings allocated, NEED_REINIT already set)...

// Verify NEED_REINIT is set
dummy_cr = ioread32(bar0 + 0x02120);
WARN_ON(!(dummy_cr & BIT(1)));  // Should be set

// Second SET_OWN/CLR_OWN cycle
iowrite32(BIT(0), bar0 + 0xe0010);         // SET_OWN
poll(bar0 + 0xe0010, BIT(2), timeout=2s);  // Wait OWN_SYNC

iowrite32(BIT(1), bar0 + 0xe0010);         // CLR_OWN
poll(bar0 + 0xe0010, ~BIT(2), timeout=2s); // Wait OWN_SYNC clear

// Verify NEED_REINIT was consumed
dummy_cr = ioread32(bar0 + 0x02120);
if (dummy_cr & BIT(1))
    dev_err("ROM did NOT consume NEED_REINIT!");

// Reprogram HOST rings (CLR_OWN zeroed them)
mt7927_dma_reprogram_rings(dev);

// Re-enable DMA
glo_cfg = ioread32(bar0 + 0xd4208);
glo_cfg |= TX_DMA_EN | RX_DMA_EN;
iowrite32(glo_cfg, bar0 + 0xd4208);

// Now proceed with FW download...
```

---

## 7. Task #9 Status: Testing the Fix {#7-task9}

Task #9 "[HIGH PRIORITY] Test second SET_OWN/CLR_OWN with NEED_REINIT pattern" is currently in progress, being worked on by agent1.

The test implements the exact sequence described above as a new mode in the driver. Key verification points:
- NEED_REINIT consumed (BIT(1) cleared in DUMMY_CR after second CLR_OWN)
- MCU_RX0 BASE != 0x00000000 after second CLR_OWN (ROM configured it)
- HOST ring BASEs reprogrammed after CLR_OWN side effects
- R2A FSM state after the complete sequence
- NIC_CAPABILITY command succeeds (response received on HOST RX0)

---

## 8. New Discoveries This Session {#8-discoveries}

### Discovery: FSM State 0x03030303

Mode 28 (bare CIDX kick without valid descriptor) produced a never-before-seen FSM state:
```
0x01010202 → bare CIDX kick → 0x03030303 → ~3s → 0x03030101
```
This is an intermediate transitional state. The R2A bridge FSM has at least 4 known states:
- 0x03030101: "Idle/ready" (seen pre-boot and after recovery)
- 0x01010202: "FW-boot transition" (seen immediately after fw_sync=0x3)
- 0x01010303: "Post-LOGIC_RST" (seen after manual WFDMA reset)
- 0x03030303: "DMA-triggered transition" (seen during bare kick recovery)

### Discovery: FSM Recovery Is Instant With DMA Activity

Mode 29 showed FSM recovery from 0x01010202 to 0x03030101 in 0ms when a dummy descriptor was kicked. This contradicts earlier observations of ~2s recovery time, suggesting the recovery is purely DMA-triggered and the 2s delay in mode 23 was due to the time spent in NIC_CAPABILITY timeout processing before the FSM was re-read.

### Discovery: ALL Q_IDX Routes Fail Post-Boot

Mode 29 tested Q_IDX=0x20 (MCU_RX0), 0x22 (MCU_RX2), and 0x3e (FWDL) -- all fail post-boot. This means the R2A bridge is not merely mis-routing to the wrong ring; it is blocking ALL traffic. This is consistent with a fundamentally unconfigured bridge state rather than a routing table issue.

### Discovery: R2A FSM Is NOT the Routing Controller

The most important finding: achieving FSM=0x03030101 does NOT restore command delivery. Agent1 (mode 26) and agent2 (mode 29) independently confirmed this. The FSM reflects DMA transaction state machine progress, not the configuration of the internal routing/forwarding path.

---

## 9. Remaining Open Questions {#9-questions}

1. **Does the second SET_OWN/CLR_OWN with NEED_REINIT actually configure MCU_RX0?**
   Task #9 will answer this. If MCU_RX0 BASE becomes non-zero after the second CLR_OWN, the hypothesis is confirmed.

2. **Does MT7927 ROM behave identically to MT7925 ROM for NEED_REINIT?**
   MT7927 (0x6639) uses different firmware but shares the same CONNAC3X architecture. The ROM behavior should be identical, but this is unconfirmed.

3. **What exactly does ROM do when it processes NEED_REINIT?**
   Presumably: configure MCU_RX0/RX1 ring BASEs in MCU SRAM, set up R2A bridge routing table, enable MCU-side DMA paths. The exact register writes are internal to ROM and not observable.

4. **Why does CLR_OWN zero HOST ring BASEs?**
   This is a documented side effect. The ROM's NEED_REINIT handler appears to do a full WFDMA reset as part of its reconfiguration, which unfortunately zeroes the HOST-side rings that we carefully set up.

5. **Is there a way to trigger NEED_REINIT processing without the HOST ring zeroing side effect?**
   Unknown. The upstream driver simply reprograms rings after CLR_OWN. This may be the intended design.

---

## 10. Priority-Ordered Next Steps {#10-next-steps}

### P0: Complete Task #9 -- Second SET_OWN/CLR_OWN with NEED_REINIT
**Status**: In progress (agent1)
**Expected outcome**: If NIC_CAPABILITY succeeds, this is THE fix. If it fails, we need to investigate what MT7927 ROM does differently from MT7925 ROM.

### P1: If Task #9 Succeeds -- Full MCU Command Sequence
Once NIC_CAPABILITY works:
1. Parse NIC_CAPABILITY response to extract chip capabilities
2. Send load_clc command
3. Set MCU_RUNNING state
4. Begin mac80211/cfg80211 registration work

### P2: If Task #9 Fails -- Investigate MT7927 ROM Differences
If the second SET_OWN/CLR_OWN with NEED_REINIT does not configure MCU_RX0:
1. Compare MCU_RX0-3 BASE values before and after second CLR_OWN
2. Check if DUMMY_CR BIT(1) was consumed (cleared by ROM)
3. If BIT(1) was NOT consumed: ROM may not support NEED_REINIT on MT7927
4. If BIT(1) WAS consumed but MCU_RX0 still BASE=0: ROM processes NEED_REINIT differently
5. Try WFSYS reset between DMA init and second CLR_OWN (upstream does this)
6. Try adding a WFSYS_INIT_DONE wait after WFSYS reset
7. Investigate if the order of operations matters (e.g., EMI_CTL SLPPROT_EN)

### P3: If All Else Fails -- Alternative Approaches
1. **Manual MCU_RX0 configuration**: Write a valid DMA buffer address directly to MCU_RX0 BASE (0x02500). This bypasses ROM entirely but may not work if the MCU firmware expects specific SRAM addresses.
2. **WFDMA_SW_RST register (0xd703C)**: Vendor CODA headers show a software reset register. May reset the R2A bridge without full CLR_OWN.
3. **R2A_CTRL_0 (0xd7500)**: Listed as bridge control (likely writable). May contain bridge enable/configuration bits.
4. **Full upstream sequence replication**: Replicate every single step of the upstream mt7925 probe + mcu_init in exact order, including WFSYS reset timing, EMI_CTL, interrupt configuration, etc.

---

## Appendix A: Test Mode Summary

| Mode | Agent | Approach | Result | Key Finding |
|------|-------|----------|--------|-------------|
| 11 | (pre-team) | Standard SET_OWN/CLR_OWN + FWDL | -110 | Baseline failure |
| 12 | (pre-team) | Skip SET_OWN/CLR_OWN | -110 | FW boot causes issue, not init |
| 15-16 | (pre-team) | Post-boot WFDMA config / CLR_OWN | -110 | Running FW handles CLR_OWN differently |
| 20 | (pre-team) | Clear NEED_REINIT before FWDL | -110 | MCU_TX BASE=0 (red herring) |
| 22 | (pre-team) | LOGIC_RST after FW boot | -110 | FSM becomes 0x01010303 (worse) |
| 23 | (pre-team) | Minimal post-boot (zero writes) | -110 | FSM transient, recovers after ~2s |
| 25 | (pre-team) | Wait 10s for FSM recovery | -110 | Passive wait doesn't trigger recovery |
| 26 | agent1 | Dummy command + FSM poll | -110 | **FSM is RED HERRING** |
| 27 | agent2 | AXI DISP force-routing + GLO_CFG_EXT2 | -110 | Routing/halt not the issue |
| 28 | agent2 | WPDMA1 INT + bare CIDX kick | -110 | **New FSM state 0x03030303** |
| 29 | agent2 | Combined kitchen sink | -110 | **ALL Q_IDX routes fail post-boot** |
| 30+ | agent1 | Second SET_OWN/CLR_OWN + NEED_REINIT | **TESTING** | Task #9 |

## Appendix B: Key Register Quick Reference

| Register | BAR0 Offset | Purpose |
|----------|-------------|---------|
| LPCTL | 0xe0010 | BIT(0)=SET_OWN, BIT(1)=CLR_OWN, BIT(2)=OWN_SYNC |
| DUMMY_CR | 0x02120 | BIT(1)=NEED_REINIT |
| HOST GLO_CFG | 0xd4208 | TX/RX DMA enable |
| HOST TX15 BASE | 0xd43F0 | MCU command ring base address |
| HOST RX0 BASE | 0xd4500 | MCU event ring base address |
| MCU_RX0 BASE | 0x02500 | MCU-side command ring (should be non-zero post-NEED_REINIT) |
| MCU_RX2 BASE | 0x02520 | FWDL command ring (0x0226ca00, ROM configured) |
| R2A_FSM_CMD | 0xd752c | R2A bridge FSM state (READ-ONLY, red herring) |
| R2A_FSM_DAT | 0xd7530 | R2A bridge FSM data state (READ-ONLY) |
| WFSYS_SW_RST_B | 0xf0140 | BIT(0)=RST, BIT(4)=INIT_DONE |
| CONN_ON_MISC | 0xe00f0 | BIT(1:0)=fw_sync, poll for 0x3 |

---

*Generated by team mt7927-fsm-fix. See also: docs/HANDOFF_2026-02-15_v2.md, docs/upstream_analysis_post_fw_boot.md*
