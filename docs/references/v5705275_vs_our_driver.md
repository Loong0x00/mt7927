# Windows v5.7.0.5275 vs Our Linux Driver: Comprehensive Comparison

## Executive Summary

**Core Question**: Why do MCU commands get consumed (MCU_INT_STA=1) but responses NEVER arrive on HOST RX ring 6?

**Root Cause Assessment** (ordered by likelihood):

1. **GLO_CFG misconfiguration** — Windows sets 0x50001070|0x208000|0x5 = massive bitmask enabling TX/RX DMA, event routing, and prefetch. Our driver sets minimal bits. Missing bits likely disable MCU→HOST event routing entirely.
2. **GLO_CFG_EXT1 BIT(28) missing** — Windows explicitly sets BIT(28) in 0x7c0242b4 (BAR0+0xd42b4). We never write this register. This bit may enable MCU event delivery to HOST RX rings.
3. **Interrupt mask not configured** — Windows writes 0x2600f000 to INT enable/disable registers and toggles BIT(16) in 0x74030188 for MT6639. We don't configure interrupts at all (we poll).
4. **TXD LONG_FORMAT BIT(31) corruption** — Windows NEVER sets BIT(31) in TXD[1]. We set it by default. This changes how the WFDMA parses the descriptor and may cause the MCU to misroute or drop the response.

---

## 1. WFDMA Global Configuration (GLO_CFG at 0x7c024208 / BAR0+0xd4208)

### Windows (FUN_1401d8724 + FUN_1401e5be0 = MT6639WpdmaConfig)

**Enable path:**
```
GLO_CFG |= 0x50001070    // base enable bits (no MSI)
         // OR 0x58001070 if MSI enabled (adds BIT(27))
GLO_CFG |= 0x208000      // additional enable bits
// ... ring init ...
GLO_CFG |= 0x5           // final: TX_DMA_EN | RX_DMA_EN
```

**Combined GLO_CFG value (no MSI):** `0x50209075`
- BIT(0) = TX_DMA_EN
- BIT(2) = RX_DMA_EN
- BIT(4) = TX_WB_DDONE
- BIT(5) = ? (unknown, always set)
- BIT(6) = ? (unknown, always set)
- BIT(12) = BYTE_SWAP (big-endian DMA data)
- BIT(15) = ? (from 0x8000)
- BIT(17) = ? (from 0x20000)
- BIT(21) = OMIT_RX_INFO
- BIT(28) = TX_DMA_BUSY or prefetch-related
- BIT(30) = ? (from 0x40000000)

**Disable path:**
```
GLO_CFG &= 0xe7df7ffa    // clears TX_DMA_EN, RX_DMA_EN, and other bits
// wait for idle
0x7c02420c = 0xFFFFFFFF  // clear all TX interrupt status
0x7c024280 = 0xFFFFFFFF  // clear all RX interrupt status
```

**GLO_CFG_EXT1 (0x7c0242b4 / BAR0+0xd42b4):**
```
read 0x7c0242b4
|= 0x10000000            // BIT(28)
write 0x7c0242b4
```

### Our Driver (mt7927_dma_disable + mt7927_dma_enable)

**Enable path (mt7927_dma_enable, ~line 596):**
```c
val = readl(0xd4208);
val |= 0x5;              // TX_DMA_EN | RX_DMA_EN ONLY
writel(val, 0xd4208);
```

**Disable path (mt7927_dma_disable, ~line 619):**
```c
val = readl(0xd4208);
val &= ~(BIT(0) | BIT(2));  // clear TX/RX DMA_EN
val &= ~(BIT(4));           // clear TX_WB_DDONE
val &= ~(BIT(21));          // clear OMIT_RX_INFO
// optional: logic reset BIT(24/25)
writel(val, 0xd4208);
```

### CRITICAL DIFFERENCES:
| Aspect | Windows | Our Driver | Impact |
|--------|---------|------------|--------|
| GLO_CFG base bits | 0x50001070 | 0x0 (only 0x5 added) | **HUGE** — missing prefetch, byte swap, event routing bits |
| GLO_CFG extra bits | 0x208000 | none | Missing BIT(15), BIT(17), BIT(21) pre-enable |
| GLO_CFG_EXT1 BIT(28) | SET | never written | **May disable MCU→HOST event path** |
| Disable: clear INT status | 0x7c02420c=0xFFFFFFFF, 0x7c024280=0xFFFFFFFF | not done | Stale interrupt status may block new events |

---

## 2. Ring Configuration

### Windows (FUN_1401e4580 = MT6639InitTxRxRing)

**TX Rings (4 rings):**
```
Ring 0: 0x7c024300/304/308/30c  (TX data ring 0)
Ring 1: 0x7c024310/314/318/31c  (TX data ring 1)
Ring 2: 0x7c024320/324/328/32c  (TX data ring 2)
Ring 3: 0x7c024330/334/338/33c  (TX data ring 3)
```

**RX Rings (3 rings, non-contiguous offsets):**
```
Ring 0: 0x7c024540/544/548/54c  (offset 0x40 from 0x7c024500 — HOST events?)
Ring 1: 0x7c024560/564/568/56c  (offset 0x60 — MCU events / RX data?)
Ring 2: 0x7c024570/574/578/57c  (offset 0x70 — FWDL events?)
```

**Descriptor fixup after ring init:**
- Iterates all descriptors
- Clears BIT(31) in descriptor ctrl word (DMA_DONE)
- Applies 0x3fff0000 mask to owner field

### Our Driver

**TX Rings:**
```
Ring 15 (FWDL CMD): 0xd4300 + 15*0x10 = 0xd43f0  (tx_fwdl_cmd)
Ring 16 (FWDL DATA): 0xd4300 + 16*0x10 = 0xd4400  (tx_fwdl)
Ring 18 (WM CMD):    0xd4300 + 18*0x10 = 0xd4420  (tx_wm)
```

**RX Rings:**
```
Ring 2 (FWDL EVT):  0xd4500 + 2*0x10 = 0xd4520   (rx_fwdl)
Ring 3 (FWDL EVT2): 0xd4500 + 3*0x10 = 0xd4530   (rx_fwdl2)
Ring 6 (WM EVT):    0xd4500 + 6*0x10 = 0xd4560   (rx_wm_evt)
```

### DIFFERENCES:
| Aspect | Windows | Our Driver | Impact |
|--------|---------|------------|--------|
| TX ring IDs | 0-3 | 15, 16, 18 | Different ring routing — Windows uses data rings for MCU too? |
| RX ring offsets | 0x40/0x60/0x70 (non-standard) | n*0x10 (standard) | **CRITICAL** — Windows RX ring 1 at offset 0x60 = our ring 6 address! This means Windows maps "RX ring 1" to the SAME physical register as our "RX ring 6" |
| Descriptor fixup | clears BIT(31), masks owner | none | May leave stale DMA_DONE bits, confusing WFDMA |
| Number of rings | 4 TX + 3 RX | 3 TX + 3 RX | Minor — Windows has 4 TX rings |

**KEY INSIGHT on RX Ring Mapping:**
- Windows RX ring at offset 0x60 from 0x7c024500 = registers at 0x7c024560
- Our RX ring 6: 0xd4500 + 6*0x10 = 0xd4560 → bus addr 0x7c024560
- **These are the SAME physical registers!** Windows calls it "ring 1 (of 3)", we call it "ring 6". The MCU event ring IS being addressed correctly.
- But Windows RX ring at offset 0x40 = 0x7c024540 = our ring 4 (if we had one). Windows ring at offset 0x70 = 0x7c024570 = our ring 7 (if we had one).

---

## 3. MCU Command TXD Format

### Windows — UniCmd Path (FUN_14014eb0c = MtCmdSendSetQueryUniCmdAdv)

Header size: **0x30 bytes** (48 bytes)

```
Offset 0x00: TXD[0] = total_len | 0x41000000
             - Bits[15:0]  = total packet length
             - Bits[24:20] = Q_IDX = 0x20 (MT_TX_MCU_PORT_RX_Q0)
             - Bits[27:25] = PKT_FMT = 2 (MCU command)

Offset 0x04: TXD[1] = flags
             - BIT(14) SET   = HDR_FORMAT_V3 = 1
             - BIT(15) CLEAR = (cleared explicitly)
             - BIT(31) NEVER SET = NO LONG_FORMAT

Offset 0x08-0x1F: TXD[2]-TXD[7] = 0 (zeroed)

Offset 0x20: u16 = (header_size - 0x20) + payload_len  (body length)
Offset 0x22: u16 = CID/class (e.g. 0x8a, 0x02, 0xc0, 0x28, 0xca)
Offset 0x24: u8  = set/query flags
Offset 0x25: u8  = 0xa0 (MCU_PKT_ID, always)
Offset 0x26: u8  = ?
Offset 0x27: u8  = token/sequence number (from FUN_14009a46c)
Offset 0x28-0x2B: u32 = flags/options
             - Byte +0x2b = additional flags (set/query dimension)

Offset 0x30+: payload data
```

### Windows — Legacy/FWDL Path (FUN_1400cd2a8 = MtCmdSendSetQueryCmdAdv)

Header size: **0x40 bytes** (64 bytes) — used when flag at +0x146e621 == 0

```
Offset 0x00-0x1F: TXD[0]-TXD[7] (same TXD encoding as above)

Offset 0x20: u16 = payload_len + 0x20 (total_body_len)
Offset 0x24: u8  = set/query (param_2)
Offset 0x25: u8  = 0xa0 (MCU_PKT_ID)
Offset 0x27: u8  = token/seq

Special for FW_SCATTER (CID=0xee):
  *(u16*)(hdr + 0x24) = 0xa000   // overwrites bytes at +0x24/+0x25
  *(u8*)(hdr + 0x27)  = 0        // token = 0

Offset 0x40+: payload data
```

### Our Driver (mt7927_mcu_send_cmd, ~line 916)

Header size: **0x40 bytes** (64 bytes, sizeof(struct mt76_connac2_mcu_txd))

```c
// TXD[0]
val = FIELD_PREP(MT_TXD0_TX_BYTES, len) |          // total length
      FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) | // PKT_FMT=2
      FIELD_PREP(MT_TXD0_Q_IDX, mcu_rx_qidx);       // default 0x20
txd->txd[0] = cpu_to_le32(val);

// TXD[1]
val = (no_long_format ? 0 : MT_TXD1_LONG_FORMAT) |  // BIT(31) by DEFAULT!
      FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
txd->txd[1] = cpu_to_le32(val);

// MCU header (offset 0x20)
mcu_txd->len    = cpu_to_le16(len - sizeof(*txd));   // body len
mcu_txd->pq_id  = cpu_to_le16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU, ...));
mcu_txd->pkt_type = MCU_PKT_ID;                      // 0xa0
mcu_txd->cid    = cmd;
mcu_txd->seq    = seq;
mcu_txd->set_query = MCU_Q_SET;
mcu_txd->s2d_index = ...;
```

### CRITICAL TXD DIFFERENCES:
| Field | Windows | Our Driver | Impact |
|-------|---------|------------|--------|
| TXD[1] BIT(31) LONG_FORMAT | **NEVER set** | **Set by default** | **CRITICAL** — WFDMA interprets descriptor differently; may read extra DWORDs as TXD[2-7] and corrupt parsing |
| Header size (UniCmd) | 0x30 | 0x40 | Windows uses shorter header post-init |
| Header size (FWDL) | 0x40 | 0x40 | Match |
| Q_IDX | 0x20 | 0x20 (configurable) | Match (our default matches) |
| PKT_FMT | 2 | 2 | Match |
| MCU_PKT_ID | 0xa0 | 0xa0 | Match |
| FW_SCATTER +0x24 | 0xa000 | 0xa000 (in wm_ring path) | Match (when using wm_ring scatter path) |
| Offset +0x20 len calc | payload_len + 0x20 (legacy) | len - sizeof(txd) | Need to verify values match |
| pq_id field | not visible in RE | MCU_PQ_ID macro | May differ — Windows encodes differently at +0x20/+0x22 |

---

## 4. MSI / Interrupt Configuration

### Windows (FUN_1401e43e0 = MT6639ConfigIntMask + FUN_1401e5be0)

**Interrupt mask setup:**
```
Write 0x2600f000 to 0x7c02422c (INT_ENA_CLR — disable specific interrupts)
  OR to 0x7c024230 (INT_ENA_SET — enable specific interrupts)
  (selected by parameter)

Read 0x7c024204 (INT_SOURCE_CSR — current interrupt status)

For MT6639/7927 specifically:
  Read 0x74030188
  Set or Clear BIT(16)
  Write back to 0x74030188
```

**MSI config (in MT6639WpdmaConfig, conditional):**
```
0x7c0270f0 = 0x00660077   // MSI group mapping
0x7c0270f4 = 0x00001100   // MSI group mapping
0x7c0270f8 = 0x0030004f   // MSI group mapping
0x7c0270fc = 0x00542200   // MSI group mapping
```

**GLO_CFG MSI bit:**
```
If MSI enabled: GLO_CFG |= BIT(27) (0x08000000)
```

### Our Driver

**Interrupt configuration: NONE**
- We never write to 0x7c02422c / 0x7c024230
- We never read 0x7c024204
- We never touch 0x74030188
- We never write MSI config registers 0x7c0270f0-fc
- We poll descriptors directly (DMA_DONE bit)

### DIFFERENCES:
| Aspect | Windows | Our Driver | Impact |
|--------|---------|------------|--------|
| INT_ENA registers | configured (0x2600f000) | never written | MCU may not generate events without INT enable |
| 0x74030188 BIT(16) | toggled for MT6639 | never written | **MT6639-specific** — may control MCU event routing |
| MSI config regs | 4 registers written | none | May affect event ring routing if MSI mode is active |
| Polling vs interrupts | Event wait (KeWaitForSingleObject) | Poll DMA_DONE bit | Functional difference, but polling should still work |

---

## 5. MCU Response/Event Receive Path

### Windows (FUN_1400c9810 = MtCmdEventWait)

```
1. Send command via DMA ring
2. Store command context in slot (20 slots, 0x60 bytes each)
   - Tracks: CID, token, done_flag, error_code, response_seq
3. Call KeWaitForSingleObject(event, timeout)
   - Event is signaled by ISR when MCU response arrives
4. ISR processes HOST_INT_STA, reads RX ring descriptor
5. Matches response token to stored command slot
6. Signals event → KeWaitForSingleObject returns
7. Caller reads response from slot

For MT6639 specifically:
  - Error recovery path exists (checks chip ID)
  - Timeout handling with retry logic
```

### Our Driver (mt7927_mcu_wait_response, ~line 791)

```c
1. Send command via DMA ring
2. Poll rx_wm_evt ring (ring 6) for DMA_DONE:
   for (i = 0; i < 3000; i++) {
       ctrl = readl(desc + 0x04);  // read ctrl word
       if (ctrl & BIT(31)) {       // DMA_DONE
           // process response
           break;
       }
       mdelay(1);
   }
3. If DMA_DONE never set → timeout
```

### DIFFERENCES:
| Aspect | Windows | Our Driver | Impact |
|--------|---------|------------|--------|
| Wait mechanism | ISR + event object | Polling DMA_DONE | Polling should work if events arrive |
| Command tracking | 20-slot token matching | None (sequential) | OK for single-threaded probe |
| ISR involvement | Yes, reads HOST_INT_STA | No ISR | MCU events should still land in RX ring regardless |
| Ring being polled | Matched via ISR | ring 6 (0xd4560) | Correct register address (matches Windows ring 1) |

---

## 6. Pre-Firmware Download Init (FUN_1401e5430)

### Windows (MT6639PreFirmwareDownloadInit)

```
1. If internal flag == 0:
   - Copy 0x15e bytes of config data to device+0x1465077
2. Read fw_sync (0x7c0600f0)
3. If fw_sync == 0:
   - Clear internal flag
   - Call FUN_14000d410 (some init function)
   - Poll fw_sync for == 1 (up to 500 times, 1ms sleep each)
4. If fw_sync != 0:
   - Set internal flag = 1
   - Call FUN_14000d410
```

### Our Driver

- We read fw_sync (0xe00f0) and check for 0x3
- We don't copy any config data
- We don't call any equivalent of FUN_14000d410
- We don't have the fw_sync == 0 → poll for 1 → then proceed flow

### NOTE:
This is the PRE-firmware-download init, before FWDL starts. Our FWDL itself works (reaches fw_sync=0x3), so this specific function may not be the blocker. The issue is POST-FWDL MCU commands.

---

## 7. DMASHDL Enable (Post-FWDL)

### Windows (PostFwDownloadInit, from v5603998 RE)

```
BAR0+0xd6060 |= 0x10101   // DMASHDL enable — FIRST thing after FWDL
```

### Our Driver

- **NOT implemented** — we never write 0xd6060
- This is listed in MEMORY.md as a known missing step

### IMPACT:
DMASHDL (DMA Scheduler) controls packet scheduling between TX/RX engines. Without enabling it, the WFDMA may not properly route MCU responses to HOST RX rings.

---

## 8. PostFwDownloadInit MCU Commands (0xed target)

### Windows (from v5603998 RE)

After DMASHDL enable, Windows sends 9 MCU commands (all target=0xed):

| # | Class | Description |
|---|-------|-------------|
| 1 | 0x8a  | NIC capability query |
| 2 | 0x02  | Config {1, 0, 0x70000} |
| 3 | 0xc0  | {0x820cc800, 0x3c200} |
| 4 | 0xed  | Buffer download (optional) |
| 5 | 0x28  | DBDC config (MT6639 ONLY) |
| 6 | 0xca  | Scan config |
| 7 | 0xca  | Chip config |
| 8 | 0xca  | Log config |
| 9 | ...   | Additional setup |

### Our Driver

- **NOT implemented** — we jump straight to testing MCU_RX0

### IMPACT:
These commands may configure the FW's internal DMA routing. Without them, FW may not know to route MCU event responses to HOST RX ring 6. However, this is a chicken-and-egg problem: we can't send these commands if we can't receive responses.

---

## 9. FW Download Protocol Details

### Windows (from v5705275 RE)

```
Per firmware section:
1. Send section command (CID=0x0d, 12-byte payload: addr/len/flags)
   via FUN_1400cdc4c(..., cmd=0x0d, ...)
2. Split section into 0x800 (2048) byte chunks
3. Send each chunk (CID=0xee, token=0x10) with special header:
   - *(u16*)(hdr+0x24) = 0xa000
   - *(u8*)(hdr+0x27) = 0
   via FUN_1400cdc4c(..., cmd=0x10, token=0xee, ...)
4. Send section commit (CID=0x17, 8-byte payload with flags)
   - flags: |0x2, conditional |0x1, conditional |0x4
   via FUN_1400cdc4c(..., cmd=0x17, ...)
5. Poll fw_sync (0x7c0600f0) for == 3 (ready)
   - 1ms sleep, up to ~500 iterations
```

### Our Driver

```
Similar sequence but using raw DMA descriptors:
1. init_dl (CID=0x01/0x0d equivalent)
2. scatter in 4096-byte chunks (not 2048)
3. FW_START with option=1
4. Poll fw_sync for 0x3
```

### DIFFERENCES:
| Aspect | Windows | Our Driver | Impact |
|--------|---------|------------|--------|
| Chunk size | 0x800 (2048) | 4096 | May work (PCIe can handle larger) |
| Scatter header | 0xa000 at +0x24, seq=0 | depends on path | Need to verify our scatter path matches |
| Token for scatter | 0x10 with token=0xee | varies | Token mismatch could cause FW to not ACK |

---

## 10. Register Writes We're Missing (Complete List)

### During WFDMA Init (before FWDL):
| Register | Bus Addr | BAR0 Offset | Windows Value | Our Driver | Priority |
|----------|----------|-------------|---------------|------------|----------|
| GLO_CFG base | 0x7c024208 | 0xd4208 | \|= 0x50001070 | only \|= 0x5 | **P0 — CRITICAL** |
| GLO_CFG extra | 0x7c024208 | 0xd4208 | \|= 0x208000 | missing | **P0 — CRITICAL** |
| GLO_CFG_EXT1 | 0x7c0242b4 | 0xd42b4 | \|= 0x10000000 | never written | **P0 — CRITICAL** |
| INT status clear TX | 0x7c02420c | 0xd420c | = 0xFFFFFFFF | never written | P1 |
| INT status clear RX | 0x7c024280 | 0xd4280 | = 0xFFFFFFFF | never written | P1 |
| INT_ENA | 0x7c02422c | 0xd422c | = 0x2600f000 | never written | P1 |
| INT_SOURCE_CSR | 0x7c024204 | 0xd4204 | read | never read | P2 |
| MSI map 0 | 0x7c0270f0 | 0xd70f0 | = 0x00660077 | never written | P1 (if MSI) |
| MSI map 1 | 0x7c0270f4 | 0xd70f4 | = 0x00001100 | never written | P1 (if MSI) |
| MSI map 2 | 0x7c0270f8 | 0xd70f8 | = 0x0030004f | never written | P1 (if MSI) |
| MSI map 3 | 0x7c0270fc | 0xd70fc | = 0x00542200 | never written | P1 (if MSI) |
| CONN_INFRA | 0x74030188 | ??? | toggle BIT(16) | never written | P1 (MT6639-specific) |

### After FWDL:
| Register | Bus Addr | BAR0 Offset | Windows Value | Our Driver | Priority |
|----------|----------|-------------|---------------|------------|----------|
| DMASHDL | 0x7c026060 | 0xd6060 | \|= 0x10101 | never written | **P0 — CRITICAL** |

### TXD Format Fix:
| Field | Windows | Our Driver | Fix |
|-------|---------|------------|-----|
| TXD[1] BIT(31) | NEVER set | Set by default | **Set no_long_format=1** |

---

## 11. Recommended Fix Order

### Phase 1: GLO_CFG + TXD Fix (Most Likely to Fix MCU Response)

1. **Fix GLO_CFG initialization:**
   ```c
   // In mt7927_dma_enable():
   val = readl(bar0 + 0xd4208);
   val |= 0x50001070;    // Windows base bits
   val |= 0x208000;      // Windows extra bits
   writel(val, bar0 + 0xd4208);

   // GLO_CFG_EXT1
   val = readl(bar0 + 0xd42b4);
   val |= 0x10000000;    // BIT(28)
   writel(val, bar0 + 0xd42b4);

   // Then after ring init:
   val = readl(bar0 + 0xd4208);
   val |= 0x5;           // TX_DMA_EN | RX_DMA_EN
   writel(val, bar0 + 0xd4208);
   ```

2. **Fix TXD format — disable LONG_FORMAT:**
   ```c
   // Set no_long_format = 1 (module param or hardcode)
   // This removes BIT(31) from TXD[1]
   ```

3. **Clear interrupt status before enabling DMA:**
   ```c
   writel(0xFFFFFFFF, bar0 + 0xd420c);  // clear TX INT status
   writel(0xFFFFFFFF, bar0 + 0xd4280);  // clear RX INT status
   ```

### Phase 2: DMASHDL + Interrupt Config

4. **Enable DMASHDL after FWDL:**
   ```c
   val = readl(bar0 + 0xd6060);
   val |= 0x10101;
   writel(val, bar0 + 0xd6060);
   ```

5. **Configure interrupt mask:**
   ```c
   writel(0x2600f000, bar0 + 0xd422c);  // INT enable
   ```

### Phase 3: PostFwDownloadInit MCU Commands

6. Implement the 9 MCU commands (once we can receive responses)

---

## 12. Hypothesis: Why MCU_INT_STA=1 But No Response

The MCU IS processing our commands (MCU_INT_STA shows activity). The response IS being generated internally. But the WFDMA is NOT routing the response to HOST RX ring 6 because:

1. **GLO_CFG missing bits** — The 0x50001070 bitmask likely includes bits that enable the MCU→HOST event routing path in WFDMA. Without these bits, WFDMA accepts TX (our commands go through) but doesn't deliver RX (responses are dropped/stuck).

2. **GLO_CFG_EXT1 BIT(28)** — This may be the "event delivery enable" bit. Without it, MCU responses stay in internal WFDMA buffers and never reach HOST memory.

3. **DMASHDL not enabled** — The DMA scheduler may need to be running to arbitrate between internal queues. Without it, responses queue internally but never get scheduled for delivery.

4. **TXD LONG_FORMAT** — If BIT(31) is set, WFDMA reads 8 DWORDs (32 bytes) as TXD. If not set, it reads 2 DWORDs (8 bytes). With LONG_FORMAT, our MCU header at offset 0x08-0x1F is parsed as TXD[2-7], potentially corrupting the routing information. The MCU might still process the command (it's forgiving) but route the response incorrectly.

**Most likely single fix: GLO_CFG = 0x50209075 (all Windows bits combined) + GLO_CFG_EXT1 BIT(28)**

---

*Document generated from analysis of Windows mtkwecx.sys v5.7.0.5275 Ghidra RE output vs mt7927_init_dma.c*
*Date: 2026-02-15*
