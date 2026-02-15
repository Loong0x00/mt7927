# Vendor MT6639 Post-Boot MCU Ring Configuration Analysis

Date: 2026-02-15
Analyzed codebase: `/home/user/mt7927/mt6639/` (MediaTek vendor mobile WiFi driver for MT6639/CONNAC3X)

## Executive Summary

**KEY FINDING: The vendor driver does NOT write MCU_RX ring BASE registers at any point.
The vendor driver only configures HOST-side RX and TX rings. MCU-side RX rings (MCU_RX0,
MCU_RX1, etc.) are configured entirely by the FIRMWARE after boot, not by the driver.**

The vendor driver's complete initialization sequence is:
1. WFSYS subsystem reset (assert + poll MCU_IDLE + de-assert)
2. `halWpdmaInitRing()` -- configures HOST TX rings and HOST RX rings only
3. Firmware download via FWDL ring
4. FW_START command
5. Poll `sw_ready_bits` (WIFI_FUNC_INIT_DONE | WIFI_FUNC_N9_DONE = 0x3)
6. **Immediately** send `wlanQueryNicResourceInformation` as the first MCU command
7. Send `wlanQueryNicCapabilityV2`

There is NO intermediate step between FW ready and the first MCU command.
The FW itself must configure MCU_RX0/RX1 during its boot process, before asserting
the ready bits.

---

## 1. Vendor Driver Initialization Flow

### 1.1 Complete Boot Sequence (wlanAdapterStart)

File: `/home/user/mt7927/mt6639/common/wlan_lib.c` (line 1080)

```c
uint32_t wlanAdapterStart(IN struct ADAPTER *prAdapter,
                          IN struct REG_INFO *prRegInfo,
                          IN const u_int8_t bAtResetFlow)
{
    // Step 1: Allocate adapter memory
    nicAllocateAdapterMemory(prAdapter);

    // Step 2: Acquire driver ownership (CLR_OWN equivalent)
    ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
    nicpmSetDriverOwn(prAdapter);

    // Step 3: Initialize adapter (chip ID verify, MCR init, HIF init)
    nicInitializeAdapter(prAdapter);

    // Step 4: wlanOnPostNicInitAdapter - TX/RX init
    //   nicTxInitialize(), nicRxInitialize()

    // Step 5: halHifSwInfoInit() -- THIS IS WHERE RINGS ARE CONFIGURED
    //   Calls halWpdmaAllocRing() + halWpdmaInitRing()
    halHifSwInfoInit(prAdapter);

    // Step 6: Enable FWDL mode
    HAL_ENABLE_FWDL(prAdapter, TRUE);

    // Step 7: WFSYS Reset (MT6639 specific)
    mt6639HalCbInfraRguWfRst(prAdapter, TRUE);   // Assert reset
    mt6639HalPollWfsysSwInitDone(prAdapter);      // Poll MCU_IDLE=0x1D1E
    mt6639HalCbInfraRguWfRst(prAdapter, FALSE);   // De-assert reset

    // Step 8: Get ECO version
    wlanSetChipEcoInfo(prAdapter);

    // Step 9: Disable interrupts, FW download
    nicDisableInterrupt(prAdapter);
    nicTxInitResetResource(prAdapter);
    wlanDownloadFW(prAdapter);  // patch + RAM code + FW_START

    // Step 10: Check FW ready bits
    wlanCheckWifiFunc(prAdapter, TRUE);
    // Polls sw_sync0 (0x7C0600F0) for bits 0x3

    // Step 11: IMMEDIATELY send first MCU commands
    prAdapter->fgIsFwDownloaded = TRUE;
    wlanQueryNicResourceInformation(prAdapter);

    // For CONNAC3X: NO wlanSendDummyCmd() needed!
    // (guarded by #if CFG_SUPPORT_CONNAC3X == 0)

    wlanQueryNicCapabilityV2(prAdapter);

    // Step 12: Enable interrupts, start normal operation
    nicEnableInterrupt(prAdapter);
}
```

### 1.2 Critical Observation: No Dummy Command for CONNAC3X

File: `/home/user/mt7927/mt6639/common/wlan_lib.c` (line 1260)

```c
#if (CFG_SUPPORT_CONNAC3X == 0)
    /* 2.9 Workaround for Capability CMD packet lost issue */
    wlanSendDummyCmd(prAdapter, TRUE);
#endif
```

For CONNAC3X (MT6639/MT7927), the vendor driver skips the dummy command entirely.
Furthermore, `wlanQueryNicResourceInformation` does NOT send any MCU command --
it only calls `nicTxResetResource()` locally. The ACTUAL first MCU command is
`wlanQueryNicCapabilityV2` (CMD_ID_GET_NIC_CAPABILITY_V2), sent via HOST TX ring 15.
This means the FW must already have MCU_RX0 configured and ready to receive
HOST TX ring 15 commands by the time it asserts the ready bits.

---

## 2. halWpdmaInitRing -- HOST Ring Configuration Only

File: `/home/user/mt7927/mt6639/os/linux/hif/common/hal_pdma.c` (line 2129)

```c
void halWpdmaInitRing(struct GLUE_INFO *prGlueInfo)
{
    // 1. pdmaSetup(FALSE) -- disable TX/RX DMA
    prBusInfo->pdmaSetup(prGlueInfo, FALSE);

    // 2. Program HOST TX ring BASEs
    halWpdmaInitTxRing(prGlueInfo);

    // 3. Program HOST RX ring BASEs
    halWpdmaInitRxRing(prGlueInfo);

    // 4. Set manual prefetch offsets
    prBusInfo->wfdmaManualPrefetch(prGlueInfo);

    // 5. pdmaSetup(TRUE) -- enable TX/RX DMA
    prBusInfo->pdmaSetup(prGlueInfo, TRUE);

    // 6. Write sleep mode magic to dummy reg
    prBusInfo->setDummyReg(prGlueInfo);
}
```

### 2.1 HOST TX Rings Programmed

File: `/home/user/mt7927/mt6639/os/linux/hif/common/hal_pdma.c` (line 2159)

The driver programs these HOST TX rings:
- TX_RING_DATA0_IDX_0 (ring 0) -- Data band0 TX
- TX_RING_DATA1_IDX_1 (ring 1) -- Data band1 TX
- TX_RING_CMD_IDX_2 (mapped to HW ring 15) -- MCU commands
- TX_RING_FWDL_IDX_3 (mapped to HW ring 16) -- FW download

For each: writes BASE address, CIDX=0, and ring size to CNT register.

### 2.2 HOST RX Rings Programmed (mt6639SetRxRingHwAddr)

File: `/home/user/mt7927/mt6639/chips/mt6639/mt6639.c` (line 461)

```c
static uint8_t mt6639SetRxRingHwAddr(struct RTMP_RX_RING *prRxRing,
        struct BUS_INFO *prBusInfo, uint32_t u4SwRingIdx)
{
    switch (u4SwRingIdx) {
    case RX_RING_EVT_IDX_1:        offset = 6 * MT_RINGREG_DIFF; break;
    case RX_RING_DATA_IDX_0:       offset = 4 * MT_RINGREG_DIFF; break;
    case RX_RING_DATA1_IDX_2:      offset = 5 * MT_RINGREG_DIFF; break;
    case RX_RING_TXDONE0_IDX_3:    offset = 7 * MT_RINGREG_DIFF; break;
    }
}
```

The HOST RX ring mapping for MT6639:
- RX_RING_DATA_IDX_0 -> HW RX Ring 4 (Band0 Rx Data)
- RX_RING_DATA1_IDX_2 -> HW RX Ring 5 (Band1 Rx Data)
- RX_RING_EVT_IDX_1 -> HW RX Ring 6 (Tx Free Done / Rx Events)
- RX_RING_TXDONE0_IDX_3 -> HW RX Ring 7 (Band1 Tx Free Done)

**NOTE: These are all HOST RX rings (RX ring 4-7), NOT MCU RX rings (0-3).
The vendor driver NEVER directly programs MCU_RX ring registers.**

---

## 3. WFSYS Reset Sequence

File: `/home/user/mt7927/mt6639/chips/mt6639/hal_wfsys_reset_mt6639.c`

### 3.1 PCIe Path - Assert/De-Assert

```c
u_int8_t mt6639HalCbInfraRguWfRst(struct ADAPTER *prAdapter,
                                    u_int8_t fgAssertRst)
{
    u4AddrVal = CBTOP_RGU_BASE;  // 0x70028600 (for PCIe)

    if (fgAssertRst) {
        HAL_MCR_RD(prAdapter, u4AddrVal, &u4CrVal);
        u4CrVal |= BIT(4);  // WF_SUBSYS_RST bit
        HAL_MCR_WR(prAdapter, u4AddrVal, u4CrVal);
    } else {
        HAL_MCR_RD(prAdapter, u4AddrVal, &u4CrVal);
        u4CrVal &= ~BIT(4);
        HAL_MCR_WR(prAdapter, u4AddrVal, u4CrVal);
    }
}
```

### 3.2 Poll for MCU Idle After Reset

```c
u_int8_t mt6639HalPollWfsysSwInitDone(struct ADAPTER *prAdapter)
{
    while (TRUE) {
        HAL_MCR_RD(prAdapter, 0x81021604, &u4CrValue);
        if (u4CrValue == 0x1D1E) {  // MCU_IDLE
            break;
        }
        kalMsleep(100);
        if (u4ResetTimeCnt >= 2) {
            fgSwInitDone = FALSE;
            break;
        }
    }
}
```

### 3.3 USB Path (for reference)

For USB, the driver uses `CB_INFRA_RGU_WF_SUBSYS_RST_ADDR` (0x70028600) with
`WF_WHOLE_PATH_RST` bit [0] = 0x00000001.

**IMPORTANT: For PCIe on MT6639, the driver uses BIT(4) = WF_SUBSYS_RST, NOT BIT(0) = WF_WHOLE_PATH_RST.**
The comment says: "Falcon can't reset with WF_WHOLE_PATH_RST[0], reset with WF_SUBSYS_RST[4]."

### 3.4 CB_INFRA_RGU Register Map

File: `/home/user/mt7927/mt6639/include/chips/coda/mt6639/cb_infra_rgu.h`

```
CB_INFRA_RGU_BASE = 0x70028000

WF_SUBSYS_RST register = CB_INFRA_RGU_BASE + 0x600 = 0x70028600

Bit layout of WF_SUBSYS_RST:
  [0]  WF_WHOLE_PATH_RST         - Full WiFi path reset
  [1]  WF_PATH_BUS_RST           - Bus path reset
  [2]  WFSYS_PDN_RST_EN          - Power-down reset enable
  [3]  WF_CRYPTO_BYPASS_SUBSYS_RST
  [4]  WF_SUBSYS_RST             - WiFi subsystem reset (USED BY MT6639 PCIe)
  [5]  WF_WHOLE_PATH_RST_REVERT_EN
  [6]  BYPASS_WFDMA_SLP_PROT     - Bypass WFDMA sleep protection
  [7]  PAD_WF_SUBSYS_RST_EN
  [8:15] WF_WHOLE_PATH_RST_REVERT_CYCLE
  [16] BYPASS_WFDMA_2_SLP_PROT
```

For our PCIe MT7927 driver, the relevant register is at bus address 0x70028600
(through remap). The vendor uses BIT(4) for PCIe reset.

---

## 4. WFDMA NEED_REINIT Handshake (Deep Sleep Re-Init)

File: `/home/user/mt7927/mt6639/chips/common/cmm_asic_connac3x.c` (line 304)

### 4.1 DUMMY_CR and NEED_REINIT

```c
// DUMMY_CR = CONNAC3X_MCU_WPDMA_0_BASE + 0x120 = 0x54000120
//   (maps to bus addr 0x02120 via bus2chip: 0x54000000 -> 0x02000)
// NEED_REINIT_BIT = BIT(1)

void asicConnac3xWfdmaDummyCrWrite(struct ADAPTER *prAdapter)
{
    HAL_MCR_RD(prAdapter, CONNAC3X_WFDMA_DUMMY_CR, &u4RegValue);
    u4RegValue |= CONNAC3X_WFDMA_NEED_REINIT_BIT;  // Set BIT(1)
    HAL_MCR_WR(prAdapter, CONNAC3X_WFDMA_DUMMY_CR, u4RegValue);
}

void asicConnac3xWfdmaDummyCrRead(struct ADAPTER *prAdapter, u_int8_t *pfgResult)
{
    HAL_MCR_RD(prAdapter, CONNAC3X_WFDMA_DUMMY_CR, &u4RegValue);
    // Result TRUE if NEED_REINIT_BIT is CLEARED (== 0)
    *pfgResult = (u4RegValue & CONNAC3X_WFDMA_NEED_REINIT_BIT) == 0 ? TRUE : FALSE;
}
```

### 4.2 WFDMA Re-Init After Deep Sleep

```c
void asicConnac3xWfdmaReInit(struct ADAPTER *prAdapter)
{
    // Check if NEED_REINIT bit has been consumed (cleared by ROM)
    asicConnac3xWfdmaDummyCrRead(prAdapter, &fgResult);

    if (fgResult) {
        // NEED_REINIT was consumed - just reset TX ring software indices
        for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
            prHifInfo->TxRing[u4Idx].TxSwUsedIdx = 0;
            prHifInfo->TxRing[u4Idx].u4UsedCnt = 0;
            prHifInfo->TxRing[u4Idx].TxCpuIdx = 0;
        }
        // Check for pending RX events, process them
        // Note: Does NOT re-program ring BASE registers!
    }
}
```

**This is significant**: After deep sleep re-init via the FW backup/restore solution,
the vendor driver does NOT re-program ring BASEs. It only resets software indices.
This confirms that the FW (via the CLR_OWN / NEED_REINIT mechanism) handles
re-programming the hardware ring BASEs, and the driver just needs to sync its
software state.

### 4.3 Initialization of NEED_REINIT Handshake

At `halHifSwInfoInit` (line 1273):
```c
if ((prChipInfo->asicWfdmaReInit) && (prChipInfo->asicWfdmaReInit_handshakeInit))
    prChipInfo->asicWfdmaReInit_handshakeInit(prAdapter);
```

This calls `asicConnac3xWfdmaDummyCrWrite()` which SETS NEED_REINIT BIT(1).
This tells the ROM that on the next CLR_OWN, it should re-initialize the WFDMA rings.

---

## 5. FW Ready Check Mechanism

File: `/home/user/mt7927/mt6639/common/wlan_lib.c` (line 1560)

### 5.1 sw_ready_bits for MT6639

```c
// From mt6639.c chip_info:
.sw_sync0 = Connac3x_CONN_CFG_ON_CONN_ON_MISC_ADDR,  // 0x7C0600F0
.sw_ready_bits = WIFI_FUNC_NO_CR4_READY_BITS,          // BITS(0,1) = 0x3
.sw_ready_bit_offset = Connac3x_CONN_CFG_ON_CONN_ON_MISC_DRV_FM_STAT_SYNC_SHFT, // 0
```

### 5.2 The Ready Check Loop

```c
uint32_t wlanCheckWifiFunc(IN struct ADAPTER *prAdapter, IN u_int8_t fgRdyChk)
{
    const uint32_t ready_bits = prAdapter->chip_info->sw_ready_bits; // 0x3

    while (TRUE) {
        HAL_WIFI_FUNC_READY_CHECK(prAdapter, ready_bits, &fgResult);
        // Reads 0x7C0600F0, checks if (val & 0x3) == 0x3
        if (fgResult) break;
        // Timeout after CFG_RESPONSE_POLLING_TIMEOUT
    }
}
```

**This is the exact equivalent of our fw_sync=0x3 check.**
The register is 0x7C0600F0 (CONN_CFG_ON_CONN_ON_MISC).
WIFI_FUNC_INIT_DONE (BIT 0) + WIFI_FUNC_N9_DONE (BIT 1) = 0x3.

---

## 6. Bus2Chip Address Mapping for MT6639

File: `/home/user/mt7927/mt6639/chips/mt6639/mt6639.c` (line 107)

```c
struct PCIE_CHIP_CR_MAPPING mt6639_bus2chip_cr_mapping[] = {
    {0x830c0000, 0x00000, 0x1000}, /* WF_MCU_BUS_CR_REMAP */
    {0x54000000, 0x02000, 0x1000}, /* WFDMA PCIE0 MCU DMA0 */
    {0x55000000, 0x03000, 0x1000}, /* WFDMA PCIE0 MCU DMA1 */
    // ... (more mappings)
};
```

So `CONNAC3X_MCU_WPDMA_0_BASE` (0x54000000) maps to bus address 0x02000.
- DUMMY_CR (0x54000120) -> bus 0x02120
- MCU_RX0_BASE would be at MCU DMA0 address space

---

## 7. SER (System Error Recovery) Ring Re-Init

File: `/home/user/mt7927/mt6639/os/linux/hif/common/hal_pdma.c` (line 3260)

The SER L1 flow shows the complete ring re-initialization handshake:

```
1. MCU detects error, signals ERROR_DETECT_STOP_PDMA
2. Host stops TX/RX: nicSerStopTxRx()
3. Host sends MCU_INT_PDMA0_STOP_DONE to MCU
4. MCU resets WFDMA, signals ERROR_DETECT_RESET_DONE
5. Host re-initializes rings:
   - halWpdmaAllocRing() -- allocate TXD/RXD
   - halResetMsduToken() -- reset tokens
   - halWpdmaInitRing() -- program HOST ring BASEs
6. Host sends MCU_INT_PDMA0_INIT_DONE to MCU
7. MCU finishes recovery, signals ERROR_DETECT_RECOVERY_DONE
8. Host sends MCU_INT_PDMA0_RECOVERY_DONE
```

**CRITICAL: Even in the SER L1 flow, the host only programs HOST ring BASEs.
The MCU is responsible for programming its own MCU_RX ring BASEs. The handshake
(STOP_DONE -> RESET_DONE -> INIT_DONE -> RECOVERY_DONE) is the mechanism by
which the MCU knows when to (re-)configure MCU-side rings.**

### 7.1 SER L0.5 Reset Flow

For L0.5 (WFSYS reset), the vendor driver does:
```c
if (prAdapter->eWfsysResetState != WFSYS_RESET_STATE_IDLE) {
    halWpdmaAllocRing(prAdapter->prGlueInfo, false);  // Reset TXD/RXD
    halResetMsduToken(prAdapter);
    halWpdmaInitRing(prAdapter->prGlueInfo);  // Re-program HOST rings
}
```

After this, the WFSYS is reset (assert + de-assert via CB_INFRA_RGU), FW re-boots,
and the FW configures MCU_RX rings during its boot process.

---

## 8. Windows Driver Analysis

File: `/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (binary)

### 8.1 Key Functions Found (via strings)

```
AsicConnac3xWpdmaInitRing       -- Ring initialization
AsicConnac3xPostFwDownloadInit  -- Post-FW-download init (KEY!)
AsicConnac3xGetFwSyncValue      -- FW sync/ready check
AsicConnac3xHwRecoveryFromError -- SER equivalent
AsicConnac3xLoadFirmware        -- FW download
AsicConnac3xLoadRomPatch        -- Patch download
NdisCommonHifPciStartTxRx       -- Start TX/RX
AsicConnac3xPciXmitStartTxRx   -- Enable DMA TX/RX
ProcessNicCapabilityEvent       -- NIC capability handler
ProcessNicCapabilityV2Event     -- NIC capability V2 handler
nicUniCmdNicCapability          -- Unified NIC capability command
__TxDmaqInit                    -- TX DMA queue init
__RxDmaqInit                    -- RX DMA queue init
```

### 8.2 AsicConnac3xPostFwDownloadInit

This function exists in the Windows driver but NOT in the vendor mobile driver.
It appears to be called after firmware download. Since we cannot disassemble the
binary, we can only note its existence. It may perform additional register writes
that the mobile driver handles differently (or via the FW itself).

### 8.3 Debug Strings

```
"TX Ring Configuration"   -- Printed during TX ring setup
"RX Ring Configuration"   -- Printed during RX ring setup
"HOST_RX_INT_PCIE_SEL"   -- PCIE interrupt selection
```

These suggest the Windows driver follows a similar ring configuration pattern.

---

## 9. Key Differences from Our Driver

### 9.1 What the vendor does that we may be missing

1. **WFSYS Reset BEFORE FW download**: The vendor calls `mt6639HalCbInfraRguWfRst(TRUE)`
   (assert), polls for MCU_IDLE at 0x81021604, then `mt6639HalCbInfraRguWfRst(FALSE)`
   (de-assert), ALL BEFORE firmware download. This ensures a clean slate.

2. **Uses BIT(4) for PCIe reset, NOT BIT(0)**: The MT6639 uses `WF_SUBSYS_RST[4]`,
   not `WF_WHOLE_PATH_RST[0]`. The comment explicitly says "Falcon can't reset with
   WF_WHOLE_PATH_RST[0]".

3. **CBTOP_RGU_BASE for PCIe = 0x70028600**: This is `CB_INFRA_RGU_BASE` (0x70028000)
   + 0x600 offset, but for PCIe, the hal_wfsys_reset_mt6639.h defines it as 0x70028600
   directly (pre-added offset).

4. **halWpdmaInitRing is called BEFORE FW download**: Rings are set up, then FW is
   downloaded using the FWDL ring (TX ring 16). After FW boots, the FW itself
   configures MCU-side rings.

5. **NEED_REINIT handshake**: The vendor sets NEED_REINIT BIT(1) in DUMMY_CR right
   after ring init. On subsequent CLR_OWN (deep sleep recovery), the ROM processes
   this bit and re-configures the FWDL rings (MCU_RX2/RX3). But MCU_RX0/RX1 are
   configured by the running FW, not by ROM.

6. **Manual prefetch configuration**: The vendor calls `mt6639WfdmaManualPrefetch()`
   which sets up prefetch offsets for RX rings 4-7 and TX rings 0-2, 15-16.
   This disables auto-mode prefetch and uses fixed offsets.

### 9.2 What confirms MCU_RX0 BASE=0 is expected at host level

The vendor driver:
- NEVER reads MCU_RX0 BASE register
- NEVER writes MCU_RX0 BASE register
- Only programs HOST TX and HOST RX rings
- MCU events arrive via HOST RX rings (ring 6 = EVT), NOT via MCU_RX rings

The MCU_RX rings are internal to the WFDMA engine and are configured by the MCU
firmware. The HOST cannot and should not write these.

### 9.3 The real question

If MCU_RX0 is configured by FW, and our FW boots successfully (fw_sync=0x3),
then MCU_RX0 SHOULD be configured. The fact that HOST TX ring 15 DMA completes
but data never reaches the MCU suggests either:

1. The FW did NOT configure MCU_RX0 (bug in our FW boot sequence?)
2. The R2A bridge is filtering based on something else
3. We are missing a step that the vendor's WFSYS reset provides

---

## 10. Vendor's Complete Post-Boot Sequence (Summary)

```
Phase 1: Pre-FW-Download
  1. nicInitializeAdapter()
     - Verify chip ID
     - MCR init
     - HIF init
  2. halHifSwInfoInit()
     - DmaShdlInit (DMASHDL scheduler)
     - halWpdmaAllocRing() -- allocate ring memory
     - halWpdmaInitRing() -- program HOST TX/RX ring BASEs + enable DMA
     - asicConnac3xWfdmaDummyCrWrite() -- set NEED_REINIT in DUMMY_CR
  3. HAL_ENABLE_FWDL(TRUE)
  4. WFSYS Reset:
     - Write 0x70028600 |= BIT(4) [assert WF_SUBSYS_RST]
     - Poll 0x81021604 == 0x1D1E [MCU_IDLE]
     - Write 0x70028600 &= ~BIT(4) [de-assert]
  5. wlanSetChipEcoInfo()

Phase 2: FW Download
  6. nicDisableInterrupt()
  7. nicTxInitResetResource()
  8. wlanDownloadFW():
     a. Download patch via FWDL ring
     b. Send PATCH_FINISH, get ACK
     c. Download RAM code regions via FWDL ring
     d. Send FW_START (INIT_CMD_ID_DYN_MEM_MAP_FW_FINISH or INIT_CMD_ID_WIFI_START)
     e. Get FW_START ACK

Phase 3: Post-FW-Ready
  9. wlanCheckWifiFunc(TRUE):
     - Poll 0x7C0600F0 for bits 0x3
     - This is equivalent to our fw_sync=0x3
  10. wlanQueryNicResourceInformation() -- LOCAL ONLY (nicTxResetResource)
      NOTE: This does NOT send any MCU command! It only resets local TX resources.
  11. wlanQueryNicCapabilityV2() -- THE ACTUAL FIRST MCU COMMAND
      Sends CMD_ID_GET_NIC_CAPABILITY_V2 via wlanSendCommand (HOST TX ring 15)
      Receives response via nicRxWaitResponse (HOST RX ring 6 / EVT ring)
  12. wlanUpdateNicResourceInformation()
  13. wlanUpdateBasicConfig()
  14. nicEnableInterrupt()
```

**There is NO register write between steps 9 and 11.** The FW is expected to have
MCU_RX0 ready by the time it asserts the ready bits. Step 10 is purely local
(resets TX resource tracking), so step 11 (`wlanQueryNicCapabilityV2`) is the
actual first MCU command sent after FW boot.

---

## 11. Register Address Reference

| Register | Bus Address | Description |
|----------|-------------|-------------|
| DUMMY_CR (MCU WPDMA0) | 0x02120 | NEED_REINIT handshake |
| HOST_DMA0_BASE | 0x7C024000 | HOST WFDMA base |
| HOST TX Ring 0 BASE | 0x7C024300 | Data TX ring 0 |
| HOST TX Ring 15 BASE | 0x7C0243F0 | MCU command TX ring |
| HOST TX Ring 16 BASE | 0x7C024400 | FWDL TX ring |
| HOST RX Ring 4 BASE | 0x7C024500 | Band0 RX data |
| HOST RX Ring 5 BASE | 0x7C024510 | Band1 RX data |
| HOST RX Ring 6 BASE | 0x7C024520 | Events/Tx free done |
| HOST RX Ring 7 BASE | 0x7C024530 | Band1 Tx free done |
| CONN_ON_MISC (sw_sync0) | 0x7C0600F0 | FW ready bits |
| BN0_LPCTL | 0x7C060010 | Power control (SET/CLR_OWN) |
| BN0_IRQ_STAT | 0x7C060014 | fw_own_clear_addr |
| CBTOP_RGU_WF_SUBSYS_RST | 0x70028600 | WFSYS reset control |
| WF_TOP_CFG_ON_ROMCODE_INDEX | 0x81021604 | MCU idle poll register |
| MCU WPDMA0 base | 0x54000000 (bus: 0x02000) | MCU DMA0 registers |

---

## 12. Conclusions and Next Steps

### What this analysis tells us:

1. **MCU_RX0 BASE=0 is NOT the root cause** -- The vendor driver never programs
   MCU_RX ring BASEs. The FW does this during boot. If FW reaches ready state
   (fw_sync=0x3), MCU_RX0 should already be configured.

2. **We may be missing the WFSYS reset step**: The vendor does a full WFSYS
   subsystem reset (BIT[4] at 0x70028600) BEFORE FW download. If we are not
   doing this, the WFDMA may be in a stale state from a previous boot.

3. **BIT(4) vs BIT(0)**: The vendor explicitly uses WF_SUBSYS_RST[4] for PCIe,
   not WF_WHOLE_PATH_RST[0]. This may be critical for MT7927 as well.

4. **AsicConnac3xPostFwDownloadInit**: This function exists in the Windows driver
   but not in the mobile driver. It may perform additional initialization that
   the mobile driver gets for free from the platform (e.g., via conninfra driver).

### Recommended actions:

1. **Try WFSYS reset via CB_INFRA_RGU**: Before FW download, assert BIT(4) at
   the RGU address (need to find the MT7927 equivalent of 0x70028600), poll for
   MCU_IDLE at ROMCODE_INDEX, then de-assert.

2. **Verify MCU_RX0 BASE after fw_sync=0x3**: Read the MCU-side RX ring 0 BASE
   register (0x02500 via MCU WPDMA0 space) AFTER firmware reports ready. If it's
   still 0, the FW boot is incomplete or different from what the vendor expects.

3. **Try the SER L1 handshake pattern**: After FW is ready, try sending
   MCU_INT_PDMA0_INIT_DONE via HOST2MCU_SW_INT_SET to signal the FW that
   HOST rings are configured. The FW may be waiting for this signal before
   opening the R2A bridge for MCU commands.

4. **The vendor's first MCU command is NIC_CAPABILITY_V2**: `wlanQueryNicResourceInformation`
   is purely local (no MCU command). The actual first MCU command is
   `CMD_ID_GET_NIC_CAPABILITY_V2` sent via normal `wlanSendCommand()` -> HOST TX ring 15.
   This is the same path we use. The vendor expects this to work immediately after
   fw_sync=0x3.

5. **Investigate the WFSYS reset we may be missing**: The vendor performs
   `mt6639HalCbInfraRguWfRst(TRUE/FALSE)` BEFORE FW download. This resets the
   entire WiFi subsystem cleanly. Without this, stale WFDMA state from a previous
   boot may interfere. Try writing BIT(4) to the RGU register before our FW
   download sequence.
