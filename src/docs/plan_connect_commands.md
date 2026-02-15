# MT7927 连接和密钥 MCU 命令适配方案

**基于**: mt76/mt7925 上游驱动分析
**日期**: 2026-02-15
**状态**: 研究完成，待实现

---

## 1. 连接流程的 MCU 命令序列

从 Scan 完成到连接成功的完整流程：

```
[用户触发连接]
  ↓
1. DEV_INFO_UPDATE (CID=0x01)
   - 配置虚拟接口信息 (MAC 地址、接口类型)
   - 只在首次添加 vif 时调用
   ↓
2. BSS_INFO_UPDATE (CID=0x02) - 初始配置
   - BSS_INFO_BASIC: 网络类型、BSSID、beacon 间隔
   - BSS_INFO_QOS: EDCA 参数
   - BSS_INFO_RATE: 基本速率集
   ↓
3. STA_REC_UPDATE (CID=0x03) - 添加 AP 记录
   - STA_REC_BASIC: conn_type=CONNECTION_INFRA_STA, conn_state=CONN_STATE_CONNECT
   - STA_REC_PHY: PHY 能力 (HT/VHT/HE/EHT)
   - STA_REC_HT/VHT/HE/EHT: 对应的能力 TLV
   - STA_REC_RATE: 速率控制参数
   ↓
4. BSS_INFO_UPDATE - 完整配置 (enable=true)
   - BSS_INFO_RLM: 信道配置 (center freq, bandwidth)
   - BSS_INFO_HE: HE 参数 (如果支持)
   - BSS_INFO_BSS_COLOR: BSS Color (HE)
   ↓
[mac80211 完成 4-way handshake]
  ↓
5. STA_REC_UPDATE - 安装密钥 (仅 WPA/WPA2)
   - STA_REC_KEY_V3 (tag=0x27): PTK (Pairwise Temporal Key)
   ↓
6. STA_REC_UPDATE - 端口授权
   - STA_REC_BASIC: conn_state=CONN_STATE_PORT_SECURE
   ↓
[连接成功，可以传输数据]
```

---

## 2. 最小连接集 (Open 网络)

**连接不加密的 Open AP 所需的最少命令**：

### 2.1 DEV_INFO_UPDATE (首次 vif 创建时)

```c
// CID: 0x01, option: 0x06 (SET, fire-and-forget)
struct {
    struct {
        u8 _rsv[4];
    } hdr;
    struct dev_info_active_tlv {
        __le16 tag;      // UNI_DEV_INFO_ACTIVE = 1
        __le16 len;
        u8 active;       // 1 = 激活
        u8 band_idx;
        u8 omac_idx;
        u8 _rsv;
        u8 omac_addr[6];
    } tlv;
} __packed;
```

**关键字段**:
- `active = 1`: 激活虚拟接口
- `omac_addr`: vif->addr (虚拟接口 MAC 地址)
- `omac_idx`: 通常为 0

---

### 2.2 BSS_INFO_UPDATE - 初始配置

```c
// CID: 0x02, option: 0x06
struct {
    struct bss_req_hdr {
        u8 bss_idx;
        u8 _rsv[3];
    } hdr;

    // TLV 1: BASIC (必须第一个)
    struct mt76_connac_bss_basic_tlv {
        __le16 tag;           // UNI_BSS_INFO_BASIC = 0
        __le16 len;
        __le32 network_type;  // NETWORK_INFRA = BIT(16)
        u8 active;            // 1
        u8 _rsv0;
        __le16 bcn_interval;  // 从 beacon
        u8 bssid[6];          // AP BSSID
        u8 wmm_idx;           // 0
        u8 dtim_period;       // 从 beacon
        u8 bmc_wcid_lo;       // broadcast wcid 低字节
        u8 cipher;            // CONNAC3_CIPHER_NONE = 0 (Open 网络)
        u8 phy_mode;          // 根据 AP 能力设置
        u8 max_bssid;         // 0 (不使用 MBSSID)
        u8 non_tx_bssid;      // 0
        u8 bmc_wcid_hi;       // broadcast wcid 高字节
        u8 _rsv[2];
    } __packed;

    // TLV 2: QOS (可选)
    struct mt76_connac_bss_qos_tlv qos;

    // TLV 3: RATE (可选)
    struct bss_rate_tlv rate;
} __packed;
```

**关键字段**:
- `network_type = NETWORK_INFRA` (0x10000)
- `active = 1`
- `bssid`: AP BSSID
- `cipher = CONNAC3_CIPHER_NONE` (Open 网络)

---

### 2.3 STA_REC_UPDATE - 添加 AP 记录

```c
// CID: 0x03, option: 0x06
struct {
    struct sta_req_hdr {
        u8 bss_idx;
        u8 wlan_idx_lo;      // wcid 低 8 位
        __le16 tlv_num;      // TLV 数量
        u8 is_tlv_append;    // 0
        u8 muar_idx;         // 0
        u8 wlan_idx_hi;      // wcid 高位
        u8 _rsv;
    } hdr;

    // TLV 1: STA_REC_BASIC (tag=0)
    struct sta_rec_basic {
        __le16 tag;          // STA_REC_BASIC = 0
        __le16 len;
        __le32 conn_type;    // CONNECTION_INFRA_STA = (STA_TYPE_STA | NETWORK_INFRA)
                             // = BIT(0) | BIT(16) = 0x10001
        u8 conn_state;       // CONN_STATE_CONNECT = 1
        u8 qos;              // 1 if AP supports WMM
        __le16 aid;          // Association ID 从 assoc response
        u8 peer_addr[6];     // AP MAC address
        __le16 extra_info;   // EXTRA_INFO_NEW = BIT(1)
    } __packed;

    // TLV 2: STA_REC_PHY (tag=0x15)
    struct sta_rec_phy phy;

    // TLV 3: STA_REC_HT/VHT/HE (根据 AP 能力)
    // ...

    // TLV N: STA_REC_HDR_TRANS (tag=0x2B) - 必须
    struct sta_rec_hdr_trans {
        __le16 tag;
        __le16 len;
        u8 from_ds;     // 1 (STA 模式)
        u8 to_ds;       // 1 (STA 模式)
        u8 dis_rx_hdr_tran;  // 0
        u8 _rsv;
    } __packed;
} __packed;
```

**关键字段**:
- `conn_type = CONNECTION_INFRA_STA` (0x10001)
- `conn_state = CONN_STATE_CONNECT` (1)
- `peer_addr`: AP MAC 地址

---

### 2.4 BSS_INFO_UPDATE - 完整配置

```c
// CID: 0x02, option: 0x06
// 包含所有 BSS TLV (与 2.2 类似，但 active=1, enable=true)
// 额外添加:
// - BSS_INFO_RLM: 信道配置
// - BSS_INFO_HE/BSS_INFO_BSS_COLOR (如果支持 WiFi 6)
```

**BSS_INFO_RLM** (tag=UNI_BSS_INFO_RLM):
```c
struct bss_rlm_tlv {
    __le16 tag;
    __le16 len;
    u8 control_channel;  // 主信道号
    u8 center_chan;      // 中心频率信道
    u8 center_chan2;     // 80+80 的第二中心频率
    u8 bw;               // 带宽 (0=20MHz, 1=40MHz, 2=80MHz, 3=160MHz)
    u8 tx_streams;
    u8 rx_streams;
    u8 ht_op_info;
    u8 sco;              // Secondary channel offset (1=SCA, 3=SCB)
    u8 band;             // 1=2.4GHz, 2=5GHz, 3=6GHz
    u8 pad[3];
} __packed;
```

---

### 2.5 STA_REC_UPDATE - 端口授权

```c
// CID: 0x03, option: 0x06
// 只需要更新 STA_REC_BASIC 的 conn_state
struct {
    struct sta_req_hdr hdr;
    struct sta_rec_basic {
        ...
        u8 conn_state;  // CONN_STATE_PORT_SECURE = 2
        ...
    } basic;
} __packed;
```

**Open 网络可能不需要这一步**，因为没有 4-way handshake。

---

## 3. WPA 密钥安装

### 3.1 set_key MCU 命令格式

在 4-way handshake 完成后，mac80211 会调用 `set_key()` 回调安装 PTK (Pairwise Temporal Key)。

```c
// CID: 0x03 (STA_REC_UPDATE), option: 0x06
struct {
    struct sta_req_hdr hdr;

    // STA_REC_KEY_V3 (tag=0x27)
    struct sta_rec_sec_uni {
        __le16 tag;          // STA_REC_KEY_V3 = 0x27
        __le16 len;
        u8 add;              // 1 = 添加密钥, 0 = 删除密钥
        u8 tx_key;           // 1 (STA 使用该密钥发送)
        u8 key_type;         // 1 = pairwise key
        u8 is_authenticator; // 0 (STA 不是 authenticator)
        u8 peer_addr[6];     // AP MAC address
        u8 bss_idx;
        u8 cipher_id;        // CONNAC3_CIPHER_* 枚举值
        u8 key_id;           // 通常 PTK 是 0
        u8 key_len;          // 密钥长度 (CCMP=16, TKIP=32)
        u8 wlan_idx;         // wcid
        u8 mgmt_prot;        // 0
        u8 key[32];          // 密钥内容
        u8 key_rsc[16];      // Receive Sequence Counter (通常全零)
    } __packed;
} __packed;
```

---

### 3.2 CCMP/GCMP 密钥的 TLV 格式

**CCMP (WPA2 默认加密)**:
```c
cipher_id = CONNAC3_CIPHER_AES_CCMP;  // 4
key_len = 16;
memcpy(key, ieee80211_key_conf->key, 16);
```

**GCMP (WPA3)**:
```c
cipher_id = CONNAC3_CIPHER_GCMP;  // 11
key_len = 16;
memcpy(key, ieee80211_key_conf->key, 16);
```

**TKIP (WPA1, 已弃用)**:
```c
cipher_id = CONNAC3_CIPHER_TKIP;  // 2
key_len = 32;
// TKIP 有特殊处理: Rx/Tx MIC keys 需要交换
memcpy(key, ieee80211_key_conf->key, 16);       // TK
memcpy(key + 16, ieee80211_key_conf->key + 24, 8);  // Tx MIC
memcpy(key + 24, ieee80211_key_conf->key + 16, 8);  // Rx MIC
```

**BIP (管理帧保护)**:
```c
cipher_id = CONNAC3_CIPHER_BIP_CMAC_128;  // 6
key_id = key_conf->keyidx;  // 通常是 4 或 5
key_len = 32;
// BIP 需要 sta_key_conf 存储的额外数据
memcpy(key, sta_key_conf->key, 16);
memcpy(key + 16, ieee80211_key_conf->key, 16);
```

---

### 3.3 密钥类型映射 (mt7925_mcu_get_cipher)

```c
enum connac3_mcu_cipher_type {
    CONNAC3_CIPHER_NONE = 0,
    CONNAC3_CIPHER_WEP40 = 1,
    CONNAC3_CIPHER_TKIP = 2,
    CONNAC3_CIPHER_AES_CCMP = 4,
    CONNAC3_CIPHER_WEP104 = 5,
    CONNAC3_CIPHER_BIP_CMAC_128 = 6,
    CONNAC3_CIPHER_WEP128 = 7,
    CONNAC3_CIPHER_WAPI = 8,
    CONNAC3_CIPHER_CCMP_256 = 10,
    CONNAC3_CIPHER_GCMP = 11,
    CONNAC3_CIPHER_GCMP_256 = 12,
};

// Linux WLAN_CIPHER_SUITE_* → CONNAC3_CIPHER_*
WLAN_CIPHER_SUITE_CCMP      → CONNAC3_CIPHER_AES_CCMP (4)
WLAN_CIPHER_SUITE_TKIP      → CONNAC3_CIPHER_TKIP (2)
WLAN_CIPHER_SUITE_AES_CMAC  → CONNAC3_CIPHER_BIP_CMAC_128 (6)
WLAN_CIPHER_SUITE_GCMP      → CONNAC3_CIPHER_GCMP (11)
WLAN_CIPHER_SUITE_GCMP_256  → CONNAC3_CIPHER_GCMP_256 (12)
```

---

## 4. 状态机

### 4.1 mac80211 sta_state 转换

mac80211 定义了 STA 的状态转换：

```
IEEE80211_STA_NOTEXIST  → 添加 vif 前的状态
     ↓ (add_interface)
IEEE80211_STA_NONE      → vif 已创建，STA 未添加
     ↓ (authenticate)
IEEE80211_STA_AUTH      → 认证完成 (Open 网络跳过)
     ↓ (associate)
IEEE80211_STA_ASSOC     → 关联完成 (收到 assoc response)
     ↓ (4-way handshake, 仅 WPA)
IEEE80211_STA_AUTHORIZED → 端口授权，可以传输数据
```

驱动实现 `sta_state()` 回调处理这些转换。

---

### 4.2 mt7925 sta_state 到 MCU 命令的映射

**mt76 通用框架** (mt76_sta_state):
```c
int mt76_sta_state(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
                   struct ieee80211_sta *sta,
                   enum ieee80211_sta_state old_state,
                   enum ieee80211_sta_state new_state)
{
    if (old_state == IEEE80211_STA_NOTEXIST &&
        new_state == IEEE80211_STA_NONE)
        return mt76_sta_add(phy, vif, sta);  // 分配 wcid

    if (old_state == IEEE80211_STA_NONE &&
        new_state == IEEE80211_STA_NOTEXIST)
        mt76_sta_remove(phy, vif, sta);  // 释放 wcid

    if (old_state == IEEE80211_STA_AUTH &&
        new_state == IEEE80211_STA_ASSOC)
        ev = MT76_STA_EVENT_ASSOC;

    else if (old_state == IEEE80211_STA_ASSOC &&
             new_state == IEEE80211_STA_AUTHORIZED)
        ev = MT76_STA_EVENT_AUTHORIZE;

    else if (old_state == IEEE80211_STA_ASSOC &&
             new_state == IEEE80211_STA_AUTH)
        ev = MT76_STA_EVENT_DISASSOC;

    return dev->drv->sta_event(dev, vif, sta, ev);
}
```

**mt7925 sta_event** (通过 mt7925_mcu_sta_update):
```c
// AUTH → ASSOC
MT76_STA_EVENT_ASSOC:
    STA_REC_UPDATE (enable=true, state=MT76_STA_INFO_STATE_ASSOC)
    → 发送所有 STA TLV (BASIC, PHY, HT, VHT, HE, EHT, RATE, etc.)

// ASSOC → AUTHORIZED
MT76_STA_EVENT_AUTHORIZE:
    STA_REC_UPDATE (enable=true, state=MT76_STA_INFO_STATE_ASSOC)
    → 更新 STA_REC_BASIC: conn_state=CONN_STATE_PORT_SECURE

// ASSOC → AUTH (断开连接)
MT76_STA_EVENT_DISASSOC:
    STA_REC_UPDATE (enable=false)
    → 发送 STA_REC_REMOVE TLV
```

---

### 4.3 enum mt76_sta_info_state

```c
enum mt76_sta_info_state {
    MT76_STA_INFO_STATE_NONE,    // 未连接
    MT76_STA_INFO_STATE_AUTH,    // 已认证 (目前未使用)
    MT76_STA_INFO_STATE_ASSOC    // 已关联
};
```

传递给 `mt7925_mcu_sta_cmd()` 的 `state` 参数：
- **ASSOC 事件**: `state = MT76_STA_INFO_STATE_ASSOC`
- **AUTHORIZE 事件**: `state = MT76_STA_INFO_STATE_ASSOC` (相同)
- 区别在于 `conn_state`: ASSOC 时为 `CONN_STATE_CONNECT` (1)，AUTHORIZE 时为 `CONN_STATE_PORT_SECURE` (2)

---

### 4.4 完整连接流程时序图

```
[用户空间 wpa_supplicant]
        |
        | nl80211: NL80211_CMD_CONNECT
        ↓
[mac80211 核心]
        |
        | ieee80211_ops->add_interface
        ↓
[驱动] DEV_INFO_UPDATE
        ↓
[mac80211] 发送 Probe Request (如果需要)
        ↓
[mac80211] 发送 Auth Request
        ↓
[AP] Auth Response
        ↓
[mac80211] sta_state: NONE → AUTH
        ↓
[mac80211] 发送 Assoc Request
        ↓
[AP] Assoc Response (AID)
        ↓
[mac80211] sta_state: AUTH → ASSOC
        |
        | MT76_STA_EVENT_ASSOC
        ↓
[驱动] BSS_INFO_UPDATE (初始)
       STA_REC_UPDATE (添加 AP)
       BSS_INFO_UPDATE (完整配置)
        ↓
[mac80211/wpa_supplicant] 4-way handshake (仅 WPA/WPA2)
        |
        | ieee80211_ops->set_key (PTK)
        ↓
[驱动] STA_REC_UPDATE (STA_REC_KEY_V3)
        ↓
[mac80211] sta_state: ASSOC → AUTHORIZED
        |
        | MT76_STA_EVENT_AUTHORIZE
        ↓
[驱动] STA_REC_UPDATE (conn_state=PORT_SECURE)
        ↓
[连接成功] 数据传输开始
```

---

## 5. 实现优先级

### P0: Open 网络连接 (必须)

**目标**: 能够连接不加密的 Open WiFi AP
**需要实现**:
1. `mt7927_mcu_uni_add_dev()` - DEV_INFO_UPDATE
2. `mt7927_mcu_add_bss_info()` - BSS_INFO_UPDATE (包含多个 bss_*_tlv)
3. `mt7927_mcu_sta_update()` - STA_REC_UPDATE (包含多个 sta_*_tlv)
4. `mt7927_sta_event()` - sta_state 事件处理

**关键 TLV 函数** (按优先级):
```c
// BSS TLV
mt7927_mcu_bss_basic_tlv()      // UNI_BSS_INFO_BASIC (必须)
mt7927_mcu_bss_qos_tlv()        // QOS
mt7927_mcu_bss_rate_tlv()       // RATE
mt7927_mcu_bss_rlm_tlv()        // RLM (信道配置)

// STA TLV
mt76_connac_mcu_sta_basic_tlv() // STA_REC_BASIC (可复用 mt76 通用)
mt7927_mcu_sta_phy_tlv()        // STA_REC_PHY
mt7927_mcu_sta_ht_tlv()         // STA_REC_HT
mt7927_mcu_sta_hdr_trans_tlv()  // STA_REC_HDR_TRANS (必须)
```

**代码量估算**: ~500 行
- `mt7927_mcu_uni_add_dev()`: ~50 行
- `mt7927_mcu_add_bss_info()` + TLV helpers: ~200 行
- `mt7927_mcu_sta_update()` + TLV helpers: ~200 行
- `mt7927_sta_event()`: ~50 行

---

### P1: WPA2 连接 (高优先级)

**目标**: 能够连接 WPA2-PSK 加密的 WiFi AP
**额外需要**:
1. `mt7927_set_key()` - mac80211 set_key 回调
2. `mt7927_mcu_add_key()` - 密钥安装 MCU 命令
3. `mt7927_mcu_sta_key_tlv()` - STA_REC_KEY_V3 TLV

**关键密码套件支持**:
- CCMP (AES-CCMP-128) - WPA2 默认
- GCMP (AES-GCMP-128) - WPA3 可选
- AES-CMAC (BIP) - 管理帧保护

**代码量估算**: ~200 行
- `mt7927_set_key()`: ~80 行 (处理 link_id, multilink 等)
- `mt7927_mcu_add_key()`: ~30 行
- `mt7927_mcu_sta_key_tlv()`: ~90 行

---

### P2: 高级特性 (中优先级)

**WiFi 6 (HE) 支持**:
- `mt7927_mcu_sta_he_tlv()` - STA_REC_HE
- `mt7927_mcu_bss_he_tlv()` - BSS_INFO_HE
- `mt7927_mcu_bss_color_tlv()` - BSS_INFO_BSS_COLOR

**WiFi 7 (EHT) 支持**:
- `mt7927_mcu_sta_eht_tlv()` - STA_REC_EHT
- `mt7927_mcu_bss_eht_tlv()` - BSS_INFO_EHT

**VHT 支持** (5GHz):
- `mt7927_mcu_sta_vht_tlv()` - STA_REC_VHT

**代码量估算**: ~300 行

---

### P3: 可选增强 (低优先级)

- WPA3 (SAE)
- TKIP (已弃用，向后兼容)
- WEP (严重不安全，不建议实现)
- 802.11w (管理帧保护 MFP)
- Fast roaming (802.11r)
- Multi-link operation (MLO, WiFi 7)

---

## 6. 实现策略

### 6.1 代码复用

**可以直接复用 mt76 通用代码**:
- `mt76_connac_mcu_sta_basic_tlv()` - STA_REC_BASIC
- `mt76_connac_mcu_sta_uapsd()` - STA_REC_UAPSD
- `mt76_connac_mcu_add_tlv()` - TLV 添加辅助函数

**需要适配的部分**:
- PHY TLV: MT6639/MT7927 的 PHY 参数可能与 MT7925 不同
- RLM TLV: 信道配置需要验证寄存器映射
- KEY TLV: CONNAC3 格式已经兼容

---

### 6.2 测试计划

**P0 测试** (Open 网络):
1. 连接 Open WiFi AP
2. 检查 `iw dev wlan0 link` 显示连接状态
3. `ping` 测试基本数据传输
4. `iperf3` 吞吐量测试

**P1 测试** (WPA2):
1. 连接 WPA2-PSK AP
2. 验证 4-way handshake 完成
3. 检查密钥安装 (dmesg 日志)
4. 长时间连接稳定性测试

**P2 测试** (HE/VHT):
1. 连接 WiFi 6 AP (HE)
2. 检查协商的 MCS
3. 160MHz 带宽测试 (如果支持)

---

## 7. 关键注意事项

### 7.1 TLV 顺序

**BSS_INFO_UPDATE**:
- `BSS_INFO_BASIC` **必须是第一个 TLV**
- 其他 TLV 顺序不重要

**STA_REC_UPDATE**:
- 没有严格顺序要求
- 但建议 `STA_REC_BASIC` 在前

---

### 7.2 wcid (Wireless Client ID) 分配

- 每个 STA (包括 AP) 需要一个唯一的 `wcid`
- `wcid` 范围: 0 ~ 127 (MT7927 具体范围待确认)
- Broadcast/Multicast 使用专用 `wcid` (通常是 127 或 0)

**STA 模式 wcid 分配**:
```c
// vif 自己的 wcid (用于发送单播帧)
mvif->sta.wcid.idx = 0;

// AP 的 wcid (用于接收 AP 发来的帧)
link_sta->wcid.idx = 1;

// Broadcast wcid
mvif->sta.deflink.wcid.idx = 127;
```

---

### 7.3 bss_idx

- 每个 vif 有一个 `bss_idx`
- 范围通常 0 ~ 15
- STA 模式通常使用 `bss_idx = 0`

---

### 7.4 option 字段

```c
// 查询命令 (需要 MCU 响应)
option = 0x07;  // BIT(0)=ACK | BIT(1)=UNI | BIT(2)=need_response

// SET 命令 (fire-and-forget, 不等待响应)
option = 0x06;  // BIT(0)=ACK | BIT(1)=UNI
```

**连接命令全部使用 `option=0x06`** (fire-and-forget)，不需要等待 MCU 响应。

---

### 7.5 字节序

- **所有多字节字段使用 little-endian**
- 使用 `cpu_to_le16()`, `cpu_to_le32()` 转换

---

## 8. 参考函数清单

### mt7925 关键函数

| 函数名 | 文件 | 功能 |
|--------|------|------|
| `mt7925_mcu_add_bss_info()` | mcu.c:2826 | BSS_INFO_UPDATE 主函数 |
| `mt7925_mcu_sta_update()` | mcu.c:2012 | STA_REC_UPDATE 主函数 |
| `mt7925_mcu_sta_cmd()` | mcu.c:1958 | STA 命令封装 |
| `mt7925_mcu_add_key()` | mcu.c:1274 | 密钥安装 |
| `mt7925_mcu_sta_key_tlv()` | mcu.c:1191 | STA_REC_KEY_V3 TLV |
| `mt7925_set_key()` | main.c:666 | mac80211 set_key 回调 |
| `mt76_sta_state()` | mac80211.c:1635 | mac80211 sta_state 回调 |
| `mt76_connac_mcu_sta_basic_tlv()` | mt76_connac_mcu.c | STA_REC_BASIC TLV (通用) |

---

## 9. 下一步行动

### 立即可做

1. **实现 P0** (Open 网络连接):
   - 创建 `mt7927_mcu_uni_add_dev()`
   - 创建 `mt7927_mcu_add_bss_info()` 及其 TLV helpers
   - 创建 `mt7927_mcu_sta_update()` 及其 TLV helpers
   - 注册 `sta_state` 回调到 `ieee80211_ops`

2. **测试 P0**:
   - 使用 Open WiFi AP 测试连接
   - 验证能否获取 IP (DHCP)
   - 验证能否 ping 网关

### 后续计划

3. **实现 P1** (WPA2):
   - 添加 `mt7927_set_key()` 和 `mt7927_mcu_add_key()`
   - 测试 WPA2-PSK 连接

4. **实现 P2** (HE/VHT):
   - 添加 WiFi 6 支持的 TLV
   - 测试高速率连接

---

## 10. 总结

**连接功能核心**:
- **3 个主要 MCU 命令**: DEV_INFO_UPDATE, BSS_INFO_UPDATE, STA_REC_UPDATE
- **2 个关键回调**: `sta_state`, `set_key`
- **~700 行代码** 即可实现 Open + WPA2 连接

**优势**:
- MT7927 已有 UniCmd 通信基础 ✅
- 可以大量复用 mt76/mt7925 代码 ✅
- TLV 格式与 mt7925 高度兼容 ✅

**风险**:
- PHY 参数可能需要调整 (MT6639 vs MT7925)
- RLM (信道配置) 可能需要额外的寄存器操作
- 需要实际硬件测试验证

---

**文档版本**: 1.0
**作者**: Agent C (连接/密钥命令研究)
**完成时间**: 2026-02-15
