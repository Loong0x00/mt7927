# MT7927/MT6639 MLO 数据面不通 — 深度逆向分析报告

**日期**: 2026-03-04
**分析者**: Claude Code (基于 Windows RE + AUR 驱动源码交叉分析)
**问题**: MLO 三链路连接控制面完全正常 (valid_links=0x7)，但数据面 100% 丢包

---

## 一、Executive Summary

MLO 数据面不通的根因是 **多个层面的配置缺失同时存在**，按严重程度排序：

1. **[P0] `band_idx` 5GHz/6GHz 无法区分** — `mt7925_mt6639_hw_band_from_nl()` 将 5GHz 和 6GHz 都映射到 `band_idx=1`，导致固件无法正确路由双频段数据
2. **[P0] TXD6 DIS_MAT (Disable MAC Address Translation)** — MLO 需要 MAT 做 MLD→link 地址转换，但代码仅在 ALTX/BCN 队列禁用，数据队列的 MAT 逻辑可能不足
3. **[P0] STA_REC_MLD link[] 数组硬编码 2 条目** — 结构体 `sta_rec_mld.link[2]` 最多支持 2 个链路，三链路 MLO 丢失第三个链路的 WCID 映射
4. **[P1] BSS_MLD TLV `remap_idx` 硬编码 0xff** — 固件需要此字段做数据面地址重映射
5. **[P1] secondary link 的 STA_REC 缺少完整 state=ASSOC 配置** — 影响数据面转发表
6. **[P2] MLO_CTRL (CID=0x44) 命令未发送** — Windows 驱动有此命令但 Linux 驱动完全缺失
7. **[P2] `omac_idx` MLO 模式下强制为 0** — 所有链路共享 omac_idx=0，可能导致地址映射冲突

---

## 二、逐项详细分析

### 2.1 [P0] band_idx 5GHz/6GHz 无法区分

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/mt7925.h:66-69`
```c
static inline u8 mt7925_mt6639_hw_band_from_nl(enum nl80211_band band)
{
    return band == NL80211_BAND_2GHZ ? 0 : 1;
}
```

**问题**: MT6639 芯片有 3 个 RF band：band0=2.4GHz, band1=5GHz, band2=6GHz (或 band1=5G/6G 共享)。
当前实现将 5GHz 和 6GHz 都映射为 `band_idx=1`。在 MLO 模式下，如果同时有 5GHz 和 6GHz
链路，固件无法区分它们的数据面路由。

**影响位置** (全部使用此函数):
- `main.c:404` — `mt7925_mac_link_bss_add()` 设置 BSS band_idx
- `mcu.c:1420` — MLO ROC 的 dbdcband
- `mcu.c:2547` — BSS_INFO basic TLV 的 band_idx
- `main.c:2150` — chanctx assign

**Windows RE 对比**: Windows 驱动在 BssActivateCtrl 中 (0x140143540) 从 `rbp[3]` 读取
band_idx，此值由高层根据实际信道计算，能正确区分三个频段。

**修复建议**: 需要 marcin-fm 的 band_idx 补丁。正确映射应为：
```c
static inline u8 mt7925_mt6639_hw_band_from_nl(enum nl80211_band band)
{
    switch (band) {
    case NL80211_BAND_2GHZ: return 0;
    case NL80211_BAND_5GHZ: return 1;
    case NL80211_BAND_6GHZ: return 2;  // 或 1，取决于芯片 DBDC 配置
    default: return 0xff;
    }
}
```
但需要确认 MT6639 的实际硬件 band 编号 (2 还是其他值给 6GHz)。

---

### 2.2 [P0] TXD6 DIS_MAT — MAC Address Translation 控制

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/mac.c:810-814`
```c
val = MT_TXD6_DAS | FIELD_PREP(MT_TXD6_MSDU_CNT, 1);
if (!ieee80211_vif_is_mld(vif) ||
    (q_idx >= MT_LMAC_ALTX0 && q_idx <= MT_LMAC_BCN0))
    val |= MT_TXD6_DIS_MAT;
txwi[6] = cpu_to_le32(val);
```

**分析**: 当 `ieee80211_vif_is_mld(vif)` 为 true 时，数据帧 **不会** 设置 `DIS_MAT`，
即 MAT (MAC Address Translation) 被启用。这是正确的 — MLO 需要 MAT 将 MLD 地址转换为
link-specific 地址。

但 MAT 需要固件侧有正确的地址映射表。如果 BSS_MLD TLV 的 `remap_idx` 配置不正确 (当前
硬编码 0xff)，MAT 没有映射表可用，可能导致：
- TX: 源地址未从 MLD MAC 转为 link MAC → AP 丢弃
- RX: 目的地址未从 link MAC 转为 MLD MAC → mac80211 丢弃

**DIS_MAT 定义**: `/usr/src/mediatek-mt7927-2.1/mt76/mt76_connac3_mac.h:279`
```c
#define MT_TXD6_DIS_MAT   BIT(3)
```

**DAS (Destination Address Substitute)**: `MT_TXD6_DAS = BIT(2)` — 始终设置，允许固件
替换目标地址。MLO 模式下这是必需的。

---

### 2.3 [P0] STA_REC_MLD link[] 数组硬编码最多 2 条目

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/mcu.h:447-461`
```c
struct sta_rec_mld {
    __le16 tag;
    __le16 len;
    u8 mac_addr[ETH_ALEN];
    __le16 primary_id;
    __le16 secondary_id;
    __le16 wlan_id;
    u8 link_num;
    u8 rsv[3];
    struct {
        __le16 wlan_id;
        u8 bss_idx;
        u8 rsv;
    } __packed link[2];  // ← 硬编码最多 2 个链路！
} __packed;
```

**填充逻辑**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/mcu.c:1976`
```c
mld->link_num = min_t(u8, hweight16(mvif->valid_links), 2);
```

**问题**: 三链路 MLO (valid_links=0x7, 即 link0+link1+link2) 时:
- `link_num` 被截断为 2
- 第三个链路的 WCID/bss_idx 信息未发送给固件
- 固件不知道第三个链路的存在 → 无法做正确的多链路数据面调度

**Windows RE 对比**: Windows STA_REC_MLD_SETUP (tag=0x20, 32 字节) 结构更大，
RE 报告显示 offset 0x14 处有一个循环结构，每个条目 3 字节 (wlan_id + link_byte):
```
[0x14] entry[0]: word(wlan_id) + byte(link_info)  // 3 bytes
[0x17] entry[1]: word(wlan_id) + byte(link_info)  // 3 bytes
...
```
Windows 的 MLD_SETUP TLV 大小为 0x20 (32 字节)，比 Linux 的 STA_REC_MLD 大，
可能支持更多链路条目。

**mt76_select_links() 限制**: `/usr/src/mediatek-mt7927-2.1/mt76/mac80211.c:2073`
此函数也限制 `max_active_links=2`：
```c
u16 sel_links = mt76_select_links(vif, 2);
```

---

### 2.4 [P1] BSS_MLD TLV remap_idx 硬编码 0xff

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/mcu.c:2703`
```c
mld->remap_idx = 0xff;
```

**BSS_MLD TLV 完整结构**:
```c
struct bss_mld_tlv {        // tag=0x1A, len=0x14 (20 bytes)
    __le16 tag;
    __le16 len;
    u8 group_mld_id;        // MLD 时: mvif->bss_conf.mt76.idx; 非 MLD: 0xff
    u8 own_mld_id;          // mconf->mt76.idx + 32
    u8 mac_addr[ETH_ALEN];  // vif->addr (MLD 地址)
    u8 remap_idx;           // ← 硬编码 0xff (未配置!)
    u8 link_id;             // MLD 时: link_conf->link_id; 非 MLD: 0xff
    u8 eml_enable;          // EMLSR 支持标志
    u8 max_link_num;        // 未设置 (0)
    u8 hybrid_mode;         // 未设置 (0)
    u8 __rsv[3];
};
```

**Windows RE 对比**: `nicUniCmdBssInfoMld` (0x14014fad0) 中：
```c
// MLD mode == 3:
out[12] = mld_entry[3];     // remap_idx — 来自 MLD entry 的字段
out[14] = mld_entry[0xd];   // mld_type
out[15] = linkmap_enable;   // 条件设置
```
Windows 驱动的 `remap_idx` 来自一个 MLD entry 结构，不是 0xff。

**`max_link_num` 未设置**: 此字段应填入 MLO 链路数量（如 3），告诉固件总共有多少链路。
当前为 0。

**`hybrid_mode` 未设置**: 可能影响 STR vs EMLSR 模式选择。

---

### 2.5 [P1] Secondary Link STA_REC 配置序列问题

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/main.c:932-948`

MLO `mt7925_mac_link_sta_add()` 中，对于 primary link 和 secondary link 的处理不同:

```c
if (ieee80211_vif_is_mld(vif) && link_sta == mlink->pri_link) {
    // primary link: STATE_NONE (仅基本注册)
    ret = mt7925_mcu_sta_update(dev, link_sta, vif, true,
                                MT76_STA_INFO_STATE_NONE);
} else if (ieee80211_vif_is_mld(vif) && link_sta != mlink->pri_link) {
    // secondary link: 先更新 primary 为 ASSOC，再注册 secondary 为 ASSOC
    ret = mt7925_mcu_sta_update(dev, mlink->pri_link, vif,
                                true, MT76_STA_INFO_STATE_ASSOC);
    ret = mt7925_mcu_sta_update(dev, link_sta, vif, true,
                                MT76_STA_INFO_STATE_ASSOC);
}
```

后续在 `mt7925_mac_link_sta_assoc()` (通过 `mt7925_mac_sta_event`) 中:
```c
// 只对 deflink 执行关联状态更新!
mt7925_mcu_sta_update(dev, link_sta, vif, true, MT76_STA_INFO_STATE_ASSOC);
```

**问题**: `mt7925_mac_sta_event` 只处理 `MT76_STA_EVENT_ASSOC`，且只处理 deflink：
```c
if (ieee80211_vif_is_mld(vif)) {
    link_sta = mt792x_sta_to_link_sta(vif, sta, msta->deflink_id);
    mt7925_mac_set_links(mdev, vif);  // ← 设置 active links
}
mt7925_mac_link_sta_assoc(mdev, vif, link_sta);  // ← 只关联 deflink!
```

**影响**: secondary link 在 `change_vif_links` → `mt7925_mac_link_bss_add` →
`mt7925_mac_link_sta_add` 中被注册，但关联状态更新的时序可能不正确。
特别是 BSS_INFO 的 `conn_state` 和 STA_REC 的 state 对于 secondary links
是否被正确设置为 CONNECTED/ASSOC 状态。

---

### 2.6 [P2] MLO_CTRL 命令完全缺失

**Windows RE**: Dispatch table entry 55:
```
| 55 | 0x7e | 0x44 | 0x00 | 0x140147330 | MLO_CTRL |
```

Windows 驱动有一个 `MLO_CTRL` 命令 (outer_tag=0x7e, inner_CID=0x44)，handler 在
0x140147330。此命令 **在 AUR 驱动中完全不存在** — 没有对应的 MCU 命令发送。

**推测功能**:
- 可能用于通知固件启用/配置 MLO 数据面
- 可能设置链路间的数据调度策略 (round-robin, primary-secondary)
- 可能配置 TID-to-link 映射

**需要进一步逆向**: 函数 0x140147330 尚未被 Ghidra 分析。这是理解 MLO 数据面配置
的关键缺失。

---

### 2.7 [P2] omac_idx MLO 模式下强制为 0

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/main.c:401-402`
```c
mconf->mt76.omac_idx = ieee80211_vif_is_mld(vif) ?
                       0 : mconf->mt76.idx;
```

**问题**: MLO 模式下所有链路的 `omac_idx` 都被设为 0。在 TXD 中:
```c
val = FIELD_PREP(MT_TXD1_WLAN_IDX, wcid->idx) |
      FIELD_PREP(MT_TXD1_OWN_MAC, omac_idx);  // 所有链路都是 0
```

这意味着固件看到所有链路的 TX 帧都来自同一个 OWN_MAC 索引。固件可能依赖
不同的 omac_idx 来区分不同链路的帧。

---

### 2.8 [INFO] TXD band_idx (TGID) 设置

**文件**: `/usr/src/mediatek-mt7927-2.1/mt76/mt7925/mac.c:783-784`
```c
if (band_idx)
    val |= FIELD_PREP(MT_TXD1_TGID, band_idx);
```

`band_idx` 来自 `mconf->mt76.band_idx`，通过 `wcid->link_id` 索引到正确的 mconf。
这部分逻辑是正确的 — 每个链路的 TXD 会带上该链路的 band_idx。

但由于 2.1 节的 band_idx 映射问题 (5G/6G 都是 1)，实际效果是 5GHz 和 6GHz 链路
的 TXD 带相同的 TGID 值，固件无法区分。

---

## 三、Windows 驱动 MLO 命令序列 vs AUR 驱动对比

### Windows 连接命令序列 (从 RE 文档):
```
[1] ChipConfig
[2] BssActivateCtrl: DEV_INFO + BSS_INFO(BASIC+MLD) — 每个链路
[3] PM_DISABLE
[4] BSS_INFO (14 TLV full) — 每个链路
[5] SCAN_CANCEL
[6] CH_PRIVILEGE/ROC — 每个链路的信道请求
[7] STA_REC (13 TLV) — 每个链路
[8] Auth TX → 4-Way Handshake
[?] MLO_CTRL (CID=0x44) — 时序未知，但在 dispatch table 中存在
```

### AUR 驱动 MLO 序列 (推断):
```
[1] add_interface → mt7925_mac_link_bss_add → uni_add_dev (DEV_INFO + BSS_INFO basic)
    — 此时只注册 deflink (link0)
[2] mac80211 扫描 + 发现 MLD AP
[3] change_vif_links → 为每个新链路调用 mt7925_mac_link_bss_add:
    a) DEV_INFO + BSS_INFO(BASIC only, 无 MLD TLV!)  ← 差异!
    b) mt7925_mac_link_sta_add → BSS_INFO(full) + STA_REC
[4] mac_sta_event(ASSOC) → mac_link_sta_assoc → BSS_INFO(full) + STA_REC(ASSOC)
    — 只对 deflink!
[5] mt7925_mac_set_links → mt7925_set_mlo_roc → MLO ROC (双链路)
```

### 关键差异:

| 步骤 | Windows | AUR | 影响 |
|------|---------|-----|------|
| BssActivateCtrl | DEV_INFO + BSS_INFO(BASIC+**MLD**) | DEV_INFO + BSS_INFO(BASIC only) | 固件早期不知道 MLD 信息 |
| 链路注册 | 每个链路独立完整配置 | secondary link 与 primary 交叉更新 | 时序复杂，可能竞态 |
| MLO_CTRL | 存在 (CID=0x44) | **完全缺失** | 固件可能未启用 MLO 数据面 |
| STA_REC_MLD | 支持多链路 | 硬编码 link[2], link_num<=2 | 第三链路丢失 |
| BSS_MLD remap | 从 MLD entry 填充 | 硬编码 0xff | 地址转换表缺失 |

---

## 四、修复建议 (按优先级)

### P0 — 必须修复才能有数据

#### P0-1: 修复 band_idx 映射

需要 marcin-fm 的 band_idx 补丁，或手动确认 MT6639 的 6GHz band 编号后修改
`mt7925_mt6639_hw_band_from_nl()`。

验证方法: 加载单链路 6GHz，查看 `band_idx` 值，再加载单链路 5GHz 对比。

#### P0-2: 修复 STA_REC_MLD 支持 3 链路

```c
// mcu.h: 扩展 link 数组
struct sta_rec_mld {
    ...
    struct {
        __le16 wlan_id;
        u8 bss_idx;
        u8 rsv;
    } __packed link[3];  // 改为 3 或更多
};

// mcu.c: 去掉 min_t(..., 2) 限制
mld->link_num = hweight16(mvif->valid_links);
```

同时需要修改 `mt76_select_links()` 的 `max_active_links` 参数。

#### P0-3: 修复 BSS_MLD remap_idx

需要理解 `remap_idx` 的语义。从 Windows RE:
- Non-MLD: remap_idx=0
- MLD mode 3: remap_idx=mld_entry[3] (某个 MLD 结构的字段)

可能的值: 链路索引 (0, 1, 2) 或固件内部的映射表索引。

### P1 — 可能修复数据面路由

#### P1-1: 填充 BSS_MLD 缺失字段

```c
// mcu.c mt7925_mcu_bss_mld_tlv():
mld->max_link_num = hweight16(mvif->valid_links);  // 如 3
mld->remap_idx = mconf->mt76.idx;  // 需要验证正确值
```

#### P1-2: 确保所有链路的 STA_REC 达到 ASSOC 状态

在 `mt7925_mac_sta_event` 中，不仅对 deflink 做 ASSOC，还要对所有 active links 做:
```c
if (ieee80211_vif_is_mld(vif)) {
    unsigned long valid = mvif->valid_links;
    int link_id;
    for_each_set_bit(link_id, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
        link_sta = mt792x_sta_to_link_sta(vif, sta, link_id);
        mt7925_mac_link_sta_assoc(mdev, vif, link_sta);
    }
}
```

### P2 — 可能需要的额外命令

#### P2-1: 逆向 MLO_CTRL (0x140147330)

使用 Ghidra 分析此函数:
- 输入参数结构
- 发送的 TLV 内容
- 调用时机

这是理解 MLO 数据面最后一块拼图的关键。

#### P2-2: 初始 BSS_INFO 包含 MLD TLV

在 `mt76_connac_mcu_uni_add_dev` 中，BSS_INFO 的 BASIC TLV 后应追加 MLD TLV
(如 Windows BssActivateCtrl 所做)。

---

## 五、需要进一步逆向的 Windows 函数

| 函数地址 | 名称/用途 | 原因 |
|----------|----------|------|
| **0x140147330** | **MLO_CTRL handler** | **最关键 — MLO 数据面控制命令** |
| 0x140143540 | BssActivateCtrl MLO 路径 | 确认 MLD mode==3 时的完整行为 |
| 0x1401446d0 | STA_REC dispatch (MLO 分支) | 确认 MLO 时是否有额外 TLV |
| 0x14014e2a0 | EHT_MLD builder 的完整逻辑 | 确认 str_cap 字段的正确编码 |

---

## 六、快速验证实验建议

在不修改代码的情况下，可以通过以下实验缩小问题范围：

### 实验 1: 双链路 MLO (2.4G + 6G)
只启用 2 个不同 band 的链路 (band_idx=0 和 band_idx=1)，避免 5G/6G band_idx 冲突。
如果双链路 MLO 数据通了，说明 band_idx 是主因。

### 实验 2: 双链路 MLO (5G + 6G)
两个链路都是 band_idx=1。如果同样不通，确认 band_idx 冲突是问题。

### 实验 3: 检查 dmesg 中的 WCID/link 信息
```bash
dmesg | grep -i "wcid\|link_id\|mld\|band_idx"
```
确认每个链路的 WCID、band_idx、bss_idx 是否正确分配。

### 实验 4: tcpdump 确认 TX 是否发出
```bash
sudo tcpdump -i wlp9s0 -e -n arp
```
确认 ARP 请求是否被发送（MAC 地址是 MLD 地址还是 link 地址）。

---

## 七、总结

MLO 数据面不通是多因素叠加的结果。最可能的解决路径：

1. 先修复 `band_idx` 映射 (P0-1)，这是 marcin-fm 已经在做的
2. 扩展 `sta_rec_mld.link[]` 到 3+ (P0-2)
3. 正确填充 `bss_mld_tlv` 的 `remap_idx` 和 `max_link_num` (P0-3, P1-1)
4. 逆向 MLO_CTRL (0x140147330) 看是否有必要的数据面激活命令 (P2-1)

在 marcin-fm 的 STR MLO 补丁发布前，建议先做上述实验来验证假设，
并重点逆向 MLO_CTRL handler。

---

*分析资源: Windows RE (docs/re/), Ghidra 共识报告 (tmp/re_results/consensus/),
AUR 驱动源码 (/usr/src/mediatek-mt7927-2.1/mt76/)*
