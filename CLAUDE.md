# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

MT7927 WiFi 7 Linux 驱动开发项目 — 从零编写，基于 Windows 驱动逆向工程 + MediaTek Android MT6639 参考代码。

**关键事实**:
- MT7927 = MT6639 移动芯片的 PCIe 封装，**不是** MT76 家族！
- PCI ID: `14c3:6639`（芯片 ID 是 6639，不是 7927）
- MediaTek 从未发布 Linux WiFi 驱动，全球无人有可用驱动
- 蓝牙通过 USB (btusb/btmtk)，WiFi 是 PCIe — 完全独立

## 当前状态 (2026-02-16)

| 功能 | 状态 |
|------|------|
| PCIe + BAR0 + 电源管理 + WFDMA DMA | ✅ |
| 固件下载 (patch + 6 RAM, fw_sync=0x3) | ✅ |
| MCU UniCmd + PostFwDownloadInit | ✅ |
| mac80211 注册 (wlp9s0, 2.4G+5G) | ✅ |
| WiFi 扫描 (56-61 BSS) | ✅ |
| NAPI RX (MCU events + beacon/probe) | ✅ |
| **TX auth 帧** | **❌ 阻塞 14 sessions** |

**阻塞问题**: auth 帧通过 DMA 提交后，固件返回 TXFREE stat=1 (失败)，MIB TX 计数器全零 — 帧从未到达射频前端。详见下方 "TX Auth 帧调查" 章节。

## 构建与测试

```bash
make driver                    # 构建 → src/mt7927.ko
sudo insmod src/mt7927.ko      # 加载
sudo rmmod mt7927_pci          # 卸载
make check                     # 检查设备状态
make recover                   # PCI remove + rescan 恢复设备

# WiFi 连接测试
sudo wpa_supplicant -i wlp9s0 -c /tmp/wpa_mt7927.conf -d
# 超时版: sudo timeout 20 wpa_supplicant -i wlp9s0 -c /tmp/wpa_mt7927.conf -d
```

sudo 密码: `123456` | WiFi AP: `CMCC-Pg2Y-5G-FAST` / `7ue9pxgp`

## 代码架构

### 驱动文件 (多文件内核模块 → mt7927.ko)
| 文件 | 行数 | 职责 |
|------|------|------|
| `src/mt7927_pci.c` | ~3500 | 初始化、MCU 通信、mac80211 回调、MCU 命令构建 |
| `src/mt7927_pci.h` | ~1700 | 寄存器定义、数据结构、宏 |
| `src/mt7927_mac.c` | ~970 | TXD 构建、RXD 解析、RX 分发 |
| `src/mt7927_dma.c` | ~620 | 中断处理、NAPI poll、TX enqueue/kick/complete |

### 初始化流程 (probe)
```
pci_enable_device → ioremap BAR0
→ SET_OWN → MCU init → CLR_OWN
→ WFDMA config (GLO_CFG + prefetch)
→ Init TX/RX rings (TX15, TX16, RX4, RX6, RX7)
→ FWDL: patch → 6 RAM regions → FW_START → poll fw_sync=0x3
→ PostFwDownloadInit:
   DMASHDL → WpdmaConfig → clear FWDL bypass
   → NIC_CAP → Config → DBDC → ScanConfig/ChipConfig/LogConfig
→ Init data rings (TX ring 0) + request_irq + NAPI enable
→ Register ieee80211_hw
```

## 参考代码 (优先级)

| 优先级 | 来源 | 路径 | 备注 |
|--------|------|------|------|
| **1** | **MT6639 Android 驱动** | `mt6639/` | **芯片完全相同，最可靠** |
| 2 | Windows RE 文档 | `docs/win_v5705275_*.md` | 实际运行的驱动 |
| 3 | mt7925 上游驱动 | `mt76/mt7925/` | mac80211 参考 (不同芯片，不完全可靠) |
| 4 | Ghidra RE 文档 | `docs/references/ghidra_post_fw_init.md` | PostFwDownloadInit 分析 |

**用户明确指示**: mt7925 有问题，优先参考 MT6639 和 Windows 逆向代码。

## 致命操作 — 永远不要使用

```c
pcie_flr(pdev);                   // D3cold, 永不恢复
pci_reset_function(pdev);         // probe 中死锁, 需硬重启
writel(BIT(4), bar0+0x1f8600);    // CB_INFRA_RGU, 不可恢复
```

## 关键硬件规则

- **CLR_OWN**: 必须先 SET_OWN(BIT(0)) → wait OWN_SYNC → CLR(BIT(1))；CLR 会清零所有 HOST ring
- **FWDL bypass BIT(9)**: 在 wpdma_config 之后清除；FWDL 后不要重置 DMA
- **Windows ring layout**: TX 0,1,15,16 + RX 4,6,7
- **中断掩码**: 0x2600f010
- **RX CIDX**: write ring->tail 释放 slot 给 DMA
- **CONNAC3 PKT_TYPE**: TXS=0, TXRXV=1, NORMAL=2, RX_DUP_RFB=3, RX_TMR=4, RETRIEVE=5, TXRX_NOTIFY=6, **RX_EVENT=7**, NORMAL_MCU=8

## UniCmd 格式规则

```c
txd->len = cpu_to_le16(plen + 16);  // len 包含 16 字节内部头
// CID: Windows class 值 (0x8a=NIC_CAP), NOT mt76 UNI_CMD IDs
// option: 0x07=查询(need_response), 0x06=设置(fire-and-forget)
```

## MCU 命令 — MT6639 参考发现

### BSS_INFO (CID=0x26) — MT6639 发送 12 个 TLV
BASIC, RLM, PROTECT, IFS_TIME, **RATE**, SEC, QBSS, SAP, P2P, HE, BSS_COLOR, **MLD**
- 我们目前只发: BASIC + RLM + MLD
- **RATE TLV 可能关键** — 固件不知道用什么速率发帧

### STA_REC (CID=0x25) — MT6639 发送 10 个 TLV
BASIC, HT_INFO, VHT_INFO, HE_BASIC, HE_6G_CAP, STATE_INFO, PHY_INFO, RA_INFO, BA_OFFLOAD, UAPSD
- 我们目前只发: BASIC + RA + STATE + PHY + HDR_TRANS

### conn_state 双枚举 (易混淆!)
```c
// BSS_INFO 用: (wsys_cmd_handler_fw.h)
MEDIA_STATE_CONNECTED = 0     // 已连接
MEDIA_STATE_DISCONNECTED = 1  // 未连接

// STA_REC 用: (wlan_def.h line 1239)
STATE_CONNECTED = 1           // ← 不同的枚举!
STATE_DISCONNECT = 0
```

## TX Auth 帧调查 (14 sessions, 未解决)

### 现象
- TXFREE: stat=1 (失败), count=15 (重试次数), ~30ms 返回
- MIB TX counters = 0 — 帧从未到达射频前端
- 5GHz 和 2.4GHz 同样失败

### 已尝试的方法 (全部无效)

**TX Ring/格式 (6 种)**:
- Ring 0 CT mode + TXP | Ring 15 SF mode | Ring 15 CMD Q_IDX=0x10 | Ring 15 CMD Q_IDX=0x00
- DMA coherent buffer | Ring 0 prefetch+中断修复

**TXD 字段 (8 项)**:
- REM_TX_COUNT 15→30 | REMAINING_LIFE_TIME | DAS clear | FIXED_RATE
- Q_IDX=ALTX0(0x10) | FRAME_TYPE/SUB_TYPE | CHANNEL_SWITCH | KeepFullPwr

**MCU 命令 (9 项)**:
- DEV_INFO | BSS_INFO (BASIC+RLM+MLD) | STA_REC conn_state=0/1/2
- WTBL ADM_COUNT_CLEAR | ROC acquire+grant | ROC 后重发全部命令
- BSS_INFO MLD TLV (tag=0x1A) | STA_REC conn_state=CONNECT(1)

**频段**: 5GHz ch161, 2.4GHz ch6 — 均失败

### 高优先级未测方向

1. **DW7 TXD_LENGTH=1 (最高优先级)**
   - MT6639 always sets `TXD_LEN_1_PAGE` (bits[31:30]=1)
   - 我们 DW7=0 — 固件可能无法正确解析 TXD

2. **CMD ring + PKT_FMT=2 + Q_IDX=0x20 (MCU_Q0)**
   - MT6639 管理帧走 CMD ring (TC4), 不走数据 ring
   - PKT_FMT=2 + Q_IDX=0x20 从未测试

3. **BSS_INFO RATE TLV**
   - 固件可能不知道用什么速率发帧
   - MT6639 BSS_INFO 包含 RATE TLV, 我们缺少

4. **更多 BSS_INFO/STA_REC TLV**
   - MT6639 发 12 个 BSS_INFO TLV, 我们只有 3 个
   - MT6639 发 10 个 STA_REC TLV, 我们只有 5 个

## 历史 Bug 摘要 (已修复)

| Bug | 原因 |
|-----|------|
| MCU 事件收不到 | PKT_TYPE_RX_EVENT: CONNAC3=7, 不是 CONNAC2=1 |
| RX ring 耗尽 | CIDX 未更新: NAPI 处理后必须写 ring->tail |
| 扫描结果崩溃 | scan_completed 在 NAPI 上下文调用 — 需 work queue |
| 802.11 帧解析错位 | HDR_OFFSET 填充字节未跳过 |
| MCU 响应丢失 | INT_STA 未 Write-1-to-Clear |
| MCU polling 竞争 | mcu_wait_resp() 期间未禁用 RX6 中断 |

## 设备恢复

```bash
echo 1 | sudo tee /sys/bus/pci/devices/0000:0a:00.0/remove && sleep 2 && echo 1 | sudo tee /sys/bus/pci/rescan
# 或: make recover
```

## 开发偏好

- **回复语言**: 中文
- **代理策略**: 委托分析给子 agent，主 agent 做宏观决策
- **轻量任务用 Sonnet**，复杂任务用 Opus
- **Git 分支**: 实验性更改用 `experiment/` 分支
