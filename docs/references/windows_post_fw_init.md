# AsicConnac3xPostFwDownloadInit — Windows Driver Reverse Engineering

## 1. Function Location

- **Binary**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (v5.7.0.5275)
- **String**: `AsicConnac3xPostFwDownloadInit` at file offset 0x2241a0, VA 0x140224da0
- **Function**: `FUN_1401d4e00` (VA 0x1401d4e00 - 0x1401d5479, 1657 bytes)
- **Xref**: LEA R13, [RIP+0x4ff6a] at VA 0x1401d4e2f

## 2. Function Summary

`AsicConnac3xPostFwDownloadInit` is called AFTER firmware download completes. It performs:

1. **DMASHDL configuration** (`FUN_1401d7738`):
   - Read-modify-write 0x7c026060 |= 0x10101 (DMASHDL_OPTIONAL_CONTROL?)
   - BAR0 offset: 0xd6060

2. **Two init sub-functions** (`FUN_1401d5f08`, `FUN_1401d777c`):
   - 298 bytes each, error-checked
   - No visible register addresses or strings (indirect register access via wrappers)

3. **MCU command (CRITICAL!)** — sent via `MtCmdSendSetQueryCmdAdv` (FUN_1400cdc4c):
   ```
   Parameters:
     param1 (rcx) = adapter->mcu_ctrl (offset 0x14c0)
     param2 (dl)  = 0xc0  (command class/group?)
     param3 (r8b) = 0xed  (command ID? — note: 0xee = FW_SCATTER)
     param4 (r9d) = 0
     [rsp+0x20]   = 1     (wait for response?)
     [rsp+0x28]   = 8     (payload length)
     [rsp+0x30]   = &payload
     [rsp+0x38]   = 8     (wire payload size)

   Payload (8 bytes):
     [0:3] = 0x820cc800   (SRAM address? MCU config buffer?)
     [4:7] = 0x0003c200   (length/flags?)
   ```

4. **FUN_1401cd1e8** (1302 bytes) — post-MCU-command processing

5. **Chip-specific call for MT6639/MT7927/MT0738** (`FUN_1400d3c40`):
   - Checks device ID at [rdi+0x1f72] against 0x6639, 0x738, 0x7927
   - Only called for these specific chips
   - Sends ANOTHER MCU command via `MtCmdSendSetQueryCmdAdv`:
     ```
     cmd = 0xed, group = 0x28
     payload = 36-byte (0x24) structure built on stack
     Structure fields from parameters:
       [0x00] = param2 (dl=1 from caller)
       [0x01] = computed from param2 (sbb+and pattern → 0 or 2)
       [0x03] = param3 (r8b=0 from caller)
       [0x04] = sete result of param4 (r9b=0 → sete=1)
     ```
   - 626 bytes, with its own error logging

6. **1ms delay** (KeStallExecutionProcessor(10) × 100 = 1000μs)

7. **Additional init functions**:
   - `FUN_1401d6ae0` (406 bytes) — post-delay init
   - `FUN_1401d82c4` (1120 bytes) — conditional on byte flag
   - `FUN_1401d6994` (329 bytes) — conditional on dword != 0
   - `FUN_1401d67d4` (422 bytes) — always called

8. **Second chip-specific call** for MT6639/MT7927/MT0738/MT7925/MT0717:
   - Calls `FUN_1400c3d50` (530 bytes) — conditional on a byte flag
   - Broader chip coverage than step 5

9. **Final cleanup**: `FUN_1400c3340` (1331 bytes) — always called

## 3. Key Analysis Points

### 3.1 The MCU Command 0xed

Command ID 0xed is NOT in the FWDL whitelist (0x01/0x02/0x03/0x05/0x07/0x10/0x11/0xee/0xef).
However, 0xed = 0xee - 1, very close to FW_SCATTER (0xee). This could be:

- **INIT_CMD_ID for post-FW-download init** — a special command that tells the running FW
  to configure MCU_RX0/RX1 and open the command channel
- The payload {0x820cc800, 0x3c200} looks like an SRAM address + size/config

### 3.2 Chip-Specific MT6639/MT7927 Handling

The function has EXPLICIT chip-specific code for MT6639 (0x6639) and MT7927 (0x7927).
This sends a SECOND MCU command with a 36-byte (0x24) structure. This could be the
MT7927-specific init step that our driver is missing.

### 3.3 DMASHDL 0x7c026060

The function starts by OR-ing 0x10101 into register 0x7c026060 (BAR0 0xd6060).
This is in the DMASHDL register space. Our driver does not write this register.

## 4. Upstream MT7927 Support Status

### MT7927 IS supported upstream in mt76/mt7925:

```c
// mt76/mt7925/pci.c:
static const struct pci_device_id mt7925_pci_device_table[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x7925), ... },
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x6639),     // <-- MT7927!
        .driver_data = (kernel_ulong_t)MT7927_FIRMWARE_WM },
    { PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK, 0x0717), ... },
};
```

### Upstream handles MT7927 via mt792x helpers:

```c
// mt76/mt76_connac.h:
static inline bool is_connac_v3(struct mt76_dev *dev) {
    return mt76_chip(dev) == 0x7925 || mt76_chip(dev) == 0x6639;
}

// mt76/mt792x.h:
#define MT7927_FIRMWARE_WM    "mediatek/WIFI_RAM_CODE_MT6639_2_1.bin"
#define MT7927_ROM_PATCH      "mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin"
```

### CRITICAL: Upstream uses SAME init flow for MT7925 and MT7927

`mt7925e_mcu_init()` in `mt76/mt7925/pci_mcu.c` does NOT have any MT7927-specific branches:
1. `mt792xe_mcu_fw_pmctrl()` — SET_OWN
2. `__mt792xe_mcu_drv_pmctrl()` — CLR_OWN
3. `mt76_rmw_field(dev, MT_PCIE_MAC_PM, MT_PCIE_MAC_PM_L0S_DIS, 1)` — L0s disable
4. `mt7925_run_firmware()` — FWDL + first MCU command

**There is NO equivalent of `AsicConnac3xPostFwDownloadInit` in the upstream driver.**
The upstream driver relies entirely on the FWDL sequence + CLR_OWN to get MCU_RX0 configured.

## 5. Implications for Our Driver

### What we're missing (compared to Windows):

1. **MCU command 0xed** with payload {0x820cc800, 0x3c200} — sent AFTER FW download,
   potentially tells FW to configure MCU_RX0

2. **Chip-specific MCU command** for MT6639/MT7927 with 36-byte payload — may configure
   MT7927-specific hardware that differs from MT7925

3. **DMASHDL register 0xd6060** |= 0x10101 — enables some DMASHDL feature

### The chicken-and-egg problem:

The MCU command 0xed is sent via `MtCmdSendSetQueryCmdAdv` which uses normal MCU command
path (TX15 → MCU_RX0). But if MCU_RX0 is not configured, how can this command reach the MCU?

Possible resolutions:
- **Command may go via FWDL path** (TX16 → MCU_RX2/RX3) — check if cmd 0xed gets special routing
- **MCU_RX0 may be configured by FW at fw_sync=0x3** — and our issue is something else
  (routing, not ring config)
- **Windows does WFSYS reset before FWDL** — which may put WFDMA in a state where
  MCU_RX0 gets configured during FW boot

### Key question to investigate:

Does `MtCmdSendSetQueryCmdAdv` with cmd=0xed use TX15 (normal MCU) or TX16 (FWDL)?
If FWDL, then FW receives it on MCU_RX2/RX3 which IS configured, and this command
could be the trigger for MCU_RX0 configuration.

## 6. Call Flow Summary

```
AsicConnac3xPostFwDownloadInit (FUN_1401d4e00)
├── FUN_1401d7738: DMASHDL 0x7c026060 |= 0x10101
├── FUN_1401d5f08: init sub-function (error check)
├── FUN_1401d777c: init sub-function (error check)
├── MCU cmd: MtCmdSendSetQueryCmdAdv(0xc0, 0xed, payload={0x820cc800, 0x3c200})
├── FUN_1401cd1e8: post-MCU processing (1302 bytes)
├── [MT6639/MT7927/MT0738 only]:
│   └── FUN_1400d3c40: chip-specific MCU cmd (0x28, 0xed, 36-byte struct)
├── 1ms delay (KeStallExecutionProcessor)
├── FUN_1401d6ae0: post-delay init
├── FUN_1401d82c4: conditional init (1120 bytes)
├── FUN_1401d6994: conditional init (329 bytes)
├── FUN_1401d67d4: always-called init (422 bytes)
├── [MT6639/MT7927/MT0738/MT7925/MT0717]:
│   └── FUN_1400c3d50: broader chip-specific (530 bytes)
├── FUN_1400c3340: final cleanup (1331 bytes)
└── Return
```
