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
| WFDMA DMA 引擎 (TX 0/15/16, RX 4/6/7) | ✅ |
| 固件下载 (patch + 6 RAM regions, fw_sync=0x3) | ✅ |
| MCU 命令通信 (UniCmd, NIC_CAP 3ms 响应) | ✅ |
| PostFwDownloadInit (全部 10 步) | ✅ |
| mac80211 注册 (wlp9s0 接口) | ✅ |
| WiFi 扫描 (iw scan dump, 56-61 BSS) | ✅ |
| NAPI RX 路径 (MCU events + beacon/probe) | ✅ |
| TX 数据路径 (auth 帧发送) | ❌ 当前阻塞 |

**当前阻塞**: auth 帧通过 DMA 提交 (DIDX=CIDX)，但固件未将帧发射到空中。最可能原因是缺少 TXP (Transmit Page) scatter-gather 机制 — mt7925 PCIe 使用 CT 模式需要 TXD+TXP。

## 构建与加载

```bash
# 构建驱动 (多文件模块)
make driver          # → src/mt7927.ko

# 加载/卸载
sudo insmod src/mt7927.ko
sudo rmmod mt7927_pci
sudo dmesg | tail -60

# 检查设备 / 恢复
make check
make recover         # PCI remove + rescan

# WiFi 连接测试
sudo wpa_supplicant -i wlp9s0 -c /tmp/wpa_mt7927.conf -d
```

sudo 密码: `123456`

## 代码架构

### 驱动文件 (多文件内核模块 → mt7927.ko)
- **`src/mt7927_pci.c`** (~3200 行) — 主驱动: 初始化、MCU 通信、mac80211 回调
- **`src/mt7927_pci.h`** (~1500 行) — 寄存器定义、数据结构、宏
- **`src/mt7927_mac.c`** (~680 行) — TXD 构建、RXD 解析、RX 分发
- **`src/mt7927_dma.c`** (~600 行) — 中断处理、NAPI poll、TX enqueue/kick/complete

### 代码分工

**mt7927_pci.c**:
```
1. 寄存器读写         mt7927_rr() / mt7927_wr()
2. DMA ring 管理      mt7927_tx_ring_alloc() / mt7927_rx_ring_alloc()
3. MCU 命令 (Legacy)  mt7927_mcu_send_cmd()     — FWDL 用
4. MCU 命令 (UniCmd)  mt7927_mcu_send_unicmd()  — 固件启动后用
5. FWDL 流程          mt7927_fw_download()
6. PostFwDownloadInit  mt7927_post_fw_init()
7. mac80211 回调      mt7927_add_interface(), mt7927_sta_state(),
                      mt7927_mac80211_tx(), mt7927_bss_info_changed(),
                      mt7927_hw_scan(), mt7927_mgd_prepare_tx() (ROC)
8. MCU 命令构建       mt7927_mcu_uni_add_dev(), mt7927_mcu_add_bss_info(),
                      mt7927_mcu_sta_update(), mt7927_mcu_hw_scan()
9. Probe 入口         mt7927_pci_probe()
```

**mt7927_mac.c**:
```
TX: mt7927_mac_write_txwi() → 构建 32 字节 CONNAC3 TXD
    mt7927_tx_prepare_skb()  → TXD + skb_push
RX: mt7927_mac_fill_rx()    → 解析 RXD (GROUP 1-5) + 填充 rx_status
    mt7927_queue_rx_skb()    → 按 PKT_TYPE 分发 (NORMAL/RX_EVENT/TXS/etc)
```

**mt7927_dma.c**:
```
IRQ:  mt7927_irq_handler()   → 顶半: 保存 INT_STA, 禁中断, 调度 tasklet
      mt7927_irq_tasklet()    → 底半: 清中断, 调度 NAPI
NAPI: mt7927_poll_rx_data()  → RX ring 4 (WiFi 数据帧)
      mt7927_poll_rx_mcu()   → RX ring 6 (MCU 事件)
      mt7927_poll_tx()       → TX 完成处理
TX:   mt7927_tx_queue_skb()  → DMA map + 填描述符
      mt7927_tx_kick()       → 写 CIDX 触发 DMA
      mt7927_tx_complete()   → 扫描 DIDX, unmap + free skb
```

### 初始化流程 (probe)

```
pci_enable_device → ioremap BAR0
→ SET_OWN → MCU init → CLR_OWN
→ WFDMA config (GLO_CFG + prefetch)
→ Init TX/RX rings (TX15, TX16, RX4, RX6, RX7)
→ FWDL: patch → 6 RAM regions → FW_START → poll fw_sync=0x3
→ PostFwDownloadInit:
   DMASHDL enable → WpdmaConfig → clear FWDL bypass
   → NIC_CAP → Config (0x02, 0xc0) → DBDC → ScanConfig/ChipConfig/LogConfig
→ Init data rings (TX ring 0) + request_irq + NAPI enable
→ Register ieee80211_hw (2.4G + 5G bands, HT/VHT/HE)
```

## 关键 Bug 与修复记录

### CONNAC3 PKT_TYPE (不同于 CONNAC2!)
```
PKT_TYPE_TXS=0, TXRXV=1, NORMAL=2, RX_DUP_RFB=3, RX_TMR=4
RETRIEVE=5, TXRX_NOTIFY=6, RX_EVENT=7, NORMAL_MCU=8
```
- **Bug**: 用了 CONNAC2 的 PKT_TYPE_RX_EVENT=1，正确值是 7

### UniCmd 格式致命教训
```c
// 1. len 字段必须包含 16 字节内部头 (0x20-0x2F)
txd->len = cpu_to_le16(plen + 16);  // ✅ 不是 plen!

// 2. CID 用 Windows class 值, 不是 mt76 UNI_CMD ID
UNI_CMD_ID_NIC_CAP = 0x008a;   // ✅ 不是 0x0E!

// 3. 查询命令必须 option=0x07 (BIT(2)=need_response 必须)
UNI_CMD_OPT_SET_ACK = 0x07;    // SET 用 0x06 (fire-and-forget)
```

### 其他关键 Bug
- **INT_STA W1C**: 必须 Write-1-to-Clear 中断状态
- **NAPI/polling 竞争**: mcu_wait_resp() 期间必须禁用 RX6 中断
- **CIDX 必须更新**: NAPI 处理后写 ring->tail 到 CIDX，否则 ring 耗尽
- **PKT_TYPE_NORMAL_MCU(8)**: beacon/probe 可能在 MCU ring 出现
- **HDR_OFFSET 填充**: skb_pull 必须跳过 `2 * remove_pad` 填充字节
- **scan_completed**: 必须通过 work queue 延迟调用，不能在 NAPI 上下文
- **STA_REC at NOTEXIST→NONE**: 固件需要 WTBL 条目才能发帧

## 致命操作 — 永远不要使用

```c
pcie_flr(pdev);              // 设备进入 D3cold, 永不恢复
pci_reset_function(pdev);    // probe 中死锁, 需要硬重启
writel(BIT(4), bar0+0x1f8600); // CB_INFRA_RGU, 设备不可恢复
```

## 关键硬件事实

- **CLR_OWN 必须先 SET_OWN**: SET_OWN(BIT(0)) → wait OWN_SYNC → CLR_OWN(BIT(1))
- **CLR_OWN 会清零所有 HOST ring**: ROM 做完整 WFDMA reset
- **FWDL 后不要重置 DMA**: 会破坏固件已缓存的 DMA 状态
- **FWDL bypass 在 wpdma_config 之后清除**: wpdma_config 会重新设置 BIT(9)
- **Windows ring layout**: TX 0,1,15,16 + RX 4,6,7 (no ring 0 or 5)
- **中断掩码**: 0x2600f010 (Windows 0x2600f000 + BIT(4) for TX ring 0)
- **RX CIDX 语义**: 写 ring->tail (next-to-process) 释放 slot 给 DMA

## TX 数据路径架构 (当前问题区域)

### TXD 格式 (CONNAC3, 32 字节)
```
TXD[0]: TX_BYTES | PKT_FMT | Q_IDX
TXD[1]: WLAN_IDX | OWN_MAC | TGID | HDR_FORMAT | TID | FIXED_RATE
TXD[2]: FRAME_TYPE | SUB_TYPE | HDR_PAD
TXD[3]: REM_TX_COUNT | NO_ACK | BCM | BA_DISABLE | PROTECT_FRAME
TXD[5]: PID | TX_STATUS_HOST
TXD[6]: MSDU_CNT | DAS | DIS_MAT
```

### 关键发现 (auth timeout 调查)
- 管理帧: Q_IDX = 0x10 (MT_LMAC_ALTX0), FIXED_RATE=1, BA_DISABLE=1
- mt7925 PCIe 使用 CT 模式 (PKT_FMT=0) + TXP (24 字节 scatter-gather)
- **TXP**: TXD 后紧跟 24 字节 `mt76_connac_hw_txp`，含 DMA 地址指向 skb data
- DMA 描述符: buf0=TXD+TXP (56 bytes), buf1=skb data
- SF 模式 (PKT_FMT=1) 在 PCIe 上不工作 (已测试)

## 参考代码

| 优先级 | 来源 | 路径 | 用途 |
|--------|------|------|------|
| 1 | mt7925 上游驱动 | `mt76/mt7925/` | mac80211 集成、MCU 命令、TX/RX 路径 |
| 2 | MT6639 Android 驱动 | `mt6639/` | 硬件寄存器、CONNAC3 架构 |
| 3 | Windows RE 文档 | `docs/win_v5705275_*.md` | 启动序列、TXD 格式 |
| 4 | Ghidra RE 文档 | `docs/references/ghidra_post_fw_init.md` | PostFwDownloadInit 分析 |

## 设备恢复

```bash
echo 1 | sudo tee /sys/bus/pci/devices/0000:0a:00.0/remove
sleep 2
echo 1 | sudo tee /sys/bus/pci/rescan
# 或: make recover
```

## 测试环境

- **WiFi AP**: SSID=`CMCC-Pg2Y-5G-FAST`, password=`7ue9pxgp`, WiFi 5 路由器
- **wpa_supplicant 配置**: `/tmp/wpa_mt7927.conf`
- **电脑当前用网线上网** — WiFi 测试时需指定 wlan 接口

## 开发偏好

- **回复语言**: 中文
- **代理策略**: 委托分析给子 agent，主 agent 做宏观决策
- **轻量任务用 Sonnet** 节省成本，复杂任务用 Opus
- **agent 创建**: TeamCreate + Task with team_name
- **Git 分支**: 实验性更改用 `experiment/` 分支
