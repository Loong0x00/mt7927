# MLO 双链路修复 (2026-03-04)

## 问题

单链路 MLO 数据平面修复后 (valid_links=0x4, link_id=2, 2.4GHz ping 成功),
第二条链路始终未激活 — `mac_set_links()` 不发送 MLO ROC, `change_vif_links`
不被调用添加第二链路。

## 根因分析

### 1. ROC 条件过窄 (核心 bug)

**文件**: `mt7925/main.c:1095` — `mac_set_links()`

原代码只覆盖两种组合:
- 2.4GHz 为主链路 (任意副链路)
- 5GHz 主 + 6GHz 副

```c
// 原代码
if (band == NL80211_BAND_2GHZ ||
    (band == NL80211_BAND_5GHZ && secondary_band == NL80211_BAND_6GHZ))
```

当 5GHz 为主链路、2.4GHz 为副链路时, 条件不满足 → ROC 不发送 → 第二链路不激活。

**为什么**: mt7925 是单射频 eMLSR 设计, 5GHz+6GHz 共享同一射频,
2.4GHz 和 5GHz/6GHz 需要跨射频 ROC。原代码遗漏了 5GHz+2.4GHz 组合。

**修复**: MT6639 有 2 个射频 (band0=2.4G, band1=5G/6G), 任何跨频段组合都需要 ROC:

```c
// 修复后
if (band != secondary_band) {
    mt7925_abort_roc(mvif->phy, &mvif->bss_conf);
    mt792x_mutex_acquire(dev);
    mt7925_set_mlo_roc(mvif->phy, &mvif->bss_conf, sel_links);
    mt792x_mutex_release(dev);
}
```

### 2. Regulatory domain 阻止 5GHz 主动扫描

**问题**: `country 00` (世界默认) 将 5GHz 标记为 PASSIVE-SCAN (no-IR),
wpa_supplicant 无法在 5GHz 发送 ML probe → 看不到 5GHz BSS → 无法建立
包含 5GHz 的多链路连接。

**修复**: `sudo iw reg set US` 允许 5GHz 主动扫描。

### 3. scan_freq 优化

**问题**: 无 scan_freq 时, wpa_supplicant 可能先发现 2.4GHz BSS 并立即连接,
跳过 5GHz 扫描。后续 ML probe 因 PASSIVE-SCAN 限制失败。

**修复**: `wpa_asus_mlo.conf` 添加 `scan_freq=5785 2412`, 确保先扫 5GHz。

## 修改文件汇总

| 文件 | 修改 |
|------|------|
| `mt7925/main.c:1095` | `mac_set_links`: ROC 条件改为 `band != secondary_band` |
| `mt7925/main.c:1073` | `mac_set_links`: 添加 debug prints (暂时保留) |
| `mt7925/main.c:2041` | `change_vif_links`: 添加 debug print (暂时保留) |
| `wpa_asus_mlo.conf` | 添加 `scan_freq=5785 2412` |

## 测试结果

```
# iw dev wlp9s0 link
   Link 1 BSSID d2:cf:84:fd:2d:a6
      freq: 5785.0
   Link 2 BSSID b2:cf:84:fd:2d:a2
      freq: 2412.0

# wpa_cli status
wpa_state=COMPLETED
ap_mld_addr=60:cf:84:fd:2d:aa
wifi_generation=7
valid_links=0x6

# ping -c 10 192.168.52.1
10 packets transmitted, 10 received, 0% packet loss
rtt min/avg/max = 1.2/2.7/6.5 ms

# dmesg 关键日志
mac_set_links: deflink=1 sel=0x6 active=0x2 valid=0x6 band=1 is_mld=1
mac_set_links: secondary=2 sec_band=0 pri_band=1 → roc=1
mac_set_links: sending MLO ROC for sel=0x6
change_vif_links: old=0x2 new=0x6 add=0x4 rem=0x0
STA_MLD: link_num=2 valid=0x6
```

## MLO 连接流程 (完整)

```
1. wpa_supplicant 扫描 → 发现 5GHz BSS (link_id=1)
2. MLD 发现: valid_links=0x0007 (6G+5G+2.4G)
3. SAE auth → 4-Way Handshake → COMPLETED (primary: 5GHz, link_id=1)
4. mac_set_links() → mt76_select_links() 选择 link 1 (5G) + link 2 (2.4G)
5. ROC 条件 band(1) != secondary_band(0) → 发送 MLO ROC
6. ieee80211_set_active_links_async(0x6)
7. mac80211 调用 change_vif_links(old=0x2, new=0x6, add=0x4)
8. 驱动添加 link_id=2 BSS (band_idx=0, omac_idx=1)
9. STA_MLD TLV 更新: link_num=2, valid=0x6
10. 双链路数据平面工作
```

## 与单链路修复的关系

单链路修复 (mlo_single_link_dataplane_fix.md) 解决了 RX 数据平面:
- RX WCID NULL → `def_wcid` fallback
- omac_idx 唯一化
- band_idx 从 link_conf 获取
- BSS_MLD TLV 对齐

本修复在此基础上解决了双链路激活:
- ROC 条件覆盖所有跨频段组合
- Regulatory domain 允许 5GHz 主动扫描
- scan_freq 确保 5GHz BSS 发现
