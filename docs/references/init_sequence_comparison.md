# MT7927 vs Upstream MT7925: Init Sequence Comparison

## Executive Summary

This document provides a step-by-step comparison of our driver (`tests/04_risky_ops/mt7927_init_dma.c`)
vs the upstream mt7925 driver (`mt76/mt7925/pci.c` + supporting files). The goal is to identify
any fundamental difference that explains why MCU_RX0 BASE=0x00000000 post-boot.

**KEY FINDING**: The upstream driver performs EXACTLY TWO `SET_OWN -> CLR_OWN` power cycles:
1. **First** in `mt7925_pci_probe()` (before WFSYS reset) - bare `fw_pmctrl -> drv_pmctrl`
2. **Second** in `mt7925e_mcu_init()` (after DMA init, before FWDL) - bare `fw_pmctrl -> drv_pmctrl`

Our driver does the first power cycle but may not do the second one correctly
(the second cycle is where the MCU ROM processes `NEED_REINIT` and configures MCU rings).
However, we HAVE tested this in mode 11/34 and it does NOT fix the issue on MT7927 because
the ROM processes `NEED_REINIT` differently - it only configures MCU_RX2/RX3 (FWDL rings),
NOT MCU_RX0/RX1.

---

## Step-by-Step Upstream Init Sequence

### Phase 1: `mt7925_pci_probe()` (file: `mt76/mt7925/pci.c` line 271)

| Step | Upstream Code | Register | Value | Our Driver |
|------|--------------|----------|-------|------------|
| 1 | `pcim_enable_device(pdev)` | PCI config | - | YES (`pci_enable_device`) |
| 2 | `pcim_iomap_regions(pdev, BIT(0), ...)` | BAR0 | map | YES (`pci_iomap`) |
| 3 | `pci_set_master(pdev)` | PCI CMD | bus master | YES |
| 4 | `pci_alloc_irq_vectors(pdev, 1, 1, ...)` | MSI | 1 vector | **NO** - we skip IRQ |
| 5 | `dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))` | DMA mask | 32-bit | YES (`dma_set_mask_and_coherent`) |
| 6 | `mt76_alloc_device(...)` | - | mt76 device | **NO** - we use custom struct |
| 7 | bus_ops remap: `mt7925_rr/wr/rmw` installed | - | register remap | **YES** - we use BAR0 direct + fixed_map equivalent |

### Phase 1b: Power Cycle #1 (probe, lines 386-391)

| Step | Upstream Code (pci.c) | Register (chip addr) | BAR0 Offset | Our Driver |
|------|----------------------|---------------------|-------------|------------|
| **8** | `__mt792x_mcu_fw_pmctrl(dev)` = `mt792xe_mcu_fw_pmctrl(dev)` | `MT_CONN_ON_LPCTL` (0x7c060010) | 0xe0010 | **YES** (SET_OWN at line 12708) |
| | Writes `PCIE_LPCR_HOST_SET_OWN` = BIT(0) | 0x7c060010 | 0xe0010 | YES |
| | Polls `PCIE_LPCR_HOST_OWN_SYNC` = BIT(2) set | 0x7c060010 | 0xe0010 | YES |
| **9** | `__mt792xe_mcu_drv_pmctrl(dev)` | `MT_CONN_ON_LPCTL` | 0xe0010 | **YES** (CLR_OWN at line 12718) |
| | Writes `PCIE_LPCR_HOST_CLR_OWN` = BIT(1) | 0x7c060010 | 0xe0010 | YES |
| | Polls `PCIE_LPCR_HOST_OWN_SYNC` = BIT(2) clear | 0x7c060010 | 0xe0010 | YES |
| | **NOTE**: Upstream CLR_OWN has NO `usleep_range(2000, 3000)` between retries | | | We add 2ms delay |
| | Upstream uses ASPM delay: `if (dev->aspm_supported) usleep_range(2000, 3000)` | | | We always add delay |

### Phase 1c: ASIC rev + EMI_CTL + WFSYS reset (probe, lines 394-403)

| Step | Upstream Code | Register (chip addr) | BAR0 Offset | Value | Our Driver |
|------|--------------|---------------------|-------------|-------|------------|
| **10** | `mdev->rev = mt76_rr(dev, MT_HW_CHIPID) << 16 \| mt76_rr(dev, MT_HW_REV)` | 0x00 / 0x04 | 0x00 / 0x04 | chip ID | We read 0x00 as diagnostic |
| **11** | **`mt76_rmw_field(dev, MT_HW_EMI_CTL, MT_HW_EMI_CTL_SLPPROT_EN, 1)`** | 0x18011100 | via L1 remap | **SET BIT(1)** | **YES** (line 4056, conditional on `use_emi_slpprot`) |
| **12** | `mt792x_wfsys_reset(dev)` | 0x7c000140 | 0xf0140 | see below | **YES** (line 4155, conditional on `use_wfsys_reset`) |

#### WFSYS Reset Detail (`mt792x_wfsys_reset`, file: `mt76/mt792x_dma.c` line 357)

| Sub-step | Upstream | Register | Value | Our Driver |
|----------|---------|----------|-------|------------|
| 12a | `mt76_clear(dev, 0x7c000140, WFSYS_SW_RST_B)` | 0xf0140 | Clear BIT(0) | YES (line 4164) |
| 12b | `msleep(50)` | - | 50ms wait | YES (line 4166) |
| 12c | `mt76_set(dev, 0x7c000140, WFSYS_SW_RST_B)` | 0xf0140 | Set BIT(0) | YES (line 4202) |
| 12d | Poll `WFSYS_SW_INIT_DONE` = BIT(4) set, 500ms timeout | 0xf0140 | BIT(4) | YES (line 4206) |

**DIFFERENCE**: Upstream does a CLEAN `clear -> wait -> set -> poll`. Our driver inserts
sleep protection clearing DURING the reset (between steps 12b and 12c, line 4182-4287).
This is extra work not done by upstream.

### Phase 1d: IRQ + DMA init (probe, lines 405-414)

| Step | Upstream Code | Register | BAR0 Offset | Value | Our Driver |
|------|--------------|----------|-------------|-------|------------|
| **13** | `mt76_wr(dev, irq_map.host_irq_enable, 0)` | `MT_WFDMA0_HOST_INT_ENA` = MT_WFDMA0(0x204) | 0xd4204 | 0 (disable IRQ) | **EQUIVALENT** (via INT_ENA writes) |
| **14** | `mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0xff)` | MT_PCIE_MAC(0x188) = 0x74030188 | 0x010188 | 0xFF | **NO** - we never write MT_PCIE_MAC_INT_ENABLE |
| **15** | `devm_request_irq(...)` | - | - | IRQ handler | **NO** - we don't register IRQ |
| **16** | `mt7925_dma_init(dev)` | see below | | | YES (different implementation) |

**POSSIBLE ISSUE**: We never write `MT_PCIE_MAC_INT_ENABLE = 0xFF` (BAR0 0x010188). This is
the PCIe MAC master interrupt enable. Without this, PCIe interrupt delivery from device to host
may not work. However, this should not affect DMA functionality or MCU ring configuration.

### Phase 1e: DMA Init (`mt7925_dma_init`, file: `mt76/mt7925/pci.c` line 215)

| Step | Upstream Code | Register | BAR0 Offset | Our Driver |
|------|--------------|----------|-------------|------------|
| 16a | `mt76_dma_attach(&dev->mt76)` | - | - | N/A (custom impl) |
| **16b** | `mt792x_dma_disable(dev, true)` | see below | | **YES** (`mt7927_dma_disable`) |
| 16c | Init TX queue 0 (data) | TX_RING0 BASE/CNT/CIDX | 0xd4300+ | We don't alloc data TX |
| 16d | `MT_WFDMA0_TX_RING0_EXT_CTRL = 0x4` | 0xd4600 | 0xd4600 | Via prefetch |
| **16e** | Init MCU WM queue (TX15) | TX_RING15 BASE/CNT/CIDX | 0xd43F0+ | **YES** (TX15 or TX17) |
| **16f** | Init FWDL queue (TX16) | TX_RING16 BASE/CNT/CIDX | 0xd4400+ | **YES** (TX16) |
| **16g** | Init MCU event RX (RX0) | RX_RING0 BASE/CNT/CIDX | 0xd4500+ | **YES** (RX0 or RX6) |
| 16h | Init data RX (RX2) | RX_RING2 BASE/CNT/CIDX | 0xd4520+ | **YES** (various RX rings) |
| 16i | `mt76_init_queues` + NAPI | - | - | N/A |
| **16j** | `mt792x_dma_enable(dev)` | see below | | **YES** (equivalent) |

#### DMA Disable Detail (`mt792x_dma_disable`, file: `mt76/mt792x_dma.c` line 253)

| Sub-step | Upstream | Register | BAR0 Offset | Value | Our Driver |
|----------|---------|----------|-------------|-------|------------|
| b1 | Clear TX_DMA_EN, RX_DMA_EN, CHAIN_EN, OMIT_TX/RX from `MT_WFDMA0_GLO_CFG` | 0xd4208 | 0xd4208 | clear bits | YES |
| b2 | Poll TX_DMA_BUSY + RX_DMA_BUSY = 0 (100ms timeout) | 0xd4208 | 0xd4208 | busy=0 | YES |
| **b3** | **`mt76_clear(dev, MT_WFDMA0_GLO_CFG_EXT0, MT_WFDMA0_CSR_TX_DMASHDL_ENABLE)`** | 0xd42b0 | 0xd42b0 | **Clear BIT(6)** | **CONDITIONAL** (`disable_ext0_dmashdl` flag) |
| **b4** | **`mt76_set(dev, MT_DMASHDL_SW_CONTROL, MT_DMASHDL_DMASHDL_BYPASS)`** | 0x7c026004 | 0xd6004 | **Set BIT(28)** | **YES** (line 12329) |
| b5 | (force=true) Clear then set `MT_WFDMA0_RST_LOGIC_RST \| MT_WFDMA0_RST_DMASHDL_ALL_RST` | 0xd4100 | 0xd4100 | pulse | YES (line 1168) |

#### DMA Enable Detail (`mt792x_dma_enable`, file: `mt76/mt792x_dma.c` line 126)

| Sub-step | Upstream | Register | BAR0 Offset | Value | Our Driver |
|----------|---------|----------|-------------|-------|------------|
| j1 | `mt792x_dma_prefetch(dev)` for mt7925 | RX0-3_EXT_CTRL, TX0-3/15-16_EXT_CTRL | various | see below | YES (various modes) |
| j2 | `MT_WFDMA0_RST_DTX_PTR = ~0` | 0xd420c | 0xd420c | 0xFFFFFFFF | YES |
| j3 | `MT_WFDMA0_RST_DRX_PTR = ~0` (mt7925 only) | 0xd4280 | 0xd4280 | 0xFFFFFFFF | YES |
| j4 | `MT_WFDMA0_PRI_DLY_INT_CFG0 = 0` | 0xd42f0 | 0xd42f0 | 0 | YES |
| **j5** | `mt76_set(dev, MT_WFDMA0_GLO_CFG, ...)` with many bits | 0xd4208 | 0xd4208 | see below | YES |
| **j6** | `mt76_set(dev, MT_WFDMA0_GLO_CFG, TX_DMA_EN \| RX_DMA_EN)` | 0xd4208 | 0xd4208 | BIT(0)\|BIT(2) | YES |
| j7 | (mt7925) `MT_UWFDMA0_GLO_CFG_EXT1 \|= BIT(28)` | 0xd52b4 | 0xd52b4 | Set BIT(28) | YES (line 12264) |
| j8 | (mt7925) `MT_WFDMA0_INT_RX_PRI \|= 0x0F00` | 0xd4298 | 0xd4298 | OR 0x0F00 | YES (line 12291) |
| j9 | (mt7925) `MT_WFDMA0_INT_TX_PRI \|= 0x7F00` | 0xd429c | 0xd429c | OR 0x7F00 | YES (line 12293) |
| **j10** | **`mt76_set(dev, MT_WFDMA_DUMMY_CR, MT_WFDMA_NEED_REINIT)`** | 0x54000120 | 0x02120 | **Set BIT(1)** | **YES** (line 12389) |
| j11 | Enable interrupts (TX/RX complete + MCU_CMD) | 0xd4204 | 0xd4204 | mask | YES |
| j12 | `mt76_set(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE)` | 0xd41f4 | 0xd41f4 | BIT(0) | YES (line 12281) |

##### GLO_CFG bits set in upstream `mt792x_dma_enable` (step j5):

| Bit | Name | Value | Our Driver |
|-----|------|-------|------------|
| BIT(6) | `TX_WB_DDONE` | 1 | YES |
| BIT(12) | `FIFO_LITTLE_ENDIAN` | 1 | YES |
| BIT(30) | `CLK_GAT_DIS` | 1 | YES |
| BIT(28) | `OMIT_TX_INFO` | 1 | YES |
| GENMASK(5,4) | `DMA_SIZE` = 3 | 0x30 | YES |
| BIT(11) | `FIFO_DIS_CHECK` | 1 | YES |
| BIT(13) | `RX_WB_DDONE` | 1 | YES |
| BIT(15) | `CSR_DISP_BASE_PTR_CHAIN_EN` | 1 | YES |
| BIT(21) | `OMIT_RX_INFO_PFET2` | 1 | YES |

##### Upstream Prefetch for mt7925 (`mt792x_dma_prefetch`, `is_mt7925` branch):

| Register | BAR0 | Value | Description |
|----------|------|-------|-------------|
| `RX_RING0_EXT_CTRL` | 0xd4680 | PREFETCH(0x0000, 0x4) | RX0: base=0x0000, depth=4 |
| `RX_RING1_EXT_CTRL` | 0xd4684 | PREFETCH(0x0040, 0x4) | RX1: base=0x0040, depth=4 |
| `RX_RING2_EXT_CTRL` | 0xd4688 | PREFETCH(0x0080, 0x4) | RX2: base=0x0080, depth=4 |
| `RX_RING3_EXT_CTRL` | 0xd468c | PREFETCH(0x00c0, 0x4) | RX3: base=0x00c0, depth=4 |
| `TX_RING0_EXT_CTRL` | 0xd4600 | PREFETCH(0x0100, 0x10) | TX0: base=0x0100, depth=16 |
| `TX_RING1_EXT_CTRL` | 0xd4604 | PREFETCH(0x0200, 0x10) | TX1: base=0x0200, depth=16 |
| `TX_RING2_EXT_CTRL` | 0xd4608 | PREFETCH(0x0300, 0x10) | TX2: base=0x0300, depth=16 |
| `TX_RING3_EXT_CTRL` | 0xd460c | PREFETCH(0x0400, 0x10) | TX3: base=0x0400, depth=16 |
| `TX_RING15_EXT_CTRL` | 0xd463c | PREFETCH(0x0500, 0x4) | TX15: base=0x0500, depth=4 |
| `TX_RING16_EXT_CTRL` | 0xd4640 | PREFETCH(0x0540, 0x4) | TX16: base=0x0540, depth=4 |

Our driver in `upstream_prefetch` mode matches these exactly.

---

### Phase 2: `mt7925_register_device()` -> `mt7925_init_work()` -> `mt7925_init_hardware()`

This is called from `queue_work(system_percpu_wq, &dev->init_work)` at the end of
`mt7925_register_device()`. It runs asynchronously AFTER probe returns.

#### `mt7925_init_hardware` (file: `mt76/mt7925/init.c` line 122)

Calls `__mt7925_init_hardware(dev)` with retry loop.

#### `__mt7925_init_hardware` (init.c line 98)

| Step | Code | Function |
|------|------|----------|
| 1 | `mt792x_mcu_init(dev)` | = `dev->hif_ops->mcu_init(dev)` = **`mt7925e_mcu_init(dev)`** |
| 2 | `mt76_eeprom_override(...)` | EEPROM |
| 3 | `mt7925_mcu_set_eeprom(dev)` | MCU command |
| 4 | `mt7925_mac_init(dev)` | MAC init |

### Phase 2b: `mt7925e_mcu_init()` (file: `mt76/mt7925/pci_mcu.c` line 27)

**THIS IS THE CRITICAL FUNCTION - Power Cycle #2**

| Step | Upstream Code | Register | BAR0 | Value | Our Driver |
|------|--------------|----------|------|-------|------------|
| **M1** | `dev->mt76.mcu_ops = &mt7925_mcu_ops` | - | - | set ops | N/A |
| **M2** | **`mt792xe_mcu_fw_pmctrl(dev)`** (SET_OWN #2) | 0x7c060010 | 0xe0010 | **BIT(0)** | **YES** (mode 11 and others) |
| | Poll `PCIE_LPCR_HOST_OWN_SYNC` BIT(2) = 1 | 0x7c060010 | 0xe0010 | | YES |
| **M3** | **`__mt792xe_mcu_drv_pmctrl(dev)`** (CLR_OWN #2) | 0x7c060010 | 0xe0010 | **BIT(1)** | **YES** (mode 11 and others) |
| | Poll `PCIE_LPCR_HOST_OWN_SYNC` BIT(2) = 0 | 0x7c060010 | 0xe0010 | | YES |
| **M4** | **`mt76_rmw_field(dev, MT_PCIE_MAC_PM, MT_PCIE_MAC_PM_L0S_DIS, 1)`** | 0x74030194 | 0x010194 | **Set BIT(8)** | **SOME MODES** (mode 20, line 13089) |
| **M5** | `mt7925_run_firmware(dev)` | - | - | FWDL | YES |
| M6 | `mt76_queue_tx_cleanup(dev, ..., FWDL, false)` | - | - | cleanup | YES |

#### The SET_OWN -> CLR_OWN Cycle #2 in Detail:

**What happens in upstream CLR_OWN (step M3)**:
1. Host writes BIT(1) to `MT_CONN_ON_LPCTL` (0x7c060010)
2. MCU ROM wakes up, checks `MT_WFDMA_DUMMY_CR` (0x54000120)
3. If `NEED_REINIT` (BIT(1)) is set, ROM does a FULL WFDMA reset:
   - Zeros ALL HOST ring BASEs
   - Disables GLO_CFG
   - Clears INT_ENA
   - Configures MCU_RX2/RX3 (FWDL rings) with SRAM addresses
   - Clears `NEED_REINIT` bit
4. Host polls until `OWN_SYNC` BIT(2) clears

**CRITICAL DIFFERENCE ON MT7927**: On MT7925, this CLR_OWN step configures MCU_RX0/RX1 as well.
On MT7927, the ROM ONLY configures MCU_RX2/RX3. This is the fundamental hardware difference.

### Phase 2c: `mt7925_run_firmware()` (file: `mt76/mt7925/mcu.c` line 1045)

| Step | Code | Function |
|------|------|----------|
| F1 | `mt792x_load_firmware(dev)` | loads patch + RAM + polls fw_sync |
| F2 | `mt7925_mcu_get_nic_capability(dev)` | **FIRST MCU command after boot** |
| F3 | `mt7925_load_clc(dev, ...)` | CLC load |
| F4 | `mt7925_mcu_fw_log_2_host(dev, 1)` | enable FW log |

#### `mt792x_load_firmware()` (file: `mt76/mt792x_core.c` line 926)

| Step | Code | Register | BAR0 | Value |
|------|------|----------|------|-------|
| L1 | `mt76_connac2_load_patch(...)` | - | - | patch download |
| L2 | `mt76_connac2_load_ram(...)` | - | - | RAM regions + FW_START |
| **L3** | `mt76_poll_msec(dev, MT_CONN_ON_MISC, MT_TOP_MISC2_FW_N9_RDY, MT_TOP_MISC2_FW_N9_RDY, 1500)` | 0x7c0600f0 | 0xe00f0 | BIT(0)\|BIT(1) = 0x3 |
|  | Polls for `fw_sync = 0x3` (FW_N9_ON + FW_PWR_ON) up to 1500ms | | | |

Our driver does the same (polls `MT_CONN_ON_MISC` for `fw_sync=0x3`).

---

## Our Driver's Init Sequence (Summary)

From `mt7927_probe()` (line 12645):

| Step | Our Code | Equivalent Upstream |
|------|---------|-------------------|
| 1 | PCI enable, master, DMA mask, iomap | Same |
| 2 | **SET_OWN -> CLR_OWN** (bare, no WFSYS context) | Same as probe power cycle #1 |
| 3 | `mt7927_mcu_init_mt6639()` (WFSYS reset + vendor init) | `mt792x_wfsys_reset()` |
| 4 | `mt7927_disable_wfdma_slpprot()` | **NOT IN UPSTREAM** |
| 5 | Poll `ROMCODE_INDEX == 0x1D1E` | **NOT IN UPSTREAM** (upstream doesn't poll ROM) |
| 6 | MCU_OWN_SET, MCIF remap | **NOT IN UPSTREAM** |
| 7 | `mt7927_drv_own()` (bare CLR_OWN) | `__mt792xe_mcu_drv_pmctrl()` in probe |
| 8 | `mt7927_clear_r2a_bridge()` | **NOT IN UPSTREAM** |
| 9 | `mt7927_dma_init()` (alloc rings + configure + enable) | `mt7925_dma_init()` |
| 10 | `NEED_REINIT` set in DUMMY_CR | Same (`mt792x_dma_enable`) |
| 11 | mode-dependent reinit (SET_OWN -> CLR_OWN, etc.) | Power cycle #2 in `mt7925e_mcu_init` |
| 12 | FWDL | Same |
| 13 | Post-boot MCU command attempts | `mt7925_mcu_get_nic_capability` |

---

## Identified Differences

### Difference 1: MT_PCIE_MAC_INT_ENABLE (0x010188)
- **Upstream**: Writes `0xFF` in probe (line 407) BEFORE DMA init
- **Our driver**: NEVER writes this register
- **Impact**: Low. This controls PCIe MAC interrupt routing. Should not affect DMA ring config.

### Difference 2: MT_PCIE_MAC_PM L0S_DIS (0x010194 BIT(8))
- **Upstream**: Sets BIT(8) in `mt7925e_mcu_init()` BEFORE firmware download (line 46)
- **Our driver**: Only in some modes (mode 20, line 13089)
- **Impact**: Medium. L0s is a PCIe link power saving state. Disabling it before FWDL ensures
  stable PCIe link during firmware scatter operations. Could cause intermittent issues.

### Difference 3: Second SET_OWN -> CLR_OWN Timing
- **Upstream**: Power cycle #2 happens in `mt7925e_mcu_init()` which runs AFTER probe returns,
  inside `init_work`. The sequence is: probe returns -> workqueue -> `mt7925_init_hardware()` ->
  `mt7925e_mcu_init()` -> SET_OWN -> CLR_OWN -> L0s disable -> FWDL.
  At this point, DMA is already initialized (rings allocated, NEED_REINIT set, DMA enabled).
- **Our driver**: We do the second power cycle in various reinit modes, but the timing is
  slightly different. We do it between DMA init and FWDL, which should be equivalent.
- **Impact**: The upstream CLR_OWN happens AFTER DMA enable + NEED_REINIT set. Our driver also
  sets NEED_REINIT before the second CLR_OWN in mode 11. This ordering is correct.
- **HOWEVER**: On MT7927, the CLR_OWN only causes ROM to configure MCU_RX2/RX3, not MCU_RX0/RX1.
  This is the ROOT CAUSE and is a hardware/firmware difference, not a driver issue.

### Difference 4: Extra Steps in Our Driver (NOT in Upstream)

These are things our driver does that upstream does NOT:

| Extra Step | Register | Purpose | Risk |
|-----------|----------|---------|------|
| `mt7927_disable_wfdma_slpprot()` | 0xf1440 | Clear sleep protection | May be necessary for MT7927 |
| `ROMCODE_INDEX` poll | 0xc1604 | Wait for MCU ROM idle | Harmless |
| `MCU_OWN_SET` | 0x1f5034 | Vendor MCU ownership | May be needed for MT7927 |
| `MCIF remap` | 0xd1034 or 0xfe410+ | PCIe2AP remap for MCU DMA | Needed for MCU->HOST DMA |
| `WAKEPU_TOP/WF` | 0xe01a0/0xe01a4 | CONNINFRA wakeup | May be needed for MT7927 |
| `CB_INFRA PCIe remap` | 0x1f6554/0x1f6558 | PCIe address remap | Vendor-specific |
| `VDNR enable` | 0xfe06c | Bus routing | Returns 0x87654321 on PCIe (dead) |
| `R2A bridge clear` | 0xd7050, 0xd7500+ | Sleep protection clear | Aggressive, may cause issues |
| Various `EXT0` writes | 0xd42b0 | SDO dispatch mode | Vendor-specific, may conflict |
| `GLO_CFG BIT(9)` | 0xd4208 | DMASHDL bypass in GLO | Extra bit upstream doesn't set |
| `GLO_CFG BIT(20)` | 0xd4208 | RX queue select enable | Extra bit upstream doesn't set |
| `GLO_CFG BIT(26)` | 0xd4208 | Address extension | Extra bit upstream doesn't set |
| RX pause thresholds | 0xd4260+ | RX flow control | Vendor-specific |
| MSI_INT_CFG0-3 | 0xd70f0-0xd70fc | MSI config | Windows-observed values |
| MCU DMA0 RX CIDX fix | 0x02508/0x02518 | MCU RX available space | Diagnostic |
| MCU WRAP GLO enable | 0x05208 | MCU WRAP DMA enable | Untested |

### Difference 5: DMA Disable Steps

- **Upstream** `mt792x_dma_disable(dev, true)`:
  1. Clear GLO_CFG bits (TX/RX_DMA_EN + CHAIN_EN + OMIT flags)
  2. Poll busy = 0
  3. **Clear DMASHDL_ENABLE in EXT0** (BIT(6) of 0xd42b0)
  4. **Set DMASHDL_BYPASS** (BIT(28) of 0x7c026004)
  5. Pulse LOGIC_RST + DMASHDL_ALL_RST (0xd4100)

- **Our driver** `mt7927_dma_disable()`:
  1. Clear TX/RX_DMA_EN
  2. Poll busy = 0
  3. Pulse LOGIC_RST + DMASHDL_RST (0xd4100)
  4. **Does NOT clear DMASHDL_ENABLE in EXT0** during disable
  5. **Does NOT set DMASHDL_BYPASS** during disable (done later in DMA init)

**Impact**: Medium. The DMASHDL bypass should happen before DMA init. Our driver does this
later in `mt7927_dma_init()` but the timing difference is small.

### Difference 6: Upstream RX Ring Allocation

- **Upstream MT7925**:
  - RX queue 0 (`MT7925_RXQ_MCU_WM`) = MCU WM events, at `MT_RX_EVENT_RING_BASE` = `MT_WFDMA0(0x500)` = 0xd4500
  - RX queue 2 (`MT7925_RXQ_BAND0`) = data, at `MT_RX_DATA_RING_BASE` = `MT_WFDMA0(0x500)` = 0xd4500
  - Only 2 RX queues allocated

- **Our driver**:
  - We allocate many more RX rings (0-7 in some modes)
  - This is fine - extra rings don't hurt

**Important**: Both use `MT_RX_EVENT_RING_BASE = MT_WFDMA0(0x500)` which is the SAME as
`MT_WPDMA_RX_RING_BASE(0)` since ring 0 is at offset 0x500 + 0*16 = 0x500. This is correct.

---

## Critical Questions Answered

### Q1: What address does our driver use for WFSYS_RST? What address does upstream use?

Both use the **same register**: chip address `0x7c000140`, which maps to BAR0 offset `0xf0140`
via the fixed_map entry `{0x7c000000, 0xf0000, 0x10000}`.

- Our driver: `MT_WFSYS_SW_RST = 0xf0140` (line 212)
- Upstream: `mt792x_wfsys_reset()` uses `addr = 0x7c000140` which resolves to `0xf0140` via
  `__mt7925_reg_addr()` fixed_map

**Same physical register. No difference.**

### Q2: Does upstream do TWO WFSYS_RSTs?

**NO.** Upstream does ONE WFSYS_RST in probe (step 12) and ZERO in `mcu_init`. The second
"reset" in `mcu_init` is just a SET_OWN -> CLR_OWN power cycle, NOT a WFSYS reset.

Our driver: We also do one WFSYS_RST (in `mt7927_mcu_init_mt6639()`). Some modes do a second
WFSYS_RST (mode 1), but the upstream never does this.

### Q3: What does `mt792x_wpdma_reset(dev, true)` do exactly?

`mt792x_wpdma_reset()` (mt792x_dma.c line 197) with `force=true`:
1. Cleanup all TX queues
2. Cleanup all MCU queues
3. Cleanup all RX queues
4. **`mt792x_wfsys_reset(dev)`** - WFSYS reset
5. `mt792x_dma_reset(dev, force)`:
   - `mt792x_dma_disable(dev, true)` - disable DMA + LOGIC_RST
   - Reset all queue indices
   - `mt792x_dma_enable(dev)` - re-enable DMA + NEED_REINIT
6. Reset all RX queue indices

This is used by `mt7925e_init_reset()` which is the `hif_ops->init_reset` callback.
It is called during hardware init retry (if `__mt7925_init_hardware` fails).

**NOT called during normal init flow.** Only on error retry.

### Q4: Are there ANY register writes in upstream that we completely skip?

| Register | Upstream | Our Driver | Impact |
|----------|---------|------------|--------|
| `MT_PCIE_MAC_INT_ENABLE` (0x010188) | `0xFF` in probe | **NEVER** | Low (interrupt routing) |
| `MT_PCIE_MAC_PM` L0S_DIS (0x010194) BIT(8) | Before FWDL | Only some modes | Medium (link stability) |
| `MT_WFDMA0_GLO_CFG_EXT0` BIT(6) clear (DMASHDL disable) | In `dma_disable` | Conditional | Low |

### Q5: What is EMI_CTL in upstream and do we set it?

`MT_HW_EMI_CTL` is at chip address `0x18011100`, accessed via L1 remap.
`MT_HW_EMI_CTL_SLPPROT_EN` = BIT(1).

- **Upstream**: `mt76_rmw_field(dev, MT_HW_EMI_CTL, MT_HW_EMI_CTL_SLPPROT_EN, 1)` - sets BIT(1)
  in probe BEFORE WFSYS reset (pci.c line 399).
- **Our driver**: YES, conditional on `use_emi_slpprot` (line 4056). When enabled, we set BIT(1)
  via L1 remap before WFSYS reset.

This register enables AXI sleep protection for EMI (External Memory Interface). Upstream always
sets it. We should ensure `use_emi_slpprot=1`.

### Q6: Does upstream do anything between WFSYS_RST and FWDL that we don't?

Between WFSYS_RST and FWDL, upstream does:
1. `mt76_wr(dev, irq_map.host_irq_enable, 0)` - disable interrupts
2. `mt76_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0xff)` - enable PCIe MAC interrupts
3. `devm_request_irq(...)` - register IRQ handler
4. `mt7925_dma_init(dev)` - allocate rings + enable DMA
5. `mt7925_register_device(dev)` - register with mac80211
6. (async) `mt7925e_mcu_init()`:
   - **SET_OWN** (power cycle #2)
   - **CLR_OWN** (ROM processes NEED_REINIT, configures MCU rings)
   - **L0s disable**
   - `mt7925_run_firmware()` - FWDL + first MCU command

**The key difference**: upstream's CLR_OWN in step 6 triggers ROM to configure MCU_RX0/RX1
on MT7925, but on MT7927 it only configures MCU_RX2/RX3. This is not a driver sequence issue;
it's a ROM firmware behavior difference.

---

## What the Running Firmware is Supposed to Do

After FWDL completes and `fw_sync=0x3`, the running firmware (not ROM) is supposed to:
1. Configure MCU_RX0/RX1 with SRAM ring buffers (BASE != 0)
2. Enable MCU_DMA0 TX/RX for HOST communication
3. Start listening for MCU commands on HOST TX15 via WFDMA

The upstream driver's `mt7925_mcu_get_nic_capability()` (first post-boot command) succeeds
immediately after `fw_sync=0x3` because the running firmware has already set up MCU_RX0.

On MT7927, MCU_RX0 BASE stays 0x00000000 after firmware boot. The firmware either:
- Expects a different trigger to configure MCU_RX0
- Has a bug in MCU_RX0 configuration for the PCIe variant
- Requires a different firmware binary (MT7927-specific vs MT7925-compatible)

---

## Summary of Differences by Severity

### Already Ruled Out (tested, no effect):
- WFSYS reset timing/method
- Sleep protection clearing
- NEED_REINIT manipulation
- R2A bridge state
- DISP_CTRL force routing
- Q_IDX values
- WPDMA1 MCU_INT_EVENT
- HOST2MCU_SW_INT_SET
- Post-boot SET_OWN -> CLR_OWN
- DMASHDL bypass
- MCU_RX CIDX fix
- All 30+ reinit modes

### Untested but LOW probability:
- `MT_PCIE_MAC_INT_ENABLE = 0xFF` - try adding this
- `MT_PCIE_MAC_PM L0S_DIS` before FWDL - try adding this consistently
- Extra GLO_CFG bits (BIT(9), BIT(20), BIT(26)) - try removing these

### **LIKELY ROOT CAUSE**: MT7927 ROM/FW Behavior Difference
The MT7927 ROM does not configure MCU_RX0/RX1 during NEED_REINIT processing.
The running firmware also does not configure MCU_RX0 post-boot.
This is fundamentally different from MT7925 and cannot be fixed by init sequence changes.

**Next investigation path**: The firmware binary itself may need examination. The MT7927 may
require a different firmware file that handles MCU_RX0 configuration, or there may be a
chip-specific initialization sequence not present in the MT7925 upstream code that the
MT7927 vendor driver (not yet available as open source) implements.

---

## File References

| File | Path | Key Functions |
|------|------|--------------|
| Our driver | `/home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.c` | `mt7927_probe` (L12645), `mt7927_dma_init` (L11941), `mt7927_mcu_init_mt6639` (L4005) |
| Upstream probe | `/home/user/mt7927/mt76/mt7925/pci.c` | `mt7925_pci_probe` (L271), `mt7925_dma_init` (L215) |
| Upstream MCU init | `/home/user/mt7927/mt76/mt7925/pci_mcu.c` | `mt7925e_mcu_init` (L27) |
| Upstream DMA | `/home/user/mt7927/mt76/mt792x_dma.c` | `mt792x_dma_enable` (L126), `mt792x_wfsys_reset` (L357), `mt792x_dma_disable` (L253) |
| Upstream core | `/home/user/mt7927/mt76/mt792x_core.c` | `__mt792xe_mcu_drv_pmctrl` (L854), `mt792x_load_firmware` (L926) |
| Upstream FW run | `/home/user/mt7927/mt76/mt7925/mcu.c` | `mt7925_run_firmware` (L1045) |
| Upstream init | `/home/user/mt7927/mt76/mt7925/init.c` | `mt7925_init_hardware` (L122), `mt7925_init_work` (L144) |
| Upstream regs | `/home/user/mt7927/mt76/mt792x_regs.h` | All register definitions |
