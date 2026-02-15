# zouyonghao MT7927 Driver Analysis

**Research Date**: 2026-02-15
**Repo**: https://github.com/zouyonghao/mt7927 (master branch, commit d33a2a8)
**Approach**: Modified mt7925e driver within mt76 out-of-tree framework
**Status**: HW init + FW load WORK, MCU commands FAIL (same blocker as us)

---

## Executive Summary

zouyonghao modified the Linux mt7925e (mt76) driver to support MT7927/MT6639. They achieved:
1. CBInfra remap configuration
2. WFSYS reset via CB_INFRA_RGU BIT(4) (same as our approach)
3. MCU_IDLE state (ROMCODE_INDEX = 0x1D1E)
4. Firmware loaded successfully (MAC from EFUSE visible)
5. MSI configuration (4 registers we DON'T configure)
6. GLO_CFG_EXT1/EXT2 configuration (we DON'T do this)

**They hit the EXACT same wall as us**: MCU commands time out, no interrupts received. MCU_RX0 never gets configured by firmware.

---

## Architecture: Modified mt76 Framework

zouyonghao took the entire mt76 kernel driver tree as out-of-tree module (`mt76-outoftree/`) and added MT7927-specific files:

```
mt76-outoftree/
  mt7925/           (base mt7925e driver)
    pci.c           (MODIFIED: MT7927 probe, WFSYS reset)
    pci_mcu.c       (MODIFIED: MT7927 MCU init)
    mt7927_ccif.c   (NEW: Cross-Core Interface stub)
    mt7927_regs.h   (NEW: MT6639 register definitions)
  mt7927_fw_load.c  (NEW: Polling-based firmware loader)
  mt792x_core.c     (MODIFIED: MT7927 dispatch)
```

Key design decisions:
- **Polling-based FW loader**: ROM bootloader does NOT support WFDMA mailbox protocol
- **No mailbox ACK**: Commands sent with `wait=false` (no response expected from ROM)
- **Skip FW_START**: Instead sets AP2WF SW_INIT_DONE bit (0x7C000140 |= BIT(4))

---

## Initialization Sequence (from their code)

### Phase 1: Probe & CBInfra Remap (pci.c)

```c
// Device ID table includes 0x7927
// In mt7925_pci_probe():
if (is_mt7927(&dev->mt76)) {
    // CBInfra remap - CRITICAL for register access
    mt76_wr(dev, 0x70026554, 0x74037001);  // CBTOP_PCIE_REMAP_WF
    mt76_wr(dev, 0x70026558, 0x70007000);  // CBTOP_PCIE_REMAP_WF_BT
}
```

### Phase 2: WFSYS Reset (mt7927_wfsys_reset in pci.c)

```c
// GPIO mode configuration
mt76_wr(dev, 0x7000535c, 0x80000000);  // GPIO_MODE5
mt76_wr(dev, 0x7000536c, 0x80);        // GPIO_MODE6

// BT subsystem reset (assert then deassert)
mt76_wr(dev, 0x70028610, 0x10351);     // BT assert
msleep(10);
mt76_wr(dev, 0x70028610, 0x10340);     // BT deassert

// WF subsystem reset via CB_INFRA_RGU BIT(4)
val = mt76_rr(dev, 0x70028600);
val &= ~0x10;       // clear BIT(4)
val |= (1 << 4);    // set BIT(4) = assert
mt76_wr(dev, 0x70028600, val);
// ... wait, then deassert ...
```

**Reset values**: Assert = 0x10351, Deassert = 0x10340

**NOTE**: They reset BOTH BT and WF subsystems! We only reset WF. This may be important - resetting BT first might clear shared infrastructure state.

### Phase 3: MCU Pre-Init (mt7927e_mcu_pre_init in pci_mcu.c)

```c
// 1. Force CONN_INFRA wakeup
//    Write to CONN_INFRA_CFG_PWRCTRL_L0 (0x7C0601A0)

// 2. Poll CONN_INFRA_CFG_VERSION for 0x03010002

// 3. WF subsystem reset assertion/deassertion

// 4. Configure crypto MCU ownership
//    Write to CB_INFRA_SLP_CTRL (0x70025380)

// 5. Poll MCU IDLE: ROMCODE_INDEX (0x81021604) == 0x1D1E
//    Timeout: 100ms
```

### Phase 4: MCU Post-DMA Init (mt7927e_mcu_init in pci_mcu.c)

**THIS IS WHERE THEY DO THINGS WE DON'T:**

```c
// 1. PCIe2AP Remap for MCU mailbox
mt76_wr(dev, 0x7C021034, 0x18051803);  // PCIE2AP_REMAP_WF_1_BA

// 2. MSI Configuration (CRITICAL - WE DON'T DO THIS)
//    Set single MSI mode in WFDMA_HOST_CONFIG
msi_val = (0 << 2) & 0x0C;  // Single MSI
mt76_wr(dev, 0x7C027030, msi_val);  // WFDMA_HOST_CONFIG

//    Ring-to-MSI mapping registers
mt76_wr(dev, 0x7C0270F0, 0x00660077);  // MSI_INT_CFG0
mt76_wr(dev, 0x7C0270F4, 0x00001100);  // MSI_INT_CFG1
mt76_wr(dev, 0x7C0270F8, 0x0030004F);  // MSI_INT_CFG2
mt76_wr(dev, 0x7C0270FC, 0x00542200);  // MSI_INT_CFG3

// 3. WFDMA Extension Configuration (WE DON'T DO THIS)
mt76_wr(dev, 0x7C0242B4, 0x8C800404 | BIT(31));  // GLO_CFG_EXT1 (TX flow ctrl)
mt76_wr(dev, 0x7C0242B8, 0x44);                   // GLO_CFG_EXT2 (perf monitor)

// 4. Moving average divisor for performance
mt76_wr(dev, 0x7C0270C0, 0x36);  // WFDMA_HIF_PERF_MAVG_DIV

// 5. RX ring thresholds
//    Multiple PAUSE_RX_Q_TH registers = 0x22

// 6. Delay interrupt configuration
mt76_wr(dev, 0x7C0242E8, 0x0F000008);  // HOST_PER_DLY_INT_CFG
mt76_wr(dev, 0x7C0270E8, 0x40654065);  // DLY_IDX_CFG ring 4-7

// 7. PCIe MAC interrupt routing (from gen4m mt6639)
//    Write 0x08021000 to 0x74030074
```

### Phase 5: Firmware Loading (mt7927_fw_load.c)

```c
// Polling-based, no mailbox protocol
// 1. Load patch (PATCH_START_REQ → scatter → PATCH_FINISH)
//    - NO semaphore (ROM doesn't support it)
//    - max_len = 4096
//    - 5ms delay between chunks
//    - Aggressive queue cleanup between sends

// 2. Load RAM regions
//    - For each region: TARGET_ADDRESS_LEN_REQ → scatter
//    - Aggressive cleanup between regions (10 iterations * 10ms)
//    - Skip FW_START command (mailbox not supported)

// 3. Signal host ready
mt76_wr(dev, 0x7C000140, val | BIT(4));  // AP2WF SW_INIT_DONE

// 4. Check MCU ready
val = mt76_rr(dev, 0x7C0600F0);  // fw_sync register
```

### Phase 6: CCIF Init (mt7927_ccif.c) - Stub

```c
// Minimal stub - no real modem shared memory
mt76_wr(dev, 0x1b5180, 0x18051803);    // PCIE2AP remap for CCIF
mt76_wr(dev, 0x1a0014, 0xFFFFFFFF);    // Clear CCIF ACK
mt76_wr(dev, 0x1a001C, 0xFFFFFFFF);    // Clear CCIF RCHNUM_ACK
mt76_wr(dev, 0x1a0180, 0xFFFFFFFF);    // Enable IRQ0_MASK
mt76_wr(dev, 0x1a0184, 0xFFFFFFFF);    // Enable IRQ1_MASK
```

---

## Register Map from mt7927_regs.h

### Reset Registers (CB_INFRA_RGU)
| Register | Bus Address | Value |
|----------|-------------|-------|
| WF_SUBSYS_RST | 0x70028600 | Assert: 0x10351, Deassert: 0x10340 |
| BT_SUBSYS_RST | 0x70028610 | Assert: 0x10351, Deassert: 0x10340 |

**WF_SUBSYS_RST bit fields:**
- BIT(0): WF_WHOLE_PATH_RST (DO NOT USE on PCIe — "Falcon can't reset")
- BIT(1): WF_PATH_BUS_RST
- BIT(2): WFSYS_PDN_RST_EN
- BIT(3): WF_CRYPTO_BYPASS_SUBSYS_RST
- BIT(4): WF_SUBSYS_RST (THE ONE TO USE)
- BIT(5): WF_WHOLE_PATH_RST_REVERT_EN
- BIT(6): BYPASS_WFDMA_SLP_PROT
- BIT(7): PAD_WF_SUBSYS_RST_EN
- BIT(8-15): WF_WHOLE_PATH_RST_REVERT_CYCLE
- BIT(16): BYPASS_WFDMA_2_SLP_PROT

### CBInfra Remap
| Register | Bus Address | Value |
|----------|-------------|-------|
| CBTOP_PCIE_REMAP_WF | 0x70026554 | 0x74037001 |
| CBTOP_PCIE_REMAP_WF_BT | 0x70026558 | 0x70007000 |
| PCIE2AP_REMAP_WF_1_BA | 0x7C021034 | 0x18051803 |

### MSI Configuration
| Register | Bus Address | Value |
|----------|-------------|-------|
| WFDMA_HOST_CONFIG | 0x7C027030 | 0x00 (single MSI) |
| MSI_INT_CFG0 | 0x7C0270F0 | 0x00660077 |
| MSI_INT_CFG1 | 0x7C0270F4 | 0x00001100 |
| MSI_INT_CFG2 | 0x7C0270F8 | 0x0030004F |
| MSI_INT_CFG3 | 0x7C0270FC | 0x00542200 |

### WFDMA Extension
| Register | Bus Address | Value |
|----------|-------------|-------|
| GLO_CFG_EXT0 | 0x7C0242B0 | 0x28C004DF (gen4m) |
| GLO_CFG_EXT1 | 0x7C0242B4 | 0x8C800404 \| BIT(31) |
| GLO_CFG_EXT2 | 0x7C0242B8 | 0x44 |
| HIF_PERF_MAVG_DIV | 0x7C0270C0 | 0x36 |
| HOST_PER_DLY_INT_CFG | 0x7C0242E8 | 0x0F000008 |
| DLY_IDX_CFG_0 | 0x7C0270E8 | 0x40654065 |

### Other Key Registers
| Register | Bus Address | Value/Notes |
|----------|-------------|-------------|
| GPIO_MODE5 | 0x7000535C | 0x80000000 |
| GPIO_MODE6 | 0x7000536C | 0x80 |
| CONN_INFRA_VERSION | 0x7C011000 | 0x03010002 |
| ROMCODE_INDEX | 0x81021604 | MCU_IDLE = 0x1D1E |
| CRYPTO_MCU_OWN_SET | 0x70025380 | Ownership register |
| AP2WF_BUS_STATUS | 0x7C000140 | BIT(4) = SW_INIT_DONE |
| FW_SYNC | 0x7C0600F0 | Poll for value 3 |
| PCIe MAC INT | 0x74030074 | 0x08021000 |

---

## Comparison: zouyonghao vs Our Driver

### What zouyonghao Does That We DON'T

| Feature | zouyonghao | Our Driver | Priority |
|---------|-----------|-----------|----------|
| MSI_INT_CFG0-3 | Writes 4 MSI regs | Not written | **HIGH** |
| WFDMA_HOST_CONFIG | Sets MSI mode | Not written | **HIGH** |
| GLO_CFG_EXT1 | 0x8C800404\|BIT(31) | Not written | **HIGH** |
| GLO_CFG_EXT2 | 0x44 | Not written | MEDIUM |
| PCIe2AP Remap | 0x18051803 | Not written | **HIGH** |
| BT subsystem reset | Reset BT first | Only WF reset | MEDIUM |
| GPIO mode config | 0x7000535c/536c | Not done | LOW |
| Delay interrupt cfg | PER_DLY_INT + DLY_IDX | Not done | MEDIUM |
| RX ring thresholds | 0x22 per ring | Not done | LOW |
| PCIe MAC INT route | 0x74030074=0x08021000 | Not done | MEDIUM |
| CCIF init stub | ACK + IRQ mask | Not done | LOW |

### What We Do That zouyonghao DOESN'T

| Feature | Our Driver | zouyonghao | Notes |
|---------|-----------|-----------|-------|
| FW_START_OVERRIDE | Yes (option=1) | No (skips) | We actually send FW_START |
| Patch semaphore | Yes | No (skips) | ROM doesn't respond |
| Encryption flags | DL_MODE_ENCRYPT | Not seen | We add encryption flags |
| DMASHDL enable | 0xd6060 \|= 0x10101 | Not seen | We enable DMA scheduler |
| PostFwDownloadInit | Partially | Not at all | Neither has full sequence |

### Where We're BOTH Missing (from Windows RE)

| Feature | Windows Driver | Status |
|---------|---------------|--------|
| PostFwDownloadInit 9 MCU commands | All 9 commands | Neither driver implements |
| Full DMASHDL configuration | Extensive config | Both minimal |
| gen4m ring layout (17 TX, 12 RX) | Full ring setup | Both use mt76 ring layout |
| Command target 0xed | Used for all MCU cmds | Neither uses this |
| MtCmdSendSetQueryCmdAdv header | 0x40-byte header with 0xa0 marker | Neither implements |
| MT6639InitTxRxRing | Separate post-FWDL ring init | Neither has this |

---

## Key Observations

### 1. MSI Configuration May Be Critical
zouyonghao configures MSI but STILL gets no interrupts. This suggests MSI alone is not the fix. However, we should still add it since both the Windows driver and gen4m vendor driver configure these registers.

### 2. Both Drivers Skip PostFwDownloadInit
Neither zouyonghao nor we implement the Windows-specific PostFwDownloadInit sequence (9 MCU commands with target 0xed). This is likely the REAL missing piece. The firmware boots but never transitions to operational state because it never receives the initialization commands.

### 3. BT Reset Before WF Reset
zouyonghao resets BT subsystem BEFORE WF subsystem. The gen4m vendor driver may also do this. We only reset WF. Resetting BT first might clear shared CONN_INFRA state.

### 4. Firmware Loading Approach Differs
zouyonghao skips FW_START and sets SW_INIT_DONE instead. We send FW_START_OVERRIDE with option=1. Both reach fw_sync=0x3, suggesting both approaches work for getting FW running. But the FW may behave differently depending on how it was started.

### 5. GLO_CFG_EXT1 Has TX Flow Control
The value 0x8C800404 with BIT(31) enables TX flow control mode. Without this, the WFDMA may not properly handle TX→MCU data flow, which would explain why MCU commands never get delivered.

### 6. gen4m Confirms PCIe Must Use BIT(4)
The Motorola gen4m vendor code explicitly says: "Falcon can't reset with WF_WHOLE_PATH_RST[0], reset with WF_SUBSYS_RST[4]". This confirms our MEMORY.md note.

---

## Community Comparison Summary

| Project | Approach | WFSYS Reset | FW Load | MCU Cmds | Blocker |
|---------|----------|-------------|---------|----------|---------|
| **zouyonghao** | mt76/mt7925e mod | CB_INFRA_RGU BIT(4) | Works | Timeout | No interrupts |
| **Our project** | Bare kernel module | CB_INFRA_RGU BIT(4) | Works | Timeout | MCU_RX0 BASE=0 |
| **ehausig** | Same as ours | Same | Same | Same | Same (is our repo) |
| **Loong0x00** | Windows RE only | N/A | N/A | N/A | DMA issues |
| **gen4m (vendor)** | MTK vendor driver | BIT(4) for PCIe | Compiles | Doesn't work | Init sequence |
| **nvaert1986** | BT only (USB) | WF_WHOLE_PATH_RST | BT works | BT timeout | FUNC_CTRL opcode |
| **jfmarliere** | BT kernel patch | N/A | BT works | BT works | N/A (SOLVED) |

---

## Recommended Actions

### Immediate (High Priority)
1. **Add MSI configuration** to our driver (4 registers from zouyonghao)
2. **Add GLO_CFG_EXT1** write (0x8C800404 | BIT(31)) for TX flow control
3. **Add PCIe2AP remap** (0x7C021034 = 0x18051803)
4. **Add WFDMA_HOST_CONFIG** write for MSI mode

### Next Steps (Medium Priority)
5. **Add BT subsystem reset** before WF reset (zouyonghao's sequence)
6. **Add GLO_CFG_EXT0/EXT2** writes
7. **Add delay interrupt configuration**
8. **Add PCIe MAC interrupt routing** (0x74030074 = 0x08021000)

### Critical Path (The Real Fix)
9. **Implement PostFwDownloadInit MCU commands** - this is what BOTH zouyonghao and we are missing. The firmware boots but never gets told to initialize operational mode.
10. **Study the 0xed command format** from Windows RE more carefully
11. **Compare ring layout**: gen4m uses Ring 6 for events (MCU responses), not Ring 0

---

## Appendix: ehausig/mt7927 (Our Own Upstream Repo)

ehausig/mt7927 IS our own project repo. Same codebase (tests/04_risky_ops/mt7927_init_dma.c), same blocker. 2 commits, on indefinite hold. No additional insights beyond what we already know.

---

## Appendix: nvaert1986 BT Driver Analysis

nvaert1986's BT driver (btmtk_usb_mt6639.c) is for USB interface only. Key findings:
- Uses WF_WHOLE_PATH_RST for USB (different from PCIe BIT(4))
- BT firmware loads successfully (2 sections, 118KB)
- Stuck at WMT FUNC_CTRL (opcode 0x06) timeout
- No WiFi-related code or observations
- Later superseded by jfmarliere's simpler kernel patch approach

The BT and WiFi paths are completely separate (USB vs PCIe) and share no useful code patterns.
