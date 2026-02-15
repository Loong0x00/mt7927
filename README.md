# MT7927 WiFi 7 Linux Driver Project

## Original Author's Work

This project was originally started by [ehausig](https://github.com/ehausig/mt7927), who successfully implemented PCIe device binding and basic firmware loading. The original author put the project on hold after reaching the DMA transfer stage. Much of the initial reverse engineering work and driver structure came from their efforts.

## Current Development Status

**CRITICAL DISCOVERY**: MT7927 is actually the MT6639 mobile chip in a PCIe package, NOT part of the MT76 WiFi chip family. MediaTek has never published a Linux WiFi driver for it. We are writing one from scratch based on reverse engineering Windows drivers and studying MediaTek's Android MT6639 reference code.

### What's Working ✅
- **PCIe enumeration and BAR mapping**: Device detection and memory mapping successful
- **DMA ring initialization**: WFDMA rings configured and operational
- **Firmware download**: Complete fw_sync=0x3 achieved, firmware boots successfully
- **CLR_OWN/SET_OWN handshake**: Proper device ownership control implemented
- **Windows driver reverse engineering**: Full PostFwDownloadInit sequence documented

### Current Blocker ❌
**Post-boot MCU command communication fails**: After successful firmware boot, the first MCU command (NIC_CAPABILITY) times out with -110 error. Root cause is that firmware never configures MCU_RX0 DMA ring (BASE remains 0x00000000), preventing host-to-MCU message delivery. 52+ initialization experiments have been conducted to solve this issue.

### Key Technical Findings
- **MT6639 architecture**: PCI ID 14c3:6639, requires different init sequence than mt7925
- **PCIe FLR is fatal**: Device enters D3cold state and never recovers
- **CLR_OWN side effects**: ROM resets ALL WFDMA rings, requires full reprogramming
- **TXD format**: Must use Q_IDX=0x20 (MT_TX_MCU_PORT_RX_Q0), NO BIT(31) LONG_FORMAT flag
- **PostFwDownloadInit**: 9 MCU commands with specific DMASHDL configuration (reverse engineered from Windows driver v5603998)

## Quick Start

### Prerequisites
```bash
# Verify MT7927/MT6639 device is present
lspci -nn | grep 14c3:6639  # Should show MT7927 (note: PCI ID is 6639, not 7927)
```

### Install Firmware
```bash
# MT7927 uses MT7925 firmware files (confirmed working for firmware download phase)
sudo mkdir -p /lib/firmware/mediatek/mt7925

# Download firmware (or copy from existing installation)
# Files needed: WIFI_MT7925_PATCH_MCU_1_1_hdr.bin, WIFI_RAM_CODE_MT7925_1_1.bin
```

### Build and Load Driver
```bash
# Clone and build
git clone https://github.com/Loong0x00/mt7927
cd mt7927
make clean && make tests

# Load the test driver (current development version)
sudo insmod tests/04_risky_ops/mt7927_init_dma.ko

# Check status
sudo dmesg | tail -50
lspci -k | grep -A 3 "14c3:6639"
```

## Technical Details

### Hardware Information
- **Chip**: MediaTek MT7927 = MT6639 mobile chip in PCIe package
- **PCI ID**: 14c3:6639 (NOT 7927!)
- **Architecture**: Different from MT76 family (mt7921/mt7925), uses MT6639 mobile chipset design
- **Bluetooth**: Works via USB (btusb/btmtk), WiFi is PCIe (completely separate subsystems)

### Current Challenge: MCU Command Communication
After successful firmware boot (fw_sync=0x3), the driver encounters a blocker when sending the first MCU command:
- **Problem**: MCU_RX0 DMA ring BASE stays at 0x00000000 (never configured by firmware)
- **Symptom**: NIC_CAPABILITY command times out (-110 error)
- **Hypothesis**: Firmware expects HOST RX ring 0 to be allocated before it configures MCU_RX0
- **Status**: Testing MODE 53 with HOST RX ring 0 pre-allocation

### Eliminated Approaches (52+ modes tested)
- R2A FSM manipulation
- NEED_REINIT flag variations
- Post-boot CLR_OWN sequences
- Direct MCU_RX0 register writes
- WFSYS/CONN_INFRA reset attempts
- Full vendor DMASHDL configuration
- Various Q_IDX and TXD format combinations

## Project Structure
```
mt7927/
├── README.md                               # This file
├── Makefile                                # Build system
├── tests/04_risky_ops/
│   └── mt7927_init_dma.c                  # Main development driver (single file)
├── docs/                                   # Development logs and research notes
│   ├── session_status_2026-02-15.md       # Latest status summary
│   ├── references/                         # Windows driver RE analysis
│   └── win_v5705275_*.md                  # Detailed reverse engineering docs (14 files)
├── mt6639/                                 # Android reference code (Motorola)
└── mt76/mt7925/                           # Upstream mt7925 driver (reference only)
```

## Development Roadmap

### Current Phase: MCU Communication
- [x] PCIe device enumeration
- [x] DMA ring initialization
- [x] Firmware download (fw_sync=0x3)
- [x] Windows driver reverse engineering
- [ ] **Solve MCU_RX0 configuration issue** ← Current blocker
- [ ] Post-boot MCU command sequence
- [ ] Network interface creation

### Future Phases
- [ ] mac80211 integration
- [ ] WiFi 7 feature implementation
- [ ] Upstream submission to linux-wireless

## Research References

### Primary Code References
1. **mt6639/** - Motorola Android MT6639 driver (most relevant architecture match)
2. **mt76/mt7925/** - Upstream mt7925 driver (similar but different chip family)
3. **Windows drivers** - Reverse engineered using Ghidra:
   - DRV_WiFi_MTK_*_V5603998_* (analyzed PostFwDownloadInit sequence)
   - WiFi_AMD-MediaTek_v5.7.0.5275 (deeper MCU command analysis)

### Documentation
- See `docs/` directory for detailed development logs
- `docs/win_v5705275_*.md` - 14 files documenting Windows driver RE
- `docs/session_status_2026-02-15.md` - Latest development status
- `docs/references/` - Specific technical investigations

## Known Issues & Warnings

### CRITICAL: Dangerous Operations
- **DO NOT use `pcie_flr()`**: Puts device into D3cold, requires hard reboot
- **DO NOT use `pci_reset_function()` in probe**: Causes deadlock (device mutex conflict)
- **CB_INFRA_RGU WF_SUBSYS_RST**: Makes device unrecoverable without reboot - use sparingly

### Device Recovery
```bash
# If device becomes unresponsive (0xffffffff reads)
echo 1 | sudo tee /sys/bus/pci/devices/0000:XX:00.0/remove
sleep 2
echo 1 | sudo tee /sys/bus/pci/rescan
# Replace XX:00.0 with your device's actual PCI address
```

## Development History

This driver development has involved:
- **150+ test boots** across 52+ experimental modes
- **Complete Windows driver reverse engineering** (2 versions analyzed with Ghidra)
- **BAR0 address map validation** for MT7927/MT6639 register layout
- **Firmware encryption research** (DL_MODE_ENCRYPT flags critical for success)
- **Community research**: Confirmed we have the most advanced MT7927 Linux driver work (GitHub user ehausig's work stalled at same MCU_RX0 issue)

## FAQ

**Q: Is MT7927 the same as MT7925?**
A: No. MT7927 = MT6639 mobile chip in PCIe package, completely different architecture from MT76 family (mt7921/mt7925).

**Q: Why not use the mt7925 driver?**
A: MT7927 uses PCI ID 14c3:6639 and has different initialization requirements. Simply adding the PCI ID to mt7925 doesn't work.

**Q: Does any MT7927 Linux driver exist?**
A: No. MediaTek never published one. The ehausig/mt7927 GitHub project (the original version of this repo) reached the same MCU_RX0 blocker we're currently debugging.

**Q: Can this brick my hardware?**
A: No permanent damage is possible via software. Worst case: device becomes unresponsive until PCI rescan or system reboot.

## License
GPL v2 - Intended for upstream Linux kernel submission

## Credits
- **Original author**: [ehausig](https://github.com/ehausig/mt7927) - Initial reverse engineering and driver structure
- **Current development**: [Loong0x00](https://github.com/Loong0x00) - Firmware download implementation, Windows driver RE, MCU communication research

---

**Status as of 2026-02-15**: Firmware download working (fw_sync=0x3 achieved). Current focus: solving MCU_RX0 DMA ring configuration to enable post-boot MCU command communication. This is a from-scratch driver development effort based on reverse engineering Windows drivers and studying MediaTek's Android MT6639 reference code.
