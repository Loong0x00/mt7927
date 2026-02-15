# Upstream & Reference Driver Analysis: Post-FW-Boot Initialization

**Author:** agent3 (code analyst)
**Date:** 2026-02-15
**Purpose:** Comprehensive analysis of all available reference driver source code to identify the correct post-FW-boot initialization sequence for MT7927 (MT6639/CONNAC3X).

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [MT7927 Device Confirmation](#2-mt7927-device-confirmation)
3. [Upstream mt7925 Full Init Sequence](#3-upstream-mt7925-full-init-sequence)
4. [Key Discovery: Two SET_OWN/CLR_OWN Cycles](#4-key-discovery-two-set_ownclr_own-cycles)
5. [NEED_REINIT Handshake Mechanism](#5-need_reinit-handshake-mechanism)
6. [Post-FW-Boot MCU Command Flow](#6-post-fw-boot-mcu-command-flow)
7. [DMA Ring Layout Comparison](#7-dma-ring-layout-comparison)
8. [R2A Bridge Register Definitions](#8-r2a-bridge-register-definitions)
9. [Vendor mt6639 WFDMA ReInit Pattern](#9-vendor-mt6639-wfdma-reinit-pattern)
10. [Resume/Deep Sleep Recovery Flow](#10-resumedeep-sleep-recovery-flow)
11. [Windows Driver RE Notes](#11-windows-driver-re-notes)
12. [Specific Recommendations for agent1 and agent2](#12-specific-recommendations)
13. [Appendix: Key Register Addresses](#13-appendix-key-register-addresses)
14. [Vendor mt6639 Post-FW-Boot Sequence (Detailed)](#14-vendor-mt6639-post-fw-boot-sequence-detailed)

---

## 1. Executive Summary

**Root cause hypothesis:** Our driver performs only ONE SET_OWN/CLR_OWN cycle (in mcu_init, immediately before FW download). The upstream mt7925 driver performs TWO cycles:

1. **Probe cycle** (line 386-392 of `pci.c`): A bare SET_OWN/CLR_OWN before WFSYS reset, purely to establish driver ownership.
2. **mcu_init cycle** (line 38-43 of `pci_mcu.c`): Another SET_OWN/CLR_OWN **after** `dma_enable()` has set NEED_REINIT=1 in the DUMMY_CR register. This cycle triggers the ROM/FW to reconfigure MCU-side DMA rings.

The second cycle is critical because `mt7925_dma_init()` (which calls `mt792x_dma_enable()`) runs between the two cycles. `dma_enable()` sets `MT_WFDMA_NEED_REINIT` (BIT(1)) in the DUMMY_CR register. When the second CLR_OWN happens, the ROM sees NEED_REINIT=1 and reconfigures the WFDMA bridge and MCU-side rings.

**Our driver's mistake:** We do only one SET_OWN/CLR_OWN cycle, and it happens before DMA init. By the time we download FW and send MCU commands, the WFDMA R2A bridge has not been properly configured by the ROM's NEED_REINIT handler.

**Recommended fix:** Add a second SET_OWN/CLR_OWN cycle (with NEED_REINIT already set by dma_enable) that matches the upstream mcu_init sequence. Alternatively, manually set NEED_REINIT before the first CLR_OWN if only one cycle is used.

---

## 2. MT7927 Device Confirmation

MT7927 (PCI ID 0x6639) is explicitly supported by the upstream mt7925 driver:

```c
// mt76/mt7925/pci.c:14-21
static const struct pci_device_id mt7925_pci_device_table[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x7925),
        .driver_data = (kernel_ulong_t)MT7925_FIRMWARE_WM },
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x6639),                    // <-- MT7927
        .driver_data = (kernel_ulong_t)MT7927_FIRMWARE_WM },
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x0717),
        .driver_data = (kernel_ulong_t)MT7925_FIRMWARE_WM },
    { },
};
```

Both MT7925 and MT7927 use the same CONNAC3X architecture, same driver code paths, same register layouts. The only difference is the firmware binary name (MT7927_FIRMWARE_WM vs MT7925_FIRMWARE_WM).

---

## 3. Upstream mt7925 Full Init Sequence

The complete probe-to-first-MCU-command sequence, traced through the code:

### Phase 1: PCI Probe (`mt7925_pci_probe` in `pci.c:271-432`)

```
1. pcim_enable_device(pdev)
2. pcim_iomap_regions(pdev, BIT(0))
3. pci_set_master(pdev)
4. pci_alloc_irq_vectors(pdev, 1, 1)
5. dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))
6. mt76_alloc_device()
7. mt76_mmio_init()
8. ** __mt792x_mcu_fw_pmctrl(dev)  -->  SET_OWN (1st cycle)  **
9. ** __mt792xe_mcu_drv_pmctrl(dev)  -->  CLR_OWN (1st cycle, bare)  **
10. Read CHIPID + REV
11. Set EMI_CTL SLPPROT_EN=1
12. ** mt792x_wfsys_reset(dev)  -->  WFSYS reset (clear BIT(0)@0x7c000140, wait 50ms, set BIT(0), poll INIT_DONE) **
13. Disable host IRQ enable register
14. Set MT_PCIE_MAC_INT_ENABLE = 0xff
15. devm_request_irq()
16. ** mt7925_dma_init(dev)  -->  See Phase 2 **
17. mt7925_register_device(dev)  -->  Schedules init_work
```

### Phase 2: DMA Init (`mt7925_dma_init` in `pci.c:215-269`)

```
1. mt76_dma_attach()
2. mt792x_dma_disable(dev, true)  -->  force=true: LOGIC_RST + DMASHDL_RST
3. Allocate TX Ring 0 (data) at MT_TX_RING_BASE (0xd4300)
4. Allocate TX Ring 15 (MCU WM) at MT_TX_RING_BASE
5. Allocate TX Ring 16 (FWDL) at MT_TX_RING_BASE
6. Allocate RX Ring 0 (MCU events) at MT_RX_EVENT_RING_BASE (0xd4500)
7. Allocate RX Ring 2 (data) at MT_RX_DATA_RING_BASE (0xd4500)
8. mt76_init_queues() --> NAPI setup
9. ** mt792x_dma_enable(dev)  -->  See Phase 2a **
```

### Phase 2a: DMA Enable (`mt792x_dma_enable` in `mt792x_dma.c:126-170`)

```
1. mt792x_dma_prefetch(dev)  -->  Prefetch config
2. Reset DTX_PTR + DRX_PTR indices
3. Set delay interrupt = 0
4. Configure GLO_CFG: TX_WB_DDONE, LITTLE_ENDIAN, CLK_GAT_DIS, OMIT_TX_INFO,
                      DMA_SIZE=3, FIFO_DIS_CHECK, RX_WB_DDONE, CSR_DISP_BASE_PTR_CHAIN_EN,
                      OMIT_RX_INFO_PFET2
5. Enable TX_DMA_EN + RX_DMA_EN in GLO_CFG
6. (mt7925 specific) Set UWFDMA0_GLO_CFG_EXT1 BIT(28), INT_RX_PRI=0x0F00, INT_TX_PRI=0x7F00
7. ** mt76_set(dev, MT_WFDMA_DUMMY_CR, MT_WFDMA_NEED_REINIT)  -->  CRITICAL: Sets BIT(1) **
8. Enable interrupts for TX/RX rings + MCU_CMD
9. Enable MCU2HOST_SW_INT_ENA for WAKE_RX_PCIE
```

**KEY LINE (step 7):** This sets NEED_REINIT=BIT(1) in the DUMMY_CR (0x54000120, mapped via fixed_map). This flag persists and will be seen by the ROM during the NEXT CLR_OWN cycle.

### Phase 3: MCU Init (`mt7925e_mcu_init` in `pci_mcu.c:27-53`)

Called from init_work -> mt7925_init_hardware -> mt792x_mcu_init -> hif_ops.mcu_init:

```
1. Set mcu_ops (headroom, send_msg, parse_response)
2. ** mt792xe_mcu_fw_pmctrl(dev)  -->  SET_OWN (2nd cycle) **
3. ** __mt792xe_mcu_drv_pmctrl(dev)  -->  CLR_OWN (2nd cycle, ROM sees NEED_REINIT=1!) **
4. Disable L0S: MT_PCIE_MAC_PM L0S_DIS=1
5. ** mt7925_run_firmware(dev)  -->  See Phase 4 **
6. Cleanup FWDL queue
```

**KEY INSIGHT (steps 2-3):** This is the SECOND SET_OWN/CLR_OWN cycle. At this point, NEED_REINIT=1 has already been set by `dma_enable()` in Phase 2a. When CLR_OWN fires, the ROM processes NEED_REINIT and reconfigures MCU-side DMA rings.

### Phase 4: Firmware Load & Run (`mt7925_run_firmware` in `mcu.c:1045-1063`)

```
1. mt792x_load_firmware(dev):
   a. mt76_connac2_load_patch()  -->  patch_sem, init_dl, scatter, patch_finish, sem_release
   b. mt76_connac2_load_ram()  -->  per-region: init_dl + scatter, then FW_START_OVERRIDE(option=1)
   c. Poll fw_sync=0x3 at MT_CONN_ON_MISC (0x7c0600f0)
2. mt7925_mcu_get_nic_capability(dev)  -->  ** FIRST MCU command after FW boot **
3. mt7925_load_clc(dev)
4. Set MT76_STATE_MCU_RUNNING
5. mt7925_mcu_fw_log_2_host(dev, 1)
```

**KEY OBSERVATION:** The first MCU command (`NIC_CAPABILITY`) is sent IMMEDIATELY after fw_sync=0x3 with NO additional register writes, NO additional SET_OWN/CLR_OWN, NO bridge configuration. The bridge must already be correctly configured before FW download begins.

---

## 4. Key Discovery: Two SET_OWN/CLR_OWN Cycles

### Cycle 1: Probe (pci.c:386-392)

```c
ret = __mt792x_mcu_fw_pmctrl(dev);   // SET_OWN: BIT(0) -> LPCTL, poll OWN_SYNC=BIT(2)
if (ret)
    goto err_free_dev;

ret = __mt792xe_mcu_drv_pmctrl(dev);  // CLR_OWN: BIT(1) -> LPCTL, poll OWN_SYNC=0
if (ret)
    goto err_free_dev;
```

**Purpose:** Establish initial driver ownership. At this point, NEED_REINIT is NOT set (fresh boot), so ROM does a basic CLR_OWN without DMA reinit.

**What happens between Cycle 1 and Cycle 2:**
- WFSYS reset (clears WF subsystem, waits for INIT_DONE)
- DMA init (allocates all ring buffers, writes BASE/CNT to hardware)
- DMA enable (**sets NEED_REINIT=BIT(1) in DUMMY_CR**)
- IRQ setup
- Device registration (schedules init_work)

### Cycle 2: mcu_init (pci_mcu.c:38-43)

```c
err = mt792xe_mcu_fw_pmctrl(dev);     // SET_OWN: BIT(0) -> LPCTL, poll OWN_SYNC=BIT(2)
if (err)
    return err;

err = __mt792xe_mcu_drv_pmctrl(dev);  // CLR_OWN: BIT(1) -> LPCTL, poll OWN_SYNC=0
if (err)                               // ROM SEES NEED_REINIT=1 --> configures MCU DMA!
    return err;
```

**Purpose:** This is the CRITICAL cycle. NEED_REINIT=1 tells the ROM to set up MCU-side DMA ring configurations that enable the WFDMA R2A bridge to route commands from HOST TX15 to MCU RX.

### Why Our Driver Fails

Our driver (`mt7927_init_dma.c`) does approximately:
1. SET_OWN/CLR_OWN (one cycle)
2. WFSYS reset
3. DMA init + enable (sets NEED_REINIT)
4. FW download
5. FW_START
6. Send MCU command (fails - R2A bridge broken)

We never do the SECOND SET_OWN/CLR_OWN after step 3. The ROM never sees NEED_REINIT=1, so MCU-side DMA is never configured.

---

## 5. NEED_REINIT Handshake Mechanism

### Setting NEED_REINIT

```c
// mt792x_dma.c:158 (inside mt792x_dma_enable)
mt76_set(dev, MT_WFDMA_DUMMY_CR, MT_WFDMA_NEED_REINIT);
```

Register details:
- `MT_WFDMA_DUMMY_CR` = 0x54000120 (virtual), mapped via fixed_map to BAR0 offset
- `MT_WFDMA_NEED_REINIT` = BIT(1)

### Consuming NEED_REINIT

After CLR_OWN, the driver checks if NEED_REINIT was consumed by ROM:

```c
// mt792x_dma.c:227-251
int mt792x_wpdma_reinit_cond(struct mt792x_dev *dev)
{
    /* check if the wpdma must be reinitialized */
    if (mt792x_dma_need_reinit(dev)) {          // Reads DUMMY_CR, checks BIT(1) CLEARED
        mt76_wr(dev, dev->irq_map->host_irq_enable, 0);
        mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0x0);
        err = mt792x_wpdma_reset(dev, false);   // Full HOST-side DMA reset
        mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0xff);
        pm->stats.lp_wake++;
    }
    return 0;
}
```

**Flow:**
1. Driver sets NEED_REINIT=1 in DUMMY_CR
2. SET_OWN (hand control to ROM/FW)
3. ROM/FW sees NEED_REINIT=1, configures MCU-side DMA rings, clears BIT(1)
4. CLR_OWN (driver takes back control)
5. Driver checks: if BIT(1) was cleared, it means ROM processed it -> do HOST-side DMA reset too

### Where mt792x_wpdma_reinit_cond Is Called

In the full upstream driver, it is called in:
1. `mt792xe_mcu_drv_pmctrl()` (`mt792x_core.c:888`): After every CLR_OWN during normal runtime (PM wake)
2. `_mt7925_pci_resume()` (`pci.c:548`): During resume from suspend

Note: `__mt792xe_mcu_drv_pmctrl()` (bare, with double underscore prefix) does NOT call `wpdma_reinit_cond`. The mcu_init cycle uses this bare version. This means during init, the ROM processes NEED_REINIT internally but the driver does NOT do a full HOST-side DMA reset after the second CLR_OWN. The DMA was already freshly initialized.

---

## 6. Post-FW-Boot MCU Command Flow

### Command Construction (mt7925_mcu_fill_message, mcu.c:3470-3548)

MCU commands are constructed with a TXD (TX descriptor) containing:
- `Q_IDX = MT_TX_MCU_PORT_RX_Q0 = 0x20`
- PKT_TYPE = MCU command type (UNI_CMD, etc.)
- SEQ number for response matching

The command is sent via TX Ring 15 (MT7925_TXQ_MCU_WM = 15).

### First Command After FW Boot

```c
// mcu.c:1045-1063
int mt7925_run_firmware(struct mt792x_dev *dev)
{
    err = mt792x_load_firmware(dev);    // Loads patch + RAM, FW_START, poll fw_sync=0x3
    err = mt7925_mcu_get_nic_capability(dev);  // FIRST command - NIC_CAPABILITY query
    err = mt7925_load_clc(dev);
    set_bit(MT76_STATE_MCU_RUNNING, &dev->mphy.state);
    return mt7925_mcu_fw_log_2_host(dev, 1);
}
```

There is NO special bridge configuration, NO additional register writes, NO delays between fw_sync=0x3 and the first MCU command. The expectation is that the WFDMA R2A bridge is already correctly configured before FW download even starts.

---

## 7. DMA Ring Layout Comparison

### Upstream mt7925 (PCIe)

| Ring | Index | Purpose | Base Register |
|------|-------|---------|---------------|
| TX0  | 0     | Data    | 0xd4300+0x00  |
| TX15 | 15    | MCU WM  | 0xd4300+0xF0  |
| TX16 | 16    | FWDL    | 0xd4300+0x100 |
| RX0  | 0     | MCU Events | 0xd4500+0x00 |
| RX2  | 2     | Data    | 0xd4500+0x20  |

Source: `mt7925/pci.c:215-269`, `mt7925/mt7925.h:115-127`

### Vendor mt6639 (PCIe)

| Ring | Index | Purpose | Notes |
|------|-------|---------|-------|
| TX0  | 0     | Data (AC0) | Same base |
| TX1  | 1     | Data (AC1) | |
| TX2  | 2     | Data (AC2) | |
| TX15 | 15    | CMD (MCU WM) | Same as upstream |
| TX16 | 16    | FWDL    | Same as upstream |
| RX4  | 4     | Data Band0 | Different from upstream |
| RX5  | 5     | Data Band1 | Different from upstream |
| RX6  | 6     | MCU Events | Different from upstream (RX0 in upstream) |
| RX7  | 7     | TX Done    | Not in upstream |

Source: `mt6639/chips/mt6639/mt6639.c`

**Key difference:** Upstream uses RX0 for MCU events, vendor uses RX6. Our driver should follow upstream since we're targeting the PCIe variant. TX15 for MCU commands is consistent across all variants.

---

## 8. R2A Bridge Register Definitions

From vendor CODA auto-generated headers (`mt6639/include/chips/coda/mt6639/wf_wfdma_ext_wrap_csr.h`):

```
Base: WF_WFDMA_EXT_WRAP_CSR_BASE = 0x18027000 + CONN_INFRA_REMAPPING_OFFSET

R2A_CTRL_0        = base + 0x500   (BAR0: 0xd7500)
R2A_CTRL_1        = base + 0x504   (BAR0: 0xd7504)
R2A_CTRL_2        = base + 0x508   (BAR0: 0xd7508)
R2A_STS            = base + 0x50C   (BAR0: 0xd750C)
R2A_DMAWR_PROBE   = base + 0x510   (BAR0: 0xd7510)
R2A_DMARD_PROBE   = base + 0x514   (BAR0: 0xd7514)
R2A_WR_DBG_OUT0   = base + 0x518   (BAR0: 0xd7518)
R2A_WR_DBG_OUT1   = base + 0x51C   (BAR0: 0xd751C)
R2A_AXI_SLP_STS   = base + 0x520   (BAR0: 0xd7520)
R2A_RD_DBG_OUT0   = base + 0x524   (BAR0: 0xd7524)
R2A_RD_DBG_OUT1   = base + 0x528   (BAR0: 0xd7528)
R2A_FSM_CMD_ST    = base + 0x52C   (BAR0: 0xd752C)  <-- FSM we observe
R2A_FSM_DAT_ST    = base + 0x530   (BAR0: 0xd7530)  <-- FSM we observe
```

**FSM Values Observed:**
- `0x03030101` = Working state (pre-FW-boot, and after recovery)
- `0x01010202` = Broken state (post-FW-boot, before recovery)
- `0x01010303` = After LOGIC_RST (also broken)

These registers are READ-ONLY. The FSM state is controlled internally by the WFDMA hardware, not directly writable by the driver.

---

## 9. Vendor mt6639 WFDMA ReInit Pattern

The vendor mt6639 driver has an explicit WFDMA re-initialization pattern used after deep sleep / CLR_OWN recovery:

### Setting NEED_REINIT (handshake init)

```c
// mt6639/chips/common/cmm_asic_connac3x.c:320-333
void asicConnac3xWfdmaDummyCrWrite(struct ADAPTER *prAdapter)
{
    u_int32_t u4RegValue = 0;
    HAL_MCR_RD(prAdapter, CONNAC3X_WFDMA_DUMMY_CR, &u4RegValue);
    u4RegValue |= CONNAC3X_WFDMA_NEED_REINIT_BIT;
    HAL_MCR_WR(prAdapter, CONNAC3X_WFDMA_DUMMY_CR, u4RegValue);
}
```

This is registered in the mt6639 bus_info struct as:
```c
// mt6639/chips/mt6639/mt6639.c:392-393
.asicWfdmaReInit = asicConnac3xWfdmaReInit,
.asicWfdmaReInit_handshakeInit = asicConnac3xWfdmaDummyCrWrite,
```

### Checking NEED_REINIT Consumption

```c
// cmm_asic_connac3x.c:305-317
void asicConnac3xWfdmaDummyCrRead(struct ADAPTER *prAdapter, u_int8_t *pfgResult)
{
    u_int32_t u4RegValue = 0;
    HAL_MCR_RD(prAdapter, CONNAC3X_WFDMA_DUMMY_CR, &u4RegValue);
    *pfgResult = (u4RegValue & CONNAC3X_WFDMA_NEED_REINIT_BIT) == 0 ? TRUE : FALSE;
}
```

Returns TRUE if BIT was cleared (meaning ROM/FW processed it).

### Full ReInit Handler

```c
// cmm_asic_connac3x.c:334-381
void asicConnac3xWfdmaReInit(struct ADAPTER *prAdapter)
{
    asicConnac3xWfdmaDummyCrRead(prAdapter, &fgResult);
    if (fgResult) {  // BIT was consumed by ROM
        // PCIe/AXI path:
        // Reset TX ring software indices
        for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
            prHifInfo->TxRing[u4Idx].TxSwUsedIdx = 0;
            prHifInfo->TxRing[u4Idx].u4UsedCnt = 0;
            prHifInfo->TxRing[u4Idx].TxCpuIdx = 0;
        }
        // Check for pending RX events
        if (halWpdmaGetRxDmaDoneCnt(prAdapter->prGlueInfo, RX_RING_EVT_IDX_1))
            prAdapter->u4NoMoreRfb |= BIT(RX_RING_EVT_IDX_1);

        // Write dummy reg for sleep mode
        if (prBusInfo->setDummyReg)
            prBusInfo->setDummyReg(prAdapter->prGlueInfo);

        // Re-arm NEED_REINIT for next sleep cycle
        asicConnac3xWfdmaDummyCrWrite(prAdapter);
    }
}
```

**Key insight from vendor code:** After the ROM consumes NEED_REINIT, the vendor driver:
1. Resets HOST-side TX ring indices to 0
2. Checks for pending RX events
3. Re-arms NEED_REINIT for the next sleep/wake cycle

The comment says "WFDMA re-init flow after chip deep sleep" -- this is the same mechanism used for initial boot.

---

## 10. Resume/Deep Sleep Recovery Flow

The upstream mt7925 resume path shows the same NEED_REINIT pattern in action:

### Suspend (pci.c:450-533)

```
1. mt792x_mcu_drv_pmctrl(dev)  -->  CLR_OWN (ensure driver owns)
2. mt7925_mcu_set_deep_sleep(dev, true)
3. mt76_connac_mcu_set_hif_suspend(mdev, true, false)
4. Wait for HIF idle
5. Disable NAPI, DMA, interrupts
6. ** mt792x_mcu_fw_pmctrl(dev)  -->  SET_OWN (hand to FW, enters deep sleep) **
```

### Resume (pci.c:535-598)

```
1. ** mt792x_mcu_drv_pmctrl(dev)  -->  CLR_OWN **
   a. Calls __mt792xe_mcu_drv_pmctrl(dev)  -->  bare CLR_OWN
   b. ** Calls mt792x_wpdma_reinit_cond(dev)  -->  Checks NEED_REINIT consumed! **
2. Enable interrupts
3. Enable DMA
4. Enable NAPI
5. mt76_connac_mcu_set_hif_suspend(mdev, false, false)
```

**Critical detail in resume:** `mt792xe_mcu_drv_pmctrl()` (NOT the bare `__` version) is used. This version calls `mt792x_wpdma_reinit_cond()` which checks if ROM consumed NEED_REINIT during CLR_OWN and, if so, does a full HOST-side DMA reset.

This confirms that NEED_REINIT is the standard mechanism for ROM/FW to signal that it has reconfigured MCU-side DMA. The same mechanism should work during initial boot.

---

## 11. Windows Driver RE Notes

From `docs/win_v5705275_fw_flow.md`, the Windows driver RE data shows:

- Function names confirm CONNAC3X architecture: `AsicConnac3xWpdmaInitRing`, `AsicConnac3xLoadFirmware`
- `MT6639PreFirmwareDownloadInit` and `MT6639WpdmaConfig` suggest a similar pre-FW-download WFDMA configuration step
- The Windows flow appears to follow a similar pattern to the Linux upstream driver

No additional unique insights were found in the Windows RE data beyond what the Linux and vendor sources already reveal.

---

## 12. Specific Recommendations

### For agent1 (dummy command approach)

The "dummy command" approach may work as a workaround since the FSM does recover after ~2s with DMA activity, but it is NOT the root cause fix. The proper fix is the second SET_OWN/CLR_OWN cycle with NEED_REINIT.

If you still want to test the dummy command approach:
1. After FW_START + fw_sync=0x3, send a throwaway MCU command on TX15
2. Wait ~2s for FSM recovery (poll R2A_FSM_CMD_ST for 0x03030101)
3. Then send the real NIC_CAPABILITY command
4. But be aware this is a workaround, not a fix

### For agent1 / agent2 (NEED_REINIT fix - HIGHEST PRIORITY)

**Recommended test sequence (matching upstream exactly):**

```
1. SET_OWN  (BIT(0) -> 0x7c060010, poll OWN_SYNC=BIT(2))
2. CLR_OWN  (BIT(1) -> 0x7c060010, poll OWN_SYNC=0)      -- Cycle 1 (bare)
3. Read CHIPID + REV
4. Set EMI_CTL SLPPROT_EN=1
5. WFSYS_RESET  (clear BIT(0)@0x7c000140, wait 50ms, set BIT(0), poll BIT(4))
6. DMA disable (force=true: LOGIC_RST + DMASHDL_RST)
7. Allocate TX/RX rings (write BASE, CNT, CIDX)
8. DMA enable (prefetch, GLO_CFG, TX+RX DMA EN, **SET NEED_REINIT**, enable IRQs)
9. SET_OWN  (BIT(0) -> 0x7c060010, poll OWN_SYNC=BIT(2))  -- Cycle 2
10. CLR_OWN (BIT(1) -> 0x7c060010, poll OWN_SYNC=0)        -- Cycle 2 (ROM sees NEED_REINIT!)
11. Disable L0S
12. Download firmware (patch + RAM + FW_START + poll fw_sync=0x3)
13. Send NIC_CAPABILITY (first MCU command) -- should work now!
```

**Minimal test to validate the hypothesis:**
After our existing DMA init (which already sets NEED_REINIT), add:
```c
// SET_OWN
iowrite32(BIT(0), bar0 + 0xe0010);       // LPCTL
poll(bar0 + 0xe0010, OWN_SYNC=BIT(2));   // Wait for FW ownership

// CLR_OWN
iowrite32(BIT(1), bar0 + 0xe0010);       // LPCTL
poll(bar0 + 0xe0010, OWN_SYNC=0);        // Wait for driver ownership

// Then proceed with FW download...
```

This should cause the ROM to process NEED_REINIT=1 and configure the WFDMA R2A bridge correctly.

**After CLR_OWN, check:**
- Read DUMMY_CR: BIT(1) should be cleared (ROM consumed it)
- Read R2A_FSM_CMD_ST (0xd752c): Should show 0x03030101
- Read HOST ring BASEs (TX15=0xd43F0, TX16=0xd4400): Should be INTACT (non-zero)

**UPDATED ANALYSIS (Session 5):** The previous MEMORY.md note "CLR_OWN zeroes ALL HOST ring BASEs" applies to
the FIRST CLR_OWN (probe-time, no NEED_REINIT) and runtime PM CLR_OWN (wake-from-sleep). The SECOND CLR_OWN
in mcu_init (with NEED_REINIT set) does NOT zero HOST ring BASEs.

**Evidence:** Upstream `mt7925e_mcu_init` uses bare `__mt792xe_mcu_drv_pmctrl` (no wpdma_reinit_cond, no
ring reprogramming) and proceeds DIRECTLY to `mt7925_run_firmware()` which sends MCU commands on TX16.
If TX16 BASE were zeroed, `mt76_tx_queue_skb_raw` would DMA to address 0. Since upstream works,
HOST ring BASEs must be preserved.

**Two different ROM code paths for CLR_OWN:**
- First CLR_OWN (probe, no NEED_REINIT): Full power transition, HOST rings zeroed
- Runtime PM CLR_OWN (wake): Full WFDMA reset, HOST rings zeroed, driver uses wpdma_reinit_cond
- Init-time 2nd CLR_OWN (NEED_REINIT set): MCU-side ring config ONLY, HOST rings preserved

**Corrected sequence (NO ring reprogramming needed):**
```
1-8. (Same as above, through DMA enable which sets NEED_REINIT)
9. SET_OWN
10. CLR_OWN (ROM configures MCU-side DMA, HOST ring bases PRESERVED)
11. L0S disable: write BIT(8) to 0x10194
12. Download firmware (HOST rings intact, proceed directly)
13. Send MCU command
```

**Verify empirically:** After 2nd CLR_OWN, read TX16 BASE (0xd4400) and TX15 BASE (0xd43F0).
If non-zero: analysis confirmed, no reprogramming needed. If zeroed: add reprogramming as fallback.

### For agent2 (alternative approaches)

Based on the analysis, here are informed alternative approaches:

1. **Skip second SET_OWN/CLR_OWN, manually set NEED_REINIT before first CLR_OWN:**
   - Before the very first CLR_OWN in our init, write NEED_REINIT=BIT(1) to DUMMY_CR
   - This way the first (and only) CLR_OWN will trigger ROM's NEED_REINIT handler
   - Simpler but requires DMA to be initialized before the first CLR_OWN (chicken-and-egg issue)

2. **WFDMA_EXT_WRAP_CSR_WFDMA_SW_RST (0xd703C):**
   - The vendor CODA headers show a SW_RST register. Try writing to it to reset the WFDMA bridge without full CLR_OWN
   - Untested, may or may not reset the R2A bridge FSM

3. **Direct R2A_CTRL_0 manipulation (0xd7500):**
   - R2A_CTRL_0 is likely writable (unlike FSM_CMD_ST/FSM_DAT_ST which are read-only status)
   - May contain bridge enable/disable bits
   - Worth reading and experimenting with

---

## 13. Appendix: Key Register Addresses

### LPCTL (Power Control)
| Register | Address | Bits |
|----------|---------|------|
| MT_CONN_ON_LPCTL | 0x7c060010 (via L1 remap) | BIT(0)=SET_OWN, BIT(1)=CLR_OWN, BIT(2)=OWN_SYNC |

### WFDMA Host DMA0
| Register | BAR0 Offset | Notes |
|----------|-------------|-------|
| GLO_CFG  | 0xd4208     | TX/RX DMA enable, config flags |
| HOST_INT_ENA | 0xd4204 | Interrupt enable |
| TX_RING_BASE | 0xd4300 | +0x10 per ring (BASE, CNT, CIDX, DIDX) |
| RX_RING_BASE | 0xd4500 | +0x10 per ring |
| RST     | 0xd4100     | LOGIC_RST=BIT(0), DMASHDL_ALL_RST=BIT(5) |
| RST_DTX_PTR | 0xd420c | Reset TX DMA indices |
| RST_DRX_PTR | 0xd4280 | Reset RX DMA indices |

### WFDMA Dummy CR (NEED_REINIT)
| Register | Virtual Address | Mapped Address | Bits |
|----------|----------------|----------------|------|
| MT_WFDMA_DUMMY_CR | 0x54000120 | Via fixed_map | BIT(1)=NEED_REINIT |

### WFDMA EXT WRAP (R2A Bridge)
| Register | BAR0 Offset | Notes |
|----------|-------------|-------|
| R2A_CTRL_0 | 0xd7500 | Bridge control (likely writable) |
| R2A_CTRL_1 | 0xd7504 | Bridge control |
| R2A_CTRL_2 | 0xd7508 | Bridge control |
| R2A_STS    | 0xd750C | Bridge status |
| R2A_SLP_STS | 0xd7520 | AXI sleep status |
| R2A_FSM_CMD_ST | 0xd752C | FSM command state (READ-ONLY) |
| R2A_FSM_DAT_ST | 0xd7530 | FSM data state (READ-ONLY) |

### Misc
| Register | Address | Notes |
|----------|---------|-------|
| MT_CONN_ON_MISC (fw_sync) | 0x7c0600f0 | BIT(1:0)=fw_sync, poll for 0x3 |
| WFSYS_SW_RST_B | 0x7c000140 | BIT(0)=RST, BIT(4)=INIT_DONE |
| MT_PCIE_MAC_PM | (varies) | L0S_DIS field |
| MCU_CMD | 0xd41f0 | MCU command register |

---

## 14. Vendor mt6639 Post-FW-Boot Sequence (Detailed)

Analysis of `mt6639/common/wlan_lib.c` `wlanAdapterStart()` reveals the exact vendor post-FW-boot sequence:

```
1. wlanDownloadFW()                    -- patch + RAM download + FW_START
2. wlanCheckWifiFunc(TRUE)             -- polls sw_sync0 (0x7c0600f0) for BITS(0,1)=0x3
                                          Same register/bits as upstream fw_sync check
3. fgIsFwDownloaded = TRUE             -- mark FW as downloaded
4. wlanQueryNicResourceInformation()   -- query TX resource
5. [CONNAC3X == 0 ONLY]: wlanSendDummyCmd()  -- DISABLED for CONNAC3X!
6. wlanQueryNicCapabilityV2()          -- FIRST real MCU command
7. wlanUpdateNicResourceInformation()
8. wlanUpdateBasicConfig()
```

**Critical finding at step 5:** The dummy command workaround is explicitly disabled for CONNAC3X:
```c
// mt6639/common/wlan_lib.c:1258-1265
#if (CFG_SUPPORT_CONNAC3X == 0)
    /* 2.9 Workaround for Capability CMD packet lost issue */
    wlanSendDummyCmd(prAdapter, TRUE);
#endif
```

This confirms that for MT6639/MT7927 (CONNAC3X), no dummy command is needed or used. The WFDMA bridge must already be working by the time FW boots.

### fw_sync / sw_ready_bits Confirmation

The vendor mt6639 uses:
- `sw_sync0` = `Connac3x_CONN_CFG_ON_BASE + 0xF0` = **0x7C0600F0** (same as upstream `MT_CONN_ON_MISC`)
- `sw_ready_bits` = `WIFI_FUNC_NO_CR4_READY_BITS` = `BITS(0,1)` = **0x3**
- `DRV_FM_STAT_SYNC_SHFT` = **0** (no shift for CONNAC3X, bits are at [1:0])

This is identical to upstream's `MT_TOP_MISC2_FW_N9_RDY` poll at `MT_CONN_ON_MISC`. Both check the same register for the same value (0x3).

### FSM Registers Never Referenced in Operational Code

Searched all driver sources (upstream mt76, vendor mt6639, Windows RE):
- `R2A_FSM_CMD_ST` (0xd752C) and `R2A_FSM_DAT_ST` (0xd7530) appear ONLY in CODA auto-generated header files
- No driver code reads, writes, or checks these registers
- They are purely internal WFDMA hardware status indicators

---

## Summary of Analysis Sources

| Source | Location | Key Contribution |
|--------|----------|-----------------|
| Upstream mt7925/pci.c | mt76/mt7925/pci.c | Probe sequence, DMA init, two SET_OWN/CLR_OWN cycles |
| Upstream mt7925/pci_mcu.c | mt76/mt7925/pci_mcu.c | mcu_init: second SET_OWN/CLR_OWN before FW download |
| Upstream mt792x_dma.c | mt76/mt792x_dma.c | dma_enable sets NEED_REINIT, wpdma_reinit_cond |
| Upstream mt792x_core.c | mt76/mt792x_core.c | SET_OWN/CLR_OWN implementations |
| Upstream mt7925/mcu.c | mt76/mt7925/mcu.c | run_firmware: NIC_CAPABILITY is first command |
| Upstream mt7925/init.c | mt76/mt7925/init.c | init_work -> init_hardware -> mcu_init |
| Vendor mt6639/cmm_asic_connac3x.c | mt6639/chips/common/cmm_asic_connac3x.c | WFDMA ReInit pattern, NEED_REINIT handshake |
| Vendor mt6639/mt6639.c | mt6639/chips/mt6639/mt6639.c | Ring assignments, bus_info struct |
| Vendor CODA headers | mt6639/include/chips/coda/mt6639/wf_wfdma_ext_wrap_csr.h | R2A bridge register definitions |
| Windows RE | docs/win_v5705275_fw_flow.md | Confirms CONNAC3X architecture |
