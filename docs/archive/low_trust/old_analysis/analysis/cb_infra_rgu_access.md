# CB_INFRA_RGU PCIe Access Path Analysis

## Executive Summary

**All CB_INFRA_RGU registers are directly accessible from PCIe BAR0 with NO dynamic remapping needed.** The upstream mt7925 `fixed_map` includes a permanent mapping for the CBTOP region: `{0x70020000, 0x1f0000, 0x10000}` covering 64KB from bus 0x70020000-0x7002FFFF → BAR0 0x1f0000-0x1fffff.

**Critical finding**: The vendor mt6639 driver performs WFSYS reset (CB_INFRA_RGU BIT(4)) **BEFORE** firmware download, not after. Our driver skips this step entirely. Furthermore, the upstream `mt792x_wfsys_reset()` uses the **wrong register** for MT7927 — it writes to WF_WHOLE_PATH_RST[0] at 0x7c000140, but the vendor explicitly states: *"Falcon can't reset with WF_WHOLE_PATH_RST[0], reset with WF_SUBSYS_RST[4]."*

---

## 1. PCIe BAR0 Access Paths

### 1.1 Fixed Map (from upstream mt7925/pci.c `__mt7925_reg_addr()`)

The following fixed_map entries are relevant:

| Bus Range         | BAR0 Offset | Size    | Purpose                      |
|-------------------|-------------|---------|------------------------------|
| 0x70020000        | 0x1f0000    | 0x10000 | CBTOP (CB_INFRA_RGU, etc.)   |
| 0x81020000        | 0x0c0000    | 0x10000 | WF_TOP_MISC_ON               |
| 0x7c020000        | 0x0d0000    | 0x10000 | CONN_INFRA wfdma              |
| 0x7c060000        | 0x0e0000    | 0x10000 | conn_host_csr_top             |
| 0x7c000000        | 0x0f0000    | 0x10000 | CONN_INFRA                    |
| 0x80020000        | 0x0b0000    | 0x10000 | WF_TOP_MISC_OFF               |

### 1.2 Register → BAR0 Offset Translations

| Register Name                    | Bus Address  | Calculation                           | BAR0 Offset |
|----------------------------------|-------------|---------------------------------------|-------------|
| CB_INFRA_RGU_WF_SUBSYS_RST      | 0x70028600  | 0x1f0000 + (0x70028600 - 0x70020000)  | **0x1f8600** |
| CB_INFRA_RGU_SLP_PROT_RDY_STAT  | 0x70028730  | 0x1f0000 + (0x70028730 - 0x70020000)  | **0x1f8730** |
| WF_TOP_CFG_ON_ROMCODE_INDEX     | 0x81021604  | 0x0c0000 + (0x81021604 - 0x81020000)  | **0x0c1604** |
| CONN_INFRA_BUS_CR_PCIE2AP_REMAP | 0x7c021008  | 0x0d0000 + (0x7c021008 - 0x7c020000)  | **0x0d1008** |
| MT_WFSYS_SW_RST_B (upstream)    | 0x7c000140  | 0x0f0000 + (0x7c000140 - 0x7c000000)  | **0x0f0140** |

**Our driver already defines these correctly:**
```c
// tests/04_risky_ops/mt7927_init_dma.c lines 79-87
#define MT_CB_INFRA_RGU_WF_SUBSYS_RST  0x1f8600  // BAR0 offset
#define MT_WF_SUBSYS_RST_BIT           BIT(4)
#define MT_ROMCODE_INDEX               0xc1604   // BAR0 offset
#define MT_MCU_IDLE_VALUE              0x1D1E
```

These macros are defined but **never used** in any mode.

---

## 2. Vendor WFSYS Reset Sequence (Full Detail)

Source: `mt6639/chips/mt6639/hal_wfsys_reset_mt6639.c` (PCIe path, lines 68-210)

### 2.1 Reset Assert

```c
// Step 0: Acquire connectivity semaphore (CONN_SEMA00)
mt6639GetSemaphore(prAdapter);
  // Remaps PCIE2AP slot to 0x1807xxxx
  // Reads CONN_SEMA00_M0_OWN_STA at BAR0+0x40000 (remapped)
  // Restores original remap

// Step 1: Read-Modify-Write CB_INFRA_RGU_WF_SUBSYS_RST
u4AddrVal = CBTOP_RGU_BASE;  // = 0x70028600
HAL_MCR_RD(prAdapter, u4AddrVal, &u4CrVal);  // Read current
u4CrVal |= BIT(4);                            // Set WF_SUBSYS_RST
HAL_MCR_WR(prAdapter, u4AddrVal, u4CrVal);    // Write back
```

### 2.2 Poll for MCU Idle (ROM returns to idle after reset)

```c
// Step 2: Poll ROMCODE_INDEX for MCU_IDLE
while (TRUE) {
    HAL_MCR_RD(prAdapter, 0x81021604, &u4CrValue);  // WF_TOP_CFG_ON_ROMCODE_INDEX
    if (u4CrValue == 0xFFFFFFFF)       // MMIO read fail
        continue;
    else if (u4CrValue == 0x1D1E)      // MCU_IDLE
        break;
    kalMsleep(100);                     // 100ms between polls
    if (++u4ResetTimeCnt >= 2)          // Timeout after ~200ms
        break;
}
```

### 2.3 Reset De-Assert

```c
// Step 3: Clear WF_SUBSYS_RST bit
u4AddrVal = CBTOP_RGU_BASE;  // = 0x70028600
HAL_MCR_RD(prAdapter, u4AddrVal, &u4CrVal);
u4CrVal &= ~BIT(4);
HAL_MCR_WR(prAdapter, u4AddrVal, u4CrVal);
```

### 2.4 Semaphore Report (after de-assert)

```c
// Step 4: Check semaphore status (diagnostic)
mt6639GetSemaReport(prAdapter);
  // Reads CONN_SEMA_OWN_BY_M0_STA_REP_1 at BAR0+0x40400 (remapped)
```

### 2.5 CB_INFRA_RGU_WF_SUBSYS_RST Register Bits

From `mt6639/include/chips/coda/mt6639/cb_infra_rgu.h`:

| Bit  | Name                             | Purpose                              |
|------|----------------------------------|--------------------------------------|
| 0    | WF_WHOLE_PATH_RST                | **Doesn't work on Falcon/MT6639!**   |
| 1    | WF_PATH_BUS_RST                 | Bus path reset                       |
| 2    | WFSYS_PDN_RST_EN                | Power-down reset enable              |
| 3    | WF_CRYPTO_BYPASS_SUBSYS_RST     | Crypto bypass                        |
| **4**| **WF_SUBSYS_RST**                | **THE ONE TO USE for MT6639/MT7927** |
| 5    | WF_WHOLE_PATH_RST_REVERT_EN     | Auto-revert enable                   |
| 6    | BYPASS_WFDMA_SLP_PROT            | Bypass WFDMA sleep protection        |
| 7    | PAD_WF_SUBSYS_RST_EN            | Pad reset enable                     |
| 8-15 | WF_WHOLE_PATH_RST_REVERT_CYCLE  | Revert cycle count                   |
| 16   | BYPASS_WFDMA_2_SLP_PROT          | Bypass WFDMA2 sleep protection       |

---

## 3. Critical Timing: WFSYS Reset is BEFORE FW Download

### 3.1 Vendor Init Sequence (from vendor_post_boot_analysis.md)

```
1. CLR_OWN (driver ownership)
2. Initialize adapter (chip ID, MCR init, HIF init)
3. TX/RX init
4. halHifSwInfoInit → halWpdmaInitRing (HOST ring config)
5. HAL_ENABLE_FWDL (enable FW download mode)
6. *** WFSYS RESET (CB_INFRA_RGU BIT(4)) ***  ← THIS STEP
7. Get ECO version
8. Disable interrupts, FW download (patch + RAM + FW_START)
9. Poll fw_sync for 0x3
10. Send first MCU commands
```

### 3.2 Our Current Sequence (MISSING WFSYS RESET)

```
1. CLR_OWN (driver ownership)
2. DMA init (HOST ring config)
3. *** NO WFSYS RESET ***  ← MISSING!
4. FW download (patch + RAM + FW_START)
5. Poll fw_sync for 0x3
6. Try MCU command → FAILS (timeout, MCU_RX0 BASE=0)
```

### 3.3 Why This Matters

The WFSYS reset puts the WiFi subsystem into a known clean state. After reset:
- ROM code boots and reaches idle (ROMCODE_INDEX = 0x1D1E)
- All WFSYS internal state is reset
- After de-assert, FW download starts on a clean subsystem

Without this reset, we're downloading firmware onto a subsystem in an unknown state — possibly with stale configuration from a previous boot or the ROM's initial partial setup.

---

## 4. Upstream vs Vendor: Wrong Register for MT7927

### 4.1 Upstream `mt792x_wfsys_reset()` (mt76/mt792x_dma.c:357)

```c
int mt792x_wfsys_reset(struct mt792x_dev *dev)
{
    u32 addr = is_mt7921(&dev->mt76) ? 0x18000140 : 0x7c000140;
    mt76_clear(dev, addr, WFSYS_SW_RST_B);     // Clear BIT(0)
    msleep(50);
    mt76_set(dev, addr, WFSYS_SW_RST_B);       // Set BIT(0)
    // Poll WFSYS_SW_INIT_DONE (BIT(4)) at same address
}
```

- Uses **0x7c000140** = MT_WFSYS_SW_RST_B = BAR0 **0x0f0140**
- Controls **WF_WHOLE_PATH_RST[0]** and polls **WFSYS_SW_INIT_DONE[4]**

### 4.2 Vendor mt6639 PCIe Reset

- Uses **0x70028600** = CB_INFRA_RGU_WF_SUBSYS_RST = BAR0 **0x1f8600**
- Controls **WF_SUBSYS_RST[4]**
- Polls **ROMCODE_INDEX** at 0x81021604 (BAR0 **0x0c1604**) for 0x1D1E

### 4.3 Vendor Comment

> "Falcon can't reset with WF_WHOLE_PATH_RST[0], reset with WF_SUBSYS_RST[4]."

The upstream code is wrong for MT7927. The chip physically cannot use WF_WHOLE_PATH_RST.

---

## 5. Recommended Mode 40 Implementation

### 5.1 Pre-FW-Download WFSYS Reset Sequence

```c
// ============ Mode 40: CB_INFRA_RGU WFSYS Reset (pre-FWDL) ============

// Step 1: Read current CB_INFRA_RGU_WF_SUBSYS_RST
u32 rgu_val = mt7927_rr(dev, 0x1f8600);
dev_info(dev, "CB_INFRA_RGU_WF_SUBSYS_RST before: 0x%08x", rgu_val);

// Step 2: Assert WFSYS reset (set BIT(4))
rgu_val |= BIT(4);
mt7927_wr(dev, 0x1f8600, rgu_val);
dev_info(dev, "WFSYS reset asserted (BIT(4) set)");

// Step 3: Poll ROMCODE_INDEX for MCU_IDLE (0x1D1E)
// Timeout: 300ms (3 polls at 100ms each)
for (i = 0; i < 3; i++) {
    u32 rom_idx = mt7927_rr(dev, 0x0c1604);
    if (rom_idx == 0x1D1E) {
        dev_info(dev, "MCU_IDLE reached after %d polls", i);
        break;
    }
    if (rom_idx == 0xFFFFFFFF) {
        dev_info(dev, "MMIO read fail during reset poll");
    }
    msleep(100);
}

// Step 4: De-assert WFSYS reset (clear BIT(4))
rgu_val = mt7927_rr(dev, 0x1f8600);
rgu_val &= ~BIT(4);
mt7927_wr(dev, 0x1f8600, rgu_val);
dev_info(dev, "WFSYS reset de-asserted");

// Step 5: Small delay for subsystem stabilization
msleep(5);

// Step 6: Verify ROMCODE_INDEX
u32 rom_idx = mt7927_rr(dev, 0x0c1604);
dev_info(dev, "ROMCODE_INDEX after de-assert: 0x%08x", rom_idx);

// Step 7: Now proceed with normal init:
//   - DMA ring setup
//   - FW download
//   - MCU commands
```

### 5.2 Full Init Sequence for Mode 40

```
1. CLR_OWN (driver ownership)
2. ** WFSYS Reset: assert BIT(4) → poll MCU_IDLE → de-assert **
3. DMA ring setup (HOST rings)
4. FW download (patch + RAM + FW_START)
5. Poll fw_sync for 0x3
6. Send MCU command
```

### 5.3 Optional: Semaphore Acquisition

The vendor acquires CONN_SEMA00 before reset. This requires remapping PCIE2AP remap slot:
- Write to CONN_INFRA_BUS_CR_PCIE2AP_REMAP_WF_0_54 (BAR0 0xd1008)
- Remap slot to 0x1807xxxx for CONN_SEMA00 access
- Read BAR0+0x40000 to acquire semaphore
- Restore original remap

For a single-driver scenario, this may not be strictly necessary but is safer. Can be added as an enhancement if Mode 40 works without it.

### 5.4 Alternative: Reset AFTER FW Download

If pre-download reset alone doesn't fix MCU_RX0, try:
1. Pre-download WFSYS reset (Mode 40 above)
2. FW download
3. Post-download WFSYS reset (same sequence)
4. Re-download FW
5. MCU command

This double-reset approach ensures the subsystem is clean both before initial download and before the final boot.

---

## 6. Register Summary Table

| Purpose              | Bus Address  | BAR0 Offset | Access | Value/Bits |
|----------------------|-------------|-------------|--------|------------|
| WF_SUBSYS_RST        | 0x70028600  | 0x1f8600    | R/W    | BIT(4)     |
| ROMCODE_INDEX        | 0x81021604  | 0x0c1604    | R      | 0x1D1E = MCU_IDLE |
| SLP_PROT_RDY_STAT    | 0x70028730  | 0x1f8730    | R      | Sleep prot status |
| PCIE2AP_REMAP_WF_54  | 0x7c021008  | 0xd1008     | R/W    | Remap control |
| WFSYS_SW_RST_B       | 0x7c000140  | 0x0f0140    | R/W    | **WRONG for MT7927** |
| WF_SLP_CTRL          | 0x18001440  | 0xf1440     | R/W    | Sleep prot ctrl |
| WF_SLP_STATUS        | 0x18001444  | 0xf1444     | R      | Sleep prot status |

---

## 7. Key Risks and Considerations

1. **WFSYS reset clears DMA state**: Like CLR_OWN, this reset will zero HOST ring BASEs. Must reprogram rings AFTER the reset completes.

2. **Reset timing**: The vendor polls at 100ms intervals with a ~200ms timeout. This is generous; the reset should complete quickly.

3. **BIT(0) vs BIT(4)**: Using the wrong bit (WF_WHOLE_PATH_RST[0]) will not work on MT7927. Must use WF_SUBSYS_RST[4].

4. **BAR0 range**: 0x1f8600 requires BAR0 to be at least 2MB (0x200000). This is standard for CONNAC3X PCIe devices.

5. **No additional remap needed**: All registers are in the fixed_map, accessible directly via BAR0 offsets.
