# MLO 单链路数据平面修复 (2026-03-04)

## 问题

MLO 连接到达 COMPLETED 状态 (valid_links=0x4, ap_mld_addr=60:cf:84:fd:2d:aa)，
SAE + 4-Way Handshake 全部成功，但 RX 数据平面完全不工作 — ARP/ping 无响应。

## 根因分析

### 1. RX WCID NULL 问题 (核心 bug)

**文件**: `mt76/mt792x_mac.c:149` — `mt792x_rx_get_wcid()`

固件对 MLO 广播/组播 RX 帧使用 BSS self-STA WCID (idx=18)，而非 AP STA WCID (idx=1)。
`rx_get_wcid()` 对非单播帧检查 `wcid->sta`：BSS self-STA 的 `sta=0`，函数返回 NULL。

mac80211 需要非 NULL 的 `status->wcid` 才能投递 802.3 帧（HW_HDR_TRANS 模式）。

```
RX 8023: wcid_idx=18 wcid=0000000000000000 uni=0 sec=4
                      ^^^^^^^^^^^^^^^^ NULL!
```

**为什么非 MLO 不受影响**: 非 MLO 广播帧使用 AP STA WCID (sta=1)，函数正常返回
`&sta->vif->sta.deflink.wcid`（即 VIF deflink WCID，sta=0 但非 NULL）。

**修复**:
```c
// 原代码
if (!wcid->sta)
    return NULL;

// 修复后
if (!wcid->sta) {
    return wcid->def_wcid;  // BSS self-STA → VIF deflink WCID
}
```

`def_wcid` 在 `change_vif_links()` (main.c:2081) 中设置：
- deflink: 指向自身 `&mvif->sta.deflink.wcid`
- secondary links: 指向 deflink WCID

### 2. omac_idx 共享问题

**文件**: `mt7925/main.c:405` — `mac_link_bss_add()`

所有 MLO 链路共享 `omac_idx=0`，导致固件无法区分不同链路。

**修复**: MT6639 MLD 时，`omac_idx = mconf->mt76.idx`（每链路唯一）。

### 3. band_idx 错误

**文件**: `mt7925/main.c:416-431` — `mac_link_bss_add()`

二级链路的 `band_idx` 使用 phy chandef（主链路频段），导致 2.4GHz 链路被标记为 band_idx=1。

**修复**: 优先从 `link_conf->chanreq.oper.chan` 获取频段。

### 4. BSS_MLD TLV 对齐

**文件**: `mt7925/mcu.h` — `struct bss_mld_tlv`

与 Windows RE 对齐的 BSS_MLD TLV 结构 (20 字节)。

**文件**: `mt7925/mcu.c` — `bss_mld_tlv()` 填充函数

根据 `is_mld` 标志正确设置 link_id, group_mld_id, band_idx, omac_idx。

## 修改文件汇总

| 文件 | 修改 |
|------|------|
| `mt76/mt792x_mac.c:149` | `rx_get_wcid`: 返回 `wcid->def_wcid` 替代 NULL |
| `mt76/mt7925/main.c:405` | `mac_link_bss_add`: MT6639 MLD omac_idx 唯一化 |
| `mt76/mt7925/main.c:416` | `mac_link_bss_add`: band_idx 从 link_conf 获取 |
| `mt76/mt7925/mcu.h:365` | `bss_mld_tlv` struct 对齐 Windows RE |
| `mt76/mt7925/mcu.c:2693` | `bss_mld_tlv()` 正确填充 MLO 字段 |
| `mt76/mt7925/mcu.c` | 多处 debug prints (暂时保留) |
| `mt76/mt7925/main.c` | 多处 debug prints (暂时保留) |
| `mt76/mt7925/mac.c:402` | RX 8023 debug print (暂时保留) |

## 测试结果

```
wpa_state=COMPLETED
ap_mld_addr=60:cf:84:fd:2d:aa
wifi_generation=7
key_mgmt=SAE
pmf=1
valid_links=0x4 (link_id=2, 2.4GHz)

PING 192.168.52.1: 3/3, rtt min/avg/max = 15.9/29.9/49.3 ms
```

## 当前局限

- 只有单链路 MLO (valid_links=0x4)，双链路未激活
- BSS_MLD is_mld=1 正确发送
- STA_MLD valid=0x4, link_num=1
- 需要 `mac_set_links()` 中的 `set_mlo_roc` 和 `ieee80211_set_active_links_async` 调查
