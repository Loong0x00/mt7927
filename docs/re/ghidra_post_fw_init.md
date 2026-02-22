# Ghidra RE: mtkwecx.sys v5603998 — Post-FW Download Init Analysis

**Binary**: `DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/mtkwecx.sys`
**Version**: v5603998 (PE64, x86-64, Windows kernel driver)
**Source path found**: `e:\worktmp\easy-fwdrv-neptune-mp-mt7927_wifi_drv_win-2226-mt7927_2226_win10_ab\5999\wlan_driver\seattle\wifi_driver\windows\hal\chips\mtconnac3x.c`

## Key Function Map

| Function Name | Address | Purpose |
|---|---|---|
| AsicConnac3xPostFwDownloadInit | 0x1401c9510 | Post-FW boot initialization (MCU commands) |
| AsicConnac3xToggleWfsysRst | 0x1401cb360 | WFSYS reset toggle (CB_INFRA_RGU) |
| AsicConnac3xHalPollWfsysSwInitDone | 0x1401c3930 | Poll 0x81021604 == 0x1d1e |
| MT6639WpdmaConfig | 0x1401d8290 | WPDMA GLO_CFG enable |
| MT6639InitTxRxRing | 0x1401d6d30 | TX/RX ring setup (bus addresses) |
| AsicConnac3xWpdmaInitRing | 0x1401ccfe0 | Ring init dispatcher |
| AsicConnac3xLoadFirmware | 0x1401c5020 | FW download |
| AsicConnac3xSetCbInfraPcieSlpCfg | 0x1401cadb0 | CB_INFRA PCIe sleep config |
| AsicConnac3xLowPowerEnableInit | 0x1401c6d90 | Low power enable |
| MT7925L0WholeChipReset | 0x1401e4cc0 | Whole chip reset (MT7925 only) |

## Register Read/Write Functions

- **READ**: 0x140008f8c — `read_reg(ctx, bus_addr, &output)` → calls FUN_140052c14(pci_ctx, bus_addr, &output)
- **WRITE**: 0x140008ff8 — `write_reg(ctx, bus_addr, value)` → calls FUN_140052c14 variant

Both use a bus address space (0x7cXXXXXX, 0x70XXXXXX, 0x81XXXXXX) that is translated to MMIO by the PCI subsystem. This is NOT the same as direct BAR0 offsets.

## Critical Register Map (Bus Addresses)

| Bus Address | Name | Usage |
|---|---|---|
| **0x70028600** | CB_INFRA_RGU WF_SUBSYS_RST | WFSYS reset assert/deassert (6 occurrences) |
| **0x70028610** | CB_INFRA_RGU debug | Read for debug on reset failure |
| **0x70025018** | CB_INFRA_PCIE_SLP_CFG | Sleep config (written to 0xFFFFFFFF) |
| **0x7c060010** | CONN_INFRA_WFSYS_SW_RST | BIT(0)=assert, BIT(1)=deassert, BIT(2)=done |
| **0x7c060000** | CONN_INFRA_CFG_ON base | 2 occurrences |
| **0x7c011100** | CONN_INFRA_WAKEUP | OR BIT(1) before reset |
| **0x7c0114c0** | CONN_INFRA config | OR 0xc000e (MT7925 only) |
| **0x7c001600** | CONN_HIF sleep protection | AND ~0xF before reset |
| **0x7c001620** | CONN_HIF status 1 | Clear low 2 bits before reset |
| **0x7c001630** | CONN_HIF status 2 | Clear low 2 bits before reset |
| **0x7c026060** | WFDMA host enable? | OR 0x10101 (first call after FW boot) |
| **0x7c024208** | WFDMA HOST DMA0 GLO_CFG | OR 5 (BIT(0)\|BIT(2)) to enable |
| **0x7c0242b4** | WFDMA_GLO_CFG_EXT? | OR 0x10000000 (BIT(28)) |
| **0x7c024300** | TX Ring 0 BASE (WFDMA) | Host DMA0 |
| **0x7c024500+off** | RX Ring BASE (WFDMA) | Ring0=+0x40, Ring1=+0x60, Ring2=+0x70 |
| **0x7c027030** | Prefetch control | Read+Write |
| **0x7c0270f0-fc** | Prefetch config | Written: 0x660077, 0x1100, 0x30004f, 0x542200 |
| **0x81021604** | WFSYS_SW_INIT_DONE | Poll for value 0x1d1e |
| **0x81023f00** | Pre-reset register | Written 0xc0000100 before reset |
| **0x81023008** | Pre-reset register | Written 0 before reset |
| **0x54000000** | WF_WFDMA_MCU_DMA0 | 93 occurrences (MCU DMA base) |
| **0x55000000** | WF_WFDMA_MCU_DMA1 | 13 occurrences |
| **0x18400000** | WF_WFDMA_HOST_DMA0 (AXI) | 1 occurrence |
| **0x18500000** | WF_WFDMA_HOST_DMA1 (AXI) | 1 occurrence |

## Bus Address to BAR0 Offset Mapping

The WFDMA HOST DMA0 bus base = **0x7c024000**. In the AXI (mobile) variant, this is 0x18024000.

Register layout within HOST DMA0 (offset from 0x7c024000):
- +0x200 = INT_STA
- +0x204 = INT_ENA
- +0x208 = GLO_CFG (NOT 0x0d4!)
- +0x300 = TX Ring 0 BASE (NOT 0x500!)
- +0x310 = TX Ring 1 BASE
- +0x500+0x40 = RX Ring 0 BASE (NOT 0x2500!)
- +0x500+0x60 = RX Ring 1 BASE
- +0x500+0x70 = RX Ring 2 BASE

**IMPORTANT**: Our driver may be using WRONG offsets if they're from mt76/CONNAC2 layout instead of CONNAC3.

## AsicConnac3xToggleWfsysRst — Complete Sequence for MT6639/MT7927

```
1.  READ  0x7c011100, OR BIT(1), WRITE          — Wake CONN_INFRA
2.  Pre-reset helper (FUN_1401c1504):
    a. READ  0x7c001600, AND ~0xF, WRITE         — Clear sleep protection
    b. READ  0x7c001620, AND 0x3, WRITE if !=0   — Clear HIF status
    c. READ  0x7c001630, AND 0x3, WRITE if !=0   — Clear HIF status
3.  Driver own check (FUN_1401ccc7c)
4.  WRITE 0x81023f00 = 0xc0000100                — Pre-reset MCU register
5.  WRITE 0x81023008 = 0                          — Pre-reset MCU register
6.  READ  0x70028600                              — Get current CB_INFRA_RGU state
7.  Check if device flagged for stop → abort if so
8.  WRITE 0x70028600 |= BIT(4)                   — ASSERT WFSYS RESET
9.  Sleep 1000 (µs?)
10. READ  0x70028600 → verify BIT(4) still set   — Retry up to 5x if bit cleared
11. Sleep 20000 (µs?) — wait for reset completion
12. WRITE 0x70028600 &= ~BIT(4)                  — DEASSERT WFSYS RESET
13. Sleep 200 (µs?)
14. Poll  0x81021604 == 0x1d1e (500 iters, 100µs each) — WFSYS SW INIT DONE
15. On poll failure: READ 0x70028600, READ 0x70028610 for debug
16. WRITE 0x7c060010 = BIT(0)                    — Assert CONN_INFRA reset
17. Poll  0x7c060010 BIT(2) (up to 49 times)     — Wait for CONN_INFRA reset
18. WRITE 0x7c060010 = BIT(1)                    — Deassert CONN_INFRA reset
19. Additional status checks
```

Note: For MT7925, uses BIT(0) instead of BIT(4) at 0x70028600, plus clears BIT(6) and writes 0x7c0114c0.

## AsicConnac3xHalPollWfsysSwInitDone

```c
// Polls register 0x81021604 waiting for magic value 0x1d1e
// Up to 500 iterations with 100µs delay each = 50ms total timeout
do {
    READ(ctx, 0x81021604, &val);
    if (val == 0x1d1e) return SUCCESS;  // WFSYS init done!
    KeStallExecutionProcessor(10) * 10;  // 100µs
    retry++;
} while (retry <= 499);
return TIMEOUT;
```

## AsicConnac3xPostFwDownloadInit — Full Call Sequence

All of these are MCU commands sent AFTER FW is booted. The first register write is to 0x7c026060.

```
1.  Clear flag: *(ctx + 0x146e61c) = 0
2.  WRITE 0x7c026060 |= 0x10101              — BIT(0)|BIT(8)|BIT(16) — enable something
3.  MCU cmd (class=0x8a, target=0xed)          — Must succeed (sub2)
4.  MCU cmd (class=0x02, target=0xed)          — Data: {1, 0, 0x70000} (sub3)
5.  MCU cmd (class=0xc0, target=0xed)          — Data: {0x820cc800, 0x3c200}
6.  AsicConnac3xDownloadBufferBin              — Additional binary download (sub4)
7.  For MT6639/MT7927 only:
    MtCmdUpdateDBDCSetting (class=0x28)        — DBDC config (sub: MCU_cmd_0x7901)
8.  KeStallExecutionProcessor(10) * 100        — 1ms stall
9.  AsicConnac3xSetPassiveToActiveScan         — MCU cmd (class=0xca) (sub5)
10. AsicConnac3xSetFWChipConfig                — MCU cmd (class=0xca) (sub6)
11. AsicConnac3xSetLogLevelConfig              — MCU cmd (class=0xca) (sub7)
12. Additional MCU commands via function pointers
```

**KEY INSIGHT**: Every step after #2 is an MCU command. This means MCU commands WORK in the Windows driver immediately after FW boot. The only register write before MCU commands is 0x7c026060 |= 0x10101.

## MCU Command Routing

The MCU command sender FUN_1400c9468 has two paths:
- **CONNAC3 path** (FUN_1401481e4): Used when flag at ctx+0x146e621 == 1
- **Generic path** (FUN_1400c8b5c): Used otherwise

PostFwDownloadInit CLEARS this flag (ctx+0x146e621 = 0) at the very start.

## MT6639WpdmaConfig

```c
void MT6639WpdmaConfig(ctx, enable) {
    FUN_1401ccb54(ctx, 0, enable);  // Sub-setup
    READ(ctx, 0x7c024208, &glo_cfg);  // Read GLO_CFG
    if (enable) {
        // Prefetch config (if flag set):
        READ/WRITE(ctx, 0x7c027030);       // Re-write same value
        WRITE(ctx, 0x7c0270f0, 0x660077);  // Prefetch config
        WRITE(ctx, 0x7c0270f4, 0x1100);
        WRITE(ctx, 0x7c0270f8, 0x30004f);
        WRITE(ctx, 0x7c0270fc, 0x542200);
        // Enable DMA
        glo_cfg |= 5;  // BIT(0) | BIT(2) = TX_DMA_EN | RX_DMA_EN
        WRITE(ctx, 0x7c024208, glo_cfg);
    }
    READ(ctx, 0x7c0242b4, &val);
    val |= 0x10000000;  // BIT(28)
    WRITE(ctx, 0x7c0242b4, val);
}
```

## AsicConnac3xSetCbInfraPcieSlpCfg

```c
void SetCbInfraPcieSlpCfg(ctx) {
    READ(ctx, 0x70025018, &val);
    if (val != 0xFFFFFFFF) {  // Device present
        WRITE(ctx, 0x70025018, 0xFFFFFFFF);  // Disable all sleep
    }
}
```

## Complete Bus Address → BAR0 Offset Mapping

Using bus2chip tables from our driver source:

### Direct BAR0 Access (No Remap Needed)

| Bus Address | BAR0 Offset | bus2chip Entry | Register Name |
|---|---|---|---|
| 0x70028600 | **0x1f8600** | {0x70020000, 0x1f0000, 0x10000} | CB_INFRA_RGU WF_SUBSYS_RST |
| 0x70028610 | **0x1f8610** | same | CB_INFRA_RGU debug |
| 0x70025018 | **0x1f5018** | same | CB_INFRA PCIe sleep cfg |
| 0x81021604 | **0xc1604** | {0x81020000, 0xc0000, 0x10000} | WFSYS_SW_INIT_DONE |
| 0x81023f00 | **0xc3f00** | same | Pre-reset MCU register |
| 0x81023008 | **0xc3008** | same | Pre-reset MCU register |
| 0x7c060010 | **0xe0010** | {0x7c060000, 0xe0000, 0x10000} | CONN_INFRA_WFSYS_SW_RST |
| 0x7c001600 | **0xf1600** | {0x7c000000, 0xf0000, 0x10000} | Sleep protection enable |
| 0x7c001620 | **0xf1620** | same | HIF status 1 |
| 0x7c001630 | **0xf1630** | same | HIF status 2 |
| 0x7c026060 | **0xd6060** | {0x7c020000, 0xd0000, 0x10000} | **WFDMA enable (write 0x10101!)** |
| 0x7c024208 | **0xd4208** | same | WPDMA_GLO_CFG = MT_WFDMA0(0x208) ✓ |
| 0x7c0242b4 | **0xd42b4** | same | WPDMA_GLO_CFG_EXT |
| 0x7c024300 | **0xd4300** | same | TX Ring 0 BASE = MT_WFDMA0(0x300) ✓ |
| 0x7c027030 | **0xd7030** | same | Prefetch control |

### Needs L1 Remap (0x18XXXXXX AXI addresses)

| Bus Address | AXI Address | Register Name |
|---|---|---|
| 0x7c011100 | 0x18011100 | CONN_INFRA_WAKEUP_WF |
| 0x7c0114c0 | 0x180114c0 | CONN_INFRA config (MT7925 only) |

## IMMEDIATE ACTION ITEMS for Our Driver

### 1. Try BAR0+0xd6060 |= 0x10101 After FW Boot
This is the FIRST register write in PostFwDownloadInit. It might enable MCU command routing.
```c
uint32_t val = readl(mmio_base + 0xd6060);
writel(val | 0x10101, mmio_base + 0xd6060);  // BIT(0)|BIT(8)|BIT(16)
```

### 2. Read BAR0+0xc1604 to Check WFSYS Init Done
After FW boot (fw_sync=0x3), check if this reads 0x1d1e:
```c
uint32_t init_done = readl(mmio_base + 0xc1604);
// Should be 0x1d1e if WFSYS initialized properly
```

### 3. Full ToggleWfsysRst in BAR0 Offsets
```c
// Step 1: Wake CONN_INFRA (needs L1 remap for 0x18011100)
// Step 2: Clear sleep protection
uint32_t slp = readl(mmio_base + 0xf1600);
writel(slp & ~0xf, mmio_base + 0xf1600);
// Step 3-4: Clear HIF status
uint32_t hif1 = readl(mmio_base + 0xf1620);
if (hif1 & 3) writel(hif1 & 3, mmio_base + 0xf1620);
uint32_t hif2 = readl(mmio_base + 0xf1630);
if (hif2 & 3) writel(hif2 & 3, mmio_base + 0xf1630);
// Step 5-6: Pre-reset
writel(0xc0000100, mmio_base + 0xc3f00);
writel(0, mmio_base + 0xc3008);
// Step 7: Assert WFSYS reset
uint32_t rgu = readl(mmio_base + 0x1f8600);
writel(rgu | BIT(4), mmio_base + 0x1f8600);
// Step 8: Sleep 1ms
// Step 9: Verify BIT(4) still set (retry up to 5x)
// Step 10: Sleep 20ms
// Step 11: Deassert reset
rgu = readl(mmio_base + 0x1f8600);
writel(rgu & ~BIT(4), mmio_base + 0x1f8600);
// Step 12: Sleep 200µs
// Step 13: Poll WFSYS init done
for (int i = 0; i < 500; i++) {
    if (readl(mmio_base + 0xc1604) == 0x1d1e) break;
    udelay(100);
}
// Step 14-16: CONN_INFRA reset sequence
writel(BIT(0), mmio_base + 0xe0010);  // Assert
// Poll BIT(2) at 0xe0010
writel(BIT(1), mmio_base + 0xe0010);  // Deassert
```

## Chip ID Comparisons in Driver

The driver supports these chip IDs at offset ctx+0x1f72:
- **0x6639** — MT6639 (our chip!)
- **0x7927** — MT7927
- **0x738** — Unknown variant
- **0x7925** — MT7925
- **0x717** — Unknown variant

MT6639 and MT7927 always take the same code path (treated identically).

---

## DEEP ANALYSIS: MCU Command Send Paths

### MCU Command Dispatch (FUN_1400c9468 = `MtCmdSendSetQueryCmdDispatch`)

```c
void mcu_dispatch(ctx) {
    chip_id = *(short *)(ctx + 0x1f72);
    if (is_connac3(chip_id) && *(ctx + 0x146e621) == 1) {
        MtCmdSendSetQueryCmdHelper(args);  // CONNAC3 UniCmd path
    } else {
        MtCmdSendSetQueryCmdAdv(args);     // Generic/legacy path
    }
}
```

**Critical flag `ctx+0x146e621`**: Controls which MCU send path is used.
- `0` = Generic path (legacy TXD header, 0x40 byte total)
- `1` = CONNAC3 UniCmd path (UniCmd TXD header, 0x30 byte total)

In the WoWLAN power function (`NdisCommonHifPciSetPowerbyPortWOWLan`):
- Flag is CLEARED to 0 before ToggleWfsysRst
- `DAT_14024b439` is also cleared to 0 before reset, set to 1 after re-init

### Header Size Function (FUN_1401c14f0 = `AsicConnac3xGetHeaderSize`)

```c
int get_header_size(ctx) {
    if (*(ctx + 0x146e621) != 0)
        return 0x30;  // CONNAC3 UniCmd: 48 bytes
    else
        return 0x40;  // Legacy: 64 bytes
}
```

### Generic MCU Path (FUN_1400c8b5c = `MtCmdSendSetQueryCmdAdv`)

Parameters: `(ctx, class, target, subcmd, seq_ctl, flags, payload_ptr, payload_len, callback, event, extra)`

**Command Whitelist** (when flag_146e621==0 AND DAT_14024b439==0):
Only these target IDs are allowed: `[0x01, 0x02, 0x03, 0x05, 0x07, 0x10, 0x11, 0xee, 0xef]`
**Target 0xed is NOT in this list!** — but whitelist is SKIPPED when `DAT_14024b439 == 1`.

**TXD Header Format (flag_146e621 == 0, legacy mode)**:
```
Offset  Size  Value
+0x00   4     total_length | (param_2==0 ? 0x1800000 : 0x1000000) | 0x40000000
              total_length = payload_len + 0x40
+0x04   4     flags | 0x4000
...
+0x20   2     payload_len + 0x20 (packet length)
+0x22   2     (depends on target)
+0x24   1     class (e.g., 0x8a, 0x02, 0xc0, 0xca)
+0x25   1     0xa0 (except target==0xee → 0xa000 at +0x24..0x25)
+0x27   1     sequence number (from FUN_140098c18)
+0x2b   1     flags byte
+0x40   N     payload data (memcpy)
```

**TXD Header Format (flag_146e621 == 1, CONNAC3 mode)**:
```
Offset  Size  Value
+0x00   4     total_length | 0x1000000
              total_length = payload_len + 0x30
+0x04   4     flags | 0x10000
...
+0x20   2     payload_len + 0x10 (packet length)
+0x24   1     class
+0x25   1     0xa0
+0x26   1     param_5 (extra control)
+0x27   1     sequence number
+0x29   1     subcmd (for target==0xed)
+0x2a   2     0
+0x40   N     payload data (memcpy)
```

### CONNAC3 UniCmd Path (FUN_1401481e4 = `MtCmdSendSetQueryCmdHelper`)

This path:
1. Calls `FUN_140149218` (route lookup) → returns index into routing table or -1
2. Builds a linked list of sub-commands from the routing table
3. For each sub-command, calls `MtCmdSendSetQueryUniCmdAdv` (FUN_14014866c)
4. Each UniCmdAdv call ultimately calls the SAME `MtCmdEnqueueFWCmd` (FUN_1400c3e44)

### CONNAC3 UniCmd Enqueue (FUN_14014866c = `MtCmdSendSetQueryUniCmdAdv`)

**Guard check**: Rejects if `flag_146e621==0 AND field_1467374==2`

**TXD Built at pcVar4 (allocated buffer + 0x68)**:
```
Offset  Size  Value
+0x00   4     (header_size + payload_len) | 0x41000000  [BIT(30)|BIT(24)]
+0x04   4     flags | 0x4000
+0x20   2     header_size - 0x20 + payload_len
+0x22   2     class (e.g., 0x8a)
+0x25   1     0xa0
+0x27   1     sequence number
+0x2a   1     0
+0x2b   1     option flags: 2=default, |1=need_ack, |4=need_response
+0x30   N     payload (memcpy)
```

Header size from `get_header_size()`: 0x30 (CONNAC3) or 0x40 (legacy).

Then calls `MtCmdEnqueueFWCmd` to actually submit to TX ring.

### MCU TX Ring Selection (FUN_1400ce744 = `MtCmdStoreInfo`)

This function stores command info in a circular array (20 slots) and determines which TX ring to use. The ring index is selected by `FUN_1400c5dc4` based on the command class byte.

### CONNAC3 Command Routing Table (at 0x14023fcf0)

57 entries, each 13 bytes. Format: `{class, unk1, subcmd_flags, byte6, w3, w4, w5}`

Key entries matching PostFwDownloadInit commands:
| Index | Class | Usage in PostFwDownloadInit |
|-------|-------|---------------------------|
| [0]   | 0x8a  | sub2: NIC capability query (first MCU cmd) |
| [1]   | 0x02  | sub3: Config cmd (data={1, 0, 0x70000}) |
| [2]   | 0xc0  | Config cmd (data={0x820cc800, 0x3c200}) |
| [3]   | 0x28  | MtCmdUpdateDBDCSetting (MT6639/MT7927 only) |
| [4]   | 0xca  | SetPassiveToActiveScan, SetFWChipConfig, SetLogLevelConfig |

Target 0xed entries (matched by subcmd when target==0xed):
| Index | Subcmd | Description |
|-------|--------|-------------|
| [44]  | 0x8021 | |
| [45]  | 0xa094 | |
| [46]  | 0xf01e | |
| [47]  | 0x5081 | |
| [48]  | 0x803c | |
| [49]  | 0xe0a8 | |
| [50]  | 0x30bf | |
| [55]  | 0x6001 | |
| [56]  | 0xa04f | |

## PostFwDownloadInit — Complete MCU Command Detail

### Step 1: sub1 — Enable WFDMA
```c
READ(ctx, 0x7c026060, &val);   // BAR0+0xd6060
val |= 0x10101;                 // BIT(0)|BIT(8)|BIT(16)
WRITE(ctx, 0x7c026060, val);
```

### Step 2: sub2 — First MCU Command (NIC Capability)
```c
mcu_dispatch(ctx+0x14c0, class=0x8a, target=0xed, subcmd=0, seq_ctl=0, flags=5,
             payload=NULL, payload_len=0, callback=0, event=0, extra=0);
// After success, writes capability flags at ctx+0x1466b2b..0x1466b31
```

### Step 3: sub3 — Config Command
```c
struct { uint16 tag; uint8 pad; uint32 value; } payload = {1, 0, 0x70000};
mcu_dispatch(ctx+0x14c0, class=0x02, target=0xed, subcmd=0, ...
             payload=&payload, payload_len=0xc, ...);
```

### Step 4: MCU CMD class=0xc0
```c
uint32 data[2] = {0x820cc800, 0x3c200};
mcu_dispatch(ctx+0x14c0, class=0xc0, target=0xed, subcmd=0, seq_ctl=1, flags=8,
             payload=data, payload_len=8, ...);
```

### Step 5: DownloadBufferBin (FUN_1401c2448)
Opens a buffer file via NdisOpenFile, sends it in 1KB chunks via:
```c
mcu_dispatch(ctx+0x14c0, class=0xed, target=0xed, subcmd=0x21, ...);
```
Skipped if `*(ctx + 0x1467608) != 1`.

### Step 6: MtCmdUpdateDBDCSetting (MT6639/MT7927 only)
```c
mcu_dispatch(ctx+0x14c0, class=0x28, target=0xed, subcmd=0, ...);
// With 0x24-byte payload containing DBDC params
```

### Step 7: 1ms Stall
```c
KeStallExecutionProcessor(10) * 100;  // 100 × 10µs = 1ms
```

### Steps 8-10: Config MCU Commands
All use class=0xca, target=0xed:
- **SetPassiveToActiveScan**: String "PassiveToActiveScan" + config data (0x148 bytes)
- **SetFWChipConfig**: String config + 0x148 byte payload
- **SetLogLevelConfig**: String "EvtDrvnLogCatLvl" + format string

## Driver Architecture: Vtable-Based Init

MAIN_INIT_SEQUENCE (FUN_1401d66e4) populates a CONNAC3 ASIC ops vtable at 0x14024cbd0:

| Vtable Offset | Function | Name |
|---|---|---|
| +0x00 | FUN_1401d7ae0 | chip-specific setup |
| +0x08 | FUN_1401c6210 | pre-FW setup |
| +0x10 | FUN_1401c47b0 | DMA init |
| +0x18 | (zero) | |
| +0x20 | FUN_1401c5020 | **LoadFirmware** |
| +0x28 | FUN_1401c2e90 | post-FW intermediate |
| +0x30 | (zero) | |
| +0x38 | (zero) | |
| +0x40 | thunk_FUN_1400506ac | HIF init |
| +0x48 | FUN_1401c9510 | **PostFwDownloadInit** |
| +0x50 | FUN_1401c3240 | post-init |
| +0x58 | FUN_1401cb360 | **ToggleWfsysRst** |
| +0x68 | FUN_1401ceef0 | |
| +0x70 | FUN_1401c3b20 | |
| +0x78 | FUN_1401d6d30 | **InitTxRxRing** |
| +0x80 | FUN_1401d8290 | **WpdmaConfig** |
| +0x88 | FUN_1401ccda0 | WpdmaInitRing |
| +0x90 | FUN_1401d8180 | |
| +0x178 | FUN_1401c14f0 | **GetHeaderSize (PRE_MCU_SEND)** |

## KEY INSIGHTS FOR OUR DRIVER

### 1. PostFwDownloadInit clears ctx+0x146e61c, NOT ctx+0x146e621
The flag at offset 0x146e61c is cleared to 0. This is DIFFERENT from the MCU dispatch flag at 0x146e621.

### 2. MCU commands during PostFwDownloadInit likely use GENERIC path
Since flag_146e621 is cleared before ToggleWfsysRst in the WoWLAN resume flow, and PostFwDownloadInit only clears a DIFFERENT flag (0x146e61c), the MCU dispatch during PostFwDownloadInit depends on whether flag_146e621 was set before entering PostFwDownloadInit.

### 3. The GENERIC path header is DIFFERENT from CONNAC3 UniCmd
- Legacy TXD: 0x40 total header, payload at +0x40, length field = payload + 0x20
- CONNAC3 UniCmd: 0x30 header, payload at +0x30, length field = payload + 0x10
- Both have class at +0x24, type=0xa0 at +0x25, seq at +0x27

### 4. The actual TX ring enqueue is the SAME for both paths
Both converge on `MtCmdEnqueueFWCmd` (FUN_1400c3e44), which determines the TX ring index and submits. The ring index comes from the command class byte.

### 5. WFDMA enable (0xd6060 |= 0x10101) is the ONLY pre-MCU register write
No other register writes happen between FW boot and the first MCU command.

### 6. NdisCommonHifPciSetPowerbyPortWOWLan boot sequence
For resume from WoWLAN (calls ToggleWfsysRst):
```
1. Clear flag_146e621 = 0
2. Clear DAT_14024b439 = 0
3. ToggleWfsysRst() — full 16-step reset + poll WFSYS_SW_INIT_DONE
4. Clear error flags
5. Call FUN_1401f0be4() — main init (LoadFW + PostFwDownloadInit via vtable)
6. Set DAT_14024b439 = 1
```
