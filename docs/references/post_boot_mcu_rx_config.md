# Post-Boot MCU_RX0 Configuration Analysis

## Date: 2026-02-15
## Author: agent3 (code analysis)

---

## 1. Executive Summary

Deep analysis of vendor mt6639, upstream mt7925, and our driver reveals **three critical differences** in WFDMA configuration that likely explain why post-boot MCU commands fail (-110):

1. **Ring number mapping**: Vendor uses HW rings 4-7 for HOST RX; upstream uses rings 0-2
2. **Prefetch mode**: Vendor uses MANUAL mode (chain_en=0); upstream uses AUTO mode (chain_en=1)
3. **Our driver uses a HYBRID config** that matches NEITHER reference implementation

**Key finding**: The upstream driver DOES support MT7927 (device ID 0x6639 in PCI table) using the mt7925 ring layout (RX0-3). This is strong evidence that RX0-3 prefetch is the correct approach for PCIe MT7927.

**New finding on CLR_OWN**: The vendor's CONNAC3X WfdmaReInit does NOT re-init HOST rings after CLR_OWN. This suggests the ROM's NEED_REINIT processing may NOT wipe HOST ring BASEs on CONNAC3X, contrary to our earlier assumption.

---

## 2. Vendor mt6639 Prefetch Analysis

### Source: `mt6639/chips/mt6639/mt6639.c:575-616`

```c
static void mt6639WfdmaManualPrefetch(struct GLUE_INFO *prGlueInfo)
{
    // Step 1: DISABLE auto-mode
    val &= ~WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN_MASK;

    // Step 2: RX rings 4-7 ONLY (starting val = 0x00000004)
    // RX4: 0x00000004 (base=0x0000, depth=4)
    // RX5: 0x00400004 (base=0x0040, depth=4)
    // RX6: 0x00800004 (base=0x0080, depth=4)
    // RX7: 0x00C00004 (base=0x00C0, depth=4)

    // Step 3: TX rings 0-2, 15-16
    // TX0:  0x01000004 (base=0x0100, depth=4)
    // TX1:  0x01400004 (base=0x0140, depth=4)
    // TX2:  0x01800004 (base=0x0180, depth=4)
    // TX15: 0x01C00004 (base=0x01C0, depth=4)
    // TX16: 0x02000004 (base=0x0200, depth=4)

    // Step 4: Reset DTX + DRX pointers
}
```

### Vendor Ring Mapping (mt6639SetRxRingHwAddr):
| SW Index | HW Ring | Purpose | Prefetch |
|----------|---------|---------|----------|
| RX_RING_DATA_IDX_0 | Ring 4 | Band0 Data RX | Yes |
| RX_RING_DATA1_IDX_2 | Ring 5 | Band1 Data RX | Yes |
| RX_RING_EVT_IDX_1 | **Ring 6** | **MCU Events** | Yes |
| RX_RING_TXDONE0_IDX_3 | Ring 7 | TX Done | Yes |

### Vendor IRQ mapping:
- rx_done_int_sts_4 through rx_done_int_sts_7

**KEY**: Vendor mt6639 is a **mobile/AXI** driver. The PCIe variant (MT7927) should follow upstream.

---

## 3. Upstream mt7925 Prefetch Analysis

### Source: `mt76/mt792x_dma.c:90-124`

```c
// For is_mt7925() branch:
// RX rings 0-3:
// RX0: PREFETCH(0x0000, 0x4) = 0x00000004  (MCU events)
// RX1: PREFETCH(0x0040, 0x4) = 0x00400004  (MCU WA/TX done)
// RX2: PREFETCH(0x0080, 0x4) = 0x00800004  (Data)
// RX3: PREFETCH(0x00c0, 0x4) = 0x00C00004  (unused)

// TX rings 0-3, 15, 16:
// TX0:  PREFETCH(0x0100, 0x10) = 0x01000010  (Data, depth 16!)
// TX1:  PREFETCH(0x0200, 0x10) = 0x02000010
// TX2:  PREFETCH(0x0300, 0x10) = 0x03000010
// TX3:  PREFETCH(0x0400, 0x10) = 0x04000010
// TX15: PREFETCH(0x0500, 0x4)  = 0x05000004  (MCU CMD)
// TX16: PREFETCH(0x0540, 0x4)  = 0x05400004  (FWDL)
```

### Upstream Ring Mapping:
| Queue | HW Ring | Purpose | Base Register |
|-------|---------|---------|---------------|
| MT_RXQ_MCU (q_rx[0]) | **Ring 0** | **MCU Events** | MT_WFDMA0(0x500) |
| MT_RXQ_MCU_WA (q_rx[1]) | Ring 1 | TX Done | MT_WFDMA0(0x510) |
| MT_RXQ_MAIN (q_rx[2]) | Ring 2 | Data RX | MT_WFDMA0(0x520) |
| MT_MCUQ_WM | Ring 15 | MCU CMD TX | MT_TX_RING_BASE + 15*0x10 |
| MT_MCUQ_FWDL | Ring 16 | FWDL TX | MT_TX_RING_BASE + 16*0x10 |

### Upstream IRQ mapping:
```c
.rx = {
    .data_complete_mask = HOST_RX_DONE_INT_ENA2,  // BIT(2) - Ring 2
    .wm_complete_mask = HOST_RX_DONE_INT_ENA0,    // BIT(0) - Ring 0
},
```

### Upstream GLO_CFG:
- **CSR_DISP_BASE_PTR_CHAIN_EN = 1** (auto-mode ENABLED alongside explicit EXT_CTRL values)

---

## 4. Our Driver's Current Config (HYBRID - PROBLEM!)

### Source: `tests/04_risky_ops/mt7927_init_dma.c`

Our `mt7927_wfdma_manual_prefetch()` configures **BOTH**:
- RX0-3 (upstream pattern) AND
- RX4-7 (vendor pattern)

This creates a **hybrid config that matches NEITHER reference**:
```
RX0: 0x00000004  ← upstream
RX1: 0x00400004  ← upstream
RX2: 0x00800004  ← upstream
RX3: 0x00C00004  ← upstream
RX4: 0x01000004  ← vendor
RX5: 0x01400004  ← vendor
RX6: 0x01800004  ← vendor
RX7: 0x01C00004  ← vendor
TX0-2, TX15-16   ← continuing sequence
```

**PROBLEM**: The WFDMA prefetch SRAM has limited space. Configuring 8 RX rings + 5 TX rings may overflow the prefetch SRAM buffer, or the booting FW may read the prefetch config and get confused by the hybrid layout.

---

## 5. Critical Finding: CLR_OWN May NOT Wipe HOST Ring BASEs

### Evidence from vendor code (`cmm_asic_connac3x.c:334-381`):

```c
void asicConnac3xWfdmaReInit(struct ADAPTER *prAdapter)
{
    asicConnac3xWfdmaDummyCrRead(prAdapter, &fgResult);
    if (fgResult) {
        // ORIGINAL CODE (commented out):
        //   halWpdmaInitRing(prAdapter->prGlueInfo, false);
        // This WOULD re-init HOST rings, but it's DISABLED

        // ACTIVE CODE: Only resets TX software indices
        for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
            TxRing[u4Idx].TxSwUsedIdx = 0;
            TxRing[u4Idx].u4UsedCnt = 0;
            TxRing[u4Idx].TxCpuIdx = 0;
        }
        // Comment says: "FW bk/sr solution" (backup/restore)
        asicConnac3xWfdmaDummyCrWrite(prAdapter); // re-set NEED_REINIT
    }
}
```

The vendor's CONNAC3X reinit does NOT re-init HOST ring BASEs after CLR_OWN. It only resets TX software counters. The comment "FW bk/sr solution" implies the FW handles backup/restore of ring state.

### Evidence from upstream code:

In `mt7925e_mcu_init`:
1. SET_OWN → CLR_OWN (ROM processes NEED_REINIT)
2. `mt7925_run_firmware()` immediately uses TX16 ring to download FW

If CLR_OWN wiped TX16 BASE, FW download would fail. Since the upstream driver works on MT7927 (device 0x6639 is in the PCI table), **HOST ring BASEs must survive CLR_OWN + NEED_REINIT**.

### Implication:
Our earlier observation that "CLR_OWN zeroes ALL HOST ring BASEs" was likely caused by something else in our init sequence (possibly our own LOGIC_RST or WFSYS_RST), not by CLR_OWN's NEED_REINIT processing itself.

---

## 6. Upstream Full Init Sequence for MT7927

```
1. mt7925_pci_probe:
   a. __mt792x_mcu_fw_pmctrl()     → SET_OWN
   b. __mt792xe_mcu_drv_pmctrl()   → CLR_OWN (NEED_REINIT not set yet - no-op)
   c. mt792x_wfsys_reset()          → Full WF subsystem reset
   d. mt7925_dma_init():
      - Allocate TX queues (ring 0, 15, 16)
      - Allocate RX queues (ring 0=MCU events, ring 2=data)
      - mt792x_dma_enable():
        * Write prefetch EXT_CTRL for RX0-3, TX0-3, TX15, TX16
        * Enable GLO_CFG with CSR_DISP_BASE_PTR_CHAIN_EN=1 (auto-mode!)
        * Enable TX+RX DMA
        * Set NEED_REINIT in DUMMY_CR
        * Enable interrupts

2. mt7925_init_work (async):
   → mt7925e_mcu_init():
     a. mt792xe_mcu_fw_pmctrl()     → SET_OWN
     b. __mt792xe_mcu_drv_pmctrl()  → CLR_OWN (NEED_REINIT IS set)
        ROM processes NEED_REINIT:
        - Configures MCU_RX2/RX3 for FWDL
        - Clears NEED_REINIT bit
        - Does NOT wipe HOST ring BASEs
     c. mt7925_run_firmware():
        - mt76_connac2_load_patch() via TX16
        - mt76_connac2_load_ram() via TX16
        - Poll MT_TOP_MISC2_FW_N9_RDY (fw_sync)
        → FW now running, has configured MCU_RX0
     d. mt7925_mcu_get_nic_capability() via TX15 → MCU_RX0 → SUCCESS
```

**CRITICAL**: Between steps 2b and 2c, FW downloads succeed, proving HOST ring BASEs survive CLR_OWN. After FW boot (fw_sync ready), MCU commands via TX15 work immediately, proving FW configures MCU_RX0 during its boot.

---

## 7. What Triggers FW to Configure MCU_RX0?

Based on the analysis, FW configures MCU_RX0 automatically during its boot process. The question is: **does FW read HOST prefetch EXT_CTRL to determine the WFDMA topology?**

### Evidence FOR this hypothesis:
1. Vendor and upstream use completely different HOST RX ring numbers (4-7 vs 0-2)
2. FW must know which HOST ring to route events to
3. The only visible difference between the two configs is the prefetch EXT_CTRL values
4. Our hybrid config (both RX0-3 AND RX4-7) matches neither → FW confused

### Evidence AGAINST:
1. No explicit code found where FW reads HOST prefetch registers
2. FW could use a compile-time constant or ROM table for the mapping

### Most likely mechanism:
The FW reads the HOST RX prefetch EXT_CTRL registers (0xd4680-0xd469c) during boot to determine which HOST RX rings are active. It then configures MCU_RX rings to route to the appropriate HOST rings.

With our hybrid config, FW may:
- See both RX0-3 AND RX4-7 configured → ambiguous
- Default to one set but configure routing for the other → mismatch
- Fail to configure MCU_RX0 at all → our observed failure

---

## 8. Recommendations

### Immediate Action (for Mode 33 / agent2):
**Use pure upstream mt7925 prefetch for MT7927 PCIe:**
- RX0-3 ONLY, RX4-7 zeroed
- CSR_DISP_BASE_PTR_CHAIN_EN = 1 (auto-mode)
- TX depths: 0x10 for data, 0x4 for MCU/FWDL
- HOST RX ring 0 = MCU events (with proper BASE + descriptors)
- HOST RX ring 2 = Data
- Interrupts: HOST_RX_DONE_INT_ENA0 (ring 0) + HOST_RX_DONE_INT_ENA2 (ring 2)

### Validation: What needs to change in our driver:
1. **Remove RX4-7 prefetch** (clear to 0)
2. **Enable CSR_DISP_BASE_PTR_CHAIN_EN** (currently we clear it)
3. **Use TX depths of 0x10** for data rings (currently 0x4)
4. **Remove hybrid dummy rings** for RX4-7
5. **Verify HOST RX0 ring has proper BASE + descriptors before CLR_OWN**

### Stop investigating:
- CLR_OWN HOST ring wipe (likely not happening)
- R2A FSM states (confirmed red herring in session 5)
- MCU_TX rings (MCU events flow via HOST RX, not MCU_TX)
- DISP_CTRL force routing (normal behavior is correct)

### Verify our assumption:
- After FW boot, read HOST RX0 EXT_CTRL - did FW change it?
- Read MCU_RX0 BASE (0x02500) - did FW set it?
- These two reads will confirm/deny the prefetch-based routing hypothesis

---

## 9. Register Reference

### HOST DMA0 EXT_CTRL Register Format (32-bit):
```
[31:16] DISP_BASE_PTR - Base pointer in prefetch SRAM (unit: 4 bytes)
[7:0]   DISP_MAX_CNT  - Max descriptor count for prefetch
```

### Key Register Addresses (BAR-relative):
| Register | Address | Description |
|----------|---------|-------------|
| HOST_RX0_EXT_CTRL | 0xd4680 | RX ring 0 prefetch config |
| HOST_RX1_EXT_CTRL | 0xd4684 | RX ring 1 prefetch config |
| HOST_RX2_EXT_CTRL | 0xd4688 | RX ring 2 prefetch config |
| HOST_RX3_EXT_CTRL | 0xd468c | RX ring 3 prefetch config |
| HOST_RX4_EXT_CTRL | 0xd4690 | RX ring 4 prefetch config |
| HOST_RX5_EXT_CTRL | 0xd4694 | RX ring 5 prefetch config |
| HOST_RX6_EXT_CTRL | 0xd4698 | RX ring 6 prefetch config |
| HOST_RX7_EXT_CTRL | 0xd469c | RX ring 7 prefetch config |
| HOST_GLO_CFG | 0xd4208 | BIT(15)=CSR_DISP_BASE_PTR_CHAIN_EN |
| MCU_RX0_BASE | 0x02500 | MCU DMA0 RX ring 0 base |
| MCU_RX1_BASE | 0x02510 | MCU DMA0 RX ring 1 base |
| WFDMA_DUMMY_CR | 0x02120 | BIT(1)=NEED_REINIT |
| GLO_CFG_EXT1 | 0xd42b4 | BIT(28) set by upstream mt7925 |
| INT_RX_PRI | 0xd4298 | Upstream sets 0x0F00 |
| INT_TX_PRI | 0xd429c | Upstream sets 0x7F00 |

---

## 10. Additional Upstream mt7925 Config

Beyond prefetch, upstream `mt792x_dma_enable` sets for mt7925:

```c
// GLO_CFG_EXT1 (0xd42b4): Set BIT(28)
mt76_rmw(dev, MT_UWFDMA0_GLO_CFG_EXT1, BIT(28), BIT(28));

// INT_RX_PRI (0xd4298): Set bits [11:8] = 0xF
mt76_set(dev, MT_WFDMA0_INT_RX_PRI, 0x0F00);

// INT_TX_PRI (0xd429c): Set bits [14:8] = 0x7F
mt76_set(dev, MT_WFDMA0_INT_TX_PRI, 0x7F00);
```

### Upstream GLO_CFG flags set:
```
MT_WFDMA0_GLO_CFG_TX_WB_DDONE
MT_WFDMA0_GLO_CFG_FIFO_LITTLE_ENDIAN
MT_WFDMA0_GLO_CFG_CLK_GAT_DIS
MT_WFDMA0_GLO_CFG_OMIT_TX_INFO
MT_WFDMA0_GLO_CFG_DMA_SIZE = 3
MT_WFDMA0_GLO_CFG_FIFO_DIS_CHECK
MT_WFDMA0_GLO_CFG_RX_WB_DDONE
MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN  ← CRITICAL: auto-mode ON
MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2
MT_WFDMA0_GLO_CFG_TX_DMA_EN
MT_WFDMA0_GLO_CFG_RX_DMA_EN
```
