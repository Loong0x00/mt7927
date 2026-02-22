# MCU_CMD Register & MCU Response Path Investigation

**Date**: 2026-02-15
**Trigger**: After sending NIC_CAPABILITY MCU command, MCU_CMD register changes from 0x0 to 0x00008000 (BIT(15) set). MCU IS responding, but NOT via DMA.

---

## 1. Register Definitions

### MCU_CMD = MCU2HOST_SW_INT_STA
| Name | WFDMA0 Offset | BAR0 Offset | Bus Address |
|------|---------------|-------------|-------------|
| MCU2HOST_SW_INT_STA | 0x1F0 | 0xd41f0 | 0x7c0241f0 |
| MCU2HOST_SW_INT_ENA (mask) | 0x1F4 | 0xd41f4 | 0x7c0241f4 |
| MCU2HOST_SW_INT_SET (MCU writes) | 0x10C | 0xd410c | 0x7c02410c |
| HOST2MCU_SW_INT_SET (host writes) | 0x108 | 0xd4108 | 0x7c024108 |
| HOST_INT_STA | 0x200 | 0xd4200 | 0x7c024200 |
| HOST_INT_ENA | 0x204 | 0xd4204 | 0x7c024204 |

**Key**: `MT_MCU_CMD_REG` in our driver = `MT_MCU_CMD` in mt76 = `MCU2HOST_SW_INT_STA`.

The MCU firmware writes to MCU2HOST_SW_INT_SET (0x10C) to SET bits in MCU2HOST_SW_INT_STA (0x1F0). The host must write-1-to-clear these bits to acknowledge.

### MCU2HOST_SW_INT_STA Bit Definitions (from vendor + mt76)

| Bit | mt76 Name | Vendor Name | Meaning |
|-----|-----------|-------------|---------|
| 0 | MT_MCU_CMD_WAKE_RX_PCIE | — | Wake RX PCIe (data ready) |
| 1 | MT_MCU_CMD_STOP_DMA_FW_RELOAD | ERROR_DETECT_STOP_PDMA_WITH_FW_RELOAD | SER: stop DMA + FW reload |
| 2 | MT_MCU_CMD_STOP_DMA | ERROR_DETECT_STOP_PDMA | SER: stop PDMA |
| 3 | MT_MCU_CMD_RESET_DONE | ERROR_DETECT_RESET_DONE | SER: reset complete |
| 4 | MT_MCU_CMD_RECOVERY_DONE | ERROR_DETECT_RECOVERY_DONE | SER: recovery complete |
| 5 | MT_MCU_CMD_NORMAL_STATE | ERROR_DETECT_MCU_NORMAL_STATE | SER: MCU normal |
| 8 | — | CP_LMAC_HANG_WORKAROUND_STEP1 | LMAC hang WA step 1 |
| 9 | — | CP_LMAC_HANG_WORKAROUND_STEP2 | LMAC hang WA step 2 |
| **15** | **NOT DEFINED** | **NOT DEFINED** | **← Our observation: 0x8000** |
| 24 | — | ERROR_DETECT_LMAC_ERROR | LMAC error |
| 25 | — | ERROR_DETECT_PSE_ERROR | PSE error |
| 26 | — | ERROR_DETECT_PLE_ERROR | PLE error |
| 27 | — | ERROR_DETECT_PDMA_ERROR | PDMA error |
| 28 | — | ERROR_DETECT_PCIE_ERROR | PCIe error |
| 31 | — | ERROR_DETECT_L1_DONE_IN_SUSPEND | L1 done in suspend |

### MCU2HOST_SW_INT_MASK (vendor driver sets this)
```c
// In cmm_asic_connac3x.c line 121:
CONNAC3X_WPDMA_MCU2HOST_SW_INT_MASK(u4HostWpdamBase), BITS(0, 15)
```
**Vendor enables bits 0-15 (mask = 0xFFFF)**. So BIT(15) IS within the valid mask range.

### ERROR_DETECT_MASK (vendor ISR checks)
```c
#define ERROR_DETECT_MASK  \
    (ERROR_DETECT_STOP_PDMA | ERROR_DETECT_RESET_DONE | \
     ERROR_DETECT_RECOVERY_DONE | ERROR_DETECT_MCU_NORMAL_STATE)
// = BIT(2) | BIT(3) | BIT(4) | BIT(5) = 0x3C
```
Vendor ISR only handles bits 2-5. Any other MCU2HOST bits get logged as "undefined SER status[0x%x]" and cleared.

**CONCLUSION**: BIT(15) = 0x8000 is a valid MCU-to-host software interrupt, enabled by the mask, but NOT handled by the standard vendor ISR. It appears to be a MT6639-firmware-specific signal with unknown semantics. It proves the MCU IS responding.

---

## 2. MCU Event Response Path (How the Host Receives MCU Responses)

### Normal Flow (mt7925 / vendor driver)
```
MCU firmware processes command
    → MCU writes response to HOST RX ring via WFDMA DMA
    → WFDMA generates HOST_INT_STA interrupt (rx_done_int_sts_N)
    → Host ISR reads HOST_INT_STA
    → Host schedules NAPI poll for RX ring
    → NAPI drains RX ring → mt7925_mcu_rx_event() parses response
    → Matches sequence number → completes waiting caller
```

### MCU2HOST_SW_INT Notification (supplementary)
```
MCU firmware sets MCU2HOST_SW_INT_SET (0x10C)
    → Sets bits in MCU2HOST_SW_INT_STA (0x1F0)
    → HOST_INT_STA BIT(29) (mcu2host_sw_int_sts) fires
    → mt792x_irq_tasklet() reads MT_MCU_CMD (0x1F0)
    → If BIT(0) WAKE_RX_PCIE: schedule data RX NAPI
    → Write back to clear MCU_CMD bits
```

### Key Insight: MCU events arrive via HOST RX rings, NOT MCU_TX rings
- MCU_TX rings (BASE=0) are for MCU's internal use — NOT for host
- The MCU uses WFDMA to DMA responses into HOST RX ring descriptors
- **MCU_TX BASE=0 is NORMAL and expected** (confirmed red herring)

---

## 3. RX Ring Mapping for MCU Events

### MT7925 (mt76 upstream)
| Ring Purpose | Software Index | HW Ring | WFDMA0 Offset | BAR0 Offset |
|---|---|---|---|---|
| MCU Event | MT_RXQ_MCU (MT7925_RXQ_MCU_WM=0) | RX Ring 0 | 0x500 | 0xd4500 |
| Data | MT_RXQ_MAIN (MT7925_RXQ_BAND0=2) | RX Ring 2 | 0x520 | 0xd4520 |

### MT6639 (vendor Android driver)
| Ring Purpose | Software Index | HW Ring | WFDMA0 Offset | BAR0 Offset |
|---|---|---|---|---|
| Band0 Data | RX_RING_DATA_IDX_0 | **RX Ring 4** | 0x540 | 0xd4540 |
| Band1 Data | RX_RING_DATA1_IDX_2 | **RX Ring 5** | 0x550 | 0xd4550 |
| **MCU Event** | **RX_RING_EVT_IDX_1** | **RX Ring 6** | **0x560** | **0xd4560** |
| TX Done | RX_RING_TXDONE0_IDX_3 | RX Ring 7 | 0x570 | 0xd4570 |

Source: `mt6639/chips/mt6639/mt6639.c:mt6639SetRxRingHwAddr()` lines 460-489:
```c
case RX_RING_EVT_IDX_1: offset = 6 * MT_RINGREG_DIFF; break;  // ring 6
case RX_RING_DATA_IDX_0: offset = 4 * MT_RINGREG_DIFF; break;  // ring 4
case RX_RING_DATA1_IDX_2: offset = 5 * MT_RINGREG_DIFF; break; // ring 5
case RX_RING_TXDONE0_IDX_3: offset = 7 * MT_RINGREG_DIFF; break; // ring 7
```

### Windows (Ghidra RE, bus addresses from ghidra_post_fw_init.md)
Windows initializes HOST RX rings at:
- 0x7c024540 (ring 4) = BAR0+0xd4540 → Data
- 0x7c024560 (ring 6) = BAR0+0xd4560 → **MCU Events**
- 0x7c024570 (ring 7) = BAR0+0xd4570 → TX Done

### Our Driver
```c
#define MT_RXQ_MCU_EVENT_RING_CONNAC3   6  // RX ring 6 for MT6639 events
static int evt_ring_qid = MT_RXQ_MCU_EVENT_RING_CONNAC3;
// MT_WPDMA_RX_RING_BASE(6) = MT_WFDMA0(0x500 + (6<<4)) = MT_WFDMA0(0x560) = BAR0+0xd4560
```
**Our RX ring 6 mapping is CORRECT for MT6639.**

---

## 4. Our Driver's MCU Response Handling — WHAT'S MISSING

### Current wait_mcu_event() — DMA-Only Polling
```c
static int mt7927_wait_mcu_event(struct mt7927_dev *dev, int timeout_ms) {
    struct mt7927_ring *ring = &dev->ring_evt;  // ring 6
    do {
        d = &ring->desc[ring->tail];
        ctrl = le32_to_cpu(d->ctrl);
        if (ctrl & MT_DMA_CTL_DMA_DONE) {  // Only checks DMA descriptor!
            // Process event...
        }
        usleep_range(1000, 2000);
    } while (time_before(jiffies, timeout));
    return -ETIMEDOUT;  // ALWAYS TIMES OUT because no DMA ever completes
}
```

**Problem**: Only polls DMA descriptors on HOST RX ring 6. Does NOT check:
1. MCU2HOST_SW_INT_STA (0xd41f0) — where MCU IS signaling (BIT(15))
2. HOST_INT_STA (0xd4200) — where WFDMA reports RX done
3. Any other mechanism for MCU response delivery

### trace_mcu_event() — Reads But Doesn't Use MCU_CMD
```c
static void mt7927_trace_mcu_event(struct mt7927_dev *dev, const char *tag) {
    u32 host_int = mt7927_rr(dev, MT_WFDMA_HOST_INT_STA);
    u32 mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);
    // Logs and clears — but doesn't act on the values!
}
```

### HOST2MCU notification — We DO This Correctly
```c
// In mt7927_kick_ring_buf():
mt7927_wr(dev, MT_HOST2MCU_SW_INT_SET, BIT(0));  // Notify MCU after TX kick
```

---

## 5. WHY MCU Events Are NOT Arriving via DMA

The MCU_CMD=0x8000 (BIT(15)) proves the MCU receives our command and responds. But no DMA completion on HOST RX ring 6. Possible root causes:

### Hypothesis A: DMASHDL Not Enabled (MOST LIKELY)
- **Windows writes `BAR0+0xd6060 |= 0x10101` BEFORE first MCU command** (PostFwDownloadInit step 1)
- DMASHDL (DMA Scheduler) controls packet flow between MCU and WFDMA
- Without DMASHDL enabled, the MCU might queue the response but WFDMA never picks it up
- This would explain: MCU2HOST_SW_INT_STA set (MCU tried), but no DMA (WFDMA blocked)
- **Our driver NEVER writes to 0xd6060**

### Hypothesis B: WFDMA GLO_CFG RX_DMA_EN Not Set
- After CLR_OWN reset, GLO_CFG gets zeroed
- We re-enable via `mt7927_dma_reprogram_rings()` but need to verify RX_DMA_EN (BIT(2)) is set
- Windows: `GLO_CFG |= 5` (BIT(0) TX_DMA_EN | BIT(2) RX_DMA_EN)

### Hypothesis C: Prefetch Engine Not Configured
- Windows writes prefetch config: 0xd70f0=0x660077, 0xd70f4=0x1100, 0xd70f8=0x30004f, 0xd70fc=0x542200
- Without prefetch, the WFDMA may stall on ring 6

### Hypothesis D: FW Needs PostFwDownloadInit Commands First
- Windows sends 9 MCU commands via PostFwDownloadInit before normal operation
- The first command (class=0x8a NIC_CAPABILITY) IS what we're sending
- But Windows does it AFTER enabling DMASHDL (0xd6060 |= 0x10101)
- The FW might need DMASHDL active to route responses through WFDMA

### Hypothesis E: BIT(15) IS The Response (Register-Based Mailbox)
- Less likely, but possible: the ROM bootloader or early FW might use register-based signaling
- During FWDL, ROM used polling-based protocol (no DMA for responses)
- Post-boot FW might initially use a similar mechanism until configured

---

## 6. MCU2HOST_SW_INT Handling in Vendor/mt76

### mt76 mt792x_irq_tasklet (mt792x_dma.c:28-73)
```c
void mt792x_irq_tasklet(unsigned long data) {
    // Read and clear HOST_INT_STA
    intr = mt76_rr(dev, MT_WFDMA0_HOST_INT_STA);
    mt76_wr(dev, MT_WFDMA0_HOST_INT_STA, intr);

    // Check MCU2HOST software interrupt (BIT(29) of HOST_INT_STA)
    if (intr & MT_INT_MCU_CMD) {  // MT_INT_MCU_CMD = BIT(29) = MCU2HOST_SW_INT_ENA
        u32 intr_sw = mt76_rr(dev, MT_MCU_CMD);  // Read MCU2HOST_SW_INT_STA
        mt76_wr(dev, MT_MCU_CMD, intr_sw);        // ACK (write-1-to-clear)
        if (intr_sw & MT_MCU_CMD_WAKE_RX_PCIE) {  // BIT(0)
            mask |= irq_map->rx.data_complete_mask;
            intr |= irq_map->rx.data_complete_mask;
        }
    }
    // Schedule NAPI for matching RX/TX rings
    if (intr & irq_map->rx.wm_complete_mask)
        napi_schedule(&dev->mt76.napi[MT_RXQ_MCU]);  // RX event NAPI
}
```

### MT6639 Vendor: MCU2HOST for SER (Error Recovery)
```c
// cmm_asic_connac3x.c:772-790
kalDevRegRead(prGlueInfo, MCU2HOST_SW_INT_STA, &u4Status);
if (u4Status & ERROR_DETECT_MASK) {
    halHwRecoveryFromError(prAdapter);  // SER handler
} else {
    // "undefined SER status[0x%x]" — BIT(15) would hit this path
}
```

### MT6639 Vendor: Interrupt Enable Mask
```c
// mt6639.c:654-671 — mt6639ConfigIntMask
u4WrVal =
    HOST_RX_DONE_INT_ENA4_MASK |  // RX ring 4 (data)
    HOST_RX_DONE_INT_ENA5_MASK |  // RX ring 5 (data)
    HOST_RX_DONE_INT_ENA6_MASK |  // RX ring 6 (MCU event!)
    HOST_RX_DONE_INT_ENA7_MASK |  // RX ring 7 (TX done)
    HOST_TX_DONE_INT_ENA0..2_MASK |
    HOST_TX_DONE_INT_ENA15..16_MASK |
    mcu2host_sw_int_ena_MASK;     // MCU2HOST software interrupt
```

---

## 7. MT6639 MCU Command Packet Format

### PQ_ID Header Field
```c
// nic_cmd_event.h:87, nic_init_cmd_event.h:91
#define CMD_PQ_ID       (0x8000)   // Port1, Queue 0
#define INIT_CMD_PQ_ID  (0x8000)   // Port1, Queue 0
// Event header: u2PQ_ID; /* Must be 0x8000 (Port1, Queue 0) */
```
**NOTE**: 0x8000 also appears as PQ_ID in command/event packet headers. This is a **coincidence** with MCU_CMD BIT(15) — they are different fields in different contexts.

---

## 8. Recommended Next Steps

### Priority 1: Enable DMASHDL (BAR0+0xd6060 |= 0x10101)
This is the #1 most likely fix. Windows does this as the VERY FIRST thing after FW boot, before any MCU command. Add to our driver BEFORE sending NIC_CAPABILITY:
```c
u32 val = mt7927_rr(dev, 0xd6060);   // Read current DMASHDL config
mt7927_wr(dev, 0xd6060, val | 0x10101);  // Enable BIT(0)|BIT(8)|BIT(16)
```

### Priority 2: Verify GLO_CFG Has RX_DMA_EN
After DMASHDL enable, confirm GLO_CFG (BAR0+0xd4208) has BIT(2) set:
```c
u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
if (!(glo & BIT(2))) {
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo | BIT(0) | BIT(2));  // TX+RX DMA EN
}
```

### Priority 3: Add MCU2HOST_SW_INT Monitoring to wait_mcu_event()
Even if DMA works after DMASHDL, we should also check the interrupt registers:
```c
// In wait_mcu_event loop, add:
u32 host_int = mt7927_rr(dev, MT_WFDMA_HOST_INT_STA);
u32 mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);
if (host_int & HOST_RX_DONE_INT_ENA(evt_ring_qid)) {
    mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, host_int);  // ACK
    // Re-check DMA descriptor
}
if (mcu_cmd) {
    mt7927_wr(dev, MT_MCU_CMD_REG, mcu_cmd);  // ACK MCU2HOST
}
```

### Priority 4: After DMASHDL Works, Implement Full PostFwDownloadInit
Once MCU events flow, implement the remaining Windows init commands:
1. NIC_CAPABILITY (class=0x8a) — already attempted
2. Config (class=0x02, data={1,0,0x70000})
3. Config (class=0xc0, data={0x820cc800, 0x3c200})
4. DBDC (class=0x28, MT6639 only)
5. Scan/chip/log config (class=0xca ×3)

---

## 9. Summary of Key Findings

| Finding | Status | Impact |
|---------|--------|--------|
| MCU_CMD = MCU2HOST_SW_INT_STA | Confirmed | Register identity clarified |
| BIT(15) = valid MCU→Host interrupt | Confirmed | MCU IS alive and responding |
| MCU events arrive via HOST RX ring | Confirmed | Ring 6 for MT6639, ring 0 for MT7925 |
| Our RX ring 6 mapping correct | Confirmed | Not the issue |
| DMASHDL never enabled (0xd6060) | **MISSING** | **Likely root cause** |
| Our wait_mcu_event() DMA-only | Known | Should also check INT registers |
| PostFwDownloadInit missing | Known | Required for full operation |
| MCU_TX BASE=0 is normal | Confirmed | Red herring eliminated |

**The FW is alive. It sees our commands. It's trying to respond via MCU2HOST_SW_INT BIT(15). But WFDMA can't DMA the actual response data to HOST RX ring because DMASHDL at 0xd6060 is not enabled.**
