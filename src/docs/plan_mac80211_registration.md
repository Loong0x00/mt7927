# MT7927 mac80211 注册适配方案

**研究目标**: 从 mt7925 驱动中提取 mac80211 注册架构，适配到 mt7927 standalone 驱动

**参考文件**:
- `mt76/mt7925/init.c` - 注册流程
- `mt76/mt7925/main.c` - ieee80211_ops 实现
- `mt76/mt792x_core.c` - mt792x_init_wiphy()
- `mt76/mac80211.c` - mt76_register_device() 封装

---

## 1. 最小可行 ieee80211_ops

### 1.1 让 wlan0 出现的最小集（6 个必需回调）

```c
const struct ieee80211_ops mt7927_ops = {
	/* === 核心必需 (6) === */
	.tx                = mt7927_tx,              // [P0] 发送数据帧 (~50 行)
	.start             = mt7927_start,           // [P0] 启动硬件 (~20 行)
	.stop              = mt7927_stop,            // [P0] 停止硬件 (~10 行)
	.add_interface     = mt7927_add_interface,   // [P0] 添加 vif (~30 行)
	.remove_interface  = mt7927_remove_interface,// [P0] 删除 vif (~10 行)
	.config            = mt7927_config,          // [P0] 配置参数 (~20 行)
};
```

**实现复杂度**:
- **P0 (最高优先级)**: 上述 6 个，总代码量 ~140 行（可从 mt7925 直接适配）
- 这 6 个回调足以让 `wlan0` 出现，但无法扫描和连接

---

### 1.2 添加扫描功能（再增加 3 个）

```c
const struct ieee80211_ops mt7927_ops = {
	/* ... 前面 6 个 ... */

	/* === 扫描功能 (3) === */
	.hw_scan           = mt7927_hw_scan,         // [P1] 硬件扫描 (~40 行)
	.cancel_hw_scan    = mt7927_cancel_hw_scan,  // [P1] 取消扫描 (~10 行)
	.configure_filter  = mt7927_configure_filter,// [P1] RX 过滤器 (~20 行)
};
```

**实现复杂度**:
- **P1 (高优先级)**: 3 个回调，总代码量 ~70 行
- 依赖 MCU 扫描命令（需要同步研究 Agent B 的输出）

---

### 1.3 添加连接功能（再增加 5 个）

```c
const struct ieee80211_ops mt7927_ops = {
	/* ... 前面 9 个 ... */

	/* === 连接/认证 (5) === */
	.sta_state         = mt7927_sta_state,       // [P2] STA 状态机 (~30 行, mt76 通用)
	.set_key           = mt7927_set_key,         // [P2] 密钥管理 (~60 行)
	.conf_tx           = mt7927_conf_tx,         // [P2] QoS 参数 (~15 行)
	.link_info_changed = mt7927_link_info_changed,// [P2] BSS 信息变更 (~80 行)
	.vif_cfg_changed   = mt7927_vif_cfg_changed, // [P2] VIF 配置变更 (~20 行)
};
```

**实现复杂度**:
- **P2 (中优先级)**: 5 个回调，总代码量 ~205 行
- 依赖 MCU 连接命令（需要同步研究 Agent C 的输出）

---

### 1.4 完整功能回调（可选/延后）

```c
const struct ieee80211_ops mt7927_ops = {
	/* ... 前面 14 个 ... */

	/* === 可选功能 (可延后) === */
	.ampdu_action      = mt7927_ampdu_action,    // [P3] AMPDU 聚合
	.set_rts_threshold = mt7927_set_rts_threshold,
	.get_stats         = mt7927_get_stats,       // [P3] 统计信息
	.get_txpower       = mt76_get_txpower,       // [P3] 发射功率
	.wake_tx_queue     = mt76_wake_tx_queue,     // [P3] TX queue 管理
	.sta_statistics    = mt7927_sta_statistics,  // [P3] STA 统计
	.flush             = mt7927_flush,           // [P3] 刷新队列

	/* === 高级功能 (延后) === */
	.start_ap / .stop_ap                         // [P4] AP 模式
	.suspend / .resume                            // [P4] 电源管理
	.remain_on_channel / .cancel_remain_on_channel // [P4] P2P ROC
	.add_chanctx / .remove_chanctx / ...         // [P4] 多信道上下文
};
```

**mt7925 完整 ops 有 64 个回调**，但上述 14 个 (P0-P2) 已足够实现基本 STA 连接功能。

---

## 2. 数据结构变更

### 2.1 mt7927_dev 必须新增的字段

参考 `mt792x_dev` (mt792x.h:219)，mt7927_dev 需要增加以下字段：

```c
struct mt7927_dev {
	/* === 已有字段 (现在的驱动) === */
	struct pci_dev *pdev;
	void __iomem *bar0;
	// ... 现有的 DMA ring 等 ...

	/* === 新增字段 (mac80211 必需) === */

	/* [P0] 核心 mac80211 对象 */
	struct ieee80211_hw *hw;         // mac80211 硬件抽象
	struct mt7927_phy phy;           // PHY 层信息（频段/能力）

	/* [P0] VIF/STA 管理 */
	u64 vif_mask;                    // VIF 分配位图 (最多 4 个接口)
	u64 omac_mask;                   // OMAC 地址分配位图
	struct mt7927_vif *vif_list[4];  // VIF 指针数组

	/* [P0] WCID (无线客户端 ID) 管理 */
	struct mt7927_wcid global_wcid;  // 广播帧的 WCID
	struct mt7927_wcid *wcid[20];    // WCID 池 (MT792x_WTBL_SIZE=20)

	/* [P1] 工作队列和状态 */
	struct work_struct init_work;    // 异步初始化工作
	struct delayed_work mac_work;    // MAC 层定时任务
	struct delayed_work scan_work;   // 扫描工作队列
	wait_queue_head_t wait;          // 等待队列

	/* [P2] 能力和特性 */
	u8 fw_features;                  // 固件特性标志 (CNM, ...)
	bool hw_init_done;               // 硬件初始化完成标志

	/* [P3] 电源管理 (可延后) */
	// struct mt76_connac_pm pm;
};
```

---

### 2.2 新增 struct mt7927_phy

参考 `mt792x_phy` (mt792x.h:154)：

```c
struct mt7927_phy {
	struct ieee80211_hw *hw;         // 指向 ieee80211_hw
	struct mt7927_dev *dev;          // 指向父设备

	/* [P0] 频段能力 */
	struct {
		bool has_2ghz;
		bool has_5ghz;
		bool has_6ghz;               // 可选
	} cap;

	/* [P0] 频段信息 */
	struct mt76_sband sband_2g;      // 2.4GHz 频段
	struct mt76_sband sband_5g;      // 5GHz 频段
	struct mt76_sband sband_6g;      // 6GHz (可选)

	/* [P0] 天线配置 */
	u8 antenna_mask;                 // 天线掩码 (e.g. 0x03 = 2x2)
	u16 chainmask;                   // 链路掩码

	/* [P1] 扫描支持 */
	struct sk_buff_head scan_event_list; // 扫描事件队列

	/* [P2] 其他能力 */
	u64 chip_cap;                    // 芯片能力位图
	u16 eml_cap;                     // EML (增强型 MLO) 能力
};
```

**对比 mt792x_dev 可省略的字段**:
- `pm` (电源管理) - 初期可以全部注释掉
- `coredump` - 调试用，初期可省略
- `ipv6_ns_work` - IPv6 NS offload，初期可省略
- `clc[]` (Country Limit Configuration) - 初期用固定区域

---

### 2.3 新增 struct mt7927_vif

参考 `mt792x_vif` (mt792x.h:138)：

```c
struct mt7927_vif {
	struct mt76_vif mt76;            // mt76 通用 VIF 信息
	struct mt7927_phy *phy;          // 指向 PHY
	struct mt7927_sta sta;           // 内嵌的 STA (用于 STA 模式)

	/* [P2] BSS 配置 */
	struct ewma_rssi rssi;           // RSSI EWMA 滤波器
	struct ieee80211_tx_queue_params queue_params[4]; // QoS 参数
};
```

---

### 2.4 新增 struct mt7927_wcid / mt7927_sta

```c
struct mt7927_wcid {
	u16 idx;                         // WCID 索引 (0-19)
	u8 hw_key_idx;                   // 硬件密钥索引
	u32 tx_info;                     // TX 信息标志
	struct ieee80211_sta *sta;       // 关联的 STA
};

struct mt7927_sta {
	struct mt7927_wcid wcid;         // WCID (必须是第一个字段)
	struct mt7927_vif *vif;          // 所属 VIF

	/* [P3] 速率控制/统计 (可延后) */
	// int ack_signal;
	// unsigned long airtime[8];
};
```

---

## 3. 频段/能力声明

### 3.1 2.4GHz 频段配置

从 `mt76/mac80211.c:30` 复制：

```c
static const struct ieee80211_channel mt7927_channels_2ghz[] = {
	CHAN2G(1, 2412),  CHAN2G(2, 2417),  CHAN2G(3, 2422),  CHAN2G(4, 2427),
	CHAN2G(5, 2432),  CHAN2G(6, 2437),  CHAN2G(7, 2442),  CHAN2G(8, 2447),
	CHAN2G(9, 2452),  CHAN2G(10, 2457), CHAN2G(11, 2462), CHAN2G(12, 2467),
	CHAN2G(13, 2472), CHAN2G(14, 2484), // 总共 14 个通道
};

#define CHAN2G(chan, freq) \
	{ .band = NL80211_BAND_2GHZ, .center_freq = (freq), \
	  .hw_value = (chan), .max_power = 20 }
```

**HT Capabilities (2.4GHz)**:

```c
dev->phy.sband_2g.sband.ht_cap.cap =
	IEEE80211_HT_CAP_SGI_20 |            // Short GI @ 20MHz
	IEEE80211_HT_CAP_SGI_40 |            // Short GI @ 40MHz
	IEEE80211_HT_CAP_TX_STBC |           // TX STBC
	IEEE80211_HT_CAP_RX_STBC |           // RX STBC
	IEEE80211_HT_CAP_LDPC_CODING |       // LDPC
	IEEE80211_HT_CAP_MAX_AMSDU;          // 7935 字节 AMSDU

dev->phy.sband_2g.sband.ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_2; // 4 μs
dev->phy.sband_2g.sband.ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
```

---

### 3.2 5GHz 频段配置

从 `mt76/mac80211.c:47` 复制：

```c
static const struct ieee80211_channel mt7927_channels_5ghz[] = {
	/* UNII-1 */
	CHAN5G(36, 5180),  CHAN5G(40, 5200),  CHAN5G(44, 5220),  CHAN5G(48, 5240),
	/* UNII-2A */
	CHAN5G(52, 5260),  CHAN5G(56, 5280),  CHAN5G(60, 5300),  CHAN5G(64, 5320),
	/* UNII-2C */
	CHAN5G(100, 5500), CHAN5G(104, 5520), CHAN5G(108, 5540), CHAN5G(112, 5560),
	CHAN5G(116, 5580), CHAN5G(120, 5600), CHAN5G(124, 5620), CHAN5G(128, 5640),
	CHAN5G(132, 5660), CHAN5G(136, 5680), CHAN5G(140, 5700), CHAN5G(144, 5720),
	/* UNII-3 */
	CHAN5G(149, 5745), CHAN5G(153, 5765), CHAN5G(157, 5785), CHAN5G(161, 5805),
	CHAN5G(165, 5825), CHAN5G(169, 5845), CHAN5G(173, 5865), CHAN5G(177, 5885),
	// 总共 28 个通道
};

#define CHAN5G(chan, freq) \
	{ .band = NL80211_BAND_5GHZ, .center_freq = (freq), \
	  .hw_value = (chan), .max_power = 20 }
```

**VHT Capabilities (5GHz)**:

```c
dev->phy.sband_5g.sband.vht_cap.cap =
	IEEE80211_VHT_CAP_SHORT_GI_80 |                  // Short GI @ 80MHz
	IEEE80211_VHT_CAP_SHORT_GI_160 |                 // Short GI @ 160MHz
	IEEE80211_VHT_CAP_TXSTBC |                       // TX STBC
	IEEE80211_VHT_CAP_RXSTBC_1 |                     // RX STBC 1 stream
	IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |        // Max MPDU 11454
	IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK| // Max AMPDU (1MB)
	IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE |        // SU Beamformee
	IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE |        // MU Beamformee
	FIELD_PREP(IEEE80211_VHT_CAP_BEAMFORMEE_STS_MASK, 3) | // 3 STS
	IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;        // 160MHz 支持

dev->phy.sband_5g.sband.vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(0xfffa); // MCS 0-9, 2SS
dev->phy.sband_5g.sband.vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(0xfffa);
```

---

### 3.3 HE Capabilities (WiFi 6)

从 `mt76/mt7925/main.c:16` 的 `mt7925_init_he_caps()` 适配：

```c
/* 2.4GHz HE 能力 */
he_cap_elem->phy_cap_info[0] =
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G |
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_2G; // STA 模式

/* 5GHz HE 能力 */
he_cap_elem->phy_cap_info[0] =
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_5G;

/* 公共 HE 能力 */
he_cap_elem->mac_cap_info[0] = IEEE80211_HE_MAC_CAP0_HTC_HE;
he_cap_elem->mac_cap_info[3] =
	IEEE80211_HE_MAC_CAP3_OMI_CONTROL |
	IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_EXT_3; // Max AMPDU (16MB)

he_cap_elem->phy_cap_info[1] = IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD;
he_cap_elem->phy_cap_info[2] =
	IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
	IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
	IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ;

/* HE MCS/NSS (假设 2x2, MCS 0-11) */
he_mcs->rx_mcs_80 = cpu_to_le16(0xfffa);  // MCS 0-9
he_mcs->tx_mcs_80 = cpu_to_le16(0xfffa);
```

**代码来源**: 直接从 `mt7925_init_he_caps()` 复制 ~100 行

---

### 3.4 EHT Capabilities (WiFi 7) - 可选

从 `mt76/mt7925/main.c:161` 的 `mt7925_init_eht_caps()` 适配：

```c
/* 仅在固件支持 EHT 时启用 */
if (dev->has_eht) {
	eht_cap->has_eht = true;
	eht_cap_elem->mac_cap_info[0] =
		IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS |
		IEEE80211_EHT_MAC_CAP0_OM_CONTROL;

	eht_cap_elem->phy_cap_info[0] =
		IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
		IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
		IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE;

	/* EHT MCS/NSS (2x2, MCS 0-13) */
	u8 val = (nss << 0) | (nss << 4); // RX/TX NSS
	eht_nss->bw._80.rx_tx_mcs9_max_nss = val;
	eht_nss->bw._80.rx_tx_mcs11_max_nss = val;
	eht_nss->bw._80.rx_tx_mcs13_max_nss = val;
}
```

**代码来源**: 直接从 `mt7925_init_eht_caps()` 复制 ~80 行

**注意**: EHT 支持可以先跳过，优先保证 HT/VHT/HE 工作。

---

## 4. 集成点

### 4.1 现有 probe 流程

```c
mt7927_pci_probe()
	→ pci_enable_device
	→ ioremap BAR0
	→ SET_OWN
	→ mt7927_mcu_init_mt6639()         // MCU 初始化
	→ CLR_OWN
	→ mt7927_wpdma_config()            // WFDMA GLO_CFG
	→ mt7927_init_tx_rx_rings()        // TX15/16, RX4/6/7
	→ mt7927_fw_download()             // FWDL (patch + 6 RAM)
	→ mt7927_post_fw_init()            // PostFwDownloadInit MCU 命令
	→ [END]
```

---

### 4.2 新增 mac80211 注册流程

在 `mt7927_post_fw_init()` **成功后**，增加以下步骤：

```c
mt7927_pci_probe()
	→ ... (前面流程不变) ...
	→ mt7927_post_fw_init()            // ← 现有的最后一步

	/* === 新增部分 === */
	→ mt7927_alloc_device()            // [新增] 分配 ieee80211_hw
	→ mt7927_register_device()         // [新增] 注册到 mac80211
	→ return 0;

/* 新增函数 1: 分配 ieee80211_hw */
static int mt7927_alloc_device(struct mt7927_dev *dev)
{
	struct ieee80211_hw *hw;
	int ret;

	/* 1. 分配 ieee80211_hw (包含私有数据 mt7927_phy) */
	hw = ieee80211_alloc_hw(sizeof(struct mt7927_phy), &mt7927_ops);
	if (!hw)
		return -ENOMEM;

	dev->hw = hw;
	dev->phy.hw = hw;
	dev->phy.dev = dev;
	SET_IEEE80211_DEV(hw, &dev->pdev->dev);

	/* 2. 初始化工作队列 */
	INIT_WORK(&dev->init_work, mt7927_init_work);
	INIT_DELAYED_WORK(&dev->mac_work, mt7927_mac_work);
	INIT_DELAYED_WORK(&dev->scan_work, mt7927_scan_work);
	init_waitqueue_head(&dev->wait);
	skb_queue_head_init(&dev->phy.scan_event_list);

	/* 3. 设置芯片能力 */
	dev->phy.cap.has_2ghz = true;
	dev->phy.cap.has_5ghz = true;
	dev->phy.cap.has_6ghz = false;  // 可选
	dev->phy.antenna_mask = 0x03;   // 2x2 MIMO
	dev->phy.chainmask = 0x03;

	return 0;
}

/* 新增函数 2: 注册到 mac80211 */
static int mt7927_register_device(struct mt7927_dev *dev)
{
	struct ieee80211_hw *hw = dev->hw;
	int ret;

	/* 1. 初始化 wiphy 参数 */
	ret = mt7927_init_wiphy(hw);
	if (ret)
		return ret;

	/* 2. 设置频段能力 (HT caps) */
	mt7927_init_2g_caps(dev);    // 设置 2.4GHz HT caps
	mt7927_init_5g_caps(dev);    // 设置 5GHz VHT caps

	/* 3. 设置 HE/EHT 能力 (异步在 init_work 中) */
	queue_work(system_wq, &dev->init_work);

	return 0;
}

/* 新增函数 3: 异步初始化工作 (仿照 mt7925_init_work) */
static void mt7927_init_work(struct work_struct *work)
{
	struct mt7927_dev *dev = container_of(work, struct mt7927_dev, init_work);
	struct ieee80211_hw *hw = dev->hw;
	int ret;

	/* 1. 设置 HE/EHT 能力 (需要天线配置) */
	mt7927_set_stream_he_eht_caps(&dev->phy);

	/* 2. 注册到 mac80211 */
	ret = ieee80211_register_hw(hw);
	if (ret) {
		dev_err(dev->dev, "ieee80211_register_hw failed: %d\n", ret);
		return;
	}

	/* 3. 标记初始化完成 */
	dev->hw_init_done = true;
	dev_info(dev->dev, "wlan0 registered successfully\n");
}
```

**关键点**:
- 使用 **work queue** 异步注册，避免阻塞 probe
- HE/EHT caps 需要在天线配置后设置（因为依赖 `antenna_mask`）
- 仿照 mt7925 的 `mt7925_init_work()` 流程

---

### 4.3 初始化顺序图

```
mt7927_pci_probe()
    ↓
[硬件初始化]
    ├─ PCIe 初始化
    ├─ SET_OWN → CLR_OWN
    ├─ WFDMA 配置
    ├─ TX/RX rings
    ↓
[固件启动]
    ├─ FWDL (patch + RAM)
    ├─ PostFwDownloadInit
    ↓
[mac80211 注册] ← 新增
    ├─ mt7927_alloc_device()
    │   ├─ ieee80211_alloc_hw()
    │   ├─ 初始化工作队列
    │   └─ 设置芯片能力
    │
    ├─ mt7927_register_device()
    │   ├─ mt7927_init_wiphy()
    │   ├─ 设置 2.4G/5G HT/VHT caps
    │   └─ queue_work(init_work)
    │
    └─ [异步] mt7927_init_work()
        ├─ 设置 HE/EHT caps
        ├─ ieee80211_register_hw()  ← wlan0 出现
        └─ 标记 hw_init_done = true
```

---

## 5. 代码量估算

### 5.1 分阶段实现代码量

| 阶段 | 功能 | 新增代码 | 复用代码 | 总工作量 |
|------|------|----------|----------|----------|
| **P0** | wlan0 出现 | ~200 行 | ~300 行 (mt7925 直接抄) | ~500 行 |
| **P1** | 硬件扫描 | ~100 行 | ~150 行 | ~250 行 |
| **P2** | 连接/加密 | ~150 行 | ~250 行 | ~400 行 |
| **P3** | AMPDU/统计 | ~100 行 | ~200 行 | ~300 行 |
| **总计** | 完整 STA 功能 | ~550 行 | ~900 行 | ~1450 行 |

---

### 5.2 各部分代码详细估算

#### 5.2.1 数据结构定义 (~150 行)

```c
// src/mt7927_pci.h 新增
struct mt7927_phy { ... };           // ~30 行
struct mt7927_vif { ... };           // ~20 行
struct mt7927_sta { ... };           // ~15 行
struct mt7927_wcid { ... };          // ~10 行
频段/通道定义 (channels_2g/5g)        // ~50 行
HT/VHT/HE/EHT caps 结构体             // ~30 行
```

---

#### 5.2.2 ieee80211_ops 回调实现 (~400 行)

```c
// P0: 核心回调 (~140 行)
mt7927_tx()                          // ~50 行 (调用现有 TX ring)
mt7927_start()                       // ~20 行 (MCU 命令)
mt7927_stop()                        // ~10 行
mt7927_add_interface()               // ~30 行 (分配 VIF/WCID)
mt7927_remove_interface()            // ~10 行
mt7927_config()                      // ~20 行 (信道切换)

// P1: 扫描回调 (~70 行)
mt7927_hw_scan()                     // ~40 行 (MCU scan cmd)
mt7927_cancel_hw_scan()              // ~10 行
mt7927_configure_filter()            // ~20 行

// P2: 连接回调 (~205 行)
mt7927_sta_state()                   // ~30 行 (复用 mt76_sta_state)
mt7927_set_key()                     // ~60 行 (MCU key cmd)
mt7927_conf_tx()                     // ~15 行
mt7927_link_info_changed()           // ~80 行 (BSS 信息)
mt7927_vif_cfg_changed()             // ~20 行
```

**可直接从 mt7925 复制的函数**:
- `mt7927_tx()` ← `mt792x_tx()` (mt792x_core.c)
- `mt7927_sta_state()` ← `mt76_sta_state()` (mt76/mac80211.c, 通用实现)
- `mt7927_configure_filter()` ← `mt7925_configure_filter()` (main.c:678)

---

#### 5.2.3 wiphy 初始化 (~200 行)

```c
mt7927_init_wiphy()                  // ~100 行 (从 mt792x_init_wiphy 复制)
mt7927_init_2g_caps()                // ~50 行 (HT caps)
mt7927_init_5g_caps()                // ~50 行 (VHT caps)
```

---

#### 5.2.4 HE/EHT 能力设置 (~250 行)

```c
mt7927_init_he_caps()                // ~100 行 (从 mt7925_init_he_caps 复制)
mt7927_init_eht_caps()               // ~80 行 (从 mt7925_init_eht_caps 复制)
mt7927_set_stream_he_eht_caps()      // ~70 行 (设置到 sband)
```

**可 100% 复制**: 从 `mt76/mt7925/main.c` 的 `mt7925_init_he_caps()` 等函数直接复制，只需改函数名。

---

#### 5.2.5 注册流程代码 (~150 行)

```c
mt7927_alloc_device()                // ~50 行
mt7927_register_device()             // ~30 行
mt7927_init_work()                   // ~20 行
mt7927_mac_work()                    // ~30 行 (周期性任务)
mt7927_scan_work()                   // ~20 行 (扫描结果处理)
```

---

#### 5.2.6 辅助函数 (~200 行)

```c
mt7927_wcid_alloc/free()             // ~30 行 (WCID 分配)
mt7927_vif_alloc/free()              // ~30 行 (VIF 分配)
mt7927_config_mac_addr()             // ~20 行 (MAC 地址配置)
mt7927_update_channel()              // ~40 行 (信道更新)
mt7927_tx_queue_mapping()            // ~30 行 (TX queue 映射)
mt7927_rx_poll_complete()            // ~20 行 (RX 轮询完成)
其他辅助函数                           // ~30 行
```

---

### 5.3 可直接复制的代码比例

| 来源 | 可复制代码 | 需修改 | 比例 |
|------|-----------|--------|------|
| **mt76/mt7925/** | ~700 行 | ~100 行 | 85% 可直接复制 |
| **mt76/mt792x_core.c** | ~200 行 | ~50 行 | 80% 可直接复制 |
| **mt76/mac80211.c** | ~100 行 | ~20 行 | 80% 可直接复制 (mt76 通用代码) |
| **新编写** | 0 行 | ~450 行 | 需要自己写 |

**总代码量**: ~1450 行，其中 ~900 行可从 mt7925 复制，~450 行需要适配修改。

---

## 6. 关键注意事项

### 6.1 与现有驱动的集成点

**现有驱动已有的功能**:
- ✅ DMA TX/RX rings (TX15/16, RX4/6/7)
- ✅ MCU 命令框架 (`mt7927_mcu_send_unicmd()`)
- ✅ 中断处理 (RX6 接收 MCU 响应)

**需要适配的部分**:
- ❌ TX 数据帧格式 (现在只有 MCU 命令的 TXD)
- ❌ RX 数据帧解析 (现在只解析 MCU 事件)
- ❌ WCID/VIF 管理 (现在没有这些概念)

---

### 6.2 TX 数据帧 TXD 格式

**现有 MCU UniCmd TXD** (0x30 字节):
```
+0x00  TXD[0-7]: 通用 TXD (0x30 字节)
+0x20  UniCmd 内部头 (16 字节)
+0x30  Payload
```

**数据帧 TXD 格式** (需新增):
```
+0x00  TXD[0-7]: 通用 TXD (0x30 字节)
+0x30  HW TXP (可选，hardware TX descriptor)
+0x xx  802.11 数据帧
```

**参考**: `mt76/mt7925/mac.c` 的 `mt7925_mac_write_txwi()`

---

### 6.3 RX 数据帧解析

**现有 RX 处理** (仅 MCU 事件):
```c
mt7927_irq_handler()
    → 检测到 RX6 中断
    → mt7927_rx_poll()  // 只解析 MCU 事件
```

**需要新增** (数据帧路径):
```c
mt7927_irq_handler()
    → 检测到 RX4 中断  // 数据帧
    → mt7927_rx_data()
        → 解析 RXD
        → 构造 sk_buff
        → ieee80211_rx_napi(skb)  // 上报给 mac80211
```

**参考**: `mt76/mt7925/mac.c` 的 `mt7925_queue_rx_skb()`

---

### 6.4 频段检测

MT7927 的频段支持需要从 **EEPROM** 或固件查询：

```c
// 从 NIC_CAP MCU 响应中获取
ret = mt7927_mcu_get_nic_capability(dev);
if (ret == 0) {
	dev->phy.cap.has_2ghz = !!(cap & MT_NIC_CAP_2GHZ);
	dev->phy.cap.has_5ghz = !!(cap & MT_NIC_CAP_5GHZ);
	dev->phy.cap.has_6ghz = !!(cap & MT_NIC_CAP_6GHZ);
}
```

**备选方案**: 初期可硬编码 `has_2ghz=true, has_5ghz=true, has_6ghz=false`

---

### 6.5 天线配置

MT7927 的天线数量从 **ChipID** 或 NIC_CAP 响应获取：

```c
// 从 EEPROM 或 NIC_CAP 查询
u8 nss = mt7927_get_nss(dev);  // 返回 1 或 2
dev->phy.antenna_mask = (1 << nss) - 1;  // 0x01 (1x1) or 0x03 (2x2)
dev->phy.chainmask = dev->phy.antenna_mask;
```

**备选方案**: 初期可硬编码为 `2x2 MIMO (antenna_mask = 0x03)`

---

## 7. 实现优先级建议

### Phase 1: 让 wlan0 出现 (P0)

**目标**: `ip link` 能看到 `wlan0`，但无法 scan/connect

**步骤**:
1. 新增数据结构 (~150 行)
2. 实现 6 个核心 ieee80211_ops 回调 (~140 行)
3. 实现 wiphy 初始化 + 2.4G/5G caps (~200 行)
4. 修改 probe 流程，增加 mac80211 注册 (~100 行)

**预计工作量**: ~600 行，1-2 天

**验证**: `ip link`, `iw dev wlan0 info`

---

### Phase 2: 硬件扫描 (P1)

**目标**: `iw dev wlan0 scan` 能看到 AP 列表

**步骤**:
1. 实现 `mt7927_hw_scan()` (调用 MCU scan cmd) (~40 行)
2. 实现扫描结果解析 (RX MCU 事件) (~60 行)
3. 实现 `mt7927_configure_filter()` (~20 行)

**前置依赖**: Agent B 的扫描 MCU 命令研究

**预计工作量**: ~250 行，1-2 天

**验证**: `iw dev wlan0 scan`

---

### Phase 3: 连接/加密 (P2)

**目标**: `wpa_supplicant` 能连接到 WPA2 AP

**步骤**:
1. 实现 `mt7927_set_key()` (MCU key cmd) (~60 行)
2. 实现 `mt7927_link_info_changed()` (BSS 信息更新) (~80 行)
3. 实现 `mt7927_sta_state()` (~30 行)
4. 实现 TX 数据帧路径 (~150 行)
5. 实现 RX 数据帧路径 (~100 行)

**前置依赖**: Agent C 的连接 MCU 命令研究

**预计工作量**: ~600 行，3-4 天

**验证**: `wpa_supplicant -i wlan0 -c /etc/wpa_supplicant.conf`, `ping 8.8.8.8`

---

### Phase 4: 优化和高级功能 (P3)

**目标**: AMPDU, 电源管理, AP 模式

**步骤**:
1. 实现 `mt7927_ampdu_action()` (~40 行)
2. 实现电源管理 (suspend/resume) (~100 行)
3. 实现 AP 模式 (start_ap/stop_ap) (~150 行)

**预计工作量**: ~400 行，2-3 天

---

## 8. 总结

### 8.1 核心要点

1. **最小可行集**: 6 个核心回调 (tx/start/stop/add_interface/remove_interface/config) 让 wlan0 出现
2. **代码复用率高**: ~900 行代码可从 mt7925 直接复制，修改量 <15%
3. **分阶段实现**: P0 → P1 → P2 → P3，每阶段都有可验证的里程碑
4. **关键集成点**: 在 `mt7927_post_fw_init()` 成功后异步注册 mac80211

---

### 8.2 与其他 Agent 的协作点

- **Agent B (scan)**: 需要提供扫描 MCU 命令格式 (CID, payload, 结果解析)
- **Agent C (connect)**: 需要提供连接/密钥 MCU 命令格式
- **Agent D (data path)**: 需要提供数据帧 TXD/RXD 格式

---

### 8.3 风险点

1. **TX/RX 数据帧格式**: Windows RE 未涵盖，需要从 mt7925 完全复制
2. **WCID 分配策略**: mt7925 使用 `MT792x_WTBL_SIZE=20`，需验证 mt7927 硬件是否一致
3. **固件能力查询**: 需要通过 NIC_CAP MCU 响应确认 EHT 支持、天线数量等
4. **中断路由**: 数据帧中断 (RX4) 可能需要额外的中断使能配置

---

**文档完成时间**: 2026-02-15
**参考驱动版本**: mt76 (Linux 6.18.9), mt7925 驱动
**下一步**: 等待 Agent B/C/D 的研究成果，然后开始 Phase 1 实现
