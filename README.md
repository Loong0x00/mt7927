# MT7927 Linux WiFi Driver

从零编写的 MediaTek MT7927 (MT6639) WiFi 7 PCIe Linux 内核驱动，基于 Windows 驱动逆向工程开发。

**MediaTek 从未发布过该芯片的 Linux WiFi 驱动，这是全球第一个可用的开源实现。**

## 芯片信息

| 项目 | 说明 |
|------|------|
| 芯片 | MT7927 = MT6639 移动芯片的 PCIe 封装 |
| PCI ID | `14c3:6639` |
| WiFi | PCIe 接口，WiFi 7 (802.11be) |
| 蓝牙 | USB 接口 (btusb/btmtk)，与 WiFi 独立 |
| 注意 | **不属于 MT76 家族**，MT76 驱动不适用 |

## 当前状态

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
| 5GHz 支持 | 待测试 |
| WiFi 6/7 高级特性 | 待开发 |

## 构建

```bash
# 需要内核头文件 (linux-headers)
make driver          # 构建 → src/mt7927.ko

sudo insmod src/mt7927.ko    # 加载
sudo rmmod mt7927            # 卸载
```

## 连接 WiFi

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

## 代码结构

```
src/
├── mt7927_pci.c    # 初始化、MCU 通信、mac80211 回调
├── mt7927_pci.h    # 寄存器定义、数据结构、宏
├── mt7927_mac.c    # TXD 构建、RXD 解析、RX 帧分发
└── mt7927_dma.c    # 中断处理、NAPI poll、DMA 操作
```

四个文件编译为单一内核模块 `mt7927.ko`。

## 文档

### `docs/re/` — Windows 逆向工程文档 (52 篇)

本驱动的**唯一参考来源**。通过 Ghidra 逆向 Windows 驱动二进制，记录了：
- MCU 命令格式和 CID 映射 (`win_re_cid_mapping.md`)
- BSS_INFO / STA_REC TLV 结构 (`win_re_bss_info_all_tlvs.md`, `win_re_codex_starec_verify.md`)
- TXD/DMA 描述符格式 (`win_re_full_txd_dma_path.md`, `win_re_dma_descriptor_format.md`)
- 完整连接流程 (`win_re_full_connect_cmd_sequence.md`)
- 固件下载和初始化 (`win_re_class02_and_postinit.md`)
- 寄存器映射和 RF/PHY 配置 (`windows_register_map.md`, `win_re_full_rf_phy_config.md`)

### `docs/debug/` — 当前调试记录

开发过程中的问题分析和诊断记录。

### `docs/archive/` — 归档文档

早期开发过程中的笔记、计划和会话记录。`low_trust/` 子目录包含 mt6639 Android 驱动和 mt76 上游驱动的参考代码摘录，这些在开发过程中被证明**不可信**，仅保留供历史参考。

## 开发历程

本项目经历 44 个开发 session，主要里程碑：

- **S1-S21**: PCIe 初始化、固件下载、DMA 引擎、MCU 通信
- **S22**: WiFi 扫描工作
- **S31**: TX 空口发射确认 (AR9271 抓包验证)
- **S39**: RX Ring 4 DMA 投递修复
- **S40**: Auth + Association 成功 (突破 28 session 阻塞)
- **S43**: WPA2 4-Way Handshake 完成，WiFi 连接成功
- **S44**: 硬件加密实现，DHCP + 互联网 ping 通

## 固件

驱动需要 MediaTek 固件文件，应放置在 `/lib/firmware/mediatek/` 下。固件文件为专有二进制，不包含在本仓库中。

## 许可证

驱动源码基于 GPL-2.0 许可证（Linux 内核模块要求）。

## 免责声明

本项目基于合法的逆向工程开发，用于实现硬件兼容性（互操作性）。仓库中不包含任何专有二进制文件或受版权保护的源代码。
