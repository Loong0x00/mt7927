# MT7927 Linux WiFi Driver

[English](#english) | [中文](#中文)

---

<a id="中文"></a>

## 中文

从零编写的 MediaTek MT7927 (MT6639) WiFi 7 PCIe Linux 内核驱动，基于 Windows 驱动逆向工程开发。

**MediaTek 从未发布过该芯片的 Linux WiFi 驱动，这是全球第一个可用的开源实现。**

> **注意：** 本项目全程使用 [Claude Code](https://claude.ai/code)（Anthropic AI 编程代理）开发，开发者本人不具备编程背景。代码在不到两周内完成，目前仅保证基本功能可用。**代码需要有经验的内核开发者审查和优化**，欢迎贡献。

### 芯片信息

| 项目 | 说明 |
|------|------|
| 芯片 | MT7927 = MT6639 移动芯片的 PCIe 封装 |
| PCI ID | `14c3:6639` |
| WiFi | PCIe 接口，WiFi 7 (802.11be) |
| 蓝牙 | USB 接口 (btusb/btmtk)，与 WiFi 独立 |
| 注意 | **不属于 MT76 家族**，MT76 驱动不适用 |

### 当前状态

| 功能 | 状态 |
|------|------|
| PCIe + BAR0 + 电源管理 | :white_check_mark: |
| WFDMA DMA 引擎 | :white_check_mark: |
| 固件下载 (patch + 6 RAM regions) | :white_check_mark: |
| MCU UniCmd 通信 | :white_check_mark: |
| mac80211 注册 (2.4G + 5G) | :white_check_mark: |
| WiFi 扫描 (135+ BSS) | :white_check_mark: |
| 802.11 Authentication | :white_check_mark: |
| 802.11 Association | :white_check_mark: |
| WPA2-PSK 4-Way Handshake | :white_check_mark: |
| 硬件加密 (HW Crypto) | :white_check_mark: |
| DHCP + 互联网连接 | :white_check_mark: |
| 5GHz | 待测试 |
| 6GHz / 320MHz 带宽 | 未测试（开发者仅有 WiFi 5 路由器） |
| WiFi 6/7 高级特性 | 待开发 |

### 固件获取

驱动需要 MediaTek 固件文件，放置在 `/lib/firmware/mediatek/` 下。固件为专有二进制，不包含在本仓库中。

可从华硕主板驱动页面提取固件（WiFi 驱动包内含固件文件）：

> https://rog.asus.com/motherboards/rog-crosshair/rog-crosshair-x870e-hero/helpdesk_download/

下载 WiFi 驱动 → 解压 → 在 `WiFi_AMD-MediaTek_*/` 目录下找到 `.bin` 固件文件 → 复制到 `/lib/firmware/mediatek/`。

### 构建

```bash
# 需要内核头文件 (linux-headers)
make driver          # 构建 → src/mt7927.ko

sudo insmod src/mt7927.ko    # 加载
sudo rmmod mt7927            # 卸载
```

### 连接 WiFi

```bash
# 创建 wpa_supplicant 配置
cat > /tmp/wpa.conf << 'EOF'
ctrl_interface=/var/run/wpa_supplicant

network={
    ssid="YOUR_SSID"
    psk="YOUR_PASSWORD"
    key_mgmt=WPA-PSK
}
EOF

# 启动
sudo ip link set wlp9s0 up
sudo wpa_supplicant -i wlp9s0 -c /tmp/wpa.conf -B
sudo dhcpcd wlp9s0
```

### 代码结构

```
src/
├── mt7927_pci.c    # 初始化、MCU 通信、mac80211 回调 (~4600 行)
├── mt7927_pci.h    # 寄存器定义、数据结构、宏 (~1970 行)
├── mt7927_mac.c    # TXD 构建、RXD 解析、RX 帧分发 (~1500 行)
└── mt7927_dma.c    # 中断处理、NAPI poll、DMA 操作 (~900 行)
```

四个文件编译为单一内核模块 `mt7927.ko`（约 9000 行）。

### 文档

#### `docs/re/` — Windows 逆向工程文档 (52 篇)

本驱动的**唯一参考来源**。通过 Ghidra 逆向 Windows 驱动二进制，记录了：
- MCU 命令格式和 CID 映射 (`win_re_cid_mapping.md`)
- BSS_INFO / STA_REC TLV 结构 (`win_re_bss_info_all_tlvs.md`, `win_re_codex_starec_verify.md`)
- TXD/DMA 描述符格式 (`win_re_full_txd_dma_path.md`, `win_re_dma_descriptor_format.md`)
- 完整连接流程 (`win_re_full_connect_cmd_sequence.md`)
- 固件下载和初始化 (`win_re_class02_and_postinit.md`)
- 寄存器映射和 RF/PHY 配置 (`windows_register_map.md`, `win_re_full_rf_phy_config.md`)

#### `docs/debug/` — 调试记录

开发过程中的问题分析和诊断记录。

#### `docs/archive/` — 归档文档

早期开发过程中的笔记、计划和会话记录。`low_trust/` 子目录包含 mt6639 Android 驱动和 mt76 上游驱动的参考代码摘录，这些在开发过程中被证明**不可信**，仅保留供历史参考。

### 开发历程

本项目在不到两周内完成，经历 44 个开发 session，全程使用 Claude Code：

- **S1-S21**: PCIe 初始化、固件下载、DMA 引擎、MCU 通信
- **S22**: WiFi 扫描工作
- **S31**: TX 空口发射确认 (AR9271 抓包验证)
- **S39**: RX Ring 4 DMA 投递修复
- **S40**: Auth + Association 成功 (突破 28 session 阻塞)
- **S43**: WPA2 4-Way Handshake 完成，WiFi 连接成功
- **S44**: 硬件加密实现，DHCP + 互联网 ping 通

### 许可证

驱动源码基于 GPL-2.0 许可证（Linux 内核模块要求）。

### 免责声明

本项目基于合法的逆向工程开发，用于实现硬件兼容性（互操作性）。仓库中不包含任何专有二进制文件或受版权保护的源代码。

---

<a id="english"></a>

## English

A from-scratch Linux kernel driver for the MediaTek MT7927 (MT6639) WiFi 7 PCIe chip, developed entirely through Windows driver reverse engineering.

**MediaTek has never released a Linux WiFi driver for this chip. This is the first working open-source implementation in the world.**

> **Note:** This project was developed entirely using [Claude Code](https://claude.ai/code) (Anthropic's AI coding agent) by a non-programmer. The code was written in under two weeks and currently only guarantees basic functionality. **The code needs review and optimization by experienced kernel developers.** Contributions are welcome.

### Chip Info

| Item | Description |
|------|-------------|
| Chip | MT7927 = MT6639 mobile chip in PCIe package |
| PCI ID | `14c3:6639` |
| WiFi | PCIe interface, WiFi 7 (802.11be) |
| Bluetooth | USB interface (btusb/btmtk), separate from WiFi |
| Note | **NOT part of the MT76 family** — MT76 drivers do not apply |

### Current Status

| Feature | Status |
|---------|--------|
| PCIe + BAR0 + Power Management | :white_check_mark: |
| WFDMA DMA Engine | :white_check_mark: |
| Firmware Download (patch + 6 RAM regions) | :white_check_mark: |
| MCU UniCmd Communication | :white_check_mark: |
| mac80211 Registration (2.4G + 5G) | :white_check_mark: |
| WiFi Scanning (135+ BSS) | :white_check_mark: |
| 802.11 Authentication | :white_check_mark: |
| 802.11 Association | :white_check_mark: |
| WPA2-PSK 4-Way Handshake | :white_check_mark: |
| Hardware Encryption (HW Crypto) | :white_check_mark: |
| DHCP + Internet Access | :white_check_mark: |
| 5GHz | Untested |
| 6GHz / 320MHz Bandwidth | Untested (developer only has a WiFi 5 router) |
| WiFi 6/7 Advanced Features | Not yet implemented |

### Firmware

The driver requires MediaTek firmware files placed in `/lib/firmware/mediatek/`. Firmware is proprietary and not included in this repository.

You can extract firmware from ASUS motherboard driver packages (the WiFi driver package contains firmware files):

> https://rog.asus.com/motherboards/rog-crosshair/rog-crosshair-x870e-hero/helpdesk_download/

Download the WiFi driver → extract → find `.bin` firmware files under `WiFi_AMD-MediaTek_*/` → copy to `/lib/firmware/mediatek/`.

### Build

```bash
# Requires kernel headers (linux-headers)
make driver          # Build → src/mt7927.ko

sudo insmod src/mt7927.ko    # Load
sudo rmmod mt7927            # Unload
```

### Connect to WiFi

```bash
# Create wpa_supplicant config
cat > /tmp/wpa.conf << 'EOF'
ctrl_interface=/var/run/wpa_supplicant

network={
    ssid="YOUR_SSID"
    psk="YOUR_PASSWORD"
    key_mgmt=WPA-PSK
}
EOF

# Start
sudo ip link set wlp9s0 up
sudo wpa_supplicant -i wlp9s0 -c /tmp/wpa.conf -B
sudo dhcpcd wlp9s0
```

### Code Structure

```
src/
├── mt7927_pci.c    # Init, MCU communication, mac80211 callbacks (~4600 lines)
├── mt7927_pci.h    # Register definitions, data structures, macros (~1970 lines)
├── mt7927_mac.c    # TXD building, RXD parsing, RX frame dispatch (~1500 lines)
└── mt7927_dma.c    # Interrupt handling, NAPI poll, DMA operations (~900 lines)
```

Four files compile into a single kernel module `mt7927.ko` (~9000 lines total).

### Documentation

#### `docs/re/` — Windows Reverse Engineering Docs (52 files)

The **sole reference source** for this driver. Reverse-engineered from the Windows driver binary using Ghidra:
- MCU command formats and CID mappings (`win_re_cid_mapping.md`)
- BSS_INFO / STA_REC TLV structures (`win_re_bss_info_all_tlvs.md`, `win_re_codex_starec_verify.md`)
- TXD/DMA descriptor formats (`win_re_full_txd_dma_path.md`, `win_re_dma_descriptor_format.md`)
- Complete connection flow (`win_re_full_connect_cmd_sequence.md`)
- Firmware download and init (`win_re_class02_and_postinit.md`)
- Register maps and RF/PHY config (`windows_register_map.md`, `win_re_full_rf_phy_config.md`)

#### `docs/debug/` — Debug Logs

Problem analysis and diagnostic records from the development process.

#### `docs/archive/` — Archived Docs

Early development notes, plans, and session records. The `low_trust/` subdirectory contains code excerpts from the mt6639 Android driver and mt76 upstream driver, which were proven **unreliable** during development and are kept only for historical reference.

### Development Timeline

Completed in under two weeks across 44 development sessions, entirely using Claude Code:

- **S1-S21**: PCIe init, firmware download, DMA engine, MCU communication
- **S22**: WiFi scanning working
- **S31**: TX over-the-air confirmed (AR9271 sniffer verification)
- **S39**: RX Ring 4 DMA delivery fix
- **S40**: Auth + Association success (broke 28-session blocker)
- **S43**: WPA2 4-Way Handshake complete, WiFi connected
- **S44**: Hardware encryption, DHCP + internet ping working

### License

Driver source code is licensed under GPL-2.0 (required for Linux kernel modules).

### Disclaimer

This project is developed through lawful reverse engineering for hardware interoperability purposes. The repository contains no proprietary binaries or copyrighted source code.
