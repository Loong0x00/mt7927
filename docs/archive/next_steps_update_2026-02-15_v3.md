# MT7927 Driver Development - Consolidated Report v3
# Date: 2026-02-15 (Team Session 2: sharded-wibbling-shamir)
# Team: agent1 (hw test), agent2 (implementation), agent3 (code analysis)

---

## Executive Summary

Session 2 ran five parallel tasks: Mode 32 (HIF_SW_RST), Mode 33 (vendor prefetch), MCU_RX0 BASE polling, deep code analysis, and consolidation. Combined with session 1 findings, the investigation has now:

1. **MCU_RX0 vendor label is "FWDL" but upstream uses Q_IDX=0x20→MCU_RX0 for ALL commands** — vendor debug labels show MCU_DMA0_RX0="FWDL", but upstream mt7925 sends both FWDL AND post-boot commands with Q_IDX=0x20 (MCU_RX0). MCU_RX0 is the universal command ring. **MCU_RX0 BASE=0 after boot IS the problem.**
2. **Neither HOST driver writes MCU_RX BASEs** — searched all upstream mt76/ and vendor mt6639/. MCU-side DMA rings are configured by ROM (during NEED_REINIT) or by running FW.
3. **MCU_RX0 BASE=0 for 2+ seconds post-boot** — agent1's polling confirms FW does NOT configure MCU_RX0 after boot. This explains why all post-boot commands fail.
4. **DUMMY_CR=0xffff0003 post-boot** — BIT(0) AND BIT(1) both set. BIT(1)=NEED_REINIT re-armed by FW for next sleep/wake cycle.
5. **Our HYBRID prefetch config matches NEITHER reference** — vendor uses RX4-7 (mobile/AXI), upstream uses RX0-3 (PCIe). Our driver configures BOTH.
6. **Upstream mt7925 explicitly supports MT7927** — device ID 0x6639 in PCI table, confirming RX0-3 is correct for PCIe.
7. **CLR_OWN likely does NOT wipe HOST ring BASEs** — vendor code has ring reinit commented out ("FW bk/sr solution"), upstream relies on rings surviving.
8. **Mode 34 discovery: Bare CLR_OWN is a NO-OP on MT7927** — NEED_REINIT not consumed when only writing BIT(1) to LPCTL. MT7927 requires CONNINFRA wakeup + sleep protection disable before CLR_OWN for ROM/FW to process NEED_REINIT. Our Mode 11 does this pre-FWDL (and it works), but Mode 16's post-boot CLR_OWN was bare (and failed).

**Current understanding**: MCU_RX0 is the command ring for ALL Q_IDX=0x20 traffic. It's unconfigured (BASE=0) after boot because the running FW hasn't been triggered to set it up. FW re-sets NEED_REINIT (DUMMY_CR BIT(1)) after boot — this is FW requesting a reinit cycle. A post-boot HEAVY CLR_OWN (with wakeup+slpprot) should make the running FW process NEED_REINIT and configure MCU_RX0.

**OPEN QUESTION**: What is the status of MCU_RX1 (0x02510, the AP CMD ring) post-boot? The polling code monitors both, but we need to confirm MCU_RX1 values.

---

## Table of Contents

1. [Session 2 Task Results](#1-results)
2. [Critical Discovery: MCU Ring Purpose Mapping](#2-ring-mapping)
3. [Prefetch Configuration Analysis](#3-prefetch)
4. [CLR_OWN and NEED_REINIT Behavior](#4-clr-own)
5. [Updated Hypothesis Rankings](#5-hypotheses)
6. [Complete Test Mode Summary](#6-modes)
7. [Priority-Ordered Next Steps](#7-next-steps)
8. [Register Quick Reference](#8-registers)

---

## 1. Session 2 Task Results {#1-results}

### Task #1: Mode 32 — HIF_SW_RST (0xd703C) [agent1, COMPLETED]
**Approach**: Use WFDMA EXT_WRAP software reset register (0xd703C) to reset the R2A bridge post-boot and pre-FWDL.
**Key findings**:
- Phase A (post-boot): SW_RST applied, FSM state captured
- Phase B (pre-FWDL with NEED_REINIT): SET_OWN/CLR_OWN after SW_RST
- Result: -110 (NIC_CAPABILITY still fails)

### Task #2: Mode 33 — Pure Vendor Prefetch (RX4-7 only) [agent2, COMPLETED]
**Approach**: Use exact vendor mt6639 prefetch config (RX4-7 only, chain_en=0) to test whether FW auto-configures MCU_RX0 when it sees the mobile ring layout.
**Key findings**:
- Vendor prefetch applied (RX4-7 configured, RX0-3 zeroed)
- MCU_RX0 BASE still 0x00000000 after boot
- NIC_CAPABILITY: -110
- **Eliminated**: Vendor prefetch alone does not trigger MCU_RX0 config

### Task #3: MCU_RX0 BASE Polling [agent1, COMPLETED]
**CRITICAL DATA**:
- MCU_RX0 BASE (0x02500) stays **0x00000000** for full 2-second poll window after fw_sync=0x3
- ZERO changes observed — FW does NOT configure MCU_RX0 after boot
- DUMMY_CR = 0xffff0003 (BIT(0) + BIT(1) both set post-boot)
- **MCU_RX1 (0x02510) was also polled** — values need confirmation from agent1 logs

### Task #4: Deep Code Analysis [agent3, COMPLETED]
Full analysis: `docs/references/post_boot_mcu_rx_config.md`

Key findings:
1. **MCU_RX0 = FWDL ring** (vendor debug label "P0R0:FWDL")
2. **MCU_RX1 = AP CMD ring** (inferred from SOC5_0 pattern)
3. Neither upstream nor vendor HOST code writes MCU_RX BASEs
4. Vendor CONNAC3X WfdmaReInit has `halWpdmaInitRing()` COMMENTED OUT
5. Upstream mt7925 uses CSR_DISP_BASE_PTR_CHAIN_EN=1 (auto prefetch)
6. PCI device 0x6639 (MT7927) is in upstream mt7925_pci_device_table

### Task #6: Mode 34 — Full Upstream Init [agent2, IN PROGRESS]
**Approach**: Replicate exact upstream mt7925 init sequence including WFSYS reset timing.

---

## 2. Critical Discovery: MCU Ring Purpose Mapping {#2-ring-mapping}

### Source: `chips/mt6639/mt6639.c:388-391` (vendor debug groups)

```c
struct wfdma_group_info mt6639_wfmda_wm_rx_group[] = {
    {"P0R0:FWDL", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING0_CTRL0_ADDR},  // MCU_RX0
    {"P0R2:TXD0", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING2_CTRL0_ADDR},  // MCU_RX2
    {"P0R3:TXD1", WF_WFDMA_MCU_DMA0_WPDMA_RX_RING3_CTRL0_ADDR},  // MCU_RX3
};
// NOTE: MCU_RX1 absent from debug list but = AP CMD (from SOC5_0 pattern)
```

### MCU DMA0 RX Ring Mapping (MT6639/MT7927):
| Ring | Register | Label | Purpose | Status Post-Boot |
|------|----------|-------|---------|-----------------|
| MCU_RX0 | 0x02500 | FWDL | Firmware download commands | BASE=0 (EXPECTED, FWDL done) |
| MCU_RX1 | 0x02510 | AP CMD | Post-boot MCU commands | **UNKNOWN — needs check** |
| MCU_RX2 | 0x02520 | TXD0 | TX data from HOST | BASE=0x0226ca00 (ROM configured) |
| MCU_RX3 | 0x02530 | TXD1 | TX data from HOST | BASE=0x0226cc80 (ROM configured) |

### Implications:
- **MCU_RX0 BASE=0 after boot is NOT a bug** — it's the FWDL ring, no longer needed
- **MCU_RX1 is the actual command destination** — this needs to be non-zero for post-boot commands
- Our driver comment at line 10482 already documents this: `MCU_RX0 = FWDL ring (Q_IDX 0x20), MCU_RX1 = AP CMD (Q_IDX 0x21)`
- The polling code (lines 3766, 3781) monitors MCU_RX1 — we need those values

### Q_IDX Routing:
| Q_IDX | MCU RX Ring | Purpose |
|-------|-------------|---------|
| 0x20 | MCU_RX0 | FWDL commands (works pre-boot) |
| 0x21 | MCU_RX1 | AP CMD (post-boot commands) |
| 0x22 | MCU_RX2 | TX data 0 |
| 0x23 | MCU_RX3 | TX data 1 |

**CRITICAL**: Upstream mt7925 uses Q_IDX=0x20 for ALL MCU commands (both FWDL and post-boot). But on MT7927, if MCU_RX0=FWDL and MCU_RX1=CMD, then post-boot commands may need Q_IDX=0x21 instead!

However, the upstream mt7925 driver works with MT7927 (0x6639 in PCI table) using Q_IDX=0x20. This means either:
1. FW repurposes MCU_RX0 for CMD after boot (dual-purpose), OR
2. FW remaps Q_IDX=0x20 routing to MCU_RX1 after boot, OR
3. The ring mapping is different on PCIe vs mobile

---

## 3. Prefetch Configuration Analysis {#3-prefetch}

### Three Config Variants Compared:

| Ring | Vendor mt6639 (mobile) | Upstream mt7925 (PCIe) | Our Driver (HYBRID) |
|------|----------------------|----------------------|-------------------|
| RX0 | — | 0x00000004 (MCU events) | 0x00000004 |
| RX1 | — | 0x00400004 (TX done) | 0x00400004 |
| RX2 | — | 0x00800004 (Data) | 0x00800004 |
| RX3 | — | 0x00C00004 | 0x00C00004 |
| RX4 | 0x00000004 (Data) | — | 0x01000004 |
| RX5 | 0x00400004 (Data1) | — | 0x01400004 |
| RX6 | 0x00800004 (Events) | — | 0x01800004 |
| RX7 | 0x00C00004 (TXdone) | — | 0x01C00004 |
| chain_en | OFF | **ON** | **ON** |
| TX data depth | 0x4 | **0x10** | 0x4 |

### Key Differences:
1. **Vendor uses RX4-7** (mobile/AXI bus, ring offset starts at 4)
2. **Upstream uses RX0-3** (PCIe, standard ring offset)
3. **Our hybrid uses BOTH** — prefetch SRAM may overflow or confuse FW topology detection
4. **TX data prefetch depth**: Upstream uses 0x10 (16), vendor and ours use 0x4 (4)
5. **chain_en**: Upstream enables auto-mode, vendor disables it

### Mode 33 Result:
Pure vendor prefetch (RX4-7 only, chain_en=0) did NOT fix the problem. MCU_RX0 still BASE=0 post-boot.

### Recommendation for Mode 34:
Use pure upstream prefetch (RX0-3 only, chain_en=1, TX depth=0x10).

---

## 4. CLR_OWN and NEED_REINIT Behavior {#4-clr-own}

### What ROM Does During NEED_REINIT Processing:
From our observations and code analysis:
1. ROM sees NEED_REINIT=BIT(1) in DUMMY_CR
2. ROM configures MCU_RX2 (BASE=0x0226ca00) and MCU_RX3 (BASE=0x0226cc80) — FWDL rings
3. ROM does NOT configure MCU_RX0 or MCU_RX1
4. ROM clears NEED_REINIT BIT(1)
5. **ROM likely does NOT wipe HOST ring BASEs** (vendor code has ring reinit commented out)

### DUMMY_CR Value Timeline:
| State | DUMMY_CR | Meaning |
|-------|----------|---------|
| After DMA enable | 0xffff0002 | BIT(1)=NEED_REINIT set by driver |
| After CLR_OWN (ROM processed) | 0xffff0000 | ROM consumed NEED_REINIT |
| After FW boot (fw_sync=0x3) | 0xffff0003 | BIT(0)+BIT(1) set by FW |

### DUMMY_CR BIT(0):
- Set by running FW after boot
- Likely indicates "FW running" or "FW ready" state
- Vendor checks DUMMY_CR in `asicConnac3xWfdmaDummyCrRead` for reinit condition

### DUMMY_CR BIT(1) Re-Set by FW:
- FW re-arms NEED_REINIT after boot for the next sleep/wake cycle
- This is normal behavior — when driver later does SET_OWN/CLR_OWN for power management, ROM will process NEED_REINIT again

### HOST Ring Survival During CLR_OWN:
**Evidence that HOST rings are NOT wiped:**
1. Vendor `asicConnac3xWfdmaReInit` has `halWpdmaInitRing()` COMMENTED OUT (active code only resets TX software counters)
2. Upstream `mt7925e_mcu_init` does CLR_OWN then immediately uses TX16 for FWDL — if rings were wiped, FWDL would fail
3. Comment says "FW bk/sr solution" — FW handles backup/restore internally

**IMPORTANT**: Our earlier observation of HOST rings being zeroed was likely caused by our WFSYS_RST or LOGIC_RST, NOT by CLR_OWN itself. This changes the Mode 34 implementation — we may NOT need `mt7927_dma_reprogram_rings()` after CLR_OWN.

---

## 5. Updated Hypothesis Rankings {#5-hypotheses}

### ACTIVE Hypotheses (ordered by likelihood):

**H1 (HIGHEST): FW boot environment differs from upstream — FW doesn't configure MCU_RX0**
- Upstream: ROM processes NEED_REINIT pre-boot, then FW configures MCU_RX0 during FW boot
- Our driver: ROM processes NEED_REINIT OK, FW boots (fw_sync=0x3) but MCU_RX0 stays 0
- Possible causes: extra steps before DMA_INIT, vendor GLO_CFG bits, HOST ring layout mismatch
- FIX: Clean upstream-like init (P2) or load actual upstream mt7925 module (P0)

**H2 (HIGH): Extra steps between WFSYS_RST and DMA_INIT corrupt FW boot state**
- Our driver adds: 200ms slpprot hammering, drv_own, R2A bridge clear before DMA_INIT
- Upstream: ZERO extra steps between WFSYS_RST and DMA_INIT
- These may create WFDMA state that FW doesn't expect during boot
- FIX: Remove all extra steps (P2)

**H3 (MEDIUM): GLO_CFG vendor bits signal wrong mode to FW**
- BIT(9)=FW_DWLD_BYPASS_DMASHDL, BIT(20)=CSR_LBK_RX_Q_SEL_EN, BIT(26)=ADDR_EXT_EN
- Upstream doesn't set any of these
- FW may read GLO_CFG during boot and change behavior

**H4 (LOW): HOST ring allocation mismatch**
- Upstream: TX0, TX15, TX16, RX0(512), RX2
- Our: TX15, TX16, various RX (depends on settings)
- Missing TX0 or wrong RX allocation may confuse FW

### ELIMINATED Hypotheses (Modes 11-35):

**H_E15 (Mode 35): Post-boot HEAVY CLR_OWN triggers NEED_REINIT processing**
- FAILED: Running FW ignores NEED_REINIT on CLR_OWN (DUMMY_CR stays 0xffff0003)
- ROM processes NEED_REINIT; running FW does NOT. Completely different behavior.

**H_E14 (Mode 34): Bare CLR_OWN after upstream init processes NEED_REINIT**
- FAILED: Bare CLR_OWN without SET_OWN first is a NO-OP (no OWN_SYNC 1→0 transition)

**H_E5: Wrong Q_IDX for post-boot commands**
- RETRACTED: Upstream mt7925 uses Q_IDX=0x20 for ALL MCU commands (mcu.c:3484)
- Vendor "FWDL" label for MCU_RX0 is just a debug shorthand, not exclusive purpose

### ELIMINATED Hypotheses (DO NOT RETRY):
| # | Hypothesis | Evidence |
|---|-----------|----------|
| E1 | R2A FSM state controls routing | Modes 26, 29: FSM=0x03030101 still -110 |
| E2 | AXI dispatch mis-routing | Mode 27: force-MCU and force-HIF both fail |
| E3 | GLO_CFG_EXT2 TX halt/drop | Mode 27: all zeros |
| E4 | WPDMA1 MCU_INT_EVENT signals | Mode 28: no effect |
| E5 | Q_IDX routing to wrong ring | Mode 29: ALL Q_IDX fail (0x20, 0x22, 0x3e) |
| E6 | MCU_RX2 accessible post-boot | Mode 29: Q_IDX=0x22 fails even with valid BASE |
| E7 | WFDMA1 bridge alternative path | Mode 28: WFDMA1 disabled |
| E8 | HOST2MCU_SW_INT_SET signals | All combinations at both addresses |
| E9 | Dummy command pattern | Vendor explicitly disables for CONNAC3X |
| E10 | LOGIC_RST after boot | Mode 22: creates worse FSM state |
| E11 | Post-boot CLR_OWN (no NEED_REINIT) | Mode 16: running FW handles differently |
| E12 | MCU_TX BASE=0 blocking events | Pre-team: MCU events go via HOST RX, not MCU_TX |
| E13 | Vendor prefetch (RX4-7 only) | Mode 33: MCU_RX0 still 0, -110 |
| E14 | HIF_SW_RST bridge reset | Mode 32: -110 |

---

## 6. Complete Test Mode Summary {#6-modes}

| Mode | Session | Agent | Approach | Result | Key Finding |
|------|---------|-------|----------|--------|-------------|
| 11 | 1 | — | Standard SET_OWN/CLR_OWN + FWDL | -110 | Baseline failure |
| 12 | 1 | — | Skip SET_OWN/CLR_OWN | -110 | FW boot causes issue |
| 15-16 | 1 | — | Post-boot WFDMA config / CLR_OWN | -110 | Running FW handles CLR_OWN differently |
| 20 | 1 | — | Clear NEED_REINIT before FWDL | -110 | MCU_TX BASE=0 red herring |
| 22 | 1 | — | LOGIC_RST after boot | -110 | FSM 0x01010303 (worse) |
| 23 | 2 | — | Minimal post-boot (zero writes) | -110 | FSM transient, recovers after DMA |
| 25 | 2 | — | 10s FSM recovery wait | -110 | Passive wait fails |
| 26 | S1 | agent1 | Dummy command + FSM poll | -110 | **FSM is RED HERRING** |
| 27 | S1 | agent2 | AXI DISP force-routing | -110 | Routing not the issue |
| 28 | S1 | agent2 | WPDMA1 INT + bare CIDX kick | -110 | New FSM state 0x03030303 |
| 29 | S1 | agent2 | Combined kitchen sink | -110 | **ALL Q_IDX routes fail** |
| 30 | S1 | agent1 | Second SET_OWN/CLR_OWN + NEED_REINIT | -110 | NEED_REINIT consumed, MCU_RX0 still 0 |
| 32 | S2 | agent1 | HIF_SW_RST (0xd703C) | -110 | SW_RST insufficient |
| 33 | S2 | agent2 | Pure vendor prefetch (RX4-7) | -110 | Vendor prefetch alone insufficient |
| 34 | S2 | agent2 | Full upstream init + bare CLR_OWN | -110 | **Bare CLR_OWN is NO-OP** (no SET_OWN→CLR_OWN transition) |
| 35 | S2 | agent2 | Post-boot HEAVY CLR_OWN | -110 | **FW ignores NEED_REINIT** on SET_OWN→CLR_OWN. MCU_RX0 stays 0. |

---

## 7. Priority-Ordered Next Steps {#7-next-steps}

### Mode 35 Result: FAILED — FW ignores NEED_REINIT on post-boot CLR_OWN
**Tested**: Post-boot HEAVY CLR_OWN (SET_OWN→CLR_OWN cycle)
- CLR_OWN itself succeeds (OWN_SYNC 1→0 in ~10ms)
- But FW does NOT process NEED_REINIT — DUMMY_CR stays 0xffff0003
- MCU_RX0 BASE stays 0x00000000
- NIC_CAPABILITY: -110
**Conclusion**: ROM processes NEED_REINIT on CLR_OWN, but running FW does NOT.

### Revised Understanding (post-Mode 35):
Upstream's SET_OWN→CLR_OWN in `mt7925e_mcu_init()` (pci_mcu.c:38-42) happens **BEFORE** FW boot:
1. DMA_INIT sets HOST rings + NEED_REINIT
2. SET_OWN→CLR_OWN → **ROM** processes NEED_REINIT → configures MCU_RX2/RX3
3. FWDL uses those rings → FW_START → FW boots
4. **FW configures MCU_RX0 during its own boot sequence** (not via NEED_REINIT)
5. NIC_CAPABILITY works
On our driver, FW boots (fw_sync=0x3) but does NOT configure MCU_RX0.

### P0 (HIGHEST): Why doesn't FW configure MCU_RX0 on our hardware?
FW configures MCU_RX0 during boot on upstream but NOT on our driver. Possible causes:
1. **HOST ring layout mismatch** — upstream allocates TX0+TX15+TX16+RX0(512)+RX2. Our allocations may differ.
2. **Extra steps before DMA_INIT corrupt WFDMA state** — 200ms slpprot hammering, R2A bridge clear, drv_own.
3. **GLO_CFG vendor bits confuse FW** — BIT(9), BIT(20), BIT(26) that upstream doesn't set.
4. **Prefetch EXT_CTRL layout mismatch** — FW may read prefetch config during boot.
**Test**: Load actual upstream mt7925 module on MT7927 hardware to eliminate ALL init differences.

### P1: Manual MCU_RX0 Configuration (Bypass Approach)
**What**: Write a valid SRAM address to MCU_RX0 BASE (0x02500), set CNT and CIDX, then try NIC_CAPABILITY with Q_IDX=0x20
**Risk**: May need correct SRAM address in MCU SRAM range (0x0226xxxx)
**This bypasses FW and ROM entirely** — if it works, proves the ring config is the only issue

### P2: Clean upstream-like init (remove ALL extra steps)
**What**: New mode that exactly replicates upstream sequence: clean WFSYS_RST (4 lines) → IRQ setup → DMA_INIT → SET_OWN→CLR_OWN → FWDL → FW_START → NIC_CAPABILITY
**Key**: NO slpprot hammering, NO drv_own before DMA, NO R2A bridge clear, NO vendor GLO_CFG bits

### P3: Binary Diff of Init Register Writes
**What**: Capture every register write from upstream mt7925 during probe+init
**How**: Instrument upstream driver with MMIO tracing or compare Windows driver trace from Ghidra RE

---

## 8. Register Quick Reference {#8-registers}

### MCU DMA0 Ring Registers:
| Register | Offset | Purpose |
|----------|--------|---------|
| MCU_RX0 BASE | 0x02500 | FWDL ring base (0 after boot = normal) |
| MCU_RX0 CNT | 0x02504 | FWDL ring descriptor count |
| MCU_RX0 CIDX | 0x02508 | FWDL ring consumer index |
| MCU_RX0 DIDX | 0x0250c | FWDL ring DMA index |
| MCU_RX1 BASE | 0x02510 | **AP CMD ring base (KEY register)** |
| MCU_RX1 CNT | 0x02514 | AP CMD ring descriptor count |
| MCU_RX1 CIDX | 0x02518 | AP CMD ring consumer index |
| MCU_RX1 DIDX | 0x0251c | AP CMD ring DMA index |
| MCU_RX2 BASE | 0x02520 | TXD0 ring (ROM: 0x0226ca00) |
| MCU_RX3 BASE | 0x02530 | TXD1 ring (ROM: 0x0226cc80) |

### Key Control Registers:
| Register | Offset | Purpose |
|----------|--------|---------|
| LPCTL | 0xe0010 | BIT(0)=SET_OWN, BIT(1)=CLR_OWN, BIT(2)=OWN_SYNC |
| DUMMY_CR | 0x02120 | BIT(0)=FW_READY(?), BIT(1)=NEED_REINIT |
| WFSYS_SW_RST_B | 0x7c000140 | BIT(0)=RST, BIT(4)=INIT_DONE (mapped via bus2chip) |
| HOST GLO_CFG | 0xd4208 | BIT(15)=CHAIN_EN, TX/RX DMA enable |
| R2A FSM_CMD | 0xd752c | R2A bridge state (READ-ONLY, red herring) |
| MCU_CMD | 0xd41f0 | BIT(15)=WFSYS_INIT_DONE |
| CONN_ON_MISC | 0xe00f0 | BIT(1:0)=fw_sync, poll for 0x3 |

### Prefetch EXT_CTRL Registers:
| Register | Offset | Upstream Value | Purpose |
|----------|--------|---------------|---------|
| RX0_EXT_CTRL | 0xd4680 | 0x00000004 | MCU events ring prefetch |
| RX1_EXT_CTRL | 0xd4684 | 0x00400004 | TX done ring prefetch |
| RX2_EXT_CTRL | 0xd4688 | 0x00800004 | Data ring prefetch |
| RX3_EXT_CTRL | 0xd468c | 0x00C00004 | Unused |
| TX15_EXT_CTRL | 0xd463c | 0x05000004 | MCU CMD TX prefetch |
| TX16_EXT_CTRL | 0xd4640 | 0x05400004 | FWDL TX prefetch |

---

## Appendix A0: Extra Steps Analysis — Our Flow vs Upstream

### Side-by-side comparison (WFSYS_RST → DMA_INIT):

```
UPSTREAM mt7925 probe (pci.c:386-414):
  SET_OWN(386) → CLR_OWN(390)     ← BEFORE WFSYS_RST (moot, reset erases)
  → EMI_CTL_SLPPROT_EN(399)
  → WFSYS_RST(401)                ← Clean 4-line function (mt792x_dma.c:357-370)
  → IRQ setup(405-410)            ← Benign
  → DMA_INIT(414)                 ← HOST rings + prefetch + GLO + NEED_REINIT

OUR probe flow:
  → WFSYS_RST(4155)               ← 130+ lines with 200ms slpprot hammering
  → drv_own(12185)                 ← EXTRA: bare CLR_OWN (likely NO-OP)
  → R2A bridge clear(12202)        ← EXTRA: not in upstream
  → DMA_INIT(12207)                ← HOST rings + prefetch + GLO + NEED_REINIT(11764)
  → reinit_mode switch(12253)
```

### Extra steps assessment:
1. **drv_own (bare CLR_OWN)** — Likely NO-OP per Mode 34 discovery. Benign.
2. **R2A bridge clear** — Upstream never touches R2A during probe. Potentially disruptive.
3. **200ms slpprot hammering** — Upstream doesn't fight sleep protection at all. May affect ROM boot environment.
4. **None of these extra steps wipe HOST rings.** Ring wipe only occurs when ROM processes NEED_REINIT via proper SET_OWN→CLR_OWN.

### Vendor mt6639 comparison (NOT valid for PCIe):
Vendor mt6639 is mobile/AXI, uses CB_INFRA_RGU (not WFSYS_SW_RST), no LPCTL/CLR_OWN.
Correct PCIe comparison = upstream mt7925 = ZERO extra steps.

---

## Appendix A: Upstream mt7925 Full Init Sequence (Reference for Mode 34)

```
mt7925_pci_probe():
  1. pci_enable_device, pci_set_master
  2. mt76_mmio_init (BAR0 mapping)
  3. __mt792x_mcu_fw_pmctrl()     → SET_OWN (bare, NEED_REINIT not set)
  4. __mt792xe_mcu_drv_pmctrl()   → CLR_OWN (no reinit check)
  5. mt792x_wfsys_reset():
     - Clear BIT(0) at WFSYS_SW_RST_B
     - msleep(50)
     - Set BIT(0)
     - Poll BIT(4) = INIT_DONE (timeout 2s)
  6. mt7925_dma_init():
     a. mt792x_dma_disable(true)  → LOGIC_RST
     b. Allocate TX rings: 0 (data), 15 (MCU), 16 (FWDL)
     c. Allocate RX rings: 0 (MCU events, 512 desc), 2 (data)
     d. mt792x_dma_enable():
        - Write prefetch EXT_CTRL (RX0-3 + TX0-3,15,16)
        - GLO_CFG with CHAIN_EN=1
        - Set NEED_REINIT in DUMMY_CR
        - Enable interrupts
  7. mt7925_register_device() → queues async init_work

mt7925_init_work() [async]:
  → mt7925e_mcu_init():
    a. mt792xe_mcu_fw_pmctrl()     → SET_OWN
    b. __mt792xe_mcu_drv_pmctrl()  → CLR_OWN (ROM processes NEED_REINIT)
       ROM configures MCU_RX2/RX3 (FWDL), clears NEED_REINIT
    c. L0S_DIS: BIT(8) at 0x10194
    d. mt7925_run_firmware():
       - load_patch via TX16
       - load_ram via TX16 (6 regions, DL_MODE_ENCRYPT)
       - FW_START_OVERRIDE (option=1)
       - Poll fw_sync=0x3
       - mt7925_mcu_get_nic_capability() → TX15 → SUCCESS
       - load_clc
       - Set MCU_RUNNING
```

## Appendix B: Cross-Reference of Key Source Files

| File | Key Content |
|------|-------------|
| `mt76/mt7925/pci.c` | PCI probe, DMA init, PCI device table (includes 0x6639) |
| `mt76/mt7925/pci_mcu.c` | mcu_init: SET_OWN/CLR_OWN + run_firmware |
| `mt76/mt7925/mcu.c` | run_firmware, get_nic_capability |
| `mt76/mt792x_dma.c` | dma_enable/disable, prefetch, NEED_REINIT |
| `mt76/mt792x_core.c` | drv_pmctrl (CLR_OWN), fw_pmctrl (SET_OWN), load_firmware |
| `mt76/dma.c` | queue_alloc, sync_idx (writes BASE), queue_reset |
| `chips/mt6639/mt6639.c` | MCU DMA ring debug labels, prefetch config |
| `mt6639/chips/common/cmm_asic_connac3x.c` | WfdmaReInit (ring reinit commented out) |
| `tests/04_risky_ops/mt7927_init_dma.c` | Our driver (10500+ lines) |
| `docs/references/post_boot_mcu_rx_config.md` | Full prefetch analysis |

---

*Generated by team sharded-wibbling-shamir (agent3). See also: docs/HANDOFF_2026-02-15_v2.md, docs/references/post_boot_mcu_rx_config.md*
