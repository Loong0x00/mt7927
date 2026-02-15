# MT7927 扫描命令适配方案

**基于**: mt76/mt7925 上游驱动分析
**目标**: 为 mt7927 实现 WiFi 扫描功能
**日期**: 2026-02-15

---

## 1. 扫描请求 UniCmd 格式

### 1.1 命令 ID

```c
#define MCU_UNI_CMD_SCAN_REQ  0x16
```

**来源**: `mt76/mt76_connac_mcu.h:1285`

### 1.2 完整消息结构

扫描请求由 **1 个固定头部 + 多个 TLV** 组成：

```
+---------------------------------------+
| scan_hdr_tlv (4 字节)                 |  固定头
+---------------------------------------+
| TLV: UNI_SCAN_REQ (必需)              |  扫描参数
+---------------------------------------+
| TLV: UNI_SCAN_SSID (必需)             |  SSID 列表
+---------------------------------------+
| TLV: UNI_SCAN_BSSID (必需)            |  BSSID 过滤
+---------------------------------------+
| TLV: UNI_SCAN_CHANNEL (必需)          |  信道列表
+---------------------------------------+
| TLV: UNI_SCAN_MISC (可选)             |  随机 MAC
+---------------------------------------+
| TLV: UNI_SCAN_IE (可选)               |  Probe Request IEs
+---------------------------------------+
```

### 1.3 数据结构定义

#### 1.3.1 scan_hdr_tlv — 固定头部 (4 字节)

```c
struct scan_hdr_tlv {
    u8 seq_num;   // 扫描序列号 (0-127) | band_idx << 7
    u8 bss_idx;   // BSS 索引 (通常是 0)
    u8 pad[2];    // 填充
} __packed;
```

#### 1.3.2 UNI_SCAN_REQ (tag=1) — 扫描参数

```c
struct scan_req_tlv {
    __le16 tag;                        // = 1 (UNI_SCAN_REQ)
    __le16 len;                        // sizeof(scan_req_tlv) - 4

    u8 scan_type;                      // 0=PASSIVE, 1=ACTIVE
    u8 probe_req_num;                  // Probe Request 次数 (ACTIVE 时用 2)
    u8 scan_func;                      // BIT(0)=随机MAC, BIT(2)=DBDC扫描
    u8 src_mask;                       // 保留，通常为 0
    __le16 channel_min_dwell_time;     // 最小驻留时间 (ms)，通常为 0
    __le16 channel_dwell_time;         // 信道驻留时间 (ms)，通常为 0 (固件默认)
    __le16 timeout_value;              // 超时时间 (ms)，通常为 0
    __le16 probe_delay_time;           // Probe Request 延迟 (ms)，通常为 0
    __le32 func_mask_ext;              // 扩展功能掩码，通常为 0
} __packed;
```

**关键值**:
- `scan_type`: 0=被动扫描 (无 SSID)，1=主动扫描 (有 SSID)
- `probe_req_num`: 主动扫描时每个信道发送的 Probe Request 次数，mt7925 用 2
- `scan_func`: 常见值
  - `BIT(0)` = 使用随机 MAC 地址
  - `BIT(2)` = DBDC 扫描模式 (双频并发)
  - mt7925 默认设置 `BIT(2)` (`SCAN_FUNC_SPLIT_SCAN`)

#### 1.3.3 UNI_SCAN_SSID (tag=10) — SSID 列表

```c
struct scan_ssid_tlv {
    __le16 tag;                        // = 10 (UNI_SCAN_SSID)
    __le16 len;

    u8 ssid_type;                      // 见下方说明
    u8 ssids_num;                      // SSID 个数 (0-10)
    u8 is_short_ssid;                  // 0=完整SSID, 1=短SSID (保留)
    u8 pad;
    struct mt76_connac_mcu_scan_ssid ssids[10];  // SSID 数组
} __packed;

// SSID 结构 (36 字节)
struct mt76_connac_mcu_scan_ssid {
    __le32 ssid_len;                   // SSID 长度 (0-32)
    u8 ssid[32];                       // SSID 数据 (IEEE80211_MAX_SSID_LEN)
} __packed;
```

**ssid_type 值**:
- `BIT(0)` = 通配 SSID (被动扫描，ssids_num=0)
- `BIT(1)` = P2P 通配 SSID
- `BIT(2)` = 指定 SSID + 通配 SSID (主动扫描，ssids_num>0)

**mt7925 逻辑**:
- 被动扫描: `ssid_type = BIT(0)`, `ssids_num = 0`
- 主动扫描: `ssid_type = BIT(2)`, `ssids_num = n` (1-10)

#### 1.3.4 UNI_SCAN_BSSID (tag=11) — BSSID 过滤

```c
struct scan_bssid_tlv {
    __le16 tag;                        // = 11 (UNI_SCAN_BSSID)
    __le16 len;

    u8 bssid[6];                       // 目标 BSSID (ff:ff:ff:ff:ff:ff=全部)
    u8 match_ch;                       // 匹配信道 (6 GHz RNR 扫描用)
    u8 match_ssid_ind;                 // 匹配 SSID 索引 (0xff=不匹配)
    u8 rcpi;                           // RCPI 阈值 (保留)
    u8 match_short_ssid_ind;           // 短 SSID 索引 (保留)
    u8 pad[2];
} __packed;
```

**最简配置** (全部 BSSID):
```c
bssid = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
match_ch = 0;
match_ssid_ind = 10;  // MT7925_RNR_SCAN_MAX_BSSIDS
match_short_ssid_ind = 10;
```

#### 1.3.5 UNI_SCAN_CHANNEL (tag=12) — 信道列表

```c
struct scan_chan_info_tlv {
    __le16 tag;                        // = 12 (UNI_SCAN_CHANNEL)
    __le16 len;

    u8 channel_type;                   // 见下方说明
    u8 channels_num;                   // 信道个数 (channel_type=4 时有效)
    u8 pad[2];
    struct mt76_connac_mcu_scan_channel channels[64];
} __packed;

// 信道结构 (2 字节)
struct mt76_connac_mcu_scan_channel {
    u8 band;                           // 1=2.4GHz, 2=5GHz, 3=6GHz
    u8 channel_num;                    // 信道号 (如 1, 6, 36, 149)
} __packed;
```

**channel_type 值**:
- `0` = 全频段扫描
- `1` = 仅 2.4 GHz
- `2` = 仅 5 GHz
- `3` = P2P 社交信道 (1, 6, 11)
- `4` = **指定信道列表** (mt7925 默认值)

**示例**: 扫描 2.4GHz 信道 1, 6, 11:
```c
channel_type = 4;
channels_num = 3;
channels[0] = {.band = 1, .channel_num = 1};
channels[1] = {.band = 1, .channel_num = 6};
channels[2] = {.band = 1, .channel_num = 11};
```

#### 1.3.6 UNI_SCAN_MISC (tag=13) — 随机 MAC (可选)

```c
struct scan_misc_tlv {
    __le16 tag;                        // = 13 (UNI_SCAN_MISC)
    __le16 len;

    u8 random_mac[6];                  // 随机 MAC 地址
    u8 rsv[2];
} __packed;
```

**仅在 `scan_req_tlv.scan_func & BIT(0)` 时添加此 TLV。**

#### 1.3.7 UNI_SCAN_IE (tag=14) — Probe Request IEs (可选)

```c
struct scan_ie_tlv {
    __le16 tag;                        // = 14 (UNI_SCAN_IE)
    __le16 len;

    __le16 ies_len;                    // IE 总长度
    u8 band;                           // 0=所有频段, 1=2.4GHz, 2=5GHz
    u8 pad;
    u8 ies[];                          // IE 数据 (变长)
} __packed;
```

**mt7925 实现**: 从 `ieee80211_scan_request->ies` 复制，最大长度 `MT76_CONNAC_SCAN_IE_LEN` (600 字节)。

### 1.4 最小扫描请求示例 (被动扫描全部信道)

```c
// 1. 固定头部
struct scan_hdr_tlv hdr = {
    .seq_num = 1,   // 序列号 1, band_idx=0
    .bss_idx = 0,
};

// 2. TLV: UNI_SCAN_REQ
struct scan_req_tlv req = {
    .tag = cpu_to_le16(1),
    .len = cpu_to_le16(sizeof(req) - 4),
    .scan_type = 0,         // 被动扫描
    .probe_req_num = 0,
    .scan_func = BIT(2),    // SPLIT_SCAN
    // 其余字段为 0
};

// 3. TLV: UNI_SCAN_SSID (被动扫描，无 SSID)
struct scan_ssid_tlv ssid = {
    .tag = cpu_to_le16(10),
    .len = cpu_to_le16(sizeof(ssid) - 4),
    .ssid_type = BIT(0),    // 通配 SSID
    .ssids_num = 0,
};

// 4. TLV: UNI_SCAN_BSSID (全部 BSSID)
struct scan_bssid_tlv bssid = {
    .tag = cpu_to_le16(11),
    .len = cpu_to_le16(sizeof(bssid) - 4),
    .bssid = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .match_ssid_ind = 10,
    .match_short_ssid_ind = 10,
};

// 5. TLV: UNI_SCAN_CHANNEL (全频段)
struct scan_chan_info_tlv chan_info = {
    .tag = cpu_to_le16(12),
    .len = cpu_to_le16(sizeof(chan_info) - 4),
    .channel_type = 0,      // 全频段
    .channels_num = 0,
};

// 6. 组合成 skb，发送命令
skb = mt76_mcu_msg_alloc(...);
skb_put_data(skb, &hdr, sizeof(hdr));
skb_put_data(skb, &req, sizeof(req));
skb_put_data(skb, &ssid, sizeof(ssid));
skb_put_data(skb, &bssid, sizeof(bssid));
skb_put_data(skb, &chan_info, sizeof(chan_info));

// 发送 (mt7925 实现)
mt76_mcu_skb_send_msg(mdev, skb, MCU_UNI_CMD(SCAN_REQ), true);
```

---

## 2. 扫描结果事件格式

### 2.1 事件 ID

```c
#define MCU_UNI_EVENT_SCAN_DONE  0x0e
```

**来源**: `mt76/mt76_connac_mcu.h:1060`

### 2.2 接收路径

1. **RX Ring**: 扫描事件通过 **RX Ring 6** (MCU 事件 ring) 接收
2. **数据包类型**: `PKT_TYPE_EVENT` (由 RXD 标识)
3. **事件头**: `mt7925_mcu_rxd` (48 字节)

```c
struct mt7925_mcu_rxd {
    __le32 rxd[8];           // +0x00: RX descriptor (32 字节)
    __le16 len;              // +0x20: 数据长度
    __le16 pkt_type_id;      // +0x22: 包类型
    u8 eid;                  // +0x24: 事件 ID (0x0e = SCAN_DONE)
    u8 seq;                  // +0x25: 序列号
    u8 option;               // +0x26: 选项 (含 MCU_UNI_CMD_UNSOLICITED_EVENT)
    u8 __rsv;                // +0x27
    u8 ext_eid;              // +0x28
    u8 __rsv1[2];            // +0x29-0x2a
    u8 s2d_index;            // +0x2b
    u8 tlv[];                // +0x2c: TLV 数据
} __packed;
```

### 2.3 事件处理流程

```c
// 1. mac.c RX 处理路径
void mt7925_queue_rx_skb(struct mt76_dev *mdev, enum mt76_rxq_id q,
                         struct sk_buff *skb, u32 *info)
{
    // ...
    case PKT_TYPE_EVENT:
        mt7925_mcu_rx_event(dev, skb);  // 调用事件处理
        break;
}

// 2. mcu.c 事件分发
void mt7925_mcu_rx_event(struct mt792x_dev *dev, struct sk_buff *skb)
{
    struct mt7925_mcu_rxd *rxd = (struct mt7925_mcu_rxd *)skb->data;

    if (rxd->option & MCU_UNI_CMD_UNSOLICITED_EVENT) {
        // 异步事件分发
        switch (rxd->eid) {
        case MCU_UNI_EVENT_SCAN_DONE:
            mt7925_mcu_scan_event(dev, skb);  // 扫描事件处理
            return;  // 注意：不 free skb，由 scan_work 处理
        // ...
        }
    }
    // 同步响应事件
    mt76_mcu_rx_event(&dev->mt76, skb);
}

// 3. 扫描事件入队
static void mt7925_mcu_scan_event(struct mt792x_dev *dev, struct sk_buff *skb)
{
    struct mt792x_phy *phy = &dev->phy;

    spin_lock_bh(&dev->mt76.lock);
    __skb_queue_tail(&phy->scan_event_list, skb);  // 入队到 scan_event_list
    spin_unlock_bh(&dev->mt76.lock);

    // 触发 scan_work (延迟 120 秒超时)
    ieee80211_queue_delayed_work(mphy->hw, &phy->scan_work,
                                 MT792x_HW_SCAN_TIMEOUT);
}
```

### 2.4 扫描事件 TLV 格式

事件数据从 `rxd + sizeof(mt7925_mcu_rxd) + 4` 开始，包含多个 TLV:

```
+---------------------------------------+
| mt7925_mcu_rxd (48 字节)              |  事件头
+---------------------------------------+
| 4 字节 padding                         |
+---------------------------------------+
| TLV: UNI_EVENT_SCAN_DONE_BASIC (可选) |  扫描完成通知
+---------------------------------------+
| TLV: UNI_EVENT_SCAN_DONE_CHNLINFO (可选) | 信道信息
+---------------------------------------+
| TLV: UNI_EVENT_SCAN_DONE_NLO (可选)   |  PNO 扫描结果
+---------------------------------------+
```

#### 2.4.1 UNI_EVENT_SCAN_DONE_BASIC (tag=0)

```c
// 仅有 TLV 头，无额外数据
struct tlv {
    __le16 tag;   // = 0 (UNI_EVENT_SCAN_DONE_BASIC)
    __le16 len;   // = 4
} __packed;
```

**含义**: 扫描已完成，通知 mac80211。

**处理逻辑**:
```c
case UNI_EVENT_SCAN_DONE_BASIC:
    if (test_and_clear_bit(MT76_HW_SCANNING, &phy->mt76->state)) {
        struct cfg80211_scan_info info = { .aborted = false };
        ieee80211_scan_completed(phy->mt76->hw, &info);
    }
    break;
```

#### 2.4.2 UNI_EVENT_SCAN_DONE_CHNLINFO (tag=2)

```c
struct mt7925_mcu_scan_chinfo_event {
    u8 nr_chan;      // 扫描到的信道数 (保留)
    u8 alpha2[3];    // 国家代码 (如 "CN", "US")
} __packed;
```

**含义**: 固件报告检测到的监管域 (regulatory domain)。

**处理逻辑** (可选):
```c
case UNI_EVENT_SCAN_DONE_CHNLINFO:
    evt = (struct mt7925_mcu_scan_chinfo_event *)tlv->data;
    mt7925_regd_change(phy, evt->alpha2);  // 更新监管域
    break;
```

#### 2.4.3 UNI_EVENT_SCAN_DONE_NLO (tag=3)

```c
// 仅有 TLV 头，无额外数据
```

**含义**: PNO (Network List Offload，定期扫描) 匹配到网络。

**处理逻辑**:
```c
case UNI_EVENT_SCAN_DONE_NLO:
    ieee80211_sched_scan_results(phy->mt76->hw);  // 通知 cfg80211
    break;
```

### 2.5 扫描结果获取

**关键**: 扫描事件 **不直接包含** beacon/probe response 数据！

固件会将扫描到的 AP 信息直接上报给 mac80211 框架，通过 **RX Ring 4** (数据 ring) 接收 beacon/probe response 帧。驱动的 `mt7925_mac_fill_rx()` 函数会将这些帧传递给 `ieee80211_rx_napi()`，由 mac80211 更新扫描结果缓存。

**因此**: 驱动无需手动解析扫描结果，只需：
1. 发送扫描请求
2. 接收 `UNI_EVENT_SCAN_DONE_BASIC` 事件
3. 调用 `ieee80211_scan_completed()` 通知 mac80211

---

## 3. 与现有 UniCmd 的兼容性

### 3.1 CID 值

| 命令 | mt7925 CID | mt7927 适配 |
|------|-----------|-----------|
| 扫描请求 | `0x16` | **直接使用** |
| 扫描事件 | `0x0e` | **直接使用** |

**结论**: CID 值属于 CONNAC3 通用值，**无需转换**。

### 3.2 TXD 格式兼容性

#### 现有 UniCmd TXD (mt7927_mcu_send_unicmd)

```c
// mt7927_pci.c 现有格式
struct txd {
    __le32 txd[8];   // +0x00: TXD 0-7 (32 字节)
    // TXD[0]: total_len | Q_IDX(0x20) | PKT_FMT(2)
    // TXD[1]: flags | HDR_FORMAT_V3(1)

    __le16 len;      // +0x20: plen + 16
    __le16 cid;      // +0x22: 命令 class
    u8 rsv[2];       // +0x24-0x25
    u8 pkt_type;     // +0x26: 0xa0
    u8 seq;          // +0x27: 序列号
    u8 rsv2[2];      // +0x28-0x29
    u8 s2d_index;    // +0x2a: 0
    u8 option;       // +0x2b: 0x07 (QUERY) 或 0x06 (SET)
    u8 data[];       // +0x2c: payload
} __packed;
```

#### mt7925 scan 发送方式

```c
// mt7925/mcu.c:3066
mt76_mcu_skb_send_msg(mdev, skb, MCU_UNI_CMD(SCAN_REQ), true);

// 展开宏
#define MCU_UNI_CMD(SCAN_REQ)  (__MCU_CMD_FIELD_UNI | \
                                FIELD_PREP(__MCU_CMD_FIELD_ID, 0x16))
```

**关键**: mt7925 使用 **mt76 通用 MCU 发送函数**，会自动构建 UniCmd TXD 头部。

### 3.3 兼容性结论

| 项目 | 状态 | 说明 |
|------|------|------|
| **CID** | ✅ 兼容 | 0x16 / 0x0e 直接使用 |
| **TXD 格式** | ✅ 兼容 | **但需要调整** option 值 |
| **Payload** | ✅ 兼容 | TLV 格式通用 |

**唯一调整**: 扫描请求是 **SET 命令** (fire-and-forget，无同步响应)，需要使用:

```c
option = 0x06;  // UNI_CMD_OPT_SET (非 0x07 QUERY)
```

**原因**: 扫描是异步操作，固件会在完成时发送 `MCU_UNI_EVENT_SCAN_DONE` 事件，而不是同步响应。如果用 `option=0x07`，会导致 `mt7927_mcu_send_unicmd()` 阻塞等待不存在的同步响应。

---

## 4. 实现方案

### 4.1 需要新增的函数

#### 核心扫描函数 (3 个)

```c
// 1. 发送扫描请求
int mt7927_mcu_hw_scan(struct mt76_phy *phy,
                       struct ieee80211_vif *vif,
                       struct ieee80211_scan_request *scan_req);

// 2. 取消扫描
int mt7927_mcu_cancel_hw_scan(struct mt76_phy *phy,
                              struct ieee80211_vif *vif);

// 3. 扫描事件处理
static void mt7927_mcu_scan_event(struct mt7927_dev *dev,
                                  struct sk_buff *skb);
```

#### TLV 构建辅助函数 (可选，简化代码)

```c
// 通用 TLV 添加函数 (参考 mt76_connac_mcu_add_tlv)
static struct tlv *
mt7927_mcu_add_tlv(struct sk_buff *skb, u16 tag, u16 len)
{
    struct tlv *tlv = (struct tlv *)skb_put(skb, len);
    tlv->tag = cpu_to_le16(tag);
    tlv->len = cpu_to_le16(len);
    memset(tlv + 1, 0, len - sizeof(*tlv));  // 清零 payload
    return tlv;
}
```

### 4.2 需要新增的数据结构

**文件**: `src/mt7927_pci.h`

```c
// ========== 扫描 TLV 枚举 ==========
enum {
    UNI_SCAN_REQ = 1,
    UNI_SCAN_CANCEL = 2,
    UNI_SCAN_SSID = 10,
    UNI_SCAN_BSSID = 11,
    UNI_SCAN_CHANNEL = 12,
    UNI_SCAN_IE = 14,
    UNI_SCAN_MISC = 13,
};

enum {
    UNI_EVENT_SCAN_DONE_BASIC = 0,
    UNI_EVENT_SCAN_DONE_CHNLINFO = 2,
    UNI_EVENT_SCAN_DONE_NLO = 3,
};

// ========== 扫描请求结构 ==========
#define MT7927_SCAN_MAX_SSIDS       10
#define MT7927_SCAN_MAX_CHANNELS    64
#define MT7927_SCAN_IE_LEN          600

struct scan_hdr_tlv {
    u8 seq_num;
    u8 bss_idx;
    u8 pad[2];
} __packed;

struct scan_req_tlv {
    __le16 tag;
    __le16 len;
    u8 scan_type;
    u8 probe_req_num;
    u8 scan_func;
    u8 src_mask;
    __le16 channel_min_dwell_time;
    __le16 channel_dwell_time;
    __le16 timeout_value;
    __le16 probe_delay_time;
    __le32 func_mask_ext;
} __packed;

struct scan_ssid {
    __le32 ssid_len;
    u8 ssid[32];
} __packed;

struct scan_ssid_tlv {
    __le16 tag;
    __le16 len;
    u8 ssid_type;
    u8 ssids_num;
    u8 is_short_ssid;
    u8 pad;
    struct scan_ssid ssids[MT7927_SCAN_MAX_SSIDS];
} __packed;

struct scan_bssid_tlv {
    __le16 tag;
    __le16 len;
    u8 bssid[6];
    u8 match_ch;
    u8 match_ssid_ind;
    u8 rcpi;
    u8 match_short_ssid_ind;
    u8 pad[2];
} __packed;

struct scan_channel {
    u8 band;         // 1=2.4GHz, 2=5GHz, 3=6GHz
    u8 channel_num;
} __packed;

struct scan_chan_info_tlv {
    __le16 tag;
    __le16 len;
    u8 channel_type;
    u8 channels_num;
    u8 pad[2];
    struct scan_channel channels[MT7927_SCAN_MAX_CHANNELS];
} __packed;

struct scan_misc_tlv {
    __le16 tag;
    __le16 len;
    u8 random_mac[6];
    u8 rsv[2];
} __packed;

struct scan_ie_tlv {
    __le16 tag;
    __le16 len;
    __le16 ies_len;
    u8 band;
    u8 pad;
    u8 ies[];  // 变长
} __packed;

// ========== 扫描事件结构 ==========
struct mt7927_mcu_scan_chinfo_event {
    u8 nr_chan;
    u8 alpha2[3];
} __packed;

// ========== 驱动状态扩展 ==========
struct mt7927_dev {
    // ... 现有字段 ...

    // 新增扫描相关
    struct sk_buff_head scan_event_list;   // 扫描事件队列
    struct delayed_work scan_work;         // 扫描工作队列
    u8 scan_seq_num;                       // 扫描序列号
    unsigned long scan_state;              // BIT(0)=MT7927_SCANNING
};

#define MT7927_SCANNING  0
```

### 4.3 代码量估算

| 文件 | 新增代码 | 说明 |
|------|----------|------|
| `mt7927_pci.h` | ~200 行 | 结构体定义 |
| `mt7927_pci.c` - 扫描请求 | ~150 行 | `mt7927_mcu_hw_scan()` + TLV 构建 |
| `mt7927_pci.c` - 扫描取消 | ~30 行 | `mt7927_mcu_cancel_hw_scan()` |
| `mt7927_pci.c` - 事件处理 | ~80 行 | `mt7927_mcu_scan_event()` + scan_work |
| `mt7927_pci.c` - 初始化 | ~10 行 | 初始化 scan_event_list 和 scan_work |
| **总计** | **~470 行** | 不含空行和注释 |

**复杂度**: 中等
**参考代码**: `mt76/mt7925/mcu.c:2945-3246` (扫描请求)，`mt76/mt7925/main.c:1330-1377` (事件处理)

### 4.4 实现步骤

#### 阶段 1: 数据结构定义 (P0)

1. 在 `mt7927_pci.h` 添加所有扫描相关结构体
2. 在 `struct mt7927_dev` 添加 `scan_event_list`, `scan_work`, `scan_seq_num`
3. 在 `probe()` 初始化扫描相关字段:
   ```c
   skb_queue_head_init(&dev->scan_event_list);
   INIT_DELAYED_WORK(&dev->scan_work, mt7927_scan_work);
   dev->scan_seq_num = 0;
   ```

#### 阶段 2: 扫描请求实现 (P1)

1. 实现 `mt7927_mcu_hw_scan()`:
   - 分配 skb
   - 构建 `scan_hdr_tlv`
   - 添加 TLV: `UNI_SCAN_REQ`, `UNI_SCAN_SSID`, `UNI_SCAN_BSSID`, `UNI_SCAN_CHANNEL`
   - 可选: `UNI_SCAN_MISC` (随机 MAC), `UNI_SCAN_IE` (Probe Request IEs)
   - **调整现有 `mt7927_mcu_send_unicmd()` 支持 SET 命令**:
     ```c
     // 新增参数: is_query (bool)
     int mt7927_mcu_send_unicmd(dev, cid, payload, plen, bool is_query)
     {
         // ...
         txd->option = is_query ? 0x07 : 0x06;  // QUERY vs SET
         // ...
     }
     ```
   - 发送命令:
     ```c
     mt7927_mcu_send_unicmd(dev, 0x16, skb->data, skb->len, false);  // SET
     ```

2. 实现 `mt7927_mcu_cancel_hw_scan()`:
   - 构建 `scan_hdr + UNI_SCAN_CANCEL TLV`
   - 发送命令 (CID=0x16, option=0x06)

#### 阶段 3: 事件处理实现 (P2)

1. 修改 `mt7927_mcu_rx_event()` (或类似函数)，添加事件分发:
   ```c
   case MCU_UNI_EVENT_SCAN_DONE:
       mt7927_mcu_scan_event(dev, skb);
       return;  // 注意：不 free skb
   ```

2. 实现 `mt7927_mcu_scan_event()`:
   - 将 skb 入队到 `scan_event_list`
   - 触发 `scan_work` 延迟工作

3. 实现 `mt7927_scan_work()`:
   - 从 `scan_event_list` 取出 skb
   - 解析 TLV (跳过 rxd + 4 字节)
   - 处理 `UNI_EVENT_SCAN_DONE_BASIC` → 调用 `ieee80211_scan_completed()`
   - 处理 `UNI_EVENT_SCAN_DONE_CHNLINFO` → 可选，更新监管域
   - 释放 skb

#### 阶段 4: mac80211 集成 (P3，依赖 mac80211 注册完成)

1. 在 `ieee80211_ops` 添加回调:
   ```c
   static const struct ieee80211_ops mt7927_ops = {
       // ...
       .hw_scan = mt7927_hw_scan,
       .cancel_hw_scan = mt7927_cancel_hw_scan,
   };
   ```

2. 实现 `mt7927_hw_scan()` (包装函数):
   ```c
   static int mt7927_hw_scan(struct ieee80211_hw *hw,
                             struct ieee80211_vif *vif,
                             struct ieee80211_scan_request *req)
   {
       struct mt7927_dev *dev = hw->priv;
       struct mt76_phy *mphy = &dev->mphy;

       mutex_lock(&dev->mt76.mutex);
       int err = mt7927_mcu_hw_scan(mphy, vif, req);
       mutex_unlock(&dev->mt76.mutex);

       return err;
   }
   ```

### 4.5 测试验证

#### 最小测试 (无需 mac80211 注册)

```bash
# 1. 加载驱动
sudo insmod src/mt7927_pci.ko

# 2. 发送最小扫描请求 (调试代码)
# 在 probe() 末尾添加测试代码:
mt7927_test_scan(dev);

# 3. 检查 dmesg
sudo dmesg | grep -E "scan|SCAN_DONE"
# 预期输出:
# [  10.123] mt7927_pci: scan request sent, seq=1
# [  12.456] mt7927_pci: scan event received, eid=0x0e
# [  12.457] mt7927_pci: UNI_EVENT_SCAN_DONE_BASIC
```

#### 完整测试 (需要 mac80211 注册)

```bash
# 1. 使用 iw 触发扫描
sudo iw dev wlan0 scan

# 2. 查看扫描结果
sudo iw dev wlan0 scan dump

# 3. 检查固件日志
sudo dmesg | tail -50
```

---

## 5. 关键差异与注意事项

### 5.1 与现有 UniCmd 的差异

| 项目 | NIC_CAP/Config (0x8a/0x02) | SCAN_REQ (0x16) |
|------|---------------------------|-----------------|
| **类型** | QUERY (同步响应) | SET (异步事件) |
| **option** | `0x07` (need_response) | `0x06` (fire-and-forget) |
| **payload** | 简单 TLV (0-16 字节) | 复杂 TLV 嵌套 (~300-600 字节) |
| **响应方式** | 同步 DMA 响应 (RX6) | 异步事件 (0x0e, RX6) |
| **超时处理** | 阻塞等待 10ms | 事件队列 + 延迟工作 120s |

### 5.2 固件行为假设

1. **CID 兼容性**: 假设 MT6639 固件支持 CONNAC3 标准 CID 0x16
   - **验证方法**: 发送最小扫描请求，检查是否收到 0x0e 事件
   - **风险**: 如果固件使用不同 CID，需要查 Windows 驱动或固件符号表

2. **TLV 解析**: 假设固件严格按照 TLV 格式解析
   - **验证方法**: 对比 Windows 驱动的扫描请求 payload (通过 USB 抓包)
   - **风险**: TLV 顺序错误或缺失必需 TLV 可能导致固件忽略请求

3. **事件通知**: 假设固件会发送 `UNI_EVENT_SCAN_DONE_BASIC` 事件
   - **验证方法**: 监控 RX Ring 6 (MCU 事件 ring) 是否收到 eid=0x0e 的数据包
   - **风险**: 如果固件不发送事件，扫描会超时 (需要添加轮询机制)

### 5.3 调试建议

1. **分阶段验证**:
   - 阶段 1: 发送最小扫描请求 (被动扫描，全频段)，检查固件是否接受
   - 阶段 2: 监控 RX Ring 6，确认是否收到 0x0e 事件
   - 阶段 3: 解析事件 TLV，验证 `UNI_EVENT_SCAN_DONE_BASIC` 存在
   - 阶段 4: 检查 RX Ring 4 是否收到 beacon/probe response 帧

2. **日志增强**:
   ```c
   // 在发送扫描请求时
   dev_info(dev->dev, "scan: type=%d, ssids=%d, channels=%d\n",
            req->scan_type, ssid->ssids_num, chan_info->channels_num);

   // 在接收事件时
   dev_info(dev->dev, "scan event: eid=0x%02x, len=%d\n",
            rxd->eid, rxd->len);

   // 在解析 TLV 时
   dev_info(dev->dev, "scan tlv: tag=0x%04x, len=%d\n",
            le16_to_cpu(tlv->tag), le16_to_cpu(tlv->len));
   ```

3. **Wireshark 抓包** (如果固件支持 USB 调试接口):
   - 过滤: `usb.device_address == <device_addr> && usb.endpoint_address == 0x06`
   - 查找: CID=0x16 的 TX 包 + eid=0x0e 的 RX 包

---

## 6. 总结

### 6.1 可行性评估

| 项目 | 评估 | 说明 |
|------|------|------|
| **CID 兼容性** | ✅ 高 | CONNAC3 标准 CID，mt7925 已验证 |
| **TLV 格式** | ✅ 高 | 参考 mt7925 直接复用结构体 |
| **固件支持** | ⚠️ 中 | 需要验证 MT6639 固件是否实现扫描功能 |
| **mac80211 集成** | ✅ 高 | 标准 `ieee80211_ops` 回调 |
| **总体风险** | ⚠️ 中等 | 主要风险在固件兼容性 |

### 6.2 与现有代码的集成难度

- **低**: 扫描命令使用现有 `mt7927_mcu_send_unicmd()` (需小幅修改支持 SET 模式)
- **低**: 事件处理复用现有 RX Ring 6 路径
- **中**: 需要新增 200+ 行结构体定义
- **中**: 需要新增 ~300 行扫描逻辑代码

### 6.3 下一步建议

1. **立即实施**: 阶段 1 (结构体定义) + 阶段 2 (扫描请求)
2. **并行开发**: 阶段 3 (事件处理) 与 mac80211 注册研究同步
3. **延后集成**: 阶段 4 (mac80211 回调) 依赖 mac80211 注册完成

**优先级**: P1 (高) — 扫描是 WiFi 核心功能，是连接的前提。

---

**文档版本**: v1.0
**作者**: Scan Command Research Agent
**参考代码**: mt76/mt7925 (Linux 6.11+)
