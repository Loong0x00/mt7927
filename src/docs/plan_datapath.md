# MT7927 数据路径实现方案

**创建时间**: 2026-02-15
**分析基础**: mt76/mt7925 上游驱动 + mt7927 当前实现
**目标**: 为 mt7927 添加完整的 TX/RX 数据路径，支持 WiFi 数据帧收发

---

## 目录

1. [核心发现](#1-核心发现)
2. [CONNAC3 TXD 格式](#2-connac3-txd-格式)
3. [CONNAC3 RXD 格式](#3-connac3-rxd-格式)
4. [Ring 布局对比](#4-ring-布局对比)
5. [中断/NAPI 方案](#5-中断napi-方案)
6. [实现方案](#6-实现方案)
7. [代码量估算](#7-代码量估算)

---

## 1. 核心发现

### 1.1 MT7927 vs MT7925 架构对比

| 特性 | MT7927 (当前) | MT7925 (上游) | 差异分析 |
|------|---------------|---------------|----------|
| **芯片架构** | MT6639 CONNAC3 (PCIe) | MT7925 CONNAC3 (PCIe/SDIO) | 相同 CONNAC3，TXD/RXD 格式一致 |
| **MCU event ring** | RX ring 6 | RX ring 0 | Windows 用 ring 6，mt7925 用 ring 0 |
| **数据 TX ring** | ❌ 未实现 | TX ring 0-3 (4 AC) | 需新增 TX ring 0/1 |
| **数据 RX ring** | RX ring 4 ✅ | RX ring 4 ✅ | 一致 |
| **FWDL ring** | TX ring 16 ✅ | TX ring 16 ✅ | 一致 |
| **MCU TX ring** | TX ring 15 ✅ | TX ring 15 ✅ | 一致 |
| **DMA 结构** | 自定义 `mt7927_ring` | mt76 框架 `mt76_queue` | 需对接 mt76 框架 |
| **中断处理** | ❌ 轮询 | ✅ IRQ + NAPI | 需实现中断驱动 |
| **mac80211 集成** | ❌ 未实现 | ✅ 完整 | 最终目标 |

### 1.2 关键路径对比

**mt7927 当前流程（FWDL only）**:
```
PCIe probe → SET_OWN → CLR_OWN → Init TX15/16, RX4/6/7
→ FWDL → PostFwDownloadInit → 轮询 RX6 读 MCU 响应
```

**mt7925 完整流程（数据路径）**:
```
PCIe probe → DMA init (TX 0-3/15/16, RX 0/4)
→ FWDL → mac80211 注册 → 中断/NAPI 启用
→ 扫描/连接 → TX数据帧 (ring 0-3) → RX数据帧 (ring 4)
```

---

## 2. CONNAC3 TXD 格式

### 2.1 TXD 结构总览

**来源**: `mt76/mt76_connac3_mac.h:216-241`, `mt76/mt7925/mac.c:723-836`

CONNAC3 TXD 为 **0x20 字节（8 DWORD）**，用于描述 TX 包：

```c
struct mt76_connac3_txd {
    __le32 txd[8];  // 8 个 DWORD
} __packed;
```

### 2.2 TXD 字段定义 (数据帧)

#### TXD[0] - 总体配置
| 字段 | Bits | 描述 | 数据帧值 |
|------|------|------|----------|
| `TX_BYTES` | 15:0 | 总长度 (skb->len + TXD size) | 动态 |
| `ETH_TYPE_OFFSET` | 22:16 | 以太网类型偏移 | 0 |
| `PKT_FMT` | 24:23 | 包格式 | `MT_TX_TYPE_CT` (0, MMIO) |
| `Q_IDX` | 31:25 | 队列索引 | wmm_idx×4 + AC |

**MT_TX_TYPE_CT vs MT_TX_TYPE_CMD**:
- `MT_TX_TYPE_CT = 0`: 数据帧 (Cut-through)
- `MT_TX_TYPE_SF = 1`: SDIO store-and-forward (mt7927 不用)
- `MT_TX_TYPE_CMD = 2`: MCU 命令
- `MT_TX_TYPE_FW = 3`: FW 特殊帧

**Q_IDX 计算** (mt7925/mac.c:762-763):
```c
q_idx = wmm_idx * MT76_CONNAC_MAX_WMM_SETS +  // MT76_CONNAC_MAX_WMM_SETS=4
        mt76_connac_lmac_mapping(skb_get_queue_mapping(skb));
// mt76_connac_lmac_mapping(): AC_BK→1, AC_BE→0, AC_VI→2, AC_VO→3
```

#### TXD[1] - WCID 和头信息
| 字段 | Bits | 描述 | 数据帧值 |
|------|------|------|----------|
| `WLAN_IDX` | 11:0 | WTBL 索引 (wcid->idx) | 0-127 |
| `TGID` | 13:12 | 目标组 ID (band_idx) | 0/1 |
| `HDR_FORMAT` | 15:14 | 头格式 | `MT_HDR_FORMAT_802_3` (0) / `MT_HDR_FORMAT_802_11` (1) |
| `HDR_INFO` | 20:16 | 802.11 头长度/2 (仅 802.11) | 0 或动态 |
| `ETH_802_3` | 20 | 802.3 标志 (仅 802.3 模式) | 1 (if ethertype≥0x600) |
| `TID` | 24:21 | TID (0-7 QoS, 15 非QoS) | skb->priority & 7 |
| `OWN_MAC` | 30:25 | OMAC 索引 (mvif->omac_idx) | 0-15 |
| `FIXED_RATE` | 31 | 固定速率 (非数据帧) | 0 |

**HDR_FORMAT 选择** (mt7925/mac.c:733):
```c
bool is_8023 = info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP;
if (is_8023)
    mt7925_mac_write_txwi_8023(txwi, skb, wcid);  // HDR_FORMAT=0
else
    mt7925_mac_write_txwi_80211(dev, txwi, skb, key);  // HDR_FORMAT=1
```

#### TXD[2] - 帧类型
| 字段 | Bits | 描述 | 数据帧值 |
|------|------|------|----------|
| `SUB_TYPE` | 3:0 | 802.11 子类型 | 0 (QoS data) |
| `FRAME_TYPE` | 5:4 | 802.11 类型 | 2 (data) |
| `OWN_MAC_MAP` | 8 | OMAC 映射 | 0 |
| `RTS` | 9 | RTS 保护 | 0 |
| `HDR_PAD` | 11:10 | 头填充 | 0 |
| `DURATION` | 12 | 固定 duration | 0 |
| `HTC_VLD` | 13 | HT control 有效 | 0 |
| `FRAG` | 15:14 | 分片标志 | 0 |
| `MAX_TX_TIME` | 25:16 | 最大 TX 时间 | 0 |
| `POWER_OFFSET` | 31:26 | 功率偏移 | 0 |

#### TXD[3] - TX 控制
| 字段 | Bits | 描述 | 数据帧值 |
|------|------|------|----------|
| `BCM` | 0 | 广播/组播 | multicast |
| `NO_ACK` | 8 | 不需要 ACK | 0 (unless info->flags) |
| `HW_AMSDU` | 10 | 硬件 A-MSDU | wcid->amsdu |
| `PROTECT_FRAME` | 11 | 加密保护 | key ? 1 : 0 |
| `BA_DISABLE` | 12 | BA 禁用 | 0 |
| `REM_TX_COUNT` | 20:16 | 重传次数 | 15 |
| `SN_VALID` | 28 | 序列号有效 (injected) | 0 |
| `SEQ` | 27:16 | 序列号 (injected) | 0 |

#### TXD[4] - 保留
全零

#### TXD[5] - PID 和状态
| 字段 | Bits | 描述 | 数据帧值 |
|------|------|------|----------|
| `PID` | 7:0 | Packet ID (TX status 追踪) | pid |
| `TX_STATUS_HOST` | 10 | 请求 TX status | pid≥MT_PACKET_ID_FIRST |

**PID 机制** (mt7925/mac.c:797-801):
```c
pid = mt76_tx_status_skb_add(mdev, wcid, tx_info->skb);
if (pid >= MT_PACKET_ID_FIRST) {
    val |= MT_TXD5_TX_STATUS_HOST;  // 请求 TX status 报告
    txwi[3] |= MT_TXD3_BA_DISABLE;  // 禁用 BA
}
```

#### TXD[6] - MSDU 和速率
| 字段 | Bits | 描述 | 数据帧值 |
|------|------|------|----------|
| `MSDU_CNT` | 5:0 | MSDU 计数 | 1 |
| `DAS` | 15 | DA 源选择 | 1 |
| `DIS_MAT` | 23 | 禁用 MAT (非 MLO) | 1 |
| `TX_RATE` | 30:24 | 速率索引 (固定速率) | idx |

#### TXD[7] - 保留
全零

### 2.3 TXD vs MCU 命令对比

| 字段 | 数据帧 | MCU 命令 (Legacy 0x40) | MCU 命令 (UniCmd 0x30) |
|------|--------|------------------------|------------------------|
| **TXD size** | 0x20 (8 DWORD) | 0x40 (16 DWORD) | 0x30 (12 DWORD) |
| **PKT_FMT** | `MT_TX_TYPE_CT` (0) | `MT_TX_TYPE_CMD` (2) | `MT_TX_TYPE_CMD` (2) |
| **Q_IDX** | wmm×4 + AC | `MT_TX_MCU_PORT_RX_Q0` (0x20) | `MT_TX_MCU_PORT_RX_Q0` (0x20) |
| **LONG_FORMAT** | ❌ BIT(31)=0 | ✅ BIT(31)=1 | ✅ (隐式) |
| **内部头** | 无 | 从 +0x40 开始 | 从 +0x20 开始 (16 字节) |

**关键差异** (mt7927 当前实现):
- mt7927 用 **0x40 字节 Legacy MCU TXD** (`mt76_connac2_mcu_txd`) 做 FWDL
- mt7927 用 **0x30 字节 UniCmd TXD** 做 PostFwDownloadInit
- **数据帧需要 0x20 字节 CONNAC3 TXD** (mt7925 格式)

---

## 3. CONNAC3 RXD 格式

### 3.1 RXD 结构总览

**来源**: `mt76/mt7925/mac.c:354-619`, `mt76/mt7615/mac.h:10-59`

CONNAC3 RXD 为 **可变长度**，包含基础头 (8 DWORD) + 可选分组：

```
+0x00  RXD[0-7]   基础头 (32 字节)
+0x20  GROUP_4    Frame control/seq/qos (可选, 16 字节)
+0x30  GROUP_1    IV 信息 (可选, 16 字节)
+0x40  GROUP_2    Timestamp/AMPDU (可选, 16 字节)
+0x50  GROUP_3    P-RXV 速率信息 (可选, 16 字节)
+0x60  GROUP_5    C-RXV 扩展 (可选, 96 字节)
+xx    实际数据   802.3 or 802.11 frame
```

### 3.2 RXD 基础头字段 (RXD[0-7])

#### RXD[0] - 包类型和分组标志
| 字段 | Bits | 描述 | 检查方法 |
|------|------|------|----------|
| `PKT_TYPE` | 31:27 | 包类型 | `le32_get_bits(rxd[0], MT_RXD0_PKT_TYPE)` |
| `PKT_FLAG` | 19:16 | 包标志 | 区分 MCU event (flag=0x1) |
| `SW_PKT_TYPE` | 22:20 | SW 包类型 | 特殊处理 FRAME 类型 |

**PKT_TYPE 分类** (mt7925/mac.c:1213-1224):
```c
#define PKT_TYPE_NORMAL         0  // 数据帧
#define PKT_TYPE_RX_EVENT       1  // MCU 事件 (flag=0 时)
#define PKT_TYPE_NORMAL_MCU     1  // MCU 事件 (flag=0x1 时)
#define PKT_TYPE_TXRX_NOTIFY    6  // TX 完成通知 (MMIO only)
#define PKT_TYPE_TXS            7  // TX status
```

**类型判断逻辑** (mt7925/mac.c:1215-1224):
```c
type = le32_get_bits(rxd[0], MT_RXD0_PKT_TYPE);
flag = le32_get_bits(rxd[0], MT_RXD0_PKT_FLAG);

if (type != PKT_TYPE_NORMAL) {
    u32 sw_type = le32_get_bits(rxd[0], MT_RXD0_SW_PKT_TYPE_MASK);
    if ((sw_type & MT_RXD0_SW_PKT_TYPE_MAP) == MT_RXD0_SW_PKT_TYPE_FRAME)
        type = PKT_TYPE_NORMAL;  // 转换为数据帧
}

if (type == PKT_TYPE_RX_EVENT && flag == 0x1)
    type = PKT_TYPE_NORMAL_MCU;  // MCU 事件
```

#### RXD[1] - 索引和头转换
| 字段 | Bits | 描述 | 提取方法 |
|------|------|------|----------|
| `WLAN_IDX` | 9:0 | WTBL 索引 | `FIELD_GET(MT_RXD1_NORMAL_WLAN_IDX, rxd1)` |
| `ADDR_TYPE` | 2:1 | 地址类型 | U2M(1)/M2M(2)/B2M(3) |
| `KEY_ID` | 7:6 | 密钥 ID | 用于 CCMP IV |
| `CH_FREQ` | 15:8 | 信道频率 | `FIELD_GET(MT_RXD3_NORMAL_CH_FREQ, rxd3)` |
| `MAC_HDR_LEN` | 21:16 | 802.11 头长度 | 仅 802.11 模式 |
| `HDR_OFFSET` | 22 | 头偏移 (填充) | remove_pad |
| `HDR_TRANS` | 23 | 头转换 | 1=802.3, 0=802.11 |
| `PAYLOAD_FORMAT` | 25:24 | AMSDU 格式 | FIRST/MID/LAST |
| `GROUP_x` | 31:25 | 分组标志 | GROUP_1/2/3/4/5 |

**ADDR_TYPE 判断 unicast** (mt7925/mac.c:393):
```c
#define MT_RXD3_NORMAL_U2M     0x01  // unicast to me
#define MT_RXD3_NORMAL_MCAST   0x02  // multicast
#define MT_RXD3_NORMAL_BCAST   0x03  // broadcast

unicast = FIELD_GET(MT_RXD3_NORMAL_ADDR_TYPE, rxd3) == MT_RXD3_NORMAL_U2M;
```

#### RXD[2] - 安全和错误标志
| 字段 | Bits | 描述 | 错误处理 |
|------|------|------|----------|
| `WLAN_IDX` | 7:0 | WLAN 索引 (重复) | |
| `TID` | 11:8 | TID | |
| `SEC_MODE` | 15:12 | 安全模式 | 0=无加密 |
| `SW_BIT` | 16 | SW 位 | |
| `FCS_ERR` | 17 | FCS 错误 | `status->flag \|= RX_FLAG_FAILED_FCS_CRC` |
| `CM` | 18 | 密文不匹配 | |
| `CLM` | 19 | 密文 last MPDU | |
| `ICV_ERR` | 20 | ICV 错误 | `status->flag \|= RX_FLAG_ONLY_MONITOR` |
| `TKIP_MIC_ERR` | 21 | TKIP MIC 错误 | `status->flag \|= RX_FLAG_MMIC_ERROR` |
| `LEN_MISMATCH` | 22 | 长度不匹配 | |
| `AMSDU_ERR` | 23 | AMSDU 错误 | return -EINVAL |
| `MAX_LEN_ERROR` | 24 | 最大长度错误 | return -EINVAL |
| `HDR_TRANS_ERROR` | 25 | 头转换错误 | 需要额外填充处理 |
| `INT_FRAME` | 26 | 中间帧 | |
| `FRAG` | 27 | 分片 | |
| `NULL_FRAME` | 28 | NULL 帧 | |
| `NDATA` | 29 | 非数据 | |
| `NON_AMPDU_SUB` | 30 | 非 AMPDU 子帧 | |
| `NON_AMPDU` | 31 | 非 AMPDU | |

**解密标志设置** (mt7925/mac.c:429-434):
```c
if (FIELD_GET(MT_RXD2_NORMAL_SEC_MODE, rxd2) != 0 &&
    !(rxd1 & (MT_RXD1_NORMAL_CLM | MT_RXD1_NORMAL_CM))) {
    status->flag |= RX_FLAG_DECRYPTED;
    status->flag |= RX_FLAG_IV_STRIPPED;
    status->flag |= RX_FLAG_MMIC_STRIPPED | RX_FLAG_MIC_STRIPPED;
}
```

#### RXD[3] - 信道和校验和
| 字段 | Bits | 描述 | 用途 |
|------|------|------|------|
| `ADDR_TYPE` | 2:1 | 地址类型 (重复) | unicast 判断 |
| `CH_FREQ` | 15:8 | 信道频率 | band 判断 |
| `FCS_ERR` | 17 | FCS 错误 (重复) | |
| `UDP_TCP_SUM` | 24 | TCP/UDP 校验和 OK | CHECKSUM_UNNECESSARY |
| `IP_SUM` | 23 | IP 校验和 OK | CHECKSUM_UNNECESSARY |

**校验和处理** (mt7925/mac.c:419-421):
```c
u32 csum_mask = MT_RXD3_NORMAL_IP_SUM | MT_RXD3_NORMAL_UDP_TCP_SUM;
if (mt76_is_mmio(&dev->mt76) && (rxd3 & csum_mask) == csum_mask &&
    !(csum_status & (BIT(0) | BIT(2) | BIT(3))))
    skb->ip_summed = CHECKSUM_UNNECESSARY;
```

#### RXD[4] - AMSDU 信息
| 字段 | Bits | 描述 | 用途 |
|------|------|------|------|
| `PAYLOAD_FORMAT` | 1:0 | AMSDU 位置 | FIRST/MID/LAST |

**AMSDU 解析** (mt7925/mac.c:539-544):
```c
amsdu_info = FIELD_GET(MT_RXD4_NORMAL_PAYLOAD_FORMAT, rxd4);
status->amsdu = !!amsdu_info;
if (status->amsdu) {
    status->first_amsdu = amsdu_info == MT_RXD4_FIRST_AMSDU_FRAME;
    status->last_amsdu = amsdu_info == MT_RXD4_LAST_AMSDU_FRAME;
}
```

### 3.3 RXD 可选分组

#### GROUP_4 - Frame Control (16 字节)
**触发**: `rxd1 & MT_RXD1_NORMAL_GROUP_4`

```c
u32 v0 = le32_to_cpu(rxd[8]);   // frame_control
u32 v2 = le32_to_cpu(rxd[10]);  // seq_ctrl, qos_ctl

fc = cpu_to_le16(FIELD_GET(MT_RXD8_FRAME_CONTROL, v0));
seq_ctrl = FIELD_GET(MT_RXD10_SEQ_CTRL, v2);
qos_ctl = FIELD_GET(MT_RXD10_QOS_CTL, v2);
```

#### GROUP_1 - IV 信息 (6 字节)
**触发**: `rxd1 & MT_RXD1_NORMAL_GROUP_1`
**用途**: CCMP/TKIP/GCMP IV 提取

```c
u8 *data = (u8 *)rxd;
status->iv[0] = data[5];  // IV 逆序
status->iv[1] = data[4];
status->iv[2] = data[3];
status->iv[3] = data[2];
status->iv[4] = data[1];
status->iv[5] = data[0];
```

#### GROUP_2 - Timestamp/AMPDU (16 字节)
**触发**: `rxd1 & MT_RXD1_NORMAL_GROUP_2`

```c
status->timestamp = le32_to_cpu(rxd[12]);
status->flag |= RX_FLAG_MACTIME_START;

if (!(rxd2 & MT_RXD2_NORMAL_NON_AMPDU)) {
    status->flag |= RX_FLAG_AMPDU_DETAILS;
    status->ampdu_ref = phy->ampdu_ref;  // A-MPDU 引用号
}
```

#### GROUP_3 - P-RXV 速率信息 (16 字节)
**触发**: `rxd1 & MT_RXD1_NORMAL_GROUP_3`
**关键**: 提取速率、信号强度

```c
rxv = rxd;  // P-RXV 起始
u32 v3 = le32_to_cpu(rxv[3]);

// 信号强度 (4 天线)
status->chains = mphy->antenna_mask;
status->chain_signal[0] = to_rssi(MT_PRXV_RCPI0, v3);
status->chain_signal[1] = to_rssi(MT_PRXV_RCPI1, v3);
status->chain_signal[2] = to_rssi(MT_PRXV_RCPI2, v3);
status->chain_signal[3] = to_rssi(MT_PRXV_RCPI3, v3);

// 速率解析
mt7925_mac_fill_rx_rate(dev, status, sband, rxv, &mode);
```

#### GROUP_5 - C-RXV 扩展 (96 字节)
**触发**: `rxd1 & MT_RXD1_NORMAL_GROUP_5`
**用途**: EHT/HE radiotap 扩展信息

### 3.4 速率信息解析

**函数**: `mt7925_mac_fill_rx_rate()` (mac.c:248-351)

**P-RXV 字段** (rxv[0], rxv[2]):
```c
u32 v0 = le32_to_cpu(rxv[0]);
u32 v2 = le32_to_cpu(rxv[2]);

idx = FIELD_GET(MT_PRXV_TX_RATE, v0);    // MCS 索引
nss = FIELD_GET(MT_PRXV_NSTS, v0) + 1;   // 空间流数
stbc = FIELD_GET(MT_PRXV_HT_STBC, v2);   // STBC
gi = FIELD_GET(MT_PRXV_HT_SHORT_GI, v2); // GI (short/long)
mode = FIELD_GET(MT_PRXV_TX_MODE, v2);   // PHY 模式
dcm = FIELD_GET(MT_PRXV_DCM, v2);        // DCM (HE)
bw = FIELD_GET(MT_PRXV_FRAME_MODE, v2);  // 带宽
```

**PHY 模式分类**:
```c
switch (mode) {
case MT_PHY_TYPE_CCK:       // CCK (legacy)
case MT_PHY_TYPE_OFDM:      // OFDM (legacy)
    i = mt76_get_rate(&dev->mt76, sband, i, cck);
    break;
case MT_PHY_TYPE_HT_GF:     // HT greenfield
case MT_PHY_TYPE_HT:        // HT (802.11n)
    status->encoding = RX_ENC_HT;
    break;
case MT_PHY_TYPE_VHT:       // VHT (802.11ac)
    status->encoding = RX_ENC_VHT;
    status->nss = nss;
    break;
case MT_PHY_TYPE_HE_SU:     // HE SU (802.11ax)
case MT_PHY_TYPE_HE_EXT_SU: // HE ER SU
case MT_PHY_TYPE_HE_TB:     // HE trigger-based
case MT_PHY_TYPE_HE_MU:     // HE MU
    status->encoding = RX_ENC_HE;
    status->nss = nss;
    status->he_gi = gi;
    status->he_dcm = dcm;
    break;
case MT_PHY_TYPE_EHT_SU:    // EHT SU (802.11be, WiFi 7)
case MT_PHY_TYPE_EHT_TRIG:  // EHT trigger-based
case MT_PHY_TYPE_EHT_MU:    // EHT MU
    status->encoding = RX_ENC_EHT;
    status->nss = nss;
    status->eht.gi = gi;
    break;
}
```

---

## 4. Ring 布局对比

### 4.1 mt7925 Ring 配置

**来源**: `mt76/mt7925/pci.c:215-269`

#### TX Rings (4 数据 + 2 MCU)
```c
// mt7925_dma_init()

// 数据 TX rings (4 AC: BE/BK/VI/VO)
ret = mt76_connac_init_tx_queues(dev->phy.mt76, MT7925_TXQ_BAND0,
                                   MT7925_TX_RING_SIZE,  // 256
                                   MT_TX_RING_BASE, NULL, 0);
// 生成 TX ring 0-3 (HW qid 0-3)

// MCU WM ring
ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_WM, MT7925_TXQ_MCU_WM,
                           MT7925_TX_MCU_RING_SIZE,  // 256
                           MT_TX_RING_BASE);
// 生成 TX ring 15 (HW qid 15)

// FWDL ring
ret = mt76_init_mcu_queue(&dev->mt76, MT_MCUQ_FWDL, MT7925_TXQ_FWDL,
                           MT7925_TX_FWDL_RING_SIZE,  // 128
                           MT_TX_RING_BASE);
// 生成 TX ring 16 (HW qid 16)
```

**HW 寄存器布局**:
- TX Ring 0 BASE: `0x7c024300` (BAR0+0xd4300)
- TX Ring 1 BASE: `0x7c024310` (BAR0+0xd4310)
- ...
- TX Ring 15 BASE: `0x7c0243f0` (BAR0+0xd43f0)
- TX Ring 16 BASE: `0x7c024400` (BAR0+0xd4400)

#### RX Rings (2 rings)
```c
// RX MCU event ring (HW qid 0)
ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MCU],
                        MT7925_RXQ_MCU_WM,              // 0
                        MT7925_RX_MCU_RING_SIZE,        // 128
                        MT_RX_BUF_SIZE,                 // 2048
                        MT_RX_EVENT_RING_BASE);

// RX 数据 ring (HW qid 4)
ret = mt76_queue_alloc(dev, &dev->mt76.q_rx[MT_RXQ_MAIN],
                        MT7925_RXQ_BAND0,               // 4
                        MT7925_RX_RING_SIZE,            // 1536
                        MT_RX_BUF_SIZE,                 // 2048
                        MT_RX_DATA_RING_BASE);
```

**HW 寄存器布局**:
- RX Ring 0 BASE: `0x7c024500` (BAR0+0xd4500) — MCU event
- RX Ring 4 BASE: `0x7c024540` (BAR0+0xd4540) — 数据

### 4.2 mt7927 当前 Ring 配置

**来源**: `src/mt7927_pci.c:1414-1517`

#### TX Rings (2 rings)
```c
// TX Ring 15 (MCU WM)
ret = mt7927_tx_ring_alloc(dev, &dev->ring_wm, MT_TXQ_MCU_WM_RING,
                            MT_TXQ_MCU_WM_RING_SIZE);  // 256

// TX Ring 16 (FWDL)
ret = mt7927_tx_ring_alloc(dev, &dev->ring_fwdl, MT_TXQ_FWDL_RING,
                            MT_TXQ_FWDL_RING_SIZE);    // 128
```

**❌ 缺失**: TX ring 0-3 (数据 TX)

#### RX Rings (3 rings)
```c
// RX Ring 4 (数据)
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx4, 4,
                            MT_RXQ_DATA_RING_SIZE,  // 1536
                            MT_RX_BUF_SIZE);        // 2048

// RX Ring 6 (MCU event) ← Windows 配置
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx6, 6,
                            MT_RXQ_MCU_EVENT_RING_SIZE,  // 128
                            MT_RX_BUF_SIZE);

// RX Ring 7 (辅助)
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx7, 7,
                            MT_RXQ_AUX_RING_SIZE,  // 128
                            MT_RX_BUF_SIZE);
```

**差异分析**:
- mt7925 用 RX ring **0** 做 MCU event
- mt7927 用 RX ring **6** 做 MCU event (遵循 Windows RE)
- mt7927 额外有 RX ring 7 (用途待确认)

### 4.3 Ring 结构对比

#### mt76 框架 `mt76_queue`
**来源**: `mt76/mt76.h:233-266`

```c
struct mt76_queue {
    struct mt76_queue_regs __iomem *regs;  // HW 寄存器 (BASE/CNT/CIDX/DIDX)

    spinlock_t lock;                        // 队列锁
    spinlock_t cleanup_lock;                // 清理锁
    struct mt76_queue_entry *entry;         // 条目数组
    struct mt76_rro_desc *rro_desc;         // RRO 描述符 (可选)
    struct mt76_desc *desc;                 // DMA 描述符数组

    u16 first;                              // 第一个有效条目
    u16 head;                               // 生产者索引 (CPU 写入)
    u16 tail;                               // 消费者索引 (DMA 读取)
    u8 hw_idx;                              // HW ring 索引 (qid)
    u8 ep;                                  // USB endpoint
    int ndesc;                              // 描述符数量
    int queued;                             // 已排队数量
    int buf_size;                           // buffer 大小
    bool stopped;                           // 队列停止
    bool blocked;                           // 队列阻塞

    u8 buf_offset;                          // buffer 偏移
    u16 flags;                              // 队列标志 (WED/RRO)
    u8 magic_cnt;                           // RRO magic 计数

    __le16 *emi_cpu_idx;                    // EMI CPU 索引

    struct mtk_wed_device *wed;             // WED offload
    struct mt76_dev *dev;                   // 设备指针
    u32 wed_regs;                           // WED 寄存器基址

    dma_addr_t desc_dma;                    // 描述符 DMA 地址
    struct sk_buff *rx_head;                // RX 链表头
    struct page_pool *page_pool;            // page pool
};
```

**关键特性**:
- 使用 **mt76_desc** (4 DWORD: buf0, ctrl, buf1, info)
- 支持 WED (Wireless Ethernet Dispatch) offload
- 支持 RRO (RX Reorder Offload)
- 集成 page pool 优化内存分配
- spinlock 保护并发访问

#### mt7927 自定义 `mt7927_ring`
**来源**: `src/mt7927_pci.h:501-519`

```c
struct mt7927_ring {
    u16 qid;                     // HW ring 索引
    u16 ndesc;                   // 描述符数量
    u16 head;                    // 生产者索引
    u16 tail;                    // 消费者索引

    struct mt76_desc *desc;      // DMA 描述符数组
    dma_addr_t desc_dma;         // 描述符 DMA 地址

    void **buf;                  // RX buffer 数组 (仅 RX ring)
    dma_addr_t *buf_dma;         // RX buffer DMA 地址数组
    u32 buf_size;                // RX buffer 大小 (仅 RX ring)
};
```

**差异分析**:
- ❌ 无 spinlock (不支持并发)
- ❌ 无 entry 数组 (无 TX skb 追踪)
- ❌ 无 page pool (内存分配效率低)
- ✅ 复用 mt76_desc (兼容 mt76)
- 简化设计，仅用于 FWDL/MCU 通信

---

## 5. 中断/NAPI 方案

### 5.1 mt7925 中断处理架构

**来源**: `mt76/mt7925/pci.c:300-421`

#### 中断映射定义
```c
static const struct mt792x_irq_map irq_map = {
    .host_irq_enable = MT_WFDMA0_HOST_INT_ENA,  // 0x7c024204
    .tx = {
        .all_complete_mask = MT_INT_TX_DONE_ALL,   // 所有 TX 完成
        .mcu_complete_mask = MT_INT_TX_DONE_MCU,   // MCU TX 完成
    },
    .rx = {
        .data_complete_mask = HOST_RX_DONE_INT_ENA2,  // RX ring 4 (数据)
        .wm_complete_mask = HOST_RX_DONE_INT_ENA0,    // RX ring 0 (MCU)
    },
};
```

#### IRQ Handler 注册
```c
// mt7925_pci_probe()
ret = devm_request_irq(mdev->dev, pdev->irq, mt792x_irq_handler,
                        IRQF_SHARED, KBUILD_MODNAME, dev);

// IRQ handler: mt792x_irq_handler() (mt792x/pci.c)
static irqreturn_t mt792x_irq_handler(int irq, void *dev_instance)
{
    struct mt792x_dev *dev = dev_instance;
    struct mt76_dev *mdev = &dev->mt76;

    mt76_wr(dev, dev->irq_map->host_irq_enable, 0);  // 禁用中断

    if (!test_bit(MT76_STATE_INITIALIZED, &mdev->phy.state))
        return IRQ_NONE;

    tasklet_schedule(&mdev->irq_tasklet);  // 调度 tasklet

    return IRQ_HANDLED;
}
```

#### Tasklet 处理
```c
// Tasklet: mt792x_irq_tasklet() (mt792x/pci.c)
static void mt792x_irq_tasklet(unsigned long data)
{
    struct mt792x_dev *dev = (struct mt792x_dev *)data;
    struct mt76_dev *mdev = &dev->mt76;
    u32 intr, mask = 0;

    intr = mt76_rr(dev, MT_WFDMA0_HOST_INT_STA);  // 读中断状态
    mt76_wr(dev, MT_WFDMA0_HOST_INT_STA, intr);   // 清中断

    trace_dev_irq(mdev, intr, 0);

    mask = intr & (MT_INT_RX_DONE_ALL | MT_INT_TX_DONE_ALL);
    if (intr & dev->irq_map->rx.wm_complete_mask)
        napi_schedule(&mdev->napi[MT_RXQ_MCU]);       // 触发 RX MCU NAPI

    if (intr & dev->irq_map->rx.data_complete_mask)
        napi_schedule(&mdev->napi[MT_RXQ_MAIN]);      // 触发 RX 数据 NAPI

    if (intr & dev->irq_map->tx.all_complete_mask)
        napi_schedule(&mdev->tx_napi);                 // 触发 TX NAPI

    if (intr & MT_INT_MCU_CMD)
        wake_up(&mdev->mcu.wait);                      // MCU 命令完成

    mt76_connac_irq_enable(mdev, mask);                // 重新启用中断
}
```

#### NAPI 初始化
```c
// mt7925_dma_init()

// TX NAPI
netif_napi_add_tx(dev->mt76.tx_napi_dev, &dev->mt76.tx_napi,
                  mt792x_poll_tx);
napi_enable(&dev->mt76.tx_napi);

// RX NAPI (mt76_init_queues 自动注册)
ret = mt76_init_queues(dev, mt792x_poll_rx);  // RX poll 函数
if (ret < 0)
    return ret;

// 自动为每个 RX queue 注册 NAPI:
// mt76_for_each_q_rx(mdev, i)
//     napi_enable(&mdev->napi[i]);
```

#### NAPI Poll 函数
```c
// RX poll: mt792x_poll_rx() (mt792x/dma.c)
int mt792x_poll_rx(struct napi_struct *napi, int budget)
{
    struct mt76_dev *mdev = container_of(napi, struct mt76_dev, napi[0]);
    int qid = napi - mdev->napi;  // 获取 queue ID

    return mt76_dma_rx_poll(napi, budget);  // 调用通用 DMA poll
}

// mt76_dma_rx_poll() (mt76/dma.c:623-697)
int mt76_dma_rx_poll(struct napi_struct *napi, int budget)
{
    struct mt76_dev *dev = container_of(napi, struct mt76_dev, napi[0]);
    int qid = napi - dev->napi;
    struct mt76_queue *q = &dev->q_rx[qid];
    int cur, done = 0;
    bool more;

    rcu_read_lock();

    do {
        cur = mt76_dma_rx_process(dev, q, budget - done);
        mt76_dma_rx_fill(dev, q, false);  // 补充 RX buffer
        done += cur;
    } while (cur && done < budget);

    rcu_read_unlock();

    if (done < budget && napi_complete_done(napi, done)) {
        /* 重新启用中断 */
        dev->drv->rx_poll_complete(dev, qid);
    }

    return done;
}
```

### 5.2 mt7927 当前轮询方案

**来源**: `src/mt7927_pci.c:414-474`

#### MCU 响应轮询
```c
// mt7927_mcu_wait_resp() — 轮询 RX ring 6 读 MCU 响应
static int mt7927_mcu_wait_resp(struct mt7927_dev *dev, int timeout_ms)
{
    struct mt7927_ring *ring = &dev->ring_rx6;
    unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
    struct mt76_desc *d;
    u32 ctrl;

    do {
        d = &ring->desc[ring->tail];
        ctrl = le32_to_cpu(d->ctrl);

        if (ctrl & MT_DMA_CTL_DMA_DONE) {  // 轮询 DMA_DONE 位
            // 处理响应
            // ...

            // 清除 DMA_DONE, 重置描述符
            d->ctrl = cpu_to_le32(FIELD_PREP(MT_DMA_CTL_SD_LEN0,
                                              ring->buf_size));

            // 推进 tail, 归还 buffer
            ring->tail = (ring->tail + 1) % ring->ndesc;
            mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid), idx);

            return 0;
        }
        usleep_range(1000, 2000);  // 轮询间隔 1-2ms
    } while (time_before(jiffies, timeout));

    return -ETIMEDOUT;
}
```

**性能问题**:
- ❌ CPU 密集轮询 (1-2ms 间隔)
- ❌ 高延迟 (1-2ms 每次轮询)
- ❌ 无法处理数据帧 (仅 MCU 事件)
- ❌ 浪费电力

### 5.3 mt7927 中断改进方案

#### 目标架构
```
IRQ → mt7927_irq_handler()
   → tasklet_schedule(&dev->irq_tasklet)
      → mt7927_irq_tasklet()
         → napi_schedule(&dev->napi_rx_data)   // RX ring 4
         → napi_schedule(&dev->napi_rx_mcu)    // RX ring 6
         → napi_schedule(&dev->tx_napi)        // TX rings 0-3, 15, 16
            → mt7927_poll_rx_data() / mt7927_poll_rx_mcu() / mt7927_poll_tx()
               → 处理 DMA 描述符 → mt76_rx() / mt76_tx_complete()
```

#### 中断掩码配置
**当前**: `MT_WFDMA_INT_MASK_WIN = 0x2600f000`
- BIT(29): MCU2HOST_SW_INT (MCU 命令完成)
- BIT(25): WDT? (待确认)
- BIT(15:12): RX ring 4/5/6/7 完成

**建议增加**:
- BIT(26): TX ring 16 完成 (FWDL)
- BIT(25): TX ring 15 完成 (MCU WM)
- BIT(3:0): TX ring 0-3 完成 (数据 TX)

#### 实现步骤

**步骤 1: 注册 IRQ handler**
```c
// mt7927_pci_probe()
tasklet_init(&dev->irq_tasklet, mt7927_irq_tasklet,
             (unsigned long)dev);

ret = devm_request_irq(&pdev->dev, pdev->irq, mt7927_irq_handler,
                        IRQF_SHARED, KBUILD_MODNAME, dev);
```

**步骤 2: IRQ handler**
```c
static irqreturn_t mt7927_irq_handler(int irq, void *dev_instance)
{
    struct mt7927_dev *dev = dev_instance;

    // 禁用中断 (避免风暴)
    mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA, 0);

    tasklet_schedule(&dev->irq_tasklet);

    return IRQ_HANDLED;
}
```

**步骤 3: Tasklet 处理**
```c
static void mt7927_irq_tasklet(unsigned long data)
{
    struct mt7927_dev *dev = (struct mt7927_dev *)data;
    u32 intr;

    // 读取中断状态
    intr = mt7927_rr(dev, MT_WFDMA_HOST_INT_STA);

    // 清除中断
    mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, intr);

    // RX ring 4 (数据) 完成
    if (intr & BIT(12))
        napi_schedule(&dev->napi_rx_data);

    // RX ring 6 (MCU event) 完成
    if (intr & BIT(14))
        napi_schedule(&dev->napi_rx_mcu);

    // TX 完成 (rings 0-3, 15, 16)
    if (intr & (GENMASK(3, 0) | BIT(25) | BIT(26)))
        napi_schedule(&dev->tx_napi);

    // MCU 命令完成 (BIT(29))
    if (intr & MCU2HOST_SW_INT_STA)
        wake_up(&dev->mcu_wait);

    // 重新启用中断
    mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA, dev->int_mask);
}
```

**步骤 4: NAPI 初始化**
```c
// mt7927_pci_probe()

// TX NAPI
netif_napi_add_tx(dev->napi_dev, &dev->tx_napi,
                  mt7927_poll_tx);
napi_enable(&dev->tx_napi);

// RX 数据 NAPI
netif_napi_add(dev->napi_dev, &dev->napi_rx_data,
               mt7927_poll_rx_data);
napi_enable(&dev->napi_rx_data);

// RX MCU NAPI
netif_napi_add(dev->napi_dev, &dev->napi_rx_mcu,
               mt7927_poll_rx_mcu);
napi_enable(&dev->napi_rx_mcu);
```

**步骤 5: NAPI poll 函数**
```c
static int mt7927_poll_rx_data(struct napi_struct *napi, int budget)
{
    struct mt7927_dev *dev = container_of(napi, struct mt7927_dev,
                                           napi_rx_data);
    int done = 0;

    while (done < budget) {
        struct mt76_desc *d = &dev->ring_rx4.desc[dev->ring_rx4.tail];
        u32 ctrl = le32_to_cpu(d->ctrl);

        if (!(ctrl & MT_DMA_CTL_DMA_DONE))
            break;  // 没有更多数据

        // 处理 RX 包
        mt7927_handle_rx_data(dev, &dev->ring_rx4);

        done++;
    }

    if (done < budget) {
        napi_complete_done(napi, done);
        // 重新启用 RX ring 4 中断
        mt7927_rmw(dev, MT_WFDMA_HOST_INT_ENA, 0, BIT(12));
    }

    return done;
}
```

---

## 6. 实现方案

### 6.1 总体路线图

```
阶段 0: 基础准备 (0.5 天)
  ├─ 定义 TXD/RXD 宏 (mt7927_pci.h)
  ├─ 定义 mt7927_tx_info 结构
  └─ 规划文件组织

阶段 1: TX ring 0-3 初始化 (1 天)
  ├─ 新增 mt7927_ring ring_tx[4]
  ├─ 修改 mt7927_init_tx_rx_rings() 初始化 TX 0-3
  ├─ 实现 mt7927_tx_ring_reprogram() (CLR_OWN 后恢复)
  └─ 测试 ring 创建 (lspci, dmesg)

阶段 2: TXD 构造 (2 天)
  ├─ 实现 mt7927_mac_write_txwi_8023() (802.3 数据帧)
  ├─ 实现 mt7927_mac_write_txwi_80211() (802.11 管理帧)
  ├─ 实现 mt7927_mac_write_txwi() (顶层接口)
  └─ 实现 mt7927_tx_prepare_skb() (TX skb 准备)

阶段 3: RXD 解析 (2 天)
  ├─ 实现 mt7927_mac_fill_rx_rate() (速率信息提取)
  ├─ 实现 mt7927_mac_fill_rx() (RXD 解析主函数)
  ├─ 实现 mt7927_handle_rx_data() (RX 数据帧处理)
  └─ 实现 mt7927_handle_rx_mcu() (RX MCU 事件处理)

阶段 4: 中断/NAPI (1.5 天)
  ├─ 实现 mt7927_irq_handler()
  ├─ 实现 mt7927_irq_tasklet()
  ├─ 实现 mt7927_poll_tx()
  ├─ 实现 mt7927_poll_rx_data()
  ├─ 实现 mt7927_poll_rx_mcu()
  └─ 测试中断触发 (cat /proc/interrupts)

阶段 5: TX 数据路径 (2 天)
  ├─ 实现 mt7927_tx_queue_skb() (入队 TX skb)
  ├─ 实现 mt7927_tx_kick() (提交到硬件)
  ├─ 实现 mt7927_tx_complete() (TX 完成处理)
  └─ 集成到 mac80211 (ieee80211_ops->tx)

阶段 6: RX 数据路径 (2 天)
  ├─ 实现 mt7927_rx_poll_complete() (RX 补充 buffer)
  ├─ 集成 mt76_rx() (提交到 mac80211)
  ├─ 测试 RX 数据接收 (tcpdump)
  └─ 性能优化 (page pool, batch processing)

阶段 7: 集成测试 (1 天)
  ├─ 端到端 TX/RX 测试
  ├─ iperf3 性能测试
  ├─ 并发测试 (多 TID)
  └─ 错误处理验证

总计: **12 天** (2 周冲刺)
```

### 6.2 关键函数清单

#### 6.2.1 TX 路径函数

| 函数名 | 功能 | 参考 | 代码量 |
|--------|------|------|--------|
| `mt7927_mac_write_txwi_8023()` | 构造 802.3 TXD | mt7925/mac.c:622-654 | ~30 行 |
| `mt7927_mac_write_txwi_80211()` | 构造 802.11 TXD | mt7925/mac.c:656-721 | ~60 行 |
| `mt7927_mac_write_txwi()` | TXD 顶层接口 | mt7925/mac.c:723-836 | ~100 行 |
| `mt7927_tx_prepare_skb()` | 准备 TX skb | mt7925/pci_mac.c:8-54 | ~40 行 |
| `mt7927_tx_queue_skb()` | 入队 TX skb | mt76/dma.c:307-373 | ~60 行 |
| `mt7927_tx_kick()` | 提交到硬件 | mt76/dma.c:396-407 | ~10 行 |
| `mt7927_tx_complete()` | TX 完成处理 | mt76/dma.c:569-617 | ~40 行 |

#### 6.2.2 RX 路径函数

| 函数名 | 功能 | 参考 | 代码量 |
|--------|------|------|--------|
| `mt7927_mac_fill_rx_rate()` | 提取速率信息 | mt7925/mac.c:248-351 | ~100 行 |
| `mt7927_mac_fill_rx()` | RXD 解析主函数 | mt7925/mac.c:354-619 | ~250 行 |
| `mt7927_handle_rx_data()` | 处理 RX 数据帧 | 新增 | ~50 行 |
| `mt7927_handle_rx_mcu()` | 处理 RX MCU 事件 | 当前 mt7927_mcu_wait_resp | ~30 行 |
| `mt7927_rx_poll_complete()` | RX 补充 buffer | mt76/dma.c:169-304 | ~30 行 |
| `mt7927_queue_rx_skb()` | 提交到 mac80211 | mt7925/mac.c:1204-1252 | ~40 行 |

#### 6.2.3 中断/NAPI 函数

| 函数名 | 功能 | 参考 | 代码量 |
|--------|------|------|--------|
| `mt7927_irq_handler()` | IRQ handler | mt792x/pci.c | ~10 行 |
| `mt7927_irq_tasklet()` | Tasklet 处理 | mt792x/pci.c | ~30 行 |
| `mt7927_poll_tx()` | TX NAPI poll | mt792x/dma.c:poll_tx | ~40 行 |
| `mt7927_poll_rx_data()` | RX 数据 NAPI poll | 新增 | ~40 行 |
| `mt7927_poll_rx_mcu()` | RX MCU NAPI poll | 新增 | ~30 行 |

### 6.3 文件组织

```
src/
├── mt7927_pci.c          (主驱动, +500 行)
│   ├─ mt7927_init_tx_rx_rings() 扩展 — 初始化 TX ring 0-3
│   ├─ mt7927_irq_handler()       — IRQ handler
│   ├─ mt7927_irq_tasklet()       — Tasklet
│   └─ mt7927_pci_probe()         — 注册 NAPI, IRQ
│
├── mt7927_pci.h          (寄存器定义, +200 行)
│   ├─ TXD 宏定义 (MT_TXD0_*, MT_TXD1_*, ...)
│   ├─ RXD 宏定义 (MT_RXD0_*, MT_RXD1_*, ...)
│   └─ mt7927_tx_info 结构
│
├── mt7927_mac.c          (**新增**, ~800 行)
│   ├─ mt7927_mac_write_txwi_8023()   — 802.3 TXD
│   ├─ mt7927_mac_write_txwi_80211()  — 802.11 TXD
│   ├─ mt7927_mac_write_txwi()        — TXD 顶层
│   ├─ mt7927_mac_fill_rx_rate()      — 速率解析
│   ├─ mt7927_mac_fill_rx()           — RXD 解析
│   └─ mt7927_queue_rx_skb()          — RX 分发
│
├── mt7927_dma.c          (**新增**, ~500 行)
│   ├─ mt7927_tx_prepare_skb()        — TX skb 准备
│   ├─ mt7927_tx_queue_skb()          — TX 入队
│   ├─ mt7927_tx_kick()               — TX kick
│   ├─ mt7927_tx_complete()           — TX 完成
│   ├─ mt7927_poll_tx()               — TX NAPI poll
│   ├─ mt7927_poll_rx_data()          — RX 数据 NAPI poll
│   ├─ mt7927_poll_rx_mcu()           — RX MCU NAPI poll
│   └─ mt7927_rx_poll_complete()      — RX 补充
│
└── Kbuild                (构建脚本)
    mt7927_pci-objs += mt7927_mac.o mt7927_dma.o
```

### 6.4 需要修改的现有函数

| 函数名 | 修改内容 | 原因 |
|--------|----------|------|
| `mt7927_init_tx_rx_rings()` | 增加 TX ring 0-3 初始化 | 数据 TX 需要 |
| `mt7927_reprogram_prefetch()` | 增加 TX ring 0-3 预取配置 | CLR_OWN 后恢复 |
| `mt7927_dma_cleanup()` | 清理 TX ring 0-3 | 资源释放 |
| `mt7927_config_int_mask()` | 增加 TX/RX 数据中断掩码 | 中断处理 |
| `mt7927_mcu_wait_resp()` | 改为 NAPI 驱动 | 替换轮询 |

---

## 7. 代码量估算

### 7.1 新增代码

| 文件 | 行数 | 说明 |
|------|------|------|
| `mt7927_mac.c` | ~800 | TXD/RXD 处理主逻辑 |
| `mt7927_dma.c` | ~500 | DMA TX/RX 路径 |
| `mt7927_pci.h` | ~200 | TXD/RXD 宏定义 |
| `mt7927_pci.c` (修改) | ~500 | 中断/NAPI/ring 初始化 |

**总计新增**: **~2000 行**

### 7.2 参考代码复用度

| 来源 | 可复用度 | 说明 |
|------|----------|------|
| `mt76/mt7925/mac.c` | 70% | TXD/RXD 格式几乎一致，需适配 mt7927 结构 |
| `mt76/mt7925/pci.c` | 50% | DMA 初始化逻辑相似，需适配 ring 布局差异 |
| `mt76/dma.c` | 40% | 通用 DMA 引擎，需简化为 mt7927 专用 |
| `mt76/mt792x/*.c` | 60% | 中断/NAPI 框架可复用 |

### 7.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| **TXD 格式不兼容** | 低 | 高 | mt7925 是 CONNAC3，格式应一致 |
| **RXD GROUP 解析错误** | 中 | 中 | 参考 mt7925 GROUP_3/4 处理逻辑 |
| **中断风暴** | 中 | 高 | 正确禁用/启用中断，NAPI budget 限制 |
| **RX ring 6 vs ring 0** | 低 | 低 | mt7927 已验证 ring 6 可用 |
| **性能不达标** | 中 | 中 | page pool 优化, batch processing |

---

## 8. 下一步行动

### 8.1 立即执行 (本周)

1. **定义 TXD/RXD 宏** — 复制 mt76_connac3_mac.h 到 mt7927_pci.h
2. **初始化 TX ring 0-3** — 修改 mt7927_init_tx_rx_rings()
3. **实现 TXD 构造** — 创建 mt7927_mac.c, 实现 mt7927_mac_write_txwi()
4. **测试 ring 创建** — lspci, dmesg 验证 BASE 寄存器

### 8.2 中期目标 (下周)

1. **实现 RXD 解析** — mt7927_mac_fill_rx()
2. **中断/NAPI 集成** — mt7927_irq_handler, tasklet, poll 函数
3. **TX 数据路径** — mt7927_tx_queue_skb, mt7927_tx_complete
4. **RX 数据路径** — mt7927_handle_rx_data

### 8.3 长期目标 (下下周)

1. **mac80211 集成** — ieee80211_ops 注册
2. **扫描/连接测试** — iwlist scan, wpa_supplicant
3. **iperf3 性能测试** — 目标: >500 Mbps
4. **稳定性测试** — 长时间运行, stress test

---

**文档创建**: 2026-02-15
**作者**: Claude Code (数据路径研究 agent)
**状态**: ✅ 完成
