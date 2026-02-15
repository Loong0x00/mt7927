# MT7927 Driver Session Status - 2026-02-15

## Last Updated: Session 10-11 (Mode 50-53, WFSYS reset ELIMINATED, ring_rx0 discovery)

## FUNDAMENTAL INSIGHT
**MT7927 = MT6639 mobile chip in PCIe package. NOT from MT76 family.**
- PCI ID: 14c3:6639 — the chip IS MT6639
- MediaTek never published a Linux WiFi driver for MT7927
- mt6639/ (Motorola Android GitHub) is the CORRECT reference — same chip, AXI bus
- mt76/mt7925 is WRONG reference — different chip architecture
- "Falcon can't reset with WF_WHOLE_PATH_RST[0]" — vendor comment explains everything

## Current Blocker Summary
POST-BOOT MCU commands (NIC_CAPABILITY) fail with -110 timeout.
FW download works perfectly (fw_sync=0x3). MCU_RX0 BASE=0x00000000 — FW never configures MCU DMA RX ring 0.
Ring 15 (MCU cmd TX) NOT consumed by WFDMA (DIDX doesn't advance) without DMASHDL bypass.

**ALL reset-based hypotheses ELIMINATED (Modes 40-52). WFSYS reset is NOT the solution.**

## CRITICAL DISCOVERY: 0xe0010 = LPCTL (NOT CONN_INFRA reset!)
- Steps 14-16 in ToggleWfsysRst write to BAR0+0xe0010 = MT_CONN_ON_LPCTL
- BIT(0) = SET_OWN (put device to sleep), BIT(1) = CLR_OWN (wake), BIT(2) = OWN_SYNC
- This is the SAME register we use for SET_OWN/CLR_OWN power management!
- ToggleWfsysRst = CB_INFRA_RGU + SET_OWN/CLR_OWN only, NO separate CONN_INFRA reset exists
- Mode 40 already tested this equivalently; Mode 52 just added a redundant SET_OWN/CLR_OWN

## KEY FINDING: ring_rx0 Breaks FWDL But Changes WFDMA Routing
- Adding HOST RX ring 0 (128 entries, HW ring 0) causes Patch download -110 timeout
- **WITH ring_rx0**: Ring 15 DIDX=1 (command CONSUMED by WFDMA!)
- **WITHOUT ring_rx0**: Ring 15 DIDX=0 (command NOT consumed)
- This proves HOST RX ring 0 configuration affects WFDMA TX routing
- Conditional fix: ring_rx0 only allocated for mode=53 (commit 7668e5f)

## REMAINING HYPOTHESES
1. **FW needs specific HOST ring configuration to activate MCU_RX0** — ring_rx0 changes routing
2. **Windows driver never writes MCU_RX0** — FW auto-configures it, but what triggers FW?
3. **Only 0xd6060 |= 0x10101 (DMASHDL)** written between fw_sync=0x3 and first MCU cmd

## ELIMINATED: CB_INFRA_RGU WFSYS Reset (Modes 40, 52)

### Mode 40: CB_INFRA_RGU BIT(4) Reset — FIRST TEST INCONCLUSIVE
- Branch: experiment/mode40-cb-infra-rgu
- CB_INFRA_RGU reset SUCCEEDED: ROMCODE_INDEX reached 0x1D1E (MCU_IDLE) ✓
- But FWDL failed (-110) due to residual device state from prior mode41 test
- **NEEDS REBOOT + RETEST** with improved sequence

### Ghidra RE: Complete Windows ToggleWfsysRst (16 steps) — CORRECTED
Source: `docs/references/ghidra_post_fw_init.md` (agent4, mtkwecx.sys v5603998)
```
 1. READ  0x7c011100, OR BIT(1), WRITE          — Wake CONN_INFRA
 2. Pre-reset helper:
    a. READ  0x7c001600, AND ~0xF, WRITE         — Clear sleep protection
    b. READ  0x7c001620, AND 0x3, WRITE if !=0   — Clear HIF status
    c. READ  0x7c001630, AND 0x3, WRITE if !=0   — Clear HIF status
 3. Driver own check
 4. WRITE 0x81023f00 = 0xc0000100                — Pre-reset MCU register
 5. WRITE 0x81023008 = 0                          — Pre-reset MCU register
 6. READ  0x70028600                              — Check current state
 7. WRITE 0x70028600 |= BIT(4)                   — ASSERT WFSYS RESET
 8. Sleep 1000 (units unclear: µs or ms)
 9. READ  0x70028600 → verify BIT(4) set         — Up to 5 retries
10. Sleep 20000 (units unclear: µs or ms)
11. WRITE 0x70028600 &= ~BIT(4)                  — DEASSERT WFSYS RESET
12. Sleep 200
13. Poll  0x81021604 == 0x1d1e                   — ROMCODE_INDEX = MCU_IDLE
14. WRITE 0x7c060010 = BIT(0)                    — *** ACTUALLY SET_OWN (LPCTL) ***
15. Poll  0x7c060010 BIT(2)                      — *** ACTUALLY OWN_SYNC poll ***
16. WRITE 0x7c060010 = BIT(1)                    — *** ACTUALLY CLR_OWN ***
```
**CORRECTION**: Steps 14-16 are NOT "CONN_INFRA init". 0x7c060010 = BAR0+0xe0010 = LPCTL.
BIT(0)=SET_OWN, BIT(2)=OWN_SYNC, BIT(1)=CLR_OWN. This is just a power cycle after reset.

### Ghidra RE: PostFwDownloadInit (after FW boot, before MCU commands)
```
1. WRITE 0x7c026060 |= 0x10101  (BAR0 0xd6060, DMASHDL) — CRITICAL, only reg write before MCU cmds!
2. MCU cmd class=0x8a
3. MCU cmd class=0x02
4. MCU cmd class=0xc0 (data: {0x820cc800, 0x3c200})
5. MT6639/MT7927-SPECIFIC: MCU cmd (0x28, 0xed) — ONLY for chip IDs 0x6639/0x738/0x7927
```

### BAR0 Address Map for Key Registers
| Bus Address | BAR0 Offset | Register | Accessible? |
|-------------|-------------|----------|-------------|
| 0x70028600 | 0x1f8600 | CB_INFRA_RGU WF_SUBSYS_RST | ✓ Direct |
| 0x81021604 | 0x0c1604 | ROMCODE_INDEX (MCU_IDLE=0x1D1E) | ✓ Direct |
| 0x7c026060 | 0xd6060 | DMASHDL enable | ✓ Direct |
| 0x7c024208 | 0xd4208 | WFDMA HOST GLO_CFG | ✓ Direct |
| 0x81023f00 | 0x0c3f00 | Pre-reset MCU reg | ✓ Direct (agent4) |
| 0x81023008 | 0x0c3008 | Pre-reset MCU reg | ✓ Direct (agent4) |
| 0x7c001600 | 0xf1600 | Sleep protection | ✓ Within remap |
| 0x7c060010 | 0xe0010 | **LPCTL (SET_OWN/CLR_OWN)** — NOT WFSYS_SW_INIT_DONE! | ✓ Direct |
| 0x7c011100 | ??? | CONN_INFRA wakeup | ✗ May need remap |
| 0x70028610 | 0x1f8610 | CB_INFRA_RGU BT_RST | ✓ Validated from PCIe |
| 0x70002510 | 0x1e2510 | BT_MISC (RST_DONE) | ✓ Validated from PCIe |

### Windows PostFwDownloadInit (agent2, Task #5)
- Function at VA 0x1401d4e00 in mtkwecx.sys
- DMASHDL reg 0xd6060 |= 0x10101
- MCU cmd 0xed with payload {0x820cc800, 0x0003c200}
- MT6639/MT7927-SPECIFIC MCU cmd (only for chip IDs 0x6639, 0x738, 0x7927)
- Upstream mt76 has NO equivalent — completely missing

## CRITICAL NEW FINDING: TXD Format Mismatch (Agent4 Deep RE)

### Q_IDX Difference
- **Windows**: TXD[0] = total_len | **0x41000000** → Q_IDX=**0x20** (MT_TX_MCU_PORT_RX_Q0), PKT_FMT=2
- **Our driver**: Q_IDX=**2** (mcu_rx_qidx default) — COMPLETELY DIFFERENT routing target
- Windows uses Q_IDX=0x20 for BOTH legacy and CONNAC3 UniCmd paths — ALWAYS

### BIT(31) LONG_FORMAT Difference
- **Windows**: TXD[1] = flags | 0x4000 — NEVER sets BIT(31)
- **Our driver**: Sets MT_TXD1_LONG_FORMAT = BIT(31) by default

### TXD Header Format (Windows Legacy Path, used during PostFwDownloadInit)
```
TXD[0] = total_len | 0x41000000    (Q_IDX=0x20, PKT_FMT=2)
TXD[1] = 0x00004000                (HDR_FORMAT_V3=1, NO LONG_FORMAT)
TXD[2..7] = 0
+0x20: payload_len + 0x20          (packet length field)
+0x22: pq_id
+0x24: class (0x8a, 0x02, etc.)
+0x25: 0xa0 (pkt_type)
+0x27: sequence number
+0x40: payload data
```

### PostFwDownloadInit Full MCU Command Sequence (9 commands)
All use target=0xed:
1. `class=0x8a` — NIC capability query (no payload)
2. `class=0x02` — Config {tag=1, pad=0, value=0x70000} (12 bytes)
3. `class=0xc0` — Config {0x820cc800, 0x3c200} (8 bytes, seq_ctl=1, flags=8)
4. `class=0xed` — DownloadBufferBin (optional, subcmd=0x21, file chunks)
5. `class=0x28` — DBDC setting (MT6639/MT7927 ONLY, 0x24 byte payload)
6. 1ms stall
7. `class=0xca` — PassiveToActiveScan (0x148 bytes)
8. `class=0xca` — FWChipConfig (0x148 bytes)
9. `class=0xca` — LogLevelConfig (0x148 bytes)

### MCU Dispatch Flag
- `ctx+0x146e621`: Controls legacy (0) vs CONNAC3 UniCmd (1) path
- PostFwDownloadInit clears `ctx+0x146e61c` (different flag!)
- WoWLAN resume clears flag_146e621 to 0 BEFORE ToggleWfsysRst
- **During PostFwDownloadInit**: likely uses GENERIC/legacy path (flag=0)

### Implications
- Previous Q_IDX=0x20 tests failed with WF_WHOLE_PATH_RST — but with CB_INFRA_RGU, MCU_RX0 may actually be configured by FW
- Both Q_IDX fix AND BIT(31) removal needed to match Windows exactly
- The 0x40-byte legacy header format is what FW expects during initial PostFwDownloadInit

## Planned Mode 40 Retest (After Reboot)
Updated sequence with ALL Ghidra RE findings:
1. SET_OWN → CLR_OWN (normal power cycle)
2. Reprogram HOST rings
3. Full CB_INFRA_RGU reset (matching Windows 16-step ToggleWfsysRst):
   - Pre-reset: sleep protection clear (0xf1600), HIF status (0xf1620/0xf1630)
   - Pre-reset MCU: 0x0c3f00=0xc0000100, 0x0c3008=0
   - Assert BIT(4) at 0x1f8600, sleep 1ms
   - Verify BIT(4), sleep 20ms
   - Deassert BIT(4), sleep 200µs
   - Poll ROMCODE_INDEX (0x0c1604) for 0x1D1E
   - CONN_INFRA reset: 0xe0010 BIT(0)→poll BIT(2)→BIT(1)
4. Reprogram HOST rings (CLR_OWN wipes them)
5. FWDL (patch + RAM)
6. Poll fw_sync=0x3
7. **DMASHDL 0xd6060 |= 0x10101** (from Ghidra RE)
8. Read MCU_RX0 BASE — KEY METRIC
9. Send NIC_CAPABILITY with **Q_IDX=0x20, no BIT(31)** (from Ghidra RE)
10. If success → implement full PostFwDownloadInit (9 MCU commands)

## Mode 40 First Test Results (INCONCLUSIVE — FWDL failed)
| Register | Value | Notes |
|----------|-------|-------|
| CB_INFRA_RGU (0x1f8600) | 0x00010340 | Reset completed |
| ROMCODE_INDEX (0x0c1604) | 0x00001d1e | MCU_IDLE ✓ |
| WFSYS_SW_RST (0xf0140) | 0x00000011 | RST_B=1, INIT_DONE=1 |
| DUMMY_CR | 0xffff0002 | NEED_REINIT=1 (not consumed — FWDL failed) |
| MCU_RX0 BASE | 0x00000000 | Not configured (FW never booted) |
| CONN_ON_MISC | 0x00000001 | fw_sync=1 (FW never completed boot) |
| BT_RESET_REG (0x1f8610) | 0x000103c0 | BT idle state — bus2chip mapping validated |
| BT_MISC (0x1e2510) | 0x00000100 | BIT(8)=RST_DONE — BT hardware functional |

## Mode 41: PCIe Config — ELIMINATED
- MT_PCIE_MAC_INT_ENABLE was ALREADY 0xFFFFFFFF
- MT_PCIE_MAC_PM L0S_DIS set but FW modified during boot
- NIC_CAPABILITY=-110, MCU_RX0 BASE=0
- Branch: experiment/mode41-pcie-config

## Eliminated Hypotheses (ALL modes 11-53)
- NEED_REINIT flag (Mode 39), R2A FSM (Modes 26, 29 — red herring)
- Direct MCU_RX0 write (Mode 37), RX drain (Mode 38/45)
- DISP_CTRL, HOST2MCU_SW_INT_SET, Q_IDX, HIF_SW_RST (Mode 32)
- Vendor prefetch (Mode 33), PCIe config (Mode 41 — already correct)
- Post-boot CLR_OWN (Mode 35), Windows regs (Mode 19), GLO_CFG_EXT2
- Bypass residuals (Mode 42), vendor init order (Mode 43/44)
- **DMASHDL bypass (Mode 46/47)** — ring 15 consumed but MCU doesn't respond
- **Full vendor DMASHDL config (Mode 50)** — ring 15 STILL blocked
- **Second CLR_OWN after NEED_REINIT=1 (Mode 51)** — ROM doesn't configure MCU_RX0
- **CB_INFRA_RGU WFSYS reset (Mode 40, 52)** — FWDL works but MCU_RX0 still 0
- **"CONN_INFRA reset" at 0xe0010 (Mode 52)** — was actually LPCTL SET_OWN, broke FWDL

## File References
- Main driver: `tests/04_risky_ops/mt7927_init_dma.c`
- Build: `make tests`, sudo pw: `123456`
- Vendor ref (PRIMARY): `mt6639/` (Motorola Android driver)
- Upstream ref (secondary): `mt76/mt7925/`
- Windows RE: `DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/`
- Ghidra: `ghidra_12.0.3_PUBLIC/`
- BT drivers: `linux-driver-mediatek-mt7927-bluetooth-main/` (USB, working)

## Analysis Documents
| Doc | Agent | Summary |
|-----|-------|---------|
| docs/references/init_sequence_comparison.md | a2c9f6d | Our init vs upstream — no fundamental diff |
| docs/references/vendor_post_boot_analysis.md | a512bcf | Vendor uses CB_INFRA_RGU, not WF_WHOLE_PATH_RST |
| docs/references/cb_infra_rgu_access.md | agent1 | BAR0 access path confirmed: 0x1f8600 |
| docs/references/bt_driver_analysis.md | agent3 | BT uses USB, CB_INFRA_RGU at 0x70028610 |
| docs/references/windows_post_fw_init.md | agent2 | PostFwDownloadInit: DMASHDL + MCU cmds |
| docs/references/ghidra_post_fw_init.md | agent4 | Full 16-step ToggleWfsysRst + PostFwDownloadInit |
| docs/references/community_research.md | agent3 | No one has MT7927 WiFi on Linux; we're most advanced |

## Team Status (Session 7: mt7927-session7) — ALL COMPLETE
- **agent1**: Shutdown — CB_INFRA_RGU research (Task #1)
- **agent2**: Shutdown — PostFwDownloadInit RE (Task #5) + community research (Task #7)
- **agent3**: Shutdown — Mode 40 impl (Task #2) + BT analysis (Task #4) + Mode 41 (Task #3)
- **agent4**: Shutdown — Ghidra RE (Task #6): full ToggleWfsysRst + PostFwDownloadInit

## Session 8-9 Progress (2026-02-15, continued)

### FWDL Regression — FIXED
- Mode 40 additions broke cold-boot FWDL (-110 on patch download)
- Root cause: GLO_CFG under-configured in DMA init
- Fix: GLO_CFG two-phase init, INT_STA clearing, GLO_CFG_EXT1 BIT(28), MSI config

### Mode 42-45: ELIMINATED
- Bypass residuals (42), vendor init order (43/44), RX drain (45) — none fixed MCU_RX0

### Mode 46/47: KEY BREAKTHROUGH
**DMASHDL bypass makes ring 15 work:**
- Without bypass: ring 15 descriptor NOT consumed (DIDX stuck)
- With bypass: ring 15 descriptor CONSUMED (DIDX advances)
- But MCU still doesn't respond in either case

**Two independent blockers confirmed:**
1. DMASHDL blocks ring 15 without bypass — needs proper configuration
2. MCU doesn't respond even when packet consumed — separate issue

### Mode 48-49: Enhanced Diagnostics
- Mode 48: DMASHDL bypass + HOST2MCU interrupt + all RX ring dump
- Mode 49: Read HOST RX ring 6 during FWDL phase — confirmed MCU events go to ring 6

### Mode 50: Full Vendor DMASHDL Config — ELIMINATED
- Replicated exact Windows DMASHDL register values
- Ring 15 STILL blocked (DIDX=0), MCU_RX0 still 0x00000000
- DMASHDL configuration is NOT the blocker

### Mode 51: Second CLR_OWN After NEED_REINIT=1 — ELIMINATED
- Hypothesis: ROM configures MCU_RX0 during CLR_OWN when NEED_REINIT=1
- Result: MCU_RX0 still 0x00000000 regardless of NEED_REINIT flag
- Also disrupts device state (fw_sync drops from 0x3 to 0x1)
- mt7925 analysis does NOT apply to MT7927/MT6639

### Mode 52: Full ToggleWfsysRst (16 steps) — ELIMINATED
- Implemented complete Windows ToggleWfsysRst sequence
- CB_INFRA_RGU reset works (ROMCODE_INDEX=0x1D1E after 34 polls) ✓
- BUT steps 14-16 write SET_OWN to LPCTL → puts device to sleep → FWDL fails
- **Root cause**: 0xe0010 = LPCTL, research agent misidentified as CONN_INFRA reset
- On clean reboot (mode 40 equivalent): FWDL succeeds but MCU_RX0 still 0

### Mode 53: HOST RX Ring 0 — BREAKS FWDL (Important Clue!)
- Added ring_rx0 (128 entries at HW ring 0) for MCU WM events
- **Unexpected**: Adding ring_rx0 causes Patch download -110 on ALL modes
- **Critical observation**: With ring_rx0, ring 15 DIDX=1 (consumed!), without = DIDX=0
- HOST RX ring 0 configuration CHANGES WFDMA TX ring routing
- Fix: ring_rx0 conditional on reinit_mode==53 (commit 7668e5f)
- **Needs reboot to test on clean state**

## Session 10-11 Key Architecture Insights

### Windows Driver MCU DMA Init
- Windows NEVER writes MCU_RX0/RX1 registers — FW auto-configures them
- Only register write between fw_sync=0x3 and first MCU cmd: `0xd6060 |= 0x10101` (DMASHDL enable)
- HOST2MCU_SW_INT_SET only used for SER (system error recovery), NOT normal init

### HOST RX Ring 0 vs MT7925
- mt7925 allocates HOST RX ring 0 (512 entries, MCU WM events) BEFORE FWDL
- Our driver never had HOST RX ring 0 — only rings 4,5,6,7
- MT6639 Android maps MCU event to ring 6, mt7925 to ring 0 — chip variant difference
- Adding ring_rx0 to our driver breaks FWDL but changes ring 15 behavior (consumed vs not)

### WFDMA Routing Behavior Change
- **Without ring_rx0**: Ring 15 DIDX=0 (MCU cmd NOT consumed by WFDMA)
- **With ring_rx0**: Ring 15 DIDX=1 (MCU cmd consumed) but MCU never responds
- This suggests WFDMA dispatch logic checks HOST RX ring 0 configuration
- Ring 0 presence may enable TX→MCU routing, but something else prevents MCU from responding

## Next Steps After Reboot
1. `insmod reinit_mode=0` — verify FWDL works without ring_rx0 (baseline)
2. `rmmod + insmod reinit_mode=53` — confirm ring_rx0 breaks FWDL on clean state
3. Investigate WHY ring_rx0 changes WFDMA routing — this is the strongest clue yet
4. Consider: maybe ring_rx0 needs DIFFERENT configuration (size, offset, prefetch)?
