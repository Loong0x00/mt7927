# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

MT7927 WiFi 7 Linux 驱动开发项目 — 从零编写，基于 Windows 驱动逆向工程 + MediaTek Android MT6639 参考代码。

**关键事实**:
- MT7927 = MT6639 移动芯片的 PCIe 封装，**不是** MT76 家族！
- PCI ID: `14c3:6639`（芯片 ID 是 6639，不是 7927）
- MediaTek 从未发布 Linux WiFi 驱动，全球无人有可用驱动
- 蓝牙通过 USB (btusb/btmtk)，WiFi 是 PCIe — 完全独立

## 当前状态 (2026-02-15)

| 功能 | 状态 |
|------|------|
| PCIe 设备探测 + BAR0 映射 | ✅ |
| 电源管理 (SET_OWN/CLR_OWN) | ✅ |
| WFDMA DMA 引擎 (TX 15/16, RX 4/6/7) | ✅ |
| 固件下载 (patch + 6 RAM regions, fw_sync=0x3) | ✅ |
| MCU 命令通信 (UniCmd, NIC_CAP 3ms 响应) | ✅ |
| PostFwDownloadInit (全部 10 步: DMASHDL→NIC_CAP→Config×2→DBDC→Scan/Chip/LogCfg) | ✅ 完成 |
| mac80211 注册 / WiFi 扫描 / 连接 / 数据 | ❌ 下一阶段 |

## 构建与加载

```bash
# 构建新驱动 (主要)
make driver          # → src/mt7927_pci.ko

# 构建旧测试驱动 (已废弃，仅历史参考)
make tests           # → tests/04_risky_ops/mt7927_init_dma.ko

# 加载/卸载
sudo insmod src/mt7927_pci.ko
sudo rmmod mt7927_pci
sudo dmesg | tail -60

# 检查设备 / 恢复
make check
make recover         # PCI remove + rescan
```

sudo 密码: `123456`

## 代码架构

### 驱动文件
- **`src/mt7927_pci.c`** (~1600 行) — 主驱动，所有功能
- **`src/mt7927_pci.h`** (~680 行) — 寄存器定义、数据结构、宏

### 代码结构 (mt7927_pci.c 内部分区)

```
1. 寄存器读写         mt7927_rr() / mt7927_wr()
2. DMA ring 管理      mt7927_tx_ring_alloc() / mt7927_rx_ring_alloc()
3. 中断配置           mt7927_config_int_mask()
4. MCU 命令 (Legacy)  mt7927_mcu_send_cmd()     — 0x40 字节头, FWDL 用
5. MCU 命令 (UniCmd)  mt7927_mcu_send_unicmd()  — 0x30 字节头, 固件启动后用
6. FWDL 辅助          mt7927_patch_dl_mode() / mt7927_fw_scatter() 等
7. 初始化流程:
   7a. mt7927_mcu_init_mt6639()   — MCU 初始化
   7b. mt7927_wpdma_config()      — WFDMA 预取 + GLO_CFG
   7c. mt7927_init_tx_rx_rings()  — TX/RX ring 分配
   7d. mt7927_fw_download()       — 完整 FWDL 流程
   7e. mt7927_post_fw_init()      — PostFwDownloadInit MCU 命令
8. 诊断 dump
9. mt7927_pci_probe()             — 入口: 串联所有初始化步骤
```

### 初始化流程 (probe)

```
pci_enable_device → ioremap BAR0
→ SET_OWN → MCU init → CLR_OWN
→ WFDMA config (GLO_CFG + prefetch)
→ Init TX/RX rings (TX15, TX16, RX4, RX6, RX7)
→ FWDL: patch → 6 RAM regions → FW_START → poll fw_sync=0x3
→ PostFwDownloadInit:
   DMASHDL enable (0xd6060 |= 0x10101)
   → WpdmaConfig → clear FWDL bypass
   → NIC_CAP (class=0x8a, option=0x07) → 3ms 响应 ✅
   → Config (class=0x02, 0xc0) → fire-and-forget ✅
```

## UniCmd 格式 (CONNAC3, 固件启动后)

**三个致命教训** — 之前导致 MCU 通信失败的 bug:

```c
// 1. len 字段必须包含 16 字节内部头 (0x20-0x2F)
txd->len = cpu_to_le16(plen + 16);  // ✅ 不是 plen!

// 2. CID 用 Windows class 值, 不是 mt76 UNI_CMD ID
UNI_CMD_ID_NIC_CAP = 0x008a;   // ✅ 不是 0x0E!

// 3. 查询命令必须 option=0x07 (ACK + UNI + need_response)
// BIT(2)=need_response 缺失时固件不发 DMA 响应
UNI_CMD_OPT_SET_ACK = 0x07;    // ✅ 不是 0x03!
```

**UniCmd TXD 布局 (0x30 字节)**:
```
+0x00  TXD[0]: total_len | 0x41000000 (Q_IDX=0x20, PKT_FMT=2)
+0x04  TXD[1]: flags | 0x4000 (HDR_FORMAT_V3=1, 无 BIT(31))
+0x08  TXD[2..7]: 全零
+0x20  len:    plen + 16 (内部头 + payload)
+0x22  cid:    命令 class (0x8a, 0x02, 0xc0 等)
+0x25  pkt_type: 0xa0
+0x27  seq:    序列号
+0x2a  s2d_index: 0
+0x2b  option: 0x07 (查询) 或 0x06 (SET fire-and-forget)
+0x30  payload
```

## 致命操作 — 永远不要使用

```c
pcie_flr(pdev);              // 设备进入 D3cold, 永不恢复
pci_reset_function(pdev);    // probe 中死锁, 需要硬重启
writel(BIT(4), bar0+0x1f8600); // CB_INFRA_RGU, 设备不可恢复
```

## 关键硬件事实

- **CLR_OWN 必须先 SET_OWN**: SET_OWN(BIT(0)) → wait OWN_SYNC → CLR_OWN(BIT(1))
- **CLR_OWN 会清零所有 HOST ring**: ROM 做完整 WFDMA reset, 只配置 MCU_RX2/RX3
- **MCU_RX0 BASE=0 是正常的**: MCU 响应通过 MCU_RX2/RX3 → HOST RX6 路由
- **MCU_TX BASE=0 是正常的**: MCU 事件通过 HOST RX ring 来
- **FWDL 后不要重置 DMA**: 会破坏固件已缓存的 DMA 状态
- **FWDL bypass 在 wpdma_config 之后清除**: wpdma_config 会重新设置 BIT(9)

## BAR0 地址映射

| BAR0 偏移 | 总线地址 | 寄存器 |
|-----------|----------|--------|
| 0x0c1604 | 0x81021604 | ROMCODE_INDEX (MCU_IDLE=0x1D1E) |
| 0xd4200 | 0x7c024200 | HOST_INT_STA |
| 0xd4204 | 0x7c024204 | HOST_INT_ENA |
| 0xd4208 | 0x7c024208 | WFDMA HOST GLO_CFG |
| 0xd42b4 | 0x7c0242b4 | GLO_CFG_EXT1 |
| 0xd4300 | 0x7c024300 | TX Ring 0 BASE (WFDMA) |
| 0xd4540 | 0x7c024540 | RX Ring 4 BASE |
| 0xd4560 | 0x7c024560 | RX Ring 6 BASE (MCU 事件) |
| 0xd4570 | 0x7c024570 | RX Ring 7 BASE |
| 0xd6060 | 0x7c026060 | DMASHDL enable |
| 0xd70f0 | 0x7c0270f0 | Prefetch config (4 regs) |
| 0xe0010 | 0x7c060010 | LPCTL (SET_OWN/CLR_OWN) |
| 0x1f8600 | 0x70028600 | CB_INFRA_RGU (⚠️致命) |

## PostFwDownloadInit MCU 命令序列 (Windows RE)

```
1. DMASHDL enable: 0xd6060 |= 0x10101                   ← 已实现 ✅
2. NIC_CAP:     class=0x8a, no payload, option=0x07      ← 已实现 ✅
3. Config:      class=0x02, data={1, 0, 0x70000}         ← 已实现 ✅
4. Config:      class=0xc0, data={0x820cc800, 0x3c200}   ← 已实现 ✅
5. BufferBin:   class=0xed (optional)                     ← 跳过 (可选)
6. DBDC:        class=0x28 (MT6639 only, TLV mbmc_en=1)  ← 已实现 ✅
7. 1ms delay                                             ← 已实现 ✅
8. ScanConfig:  class=0xca, "PassiveToActiveScan"        ← 已实现 ✅
9. ChipConfig:  class=0xca, "KeepFullPwr 0"              ← 已实现 ✅
10. LogConfig:  class=0xca, "EvtDrvnLogCatLvl 0"         ← 已实现 ✅
```

## 参考代码

| 优先级 | 来源 | 路径 | 用途 |
|--------|------|------|------|
| 1 | mt7925 上游驱动 | `mt76/mt7925/` | mac80211 集成、MCU 命令 (下一阶段主参考) |
| 2 | MT6639 Android 驱动 | `mt6639/` | 硬件寄存器、CONNAC3 架构 |
| 3 | Windows RE 文档 | `docs/win_v5705275_*.md` (9 files) | 启动序列、TXD 格式 (已主要完成) |
| 4 | Ghidra RE 文档 | `docs/references/ghidra_post_fw_init.md` | PostFwDownloadInit 完整分析 |

Windows RE 仅覆盖 **固件启动 → PostFwDownloadInit**。扫描/连接/数据路径需要参考 mt7925。

## 设备恢复

```bash
# 设备读取 0xffffffff 时
echo 1 | sudo tee /sys/bus/pci/devices/0000:0a:00.0/remove
sleep 2
echo 1 | sudo tee /sys/bus/pci/rescan
# 或: make recover
```

## 下一阶段: mac80211 集成

核心思路: MCU 通信已打通，后续从 mt7925 上游驱动适配上层:
1. **mac80211 注册** — 从 `mt76/mt7925/init.c` 抄框架，改芯片参数
2. **扫描** — `mt76/mt7925/mcu.c` 的 scan MCU 命令，改 CID 适配 CONNAC3
3. **连接** — mac80211 处理 WPA，驱动转发 MCU 命令
4. **数据路径** — TX ring 0/1 发数据帧，RX ring 4 收

## 开发偏好

- **回复语言**: 中文
- **代理策略**: 委托分析给子 agent，主 agent 做宏观决策
- **轻量任务用 Sonnet** 节省成本，复杂任务用 Opus
- **多 agent 并行**: TeamCreate + Task with team_name
- **定期写 docs**: 避免上下文压缩丢失信息
- **Git 分支**: 实验性更改用 `experiment/` 分支
