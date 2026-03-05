# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

MT7927 WiFi 7 Linux 驱动开发项目 — 从零编写，基于 Windows 驱动逆向工程。

**关键事实**:
- MT7927 = MT6639 移动芯片的 PCIe 封装，**不是** MT76 家族！
- PCI ID: `14c3:6639`（芯片 ID 是 6639，不是 7927）
- MediaTek 从未发布 Linux WiFi 驱动，全球无人有可用驱动
- 蓝牙通过 USB (btusb/btmtk)，WiFi 是 PCIe — 完全独立

## 项目状态说明

**本项目自主驱动已停止开发。** 推荐使用 [@jetm](https://aur.archlinux.org/account/jetm) 的
[`mediatek-mt7927-dkms`](https://aur.archlinux.org/packages/mediatek-mt7927-dkms) AUR 包
（基于 mt7925e 打补丁）。本项目的主要价值在于 `docs/re/` 下的 Windows 逆向文档。

**当前关注**: 基于 AUR 驱动 (v2.1-22) 的 MLO 研究。

## AUR 驱动测试状态 (2026-03-06)

| 功能 | AUR v2.1-22 状态 |
|------|-------------------|
| 2.4GHz 连接 (WPA2-PSK) | ✅ TP-LINK + ASUS, ping 5/5 |
| 5GHz 连接 (WPA2-PSK) | ✅ ASUS RT-BE92U, ping 5/5 |
| 6GHz 连接 (WPA3-SAE) | ✅ ASUS RT-BE92U (320MHz, EHT), ping 5/5 |
| WiFi 7 EHT 320MHz 4K-QAM | ✅ PHY RX 5187 Mbps / TX 4803 Mbps |
| **MLO 5GHz+2.4GHz STR** | **✅ iperf3 554Mbps/30s, 双band饱和 (BN0 95%/BN1 100%)** |
| **MLO 6GHz+2.4GHz** | **⚠️ 关联成功，6GHz primary 数据路径有问题** |
| **MLO 三链路发现** | **✅ 6G+5G+2.4G 全部发现 (valid_links=0x7)** |

### MLO 补丁

**当前补丁**: `patches/mt6639-wifi-mlo-v2.patch` (5 文件, 145 行) — 适配 v2.1-22
- PR: https://github.com/jetm/mediatek-mt7927-dkms/pull/7
- jetm 正在准备内核上游提交，`Signed-off-by: 张旭涵 <Loong.0x00@gmail.com>`
- 测试结果: https://github.com/openwrt/mt76/issues/927#issuecomment-4008470225
- **cfg80211 补丁不需要** — v2.1-22 的 band_idx 修复已解决 BSS 查找问题

### MLO v2 补丁修复 (7 处, 适配 v2.1-22)

v2.1-22 已包含 band_idx 修复 (helper function, set_roc/set_mlo_roc/abort_roc dbdcband,
mac_link_bss_add band_idx, roc_handle_grant 扩展条件)，以下为 v2.1-22 之上的额外修改:

1. **MLO 能力强制** (`init.c`): `chip_cap |= MT792x_CHIP_CAP_MLO_EN`
2. **RX WCID 修复** (`mt792x_mac.c`): BSS self-STA 返回 `def_wcid` 而非 NULL
3. **omac_idx 唯一化** (`main.c`): MLD 每链路独立 omac_idx
4. **ROC 条件** (`main.c`): `band != secondary_band` 覆盖所有跨频段组合
5. **roc_iter MLD 遍历** (`mcu.c`): 遍历所有 MLD 链路匹配 bss_idx
6. **STA_REC_MLD link_num** (`mcu.c`): 允许 3 链路 (原 max 2)
7. **5GHz↔6GHz 排除** (`mac80211.c`): MT6639 同射频不能 STR

### 压力测试结果 (2026-03-06)

```
iperf3 -c 10.0.0.1 -t 30 -P 8 → 554 Mbps, 1.94 GB, 30s 稳定
MIB: BN0(2.4G) CCA_NAV=0xf2313c(~95%), BN1(5G) CCA_NAV=0xffffff(100%)
```
确认真正 STR MLO — 固件 UMAC 自动分发 MPDU，5GHz 先饱和溢出到 2.4GHz。

### 6GHz MLO 注意事项

- AP MLO 6GHz 频道: **6295 MHz (ch53)**
- 6GHz 做 primary link 时 ARP 失败，数据路径有问题 — **待调查**
- 5GHz 做 primary 时一切正常

**MT6639 射频架构**:
- 2 个射频: band0=2.4GHz, band1=5GHz/6GHz (共享)
- 最大双链路 STR: 2.4G+5G 或 2.4G+6G
- 5G+6G 不可能 (同一射频, band_idx=1)

## 自主驱动历史状态 (Session 48, 2026-02-25)

| 功能 | 状态 |
|------|------|
| PCIe + BAR0 + 电源管理 + WFDMA DMA | ✅ |
| 固件下载 (patch + 6 RAM, fw_sync=0x3) | ✅ |
| MCU UniCmd + PostFwDownloadInit | ✅ |
| mac80211 注册 (wlp9s0, 2.4G+5G) | ✅ |
| WiFi 扫描 | ✅ 135+ BSS |
| TX auth 帧 (空口) | ✅ S31 AR9271 确认 |
| RX Ring 4 DMA 投递 | ✅ S39 |
| RX 帧处理 → mac80211 | ✅ S40 四修复 |
| 802.11 Authentication | ✅ S40 |
| 802.11 Association | ✅ S40 |
| WPA2-PSK 4-Way Handshake | ✅ S43 |
| 硬件加密 (CCMP) | ✅ S44 |
| DHCP + 互联网连接 | ✅ S44 |
| **EAPOL via cfg80211 control port** | **✅ S45 (修复 mac80211 8023 fast path 鸡生蛋问题)** |
| **ROC TLV 修复 (24字节对齐mt7925)** | **✅ S48 (ROC_GRANT 成功, band_idx 正确)** |
| 5GHz | ⚠️ S48: ROC_GRANT 成功但 MPDU_ERR, 疑似 TX_RATE 编码问题 |

### S45 修复: EAPOL 投递 via cfg80211_rx_control_port

**问题**: 固件 HW_HDR_TRANS 将 802.11→802.3，驱动设 RX_FLAG_8023 走 mac80211 快速路径。
但快速路径要求 `fast_rx` 非 NULL（仅 AUTHORIZED 后设置），EAPOL 在 4-way handshake
期间到达（STA 在 ASSOC 状态）→ `fast_rx=NULL` → mac80211 无条件丢弃 = 鸡生蛋死锁。

**修复**: 检测 802.3 EAPOL (ethertype=0x888E)，通过 `cfg80211_rx_control_port()` 直接
投递给 wpa_supplicant（nl80211 control port 路径），绕过 mac80211 8023 fast path。
位置: `src/mt7927_mac.c` queue_rx_skb 中 EAPOL bypass 段。

### 连接命令序列 (对齐 Windows RE, S45 验证)
```
ChipConfig → DEV_INFO(active=1) → BssActivateCtrl(BASIC+MLD) → PM_DISABLE
→ BSS_INFO(13 TLV) → BSS_RLM(3 TLV) → SCAN_CANCEL → ROC
→ [ROC_GRANT] → STA_REC(13 TLV, STATE=0) → RX_FILTER(0x0B) → Auth TX
→ [Auth-2 via Ring6] → BSS_INFO + STA_REC(STATE=2)
→ [AssocResp via Ring6] → STA_REC(STATE=2)
→ [EAPOL Key 1/4 via Ring4 → cfg80211_rx_control_port] → 4-Way Handshake
→ [set_key PTK/GTK] → CONNECTED
```

### ⚠️ CID 重要教训
**Windows RE dispatch table 的 inner CID ≠ UniCmd header CID!**
- **当前策略**: 用已验证工作的 mt7925 CID，不盲目用 Windows inner CID
- 详见 `src/mt7927_pci.h` 中每个 CID 的注释

## 构建与测试

```bash
make driver                    # 构建 → src/mt7927.ko
sudo insmod src/mt7927.ko      # 加载
sudo rmmod mt7927              # 卸载
make check                     # 检查设备状态
make recover                   # PCI remove + rescan 恢复设备

# WiFi 连接测试
sudo timeout 25 wpa_supplicant -i wlp9s0 -c /home/user/mt7927/wpa_mt7927.conf -d
```

WiFi 测试配置: 见本地 `wpa_mt7927.conf`

## 代码架构

### 驱动文件 (多文件内核模块 → mt7927.ko)
| 文件 | 行数 | 职责 |
|------|------|------|
| `src/mt7927_pci.c` | ~4920 | 初始化、MCU 通信、mac80211 回调、MCU 命令构建 |
| `src/mt7927_pci.h` | ~2170 | 寄存器定义、数据结构、宏 |
| `src/mt7927_mac.c` | ~1600 | TXD 构建 (统一路径)、RXD 解析、RX 分发、EAPOL bypass |
| `src/mt7927_dma.c` | ~1000 | 中断处理、NAPI poll、TX enqueue/kick/complete |

### 初始化流程 (probe)
```
pci_enable_device → ioremap BAR0
→ SET_OWN → MCU init → CLR_OWN
→ WFDMA config (GLO_CFG + prefetch)
→ Init TX/RX rings (TX15, TX16, RX4, RX6, RX7)
→ FWDL: patch → 6 RAM regions → FW_START → poll fw_sync=0x3
→ PostFwDownloadInit:
   DMASHDL → WpdmaConfig → clear FWDL bypass
   → NIC_CAP → Config → WFDMA_CFG → DBDC → ScanConfig/ChipConfig/LogConfig → EFUSE_CTRL
→ Init data rings (TX ring 0, TX ring 2 mgmt) + request_irq + NAPI enable
→ Register ieee80211_hw
```

## 项目目录结构

```
mt7927/
├── CLAUDE.md               # 项目指南 (本文件)
├── Makefile                # 构建系统 (make driver / make recover)
├── wpa_mt7927.conf         # WiFi WPA 测试配置
├── patches/                # AUR 驱动补丁
│   └── mt6639-wifi-mlo.patch  # MLO 支持补丁 (submitted as PR #7)
├── src/                    # 驱动源码 (4 文件 → mt7927.ko)
│   ├── mt7927_pci.c        #   初始化 / MCU 通信 / mac80211 回调
│   ├── mt7927_pci.h        #   寄存器定义 / 数据结构
│   ├── mt7927_mac.c        #   TXD 构建 / RXD 解析 / RX 分发
│   └── mt7927_dma.c        #   中断处理 / NAPI poll / DMA 操作
├── docs/
│   ├── re/                 #   Windows RE 文档 (52 个, 唯一权威参考)
│   ├── debug/              #   当前调试文档 (eapol_ack_problem.md)
│   └── archive/            #   归档: 旧文档 / 不可信 / 过时
│       ├── low_trust/      #     mt6639/mt76 代码 + 旧分析 (禁止引用)
│       ├── old_debug/      #     过期调试记录
│       └── ...             #     旧计划/模板/驱动包等
├── tmp/                    # Ghidra 项目 / 固件 / RE 中间产物
│   ├── ghidra_project/     #   Ghidra RE 项目文件
│   ├── re_results/         #   43 函数逆向共识报告
│   ├── fw_7927/            #   固件文件
│   └── ghidra_exports/     #   Ghidra 导出数据
├── tools/                  # RE 分析脚本 (disasm/verify/diff)
└── ghidra_12.0.3_PUBLIC/   # Ghidra 工具安装
```

## 参考代码 (优先级)

| 优先级 | 来源 | 路径 | 备注 |
|--------|------|------|------|
| **1** | **Windows RE (Ghidra)** | `docs/re/win_*.md`, Windows .sys 二进制 | **唯一权威来源** |
| **1b** | **Ghidra RE 全函数分析** | `tmp/re_results/consensus/` (43 函数) | Windows 二进制逆向共识报告 |
| ~~2~~ | ~~MT6639 Android 驱动~~ | `docs/archive/low_trust/mt6639/` | **已归档不可信** |
| ~~3~~ | ~~mt7925 上游驱动~~ | `docs/archive/low_trust/mt76/` | **已归档不可信** |

**规则**:
- **Windows 逆向是唯一权威**，mt6639 和 mt76 都不可信，禁止引用
- 涉及 TXD/DMA/固件交互的字段，**必须从 Windows 二进制逆向确认**
- Ghidra 项目: `tmp/ghidra_project/mt7927_re`，Windows 驱动 .sys 文件在 `tmp/` 下
- **唯一例外**: `docs/archive/low_trust/mt6639/include/chips/coda/mt6639/` 下的 CODA 自动生成头文件可用于交叉验证寄存器定义

## 致命操作 — 永远不要使用

```c
pcie_flr(pdev);                   // D3cold, 永不恢复
pci_reset_function(pdev);         // probe 中死锁, 需硬重启
writel(BIT(4), bar0+0x1f8600);    // CB_INFRA_RGU, 不可恢复
```

## 关键硬件规则

- **CLR_OWN**: 必须先 SET_OWN(BIT(0)) → wait OWN_SYNC → CLR(BIT(1))；CLR 会清零所有 HOST ring
- **FWDL bypass BIT(9)**: 在 wpdma_config 之后清除；FWDL 后不要重置 DMA
- **Windows ring layout**: TX 0,1,2(mgmt),15,16 + RX 4,6,7
- **中断掩码**: 0x2600f050
- **RX CIDX**: write ring->tail 释放 slot 给 DMA
- **CONNAC3 PKT_TYPE**: TXS=0, TXRXV=1, NORMAL=2, RX_DUP_RFB=3, RX_TMR=4, RETRIEVE=5, TXRX_NOTIFY=6, **RX_EVENT=7**, NORMAL_MCU=8
- **MIB read-to-clear**: 读一次后归零

## UniCmd 格式规则

```c
txd->len = cpu_to_le16(plen + 16);  // len 包含 16 字节内部头
// CID: 使用已验证工作的 mt7925 值 (NOT Windows inner CID!)
// option: 0x07=查询(need_response), 0x06=设置(fire-and-forget)
```

## MCU 命令 — 当前实现状态

### BSS_INFO (CID=0x02) — 13 TLV (匹配 Windows dispatch table)
BASIC(0), RATE(0xB), SEC(0x10), QBSS(0xF), SAP(0xD), P2P(0xE), HE(5), COLOR(4), MBSSID(6), 0C(0xC), IOT(0x18), MLD(0x1A), EHT(0x1E)

### BSS_RLM (CID=0x02, 独立发送) — 3 TLV
RLM(2), PROTECT(3), IFS_TIME(0x17)

### STA_REC (CID=0x03) — 13 TLV (匹配 Windows dispatch table)
BASIC(0), RA(1), STATE(7), HT(9), VHT(0xA), PHY(0x15), BA(0x16), HE_6G(0x17), HE(0x19), MLD_SETUP(0x20), EHT_MLD(0x21), EHT(0x22), UAPSD(0x24)
- STATE: auth 阶段 state=0, post-assoc state=2 (Windows RE: JOIN=1→wire=0, CONNECTED→wire=2)

### conn_state 双枚举 (易混淆!)
```c
// BSS_INFO 用: conn_state=1 表示 BSS active (NOT connect/disconnect!)
// STA_REC 用: CONN_STATE_CONNECT=1, CONN_STATE_DISCONNECT=0
```

## 历史 Bug 摘要 (已修复，按重要性)

| Bug | 原因 |
|-----|------|
| **EAPOL 被 mac80211 丢弃 (S45)** | **8023 fast path 要求 fast_rx (仅 AUTHORIZED 后设置); 用 cfg80211_rx_control_port bypass** |
| **Auth+Assoc 失败 (28 sessions)** | **BssActivateCtrl band_idx/active/link_idx 错误 + RFCR DROP_OTHER_UC** |
| **EAPOL 不到达 (S41-S43)** | **BNRCFR bypass 导致数据帧路由错误; DEV_INFO byte[8] 错误; 802.3→802.11 头反转** |
| FC Protected bit 缺失 (S44) | 802.3→802.11 重建缺少 Protected bit → mac80211 安全降级丢弃 |
| Ring 4 DIDX=0 (38 sessions) | MDP BNRCFR 未配置 → 管理帧路由到 MCU 而非 Host |
| MCU 事件收不到 | PKT_TYPE_RX_EVENT: CONNAC3=7, 不是 CONNAC2=1 |
| RX ring 耗尽 | CIDX 未更新: NAPI 处理后必须写 ring->tail |
| Scan 回归 (0 BSS) | CID→inner 导致; 恢复 mt7925 CID + DMASHDL + EFUSE_CTRL |
| NAPI/UniCmd 竞争 | UniCmd ACK 路径未 napi_disable → NAPI 抢走响应 |

## 设备恢复

```bash
echo 1 | sudo tee /sys/bus/pci/devices/0000:09:00.0/remove && sleep 2 && echo 1 | sudo tee /sys/bus/pci/rescan
# 或: make recover
```

## 开发偏好

- **回复语言**: 中文
- **代理策略**: 委托分析给子 agent，主 agent 做宏观决策
- **轻量任务用 Sonnet**，复杂任务用 Opus
- **Git 分支**: 实验性更改用 `experiment/` 分支
- **Windows RE 优先**: 涉及固件交互必须从 Windows 确认

## Windows RE: 关键 CID 映射 (Ghidra 验证)

| outer_tag | inner_CID | 命令 |
|-----------|-----------|------|
| `0x11` | `0x01` | DEV_INFO |
| `0x05/0x12/0x16-0x19/0x1e` | `0x02` | BSS_INFO sub-TLVs |
| `0xb1` | `0x25` | STA_REC |
| `0x8a` | `0x0e` | NIC_CAP |
| `0x03` | `0x16` | SCAN_REQ |
| `0x1c` | `0x27` | CH_PRIVILEGE/ROC |
| `0x28` | `0x28` | DBDC |
| `0x02` | `0x0b` | CLASS_02 (PostFwDownloadInit) |
| `0x58` | `0x05` | EFUSE_CTRL |

完整映射表 (58 条目): `docs/re/win_re_cid_mapping.md`
