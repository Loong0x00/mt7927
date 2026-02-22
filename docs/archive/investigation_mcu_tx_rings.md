# MCU TX Rings BASE=0 Investigation Plan

Date: 2026-02-15
Blocker: MCU_TX0-3 BASE=0x00000000, MCU→HOST response path broken.

---

## Task 1 [HIGH] — Read HOST RX6 Events

**Goal**: Dump the 12 events FW wrote to HOST RX6 during FWDL (DIDX=12, CIDX=11).

**Why**: These events may contain initialization info (TX resource allocation, NIC capability
report, etc.) that the driver MUST process before FW will configure MCU TX rings for
post-boot communication. Upstream mt7925 processes early events like `WM_CONNAC3_MCU_INIT`
and capability reports — if we're silently dropping them, FW may be stuck waiting.

**How**:
1. After FW boot (fw_sync=0x3), read HOST RX6 ring descriptor entries 0..11
2. For each descriptor: dump DMA_INFO (token, buf_len), physical address, then read
   the actual payload buffer content
3. Parse TXD header to identify event type (CID/EID), sequence, payload
4. Log all 12 events in detail to dmesg

**Key registers**:
- RX6 BASE: `0xd4560` (ring base address)
- RX6 CNT:  `0xd4564`
- RX6 CIDX: `0xd4568`
- RX6 DIDX: `0xd456c`
- Descriptor stride: 12 bytes (3 x u32) for CONNAC3X RXWI

**Expected outcome**: Understand what FW is telling the host during early boot. May reveal
required handshake step.

---

## Task 2 [HIGH] — Study Upstream mt7925 Post-Boot Sequence

**Goal**: Map the exact sequence of operations upstream mt7925 performs between firmware
download completion and first successful MCU command.

**Why**: mt7925 is the closest supported relative (same CONNAC3X generation). Its post-boot
flow must include whatever step configures MCU TX rings. We need to find and replicate it.

**Key files to study**:
- `mt76/mt7925/pci_mcu.c` — `mt7925e_mcu_init()` (PCIe-specific MCU init)
- `mt76/mt7925/mcu.c` — `mt7925_mcu_init()` / `mt7925_run_firmware()`
- `mt76/mt792x_dma.c` — DMA init / ring config post-boot
- `mt76/mt76_connac_mcu.c` — shared MCU command infrastructure
- `mt76/dma.c` — generic DMA ring operations

**Questions to answer**:
1. When/how do MCU TX rings get their BASE addresses? Does FW set them, or does the host?
2. Is there a drv_pmctrl / fw_pmctrl power management handshake after FWDL?
3. Does upstream process RX events between FW_START and first MCU command?
4. What is the exact ordering: FWDL → ??? → mcu_init → first command?
5. Is there a WFDMA reinit or ring reconfiguration step after FW boots?

**Expected outcome**: Identify the missing step(s) in our init sequence.

---

## Task 3 [MEDIUM] — FWDL-Stage vs Post-Boot Response Ring Routing

**Goal**: Determine if post-boot MCU events route to a different ring than FWDL events.

**Why**: Vendor mobile driver (mt6639) switches `rx_event_port` after FW_START:
- Before FW_START: events on `RX_RING_EVT_IDX_0` (possibly ring 0 or ring 6)
- After FW_START: events on `WFDMA1_RX_RING_IDX_1`

Our driver may be polling the wrong ring for post-boot events.

**How**:
1. Check mt6639 `connac3x_rx_event_port` logic and what triggers the switch
2. Check upstream mt7925 — which ring receives post-boot events?
3. Check if MCU_DMA1 (0x03208, currently GLO=0x00000000) needs to be enabled
4. After FW boot, dump DIDX of ALL RX rings (RX0-RX7) to see which advanced

**Expected outcome**: Identify correct post-boot event ring. Possibly need MCU_DMA1 enable.

---

## Task 4 [MEDIUM] — MCU_RX0/RX1 Configuration

**Goal**: Determine if HOST must configure MCU_RX0/RX1 for post-boot command path.

**Why**: Currently MCU_RX0 and MCU_RX1 have BASE=0x00000000, while MCU_RX2/RX3 (used
during FWDL) are configured by ROM with SRAM addresses. Post-boot commands might need
to go through MCU_RX0/RX1 instead of RX2/RX3, and FW may wait for RX0/RX1 to be
configured before setting up the corresponding TX0/TX1.

**How**:
1. Check upstream: does mt7925 host write to MCU_RX0/RX1?
2. Check vendor: does mt6639 configure MCU DMA RX rings from host side?
3. Check Windows RE: any writes to 0x02500/0x02510 (MCU_RX0/RX1)?
4. If yes, determine correct SRAM addresses and configure them

**Key registers**:
- MCU_RX0 BASE: `0x02500`, CNT: `0x02504`, CIDX: `0x02508`, DIDX: `0x0250c`
- MCU_RX1 BASE: `0x02510`, CNT: `0x02514`, CIDX: `0x02518`, DIDX: `0x0251c`

**Expected outcome**: Determine if MCU_RX0/RX1 config is the missing prerequisite.

---

## Task 5 [MEDIUM] — Windows WFDMA Reconfig After FW Boot

**Goal**: Identify any WFDMA register writes the Windows driver performs after fw_sync=0x3
that we're missing.

**Why**: Windows driver writes MSI_INT_CFG0-3 and other WFDMA config registers after
firmware is ready. These may control MCU→HOST response routing or interrupt delivery.

**How**:
1. Read `docs/win_v5705275_dma_lowlevel.md` for post-boot WFDMA config details
2. Look for MSI_INT_CFG writes, interrupt routing setup, additional GLO_CFG bits
3. Compare our register state with expected Windows post-boot state
4. Implement any missing writes

**Key registers** (suspected):
- MSI_INT_CFG0: TBD from Windows RE docs
- Other WFDMA config written after fw_sync

**Expected outcome**: List of missing register writes for post-boot WFDMA config.

---

## Task 6 [LOW] — Direct SRAM Write of MCU TX Ring BASEs

**Goal**: As a last resort, try writing SRAM addresses directly to MCU_TX0 BASE.

**Why**: MCU TX rings normally point to MCU-internal SRAM. If we can guess/find correct
addresses, we could configure them from the host side. This is risky and non-standard.

**How**:
1. MCU_RX2 BASE = 0x0226ca00 (known good SRAM address set by ROM)
2. Try similar SRAM range for MCU_TX0 BASE (e.g., nearby offsets)
3. Write BASE, set reasonable CNT, CIDX=0, DIDX=0
4. Send MCU command and check if response appears

**Risk**: Writing wrong SRAM address could corrupt MCU memory / crash FW.

**Expected outcome**: If successful, proves MCU TX rings just need addressing. More likely
to reveal that the problem is elsewhere.

---

## Progress Log

| Task | Status | Result |
|------|--------|--------|
| 1    | TODO   |        |
| 2    | TODO   |        |
| 3    | TODO   |        |
| 4    | TODO   |        |
| 5    | TODO   |        |
| 6    | TODO   |        |
