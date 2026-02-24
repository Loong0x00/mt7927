# MT7927 Linux WiFi Driver

[English](#english) | [中文](#中文)

---

> ### :point_right: 寻找可直接使用的驱动？
>
> **推荐使用 [@jetm](https://aur.archlinux.org/account/jetm) 维护的 [`mediatek-mt7927-dkms`](https://aur.archlinux.org/packages/mediatek-mt7927-dkms) AUR 包** — 基于 mt7925e 驱动打补丁，2.4GHz + 5GHz 已测试可用，支持 DKMS 自动编译。
>
> ```bash
> yay -S mediatek-mt7927-dkms
> ```
>
> 追踪讨论：[openwrt/mt76#927](https://github.com/openwrt/mt76/issues/927)
>
> **本项目（从零编写的独立驱动）主要价值在于 Windows 逆向工程文档（`docs/re/`），为社区驱动开发提供参考。项目可能不会继续活跃开发。**

---

> ### :point_right: Looking for a ready-to-use driver?
>
> **Use the [`mediatek-mt7927-dkms`](https://aur.archlinux.org/packages/mediatek-mt7927-dkms) AUR package maintained by [@jetm](https://aur.archlinux.org/account/jetm)** — patches the mt7925e driver, tested on 2.4GHz + 5GHz, with DKMS auto-build support.
>
> ```bash
> yay -S mediatek-mt7927-dkms
> ```
>
> Tracking: [openwrt/mt76#927](https://github.com/openwrt/mt76/issues/927)
>
> **This project (a from-scratch standalone driver) is primarily valuable for its Windows reverse engineering documentation (`docs/re/`), serving as a reference for community driver development. Active development may not continue.**

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
├── mt7927_pci.c    # 初始化、MCU 通信、mac80211 回调 (~4920 行)
├── mt7927_pci.h    # 寄存器定义、数据结构、宏 (~2170 行)
├── mt7927_mac.c    # TXD 构建、RXD 解析、RX 帧分发、EAPOL bypass (~1600 行)
└── mt7927_dma.c    # 中断处理、NAPI poll、DMA 操作 (~1000 行)
```

四个文件编译为单一内核模块 `mt7927.ko`（约 9700 行）。

### 文档

#### `docs/re/` — Windows 逆向工程文档 (52 篇)

本驱动的**唯一参考来源**。通过 Ghidra 逆向 Windows 驱动二进制，记录了：
- MCU 命令格式和 CID 映射 (`win_re_cid_mapping.md`)
- BSS_INFO / STA_REC TLV 结构 (`win_re_bss_info_all_tlvs.md`, `win_re_codex_starec_verify.md`)
- TXD/DMA 描述符格式 (`win_re_full_txd_dma_path.md`, `win_re_dma_descriptor_format.md`)
- 完整连接流程 (`win_re_full_connect_cmd_sequence.md`)
- 固件下载和初始化 (`win_re_class02_and_postinit.md`)
- 寄存器映射和 RF/PHY 配置 (`windows_register_map.md`, `win_re_full_rf_phy_config.md`)

#### `docs/ARCHITECTURE.md` — 架构说明文档

**在提问"为什么不直接合并到 mt76"之前，请先读这份文档：[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**

面向内核开发者的技术深度文档，包含：
- 为什么不能直接改 mt76（寄存器空间、DMA、协议层面的具体差异）
- 驱动中复用的标准内核 API 完整列表（含文件/行号）
- MT6639 完全独立实现的部分（固件下载、UniCmd、TXD/RXD、硬件加密）
- 进入 mt76 主线的路径分析

#### `docs/debug/` — 调试记录

开发过程中的问题分析和诊断记录。

#### `docs/archive/` — 归档文档

早期开发过程中的笔记、计划和会话记录。`low_trust/` 子目录包含 mt6639 Android 驱动和 mt76 上游驱动的参考代码摘录，这些在开发过程中被证明**不可信**，仅保留供历史参考。

### 致谢

本项目 fork 自 [ehausig/mt7927](https://github.com/ehausig/mt7927)。原项目由 [@ehausig](https://github.com/ehausig) 发起，完成了 PCIe 设备绑定和基本固件加载的早期探索，并得出了关键结论：**MT7927 (MT6639) 与 MT7925 的寄存器空间完全不同（`0x7C0xxxxx` vs `0x7001xxxx`），修改 mt7925 驱动不可行，必须从零编写**。这一结论为本项目选择 Windows 逆向工程路线奠定了基础。

### 开发历程

本项目在不到两周内完成，经历 45 个开发 session，全程使用 Claude Code：

- **S1-S21**: PCIe 初始化、固件下载、DMA 引擎、MCU 通信
- **S22**: WiFi 扫描工作
- **S31**: TX 空口发射确认 (AR9271 抓包验证)
- **S39**: RX Ring 4 DMA 投递修复
- **S40**: Auth + Association 成功 (突破 28 session 阻塞)
- **S43**: WPA2 4-Way Handshake 完成，WiFi 连接成功
- **S44**: 硬件加密实现，DHCP + 互联网 ping 通
- **S45**: EAPOL 投递修复 (cfg80211_rx_control_port)，重启后连接稳定

### 关于 MT7927 Linux 驱动现状与风险

**市场规模：** MT7927 不是小众网卡。AMD AM5 平台的华硕（Crosshair X870E Hero、Strix X870E 全系）、技嘉（X870E AORUS Master）、微星（MPG X870I Edge Ti Evo）等主流旗舰主板几乎全部搭载 MT7927，联想 Legion Pro 7/9 笔记本也有使用。保守估计消费端已有 20-40 万张卡，随着 B850 中端板铺量还在增长。Intel 13/14 代可靠性问题后大量用户转向 AMD，这些用户中 Linux 使用比例远高于平均。

**当前社区方案（`mediatek-mt7927-dkms`）的已知问题：**

- **320MHz 未真正实现。** 补丁只填充了发给固件的 TLV，wiphy/mac80211 能力未注册——`iw phy` 无 `BW = 320` MCS map。mt7925 驱动没有 320MHz 代码路径，无法通过打补丁修复。320MHz 是 MT7927 相比 MT7925 的核心卖点。
- **电源管理完全关闭。** CLR_OWN 会重置 WFDMA 销毁 DMA ring，补丁选择直接禁用 PM。
- **存在未初始化变量 bug。** `is_mt6639_hw` 在赋值前被使用，导致中断映射选错，冷启动时驱动无法加载。

**长期风险：** 联发科有将手机端组合芯片（如 MT6639/Filogic 380）重封装为 PCIe 模块、以 mt76 风格命名推向 PC 市场的模式。当前补丁方案在 mt7925 代码中散布 `if (is_mt6639())` 分支，如果类似芯片继续出现，这种方式会导致代码越来越脆弱。正确做法是由熟悉 mt76 子系统的内核开发者创建独立的 `mt7927/` 子驱动，本项目的逆向文档（`docs/re/`）提供了这一工作所需的全部技术基础。

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
├── mt7927_pci.c    # Init, MCU communication, mac80211 callbacks (~4920 lines)
├── mt7927_pci.h    # Register definitions, data structures, macros (~2170 lines)
├── mt7927_mac.c    # TXD building, RXD parsing, RX frame dispatch, EAPOL bypass (~1600 lines)
└── mt7927_dma.c    # Interrupt handling, NAPI poll, DMA operations (~1000 lines)
```

Four files compile into a single kernel module `mt7927.ko` (~9700 lines total).

### Documentation

#### `docs/re/` — Windows Reverse Engineering Docs (52 files)

The **sole reference source** for this driver. Reverse-engineered from the Windows driver binary using Ghidra:
- MCU command formats and CID mappings (`win_re_cid_mapping.md`)
- BSS_INFO / STA_REC TLV structures (`win_re_bss_info_all_tlvs.md`, `win_re_codex_starec_verify.md`)
- TXD/DMA descriptor formats (`win_re_full_txd_dma_path.md`, `win_re_dma_descriptor_format.md`)
- Complete connection flow (`win_re_full_connect_cmd_sequence.md`)
- Firmware download and init (`win_re_class02_and_postinit.md`)
- Register maps and RF/PHY config (`windows_register_map.md`, `win_re_full_rf_phy_config.md`)

#### `docs/ARCHITECTURE.md` — Architecture Document

**Before asking "why not just merge into mt76", please read this document first: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)**

Technical deep-dive for kernel developers, covering:
- Why mt76 cannot simply be patched (concrete register space, DMA, and protocol differences)
- Complete list of standard kernel APIs reused (with file/line references)
- Fully independent MT6639-specific implementations (firmware download, UniCmd, TXD/RXD, hardware crypto)
- Roadmap for upstreaming into mt76

#### `docs/debug/` — Debug Logs

Problem analysis and diagnostic records from the development process.

#### `docs/archive/` — Archived Docs

Early development notes, plans, and session records. The `low_trust/` subdirectory contains code excerpts from the mt6639 Android driver and mt76 upstream driver, which were proven **unreliable** during development and are kept only for historical reference.

### Acknowledgments

This project is forked from [ehausig/mt7927](https://github.com/ehausig/mt7927). The original project by [@ehausig](https://github.com/ehausig) performed early exploration of PCIe device binding and basic firmware loading, and reached a critical conclusion: **MT7927 (MT6639) has a completely different register space from MT7925 (`0x7C0xxxxx` vs `0x7001xxxx`), making it impossible to adapt the mt7925 driver — a from-scratch implementation is required.** This conclusion guided the decision to pursue Windows driver reverse engineering for this project.

### Development Timeline

Completed in under two weeks across 45 development sessions, entirely using Claude Code:

- **S1-S21**: PCIe init, firmware download, DMA engine, MCU communication
- **S22**: WiFi scanning working
- **S31**: TX over-the-air confirmed (AR9271 sniffer verification)
- **S39**: RX Ring 4 DMA delivery fix
- **S40**: Auth + Association success (broke 28-session blocker)
- **S43**: WPA2 4-Way Handshake complete, WiFi connected
- **S44**: Hardware encryption, DHCP + internet ping working
- **S45**: EAPOL delivery fix (cfg80211_rx_control_port), stable after reboot

### Current State of MT7927 Linux Driver Support

**Market scale:** MT7927 is not a niche chip. It ships on nearly all flagship AMD AM5 motherboards — ASUS (Crosshair X870E Hero, full Strix X870E lineup), Gigabyte (X870E AORUS Master), MSI (MPG X870I Edge Ti Evo) — and on Lenovo Legion Pro 7/9 laptops. Conservative estimate: 200K-400K units in consumer hands, growing as B850 mid-range boards ship. Many of these users are migrating from Intel (post-13th/14th gen reliability issues) and have a high Linux adoption rate.

**Known issues with the community patch (`mediatek-mt7927-dkms`):**

- **320MHz does not actually work.** The patch only populates the STA_REC TLV sent to firmware; wiphy/mac80211 capability registration is unchanged — `iw phy` shows no `BW = 320` MCS map. mt7925's driver has no 320MHz code path to patch onto. 320MHz is MT7927's primary advantage over MT7925.
- **Power management is completely disabled.** CLR_OWN reinitializes WFDMA and destroys DMA ring configuration, so the patch disables PM entirely.
- **Uninitialized variable bug.** `is_mt6639_hw` is used before assignment, selecting the wrong IRQ map and causing boot failure.

**Long-term risk:** MediaTek repackages mobile combo chips (e.g. MT6639/Filogic 380) as PCIe modules with mt76-style naming for the PC market. The current patch approach scatters `if (is_mt6639())` branches across mt7925's codebase. If similar chips continue to appear, this pattern becomes increasingly fragile. The proper solution is a dedicated `mt7927/` sub-driver within the mt76 framework, written by a kernel developer familiar with the subsystem. The reverse engineering documentation in this project (`docs/re/`) provides the complete technical foundation for that work.

### License

Driver source code is licensed under GPL-2.0 (required for Linux kernel modules).

### Disclaimer

This project is developed through lawful reverse engineering for hardware interoperability purposes. The repository contains no proprietary binaries or copyrighted source code.
