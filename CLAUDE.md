# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

MT7927 WiFi 7 Linux 驱动开发项目 — 从零编写，基于 Windows 驱动逆向工程 + MediaTek Android MT6639 参考代码。

**关键事实**:
- MT7927 = MT6639 移动芯片的 PCIe 封装，**不是** MT76 家族！
- PCI ID: `14c3:6639`（芯片 ID 是 6639，不是 7927）
- MediaTek 从未发布 Linux WiFi 驱动，全球无人有可用驱动
- 蓝牙通过 USB (btusb/btmtk)，WiFi 是 PCIe — 完全独立

## 当前状态 (2026-02-21, Session 20)

| 功能 | 状态 |
|------|------|
| PCIe + BAR0 + 电源管理 + WFDMA DMA | ✅ |
| 固件下载 (patch + 6 RAM, fw_sync=0x3) | ✅ |
| MCU UniCmd + PostFwDownloadInit | ✅ CID 用 mt7925 值 (验证工作) |
| mac80211 注册 (wlp9s0, 2.4G+5G) | ✅ |
| NAPI/polling 竞争修复 | ✅ napi_disable/enable 保护 UniCmd ACK 路径 |
| cancel_hw_scan EBUSY 修复 | ✅ ieee80211_scan_completed(aborted=true) |
| TX Ring 2 管理帧 DMA 提交 | ✅ (DMA 消费成功, BIT(6)中断触发) |
| TXD DW0-DW7 (Ghidra 汇编级验证) | ✅ 全部匹配 Windows |
| SET_DOMAIN + BAND_CONFIG | ✅ mt7925 TLV 格式 (Windows 76B/92B 格式不工作) |
| SCAN_CFG payload | ✅ 336B 格式确认正确 |
| DMASHDL init | ✅ full init (mt6639 格式) — 不能跳过! |
| EFUSE_CTRL | ✅ CID=0x2d (mt7925) — 不能删除! |
| **WiFi 扫描** | **✅ 21 BSS (Session 20 修复回归)** |
| **TX auth 帧** | **❌ 阻塞 20 sessions** |

### Session 20 完成内容: scan 回归修复
Session 19 的 CID corrections (outer→inner) 导致 scan 从 61 BSS 回归到 0 BSS。
通过 bisect 确认 3 个根因并修复:
1. **Config 步骤 CID=0x02 与 BSS_INFO 冲突** → 跳过 Config 步骤
2. **DMASHDL full init 被移除** → 恢复 full init (mt6639 格式)
3. **EFUSE_CTRL 被删除** → 恢复 (CID=0x2d, mt7925 格式)

### ⚠️ CID 重要教训 (Session 20)
**Windows RE dispatch table 的 inner CID ≠ UniCmd header CID!**
- Session 19 从 Ghidra 分析出 outer→inner 映射，把 CID 全部改成 inner 值
- 结果: scan 完全失败，firmware 不认这些 CID
- **mt7925 的 CID 值实际工作**，尽管它们看起来像 outer tag
- 推测: MT7927 固件 CID routing 可能与 Windows dispatch table 不同
- **当前策略**: 用已验证工作的 mt7925 CID，不再盲目用 Windows inner CID
- 详见 `src/mt7927_pci.h` 中每个 CID 的注释

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
| `src/mt7927_pci.c` | ~4100 | 初始化、MCU 通信、mac80211 回调、MCU 命令构建 |
| `src/mt7927_pci.h` | ~1780 | 寄存器定义、数据结构、宏 |
| `src/mt7927_mac.c` | ~1100 | TXD 构建 (含 Ring 2 SF mode)、RXD 解析、RX 分发 |
| `src/mt7927_dma.c` | ~800 | 中断处理、NAPI poll、TX enqueue/kick/complete、Ring 2 |

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
→ Init data rings (TX ring 0, TX ring 2 mgmt) + request_irq + NAPI enable
→ Register ieee80211_hw
```

## 参考代码 (优先级)

| 优先级 | 来源 | 路径 | 备注 |
|--------|------|------|------|
| **1** | **Windows RE (Ghidra)** | `docs/re/win_*.md`, Windows .sys 二进制 | **唯一权威来源，实际运行的驱动** |
| 2 | MT6639 Android 驱动 | `mt6639/` | 同芯片辅助参考，代码不一定准确 |
| 3 | mt7925 上游驱动 | `mt76/mt7925/` | **不可信**，不同芯片，禁止用于固件相关逻辑 |
| 4 | Ghidra RE 文档 | `docs/re/ghidra_post_fw_init.md` | PostFwDownloadInit 分析 |

**用户明确指示 (Session 15)**:
- **Windows 逆向是唯一权威**，mt6639 和 mt76 都不可信，有冲突时以 Windows RE 为准
- mt7925 和 mt6639 已反复导致错误修复，"吃的亏还不够多么"
- 涉及 TXD/DMA/固件交互的字段，**必须从 Windows 二进制逆向确认**
- Ghidra 项目: `tmp/ghidra_project/mt7927_re`，Windows 驱动 .sys 文件在 `tmp/` 下

## 文档结构

| 目录 | 内容 |
|------|------|
| `docs/re/` | Windows 逆向工程文档（高/中可信度） |
| `docs/analysis/` | 对比分析（mt6639/mt7925 vs Windows） |
| `docs/debug/` | 调试日志、实验记录 |
| `docs/archive/` | 过时文档 |
| `docs/archive/low_trust/` | 低可信度（主要结论被证伪） |
| `docs/archive/discard/` | 应废弃（完全被取代） |
| `docs/re_audit_report.md` | 文档可信度审计报告 |

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
- **中断掩码**: 0x2600f010
- **RX CIDX**: write ring->tail 释放 slot 给 DMA
- **CONNAC3 PKT_TYPE**: TXS=0, TXRXV=1, NORMAL=2, RX_DUP_RFB=3, RX_TMR=4, RETRIEVE=5, TXRX_NOTIFY=6, **RX_EVENT=7**, NORMAL_MCU=8

## UniCmd 格式规则

```c
txd->len = cpu_to_le16(plen + 16);  // len 包含 16 字节内部头
// CID: 使用已验证工作的 mt7925 值 (NOT Windows inner CID!)
//   NIC_CAP:     0x8a (mt7925, works) — Windows inner=0x0e 未验证
//   BSS_INFO:    0x02
//   STA_REC:     0x03 (mt7925, works) — Windows inner=0x25 未验证
//   DEV_INFO:    0x01
//   SET_DOMAIN:  0x15 (mt7925, works) — Windows inner=0x03 测试失败!
//   BAND_CONFIG: 0x08 (mt7925, works) — Windows inner=0x49 测试失败!
//   SCAN_CFG:    0xca (mt7925, works) — Windows inner=0x0e 未验证
//   EFUSE_CTRL:  0x2d (mt7925, works) — Windows inner=0x05 未验证
// option: 0x07=查询(need_response), 0x06=设置(fire-and-forget)
```

## MCU 命令 — MT6639 参考发现

### BSS_INFO (CID=0x02) — MT6639 发送 12 个 TLV [Windows RE 确认: inner CID=0x02, NOT 0x26]
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

## TX Auth 帧调查 (17 sessions, 未解决)

### 当前现象 (Session 17, Ring 2)
- Ring 2 SF mode 提交 → DMA DIDX 前进 (DMA 消费成功)
- BIT(6) 中断触发 (Session 16 新增 — 之前没有)
- TXD DW0-DW7 全部匹配 Windows (Ghidra 汇编级验证, `docs/re/win_re_dw2_dw6_verified.md`)
- **固件完全静默** — 无 TX_DONE (eid=0x2D)、无 TXFREE、无任何错误
- **TXD 已排除为根因** — 问题在更上层 (init/config)

### 历史现象
- Ring 0/15: TXFREE stat=1 (失败), count=15, ~30ms 返回
- MIB TX counters = 0 — 帧从未到达射频前端
- 5GHz 和 2.4GHz 同样失败

### 已尝试的方法 (全部无效)

**TX Ring/格式 (8 种)**:
- Ring 0 CT mode + TXP | Ring 15 SF mode | Ring 15 CMD Q_IDX=0x10 | Ring 15 CMD Q_IDX=0x00
- DMA coherent buffer | Ring 0 prefetch+中断修复
- **Ring 2 SF mode (PKT_FMT=0, Q_IDX=8)** — DMA 消费但固件静默
- **Ring 2 SF mode (HDR_INFO=24 bytes)** — 同样静默

**TXD 字段 (8 项)**:
- REM_TX_COUNT 15→30 | REMAINING_LIFE_TIME | DAS clear | FIXED_RATE
- Q_IDX=ALTX0(0x10) | FRAME_TYPE/SUB_TYPE | CHANNEL_SWITCH | KeepFullPwr

**MCU 命令 (9 项)**:
- DEV_INFO | BSS_INFO (BASIC+RLM+MLD) | STA_REC conn_state=0/1/2
- WTBL ADM_COUNT_CLEAR | ROC acquire+grant | ROC 后重发全部命令
- BSS_INFO MLD TLV (tag=0x1A) | STA_REC conn_state=CONNECT(1)

**频段**: 5GHz ch161, 2.4GHz ch6 — 均失败

### 当前方向: TX auth 帧 (调查报告 `docs/re/win_re_hif_ctrl_investigation.md`)

**已排除**: TXD (全部匹配 Windows), DMASHDL (full init 恢复), EFUSE_CTRL (恢复)

**待修复 (按优先级)**:
1. **PLE/PSE 诊断** — TX 后读 0x820c0360/0x820c80b0 确认帧是否进入固件队列
2. **BSS_INFO 补充 TLV** — RATE + PROTECT + IFS_TIME (Windows 发 14 个，我们 3 个)
3. **Config 命令** — CID=0x0b (不能用 0x02，与 BSS_INFO 冲突), 需硬编码 CID
4. **审计报告高风险项** — STA_REC option 0x07 vs Windows 0xed, conn_type 格式

## 历史 Bug 摘要 (已修复)

| Bug | 原因 |
|-----|------|
| MCU 事件收不到 | PKT_TYPE_RX_EVENT: CONNAC3=7, 不是 CONNAC2=1 |
| RX ring 耗尽 | CIDX 未更新: NAPI 处理后必须写 ring->tail |
| 扫描结果崩溃 | scan_completed 在 NAPI 上下文调用 — 需 work queue |
| 802.11 帧解析错位 | HDR_OFFSET 填充字节未跳过 |
| MCU 响应丢失 | INT_STA 未 Write-1-to-Clear |
| MCU polling 竞争 | mcu_wait_resp() 期间未禁用 RX6 中断 |
| PostFwDownloadInit CID 错误 | 用了 outer dispatch tag 而非 inner CID (Session 18) |
| NAPI/UniCmd 竞争 | UniCmd ACK 路径未 napi_disable → NAPI 抢走响应 (Session 18) |
| cancel_hw_scan EBUSY | 未调 ieee80211_scan_completed → mac80211 scan_req 永久卡住 (Session 18) |
| BSS_INFO_BASIC packed_field | 用了 MT6639 的 0x10001, Windows 硬编码 0x00080015 (Session 18) |
| Scan 回归 (0 BSS) | Session 19 CID→inner 导致; 修复: 恢复 mt7925 CID + DMASHDL + EFUSE_CTRL (Session 20) |

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

## Windows RE: 完整 CID 映射表 (2026-02-21 分析, 58个条目)

**分析来源**: `docs/re/win_re_cid_mapping.md` — 从 `mtkwecx.sys` dispatch table `0x1402507e0` 提取

### 关键确认 (Ghidra 汇编级验证)

| outer_tag | inner_CID | 命令 |
|-----------|-----------|------|
| `0x11` | `0x01` | DEV_INFO ✓ |
| `0x05/0x12/0x16/0x17/0x18/0x19/0x1e` | `0x02` | BSS_INFO sub-TLVs ✓ |
| `0x4c` | `0x4a` | BSS_INFO_PROTECT (！不是0x02) |
| `0x0a` | `0x08` | BSS_INFO_HE (！不是0x02) |
| `0xb1` | `0x25` | STA_REC ✓ (Windows 用 R9=0xa8 作为 option 参数) |
| `0x8a` | `0x0e` | NIC_CAP ✓ |
| `0x03` | `0x16` | SCAN_REQ ✓ |
| `0x1c` | `0x27` | CH_PRIVILEGE/ROC |
| `0x28` | `0x28` | DBDC |
| `0x93` | `0x49` | BAND_CONFIG |
| `0x02` | `0x0b` | CLASS_02 (PostFwDownloadInit step3) |
| `0x5d` | `0x2c` | HIF_CTRL (仅 suspend/resume，probe中不发) |
| `0x07` | `0x03` | SET_DOMAIN (regulatory) |
| `0x08` | `0x03` | WFDMA_CFG ext |
| `0x58` | `0x05` | EFUSE_CTRL |

### STA_REC 关键发现
Windows 调用 `0x1400cdc4c`(nicUniCmdAllocEntry) 时: `DL=0xb1, R8=0xed, R9=0xa8`
- R8=0xed = option (fire-and-forget)
- R9=0xa8 = BSS info / conn_type 相关参数 (第4个参数)

### BSS_INFO_PROTECT 关键发现
outer_tag=0x4c → inner_CID=**0x4a** (NOT 0x02!)
这意味着如果我们当前用 inner=0x02 发送 PROTECT TLV，固件会拒绝！

### CLASS_02 命令
outer=0x02 → inner=0x0b, payload 12字节: `{0x01, 0x00, 0x70000}` (little-endian)
Handler验证: `byte[rdx]==0x02`, `size==0x0c`

