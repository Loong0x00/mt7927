# Mode 40 Reboot Test Results (2026-02-15)

## Test Context
- **Machine state**: Fresh reboot (clean state)
- **Test mode**: Mode 40 (CB_INFRA_RGU WFSYS reset)
- **Hypothesis**: CB_INFRA_RGU reset on clean machine might allow MCU_RX0 configuration

## Critical Results

### ❌ HYPOTHESIS ELIMINATED
**MCU_RX0 BASE remains 0x00000000** even after clean reboot + CB_INFRA_RGU reset.

### Key Observations

1. **Firmware Download**: ✓ SUCCESS
   - `fw_sync=0x00000003` - firmware booted successfully

2. **MCU_RX0 Configuration**: ✗ FAILED (BLOCKER)
   - `MCU_RX0 BASE=0x00000000` - never configured by firmware

3. **ROMCODE_INDEX**: ✗ UNEXPECTED
   - Got: `0xdead1234`
   - Expected: `0x1D1E` (MCU_IDLE)
   - Indicates CB_INFRA_RGU reset may not complete properly

4. **Ring 15 Consumption**: ✗ FAILED
   - `CIDX=0x0d, DIDX=0x0c` - descriptor not consumed
   - MCU not reading from ring 15

5. **NIC_CAPABILITY**: ✗ TIMEOUT
   - Result: `-110` (ETIMEDOUT)
   - MCU_CMD register: `0x00008000` (MCU2HOST_SW_INT_STA BIT(15))
   - Firmware signaling via register, NOT DMA

## Full dmesg Output

```
[  139.387437] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: FW re-download SUCCESS, fw_sync=0x00000003
[  139.387440] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: GLO_CFG before bypass clear: 0x5430ba75
[  139.387443] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: GLO_CFG after bypass clear: 0x5430b875
[  139.387446] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: DMASHDL_SW_CONTROL before: 0x10000000
[  139.387450] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: DMASHDL_SW_CONTROL after: 0x00000000
[  139.387452] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: GLO_CFG_EXT1 before: 0x8c800404
[  139.387455] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: GLO_CFG_EXT1 after: 0x9c800404
[  139.387461] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: DMASHDL(0xd6060): 0x00000000 -> 0x00010101 (wrote |= 0x10101)
[  139.387462] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: === DIAGNOSTIC DUMP ===
[  139.387464] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: *** MCU_RX0 BASE=0x00000000 *** (STILL ZERO - blocker)
[  139.387467] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_RX1 BASE=0x00000000
[  139.387469] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     DMASHDL_ENABLE(0xd6060)=0x00010101
[  139.387472] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     ROMCODE_INDEX(0xc1604)=0xdead1234 (expect 0x1D1E)
[  139.387475] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     CONN_INFRA_WFSYS_SW_RST(0xe0010)=0x00000000
[  139.387478] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     fw_sync(CONN_ON_MISC)=0x00000003
[  139.387480] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_DMA0 GLO_CFG=0x10703875 (TX_EN=1 RX_EN=1)
[  139.387490] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_RX0: BASE=0x00000000 CNT=0x00000200 CIDX=0x00000000 DIDX=0x00000000
[  139.387499] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_RX1: BASE=0x00000000 CNT=0x00000200 CIDX=0x00000000 DIDX=0x00000000
[  139.387508] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_RX2: BASE=0x0226ca00 CNT=0x00000028 CIDX=0x00000027 DIDX=0x00000000
[  139.387518] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_RX3: BASE=0x0226cc80 CNT=0x00000028 CIDX=0x00000027 DIDX=0x00000000
[  139.387527] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_TX0: BASE=0x00000000 CNT=0x00000200 CIDX=0x00000000 DIDX=0x00000000
[  139.387537] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_TX1: BASE=0x00000000 CNT=0x00000200 CIDX=0x00000000 DIDX=0x00000000
[  139.387545] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_TX2: BASE=0x00000000 CNT=0x00000200 CIDX=0x00000000 DIDX=0x00000000
[  139.387554] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     MCU_TX3: BASE=0x00000000 CNT=0x00000200 CIDX=0x00000000 DIDX=0x00000000
[  139.387559] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     CB_INFRA_RGU=0x00010340 RGU_DBG=0x000103c0
[  139.387567] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     HOST GLO_CFG=0x5430b875 INT_ENA=0x2e000043 INT_STA=0x00004000
[  139.387576] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40:     HOST RXQ6: BASE=0x0a23d000 CNT=0x00000080 CIDX=0x0000000b DIDX=0x0000000c
[  139.387577] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: MCU_RX0 BASE still 0 — attempting NIC_CAP anyway with Q_IDX=0x20
[  139.387578] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: NIC_CAP cmd: len=64 TXD[0]=0x41000040 TXD[1]=0x00004000 cid=0x8a pq_id=0x008a seq=14 opt=0x07
[  139.387580] [MT7927] MODE40 nic-cap-txd: 00000000: 40 00 00 41 00 40 00 00 00 00 00 00 00 00 00 00
[  139.387581] [MT7927] MODE40 nic-cap-txd: 00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  139.387582] [MT7927] MODE40 nic-cap-txd: 00000020: 20 00 8a 00 8a a0 00 0e 00 00 00 07 00 00 00 00
[  139.387582] [MT7927] MODE40 nic-cap-txd: 00000030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  139.387596] mt7927_init_dma 0000:09:00.0: kick-before: GLO=0x5430b875 EXT1=0x9c800404 INT_ENA=0x2e000043 INT_STA=0x00004000 MCU_CMD=0x00008000 SW_INT_ENA=0x000000bd
[  139.387610] mt7927_init_dma 0000:09:00.0: kick-before: q15 BASE=0x0b021000 CNT=0x00000100 CIDX=0x0000000c DIDX=0x0000000c TX_PRI=0x00000000 RX_PRI=0x00000000
[  139.387624] mt7927_init_dma 0000:09:00.0: kick-after: GLO=0x5430b877 EXT1=0x9c800404 INT_ENA=0x2e000043 INT_STA=0x00004000 MCU_CMD=0x00008000 SW_INT_ENA=0x000000bd
[  139.387637] mt7927_init_dma 0000:09:00.0: kick-after: q15 BASE=0x0b021000 CNT=0x00000100 CIDX=0x0000000d DIDX=0x0c TX_PRI=0x00000000 RX_PRI=0x00000000
[  139.487756] mt7927_init_dma 0000:09:00.0: ring15 not consumed: cpu_idx=0xd dma_idx=0xc
[  139.487763] mt7927_init_dma 0000:09:00.0: mode40-nic-cap: HOST_INT_STA=0x00004000 MCU_CMD=0x00008000
[  139.487765] mt7927_init_dma 0000:09:00.0: [MT7927] MODE40: NIC_CAPABILITY (Q_IDX=0x20) result=-110
[  139.487766] mt7927_init_dma 0000:09:00.0: [MT7927] === MODE 40: COMPLETE ===
```

## Analysis

### TXD Format
TXD format matches Windows driver exactly:
- `TXD[0]=0x41000040` (Q_IDX=0x20, PKT_FMT=2) ✓
- `TXD[1]=0x00004000` (HDR_FORMAT_V3=1, no BIT(31)) ✓

### DMASHDL Configuration
- `DMASHDL_ENABLE(0xd6060)=0x00010101` - written correctly ✓
- Mode 50 already tested full vendor DMASHDL config - no effect

### MCU Communication Path
Firmware signals via `MCU_CMD=0x8000` (MCU2HOST_SW_INT_STA BIT(15)), but:
- **MCU does NOT consume ring 15 descriptors** (DIDX stuck)
- **MCU does NOT configure MCU_RX0** (BASE=0)
- Firmware is running (fw_sync=0x3) but not using HOST→MCU DMA path

## Conclusion

**CB_INFRA_RGU WFSYS reset does NOT solve MCU_RX0 configuration issue**, even on clean reboot.

The problem is NOT:
- Residual state from previous boots ✗
- Wrong WFSYS reset method ✗
- Missing DMASHDL config ✗
- Wrong TXD format ✗

The problem IS:
- **Firmware fundamentally does NOT configure MCU_RX0 on MT7927/MT6639**
- **Firmware does NOT use HOST ring 15 for MCU commands**
- This is a FUNDAMENTAL architectural difference from MT7925

## Next Steps Required

Need to investigate Windows PostFwDownloadInit sequence:
1. The 9 MCU commands sent after FWDL (class 0x8a, 0x02, 0xc0, 0xed, 0x28, 0xca×3)
2. Whether Windows uses a DIFFERENT command path (not ring 15)
3. Whether there's a missing init sequence to enable MCU_RX0

See: `docs/references/ghidra_post_fw_init.md` for Windows RE details.
