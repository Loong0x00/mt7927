# MT7927/MT6639: 320MHz 模式下 4WAY_HANDSHAKE_TIMEOUT 修复分析

> **声明**: 本文档的分析和内容由 [Claude Code](https://claude.ai/code)（Anthropic）生成，
> 供参考使用。相关结论需要在实际硬件上进一步验证，代码改动需经过内核开发者审查。

---

## 背景

基于 marcin-fm 在 [openwrt/mt76#927](https://github.com/openwrt/mt76/issues/927) 的测试报告：

- **iteration-1 补丁**（`mt7925/mac.c` 添加 320MHz RXV 解码）已修复数据路径，
  实测 ~1 Gbps 双向吞吐，EHT-MCS 11 EHT-NSS 2 @ 320MHz。
- **残余问题**：重连周期中仍出现 `4WAY_HANDSHAKE_TIMEOUT`，即使链路之后可以恢复。

本文分析该残余问题的根本原因，并提出修复方案。

---

## 根本原因分析

### 症状解析

```
DHCP 正常工作       → TX/RX 广播帧路径正确
ping 失败 / 数据不通  → 单播数据路径在握手完成前中断
4WAY_HANDSHAKE_TIMEOUT → wpa_supplicant 未能收到 EAPOL Key 帧
```

DHCP 正常但 4-way 握手失败这个组合，强烈指向一个问题：
**EAPOL 帧在到达 wpa_supplicant 之前就被丢弃了。**

### mac80211 8023 fast path 的限制

MT7927/MT6639 固件启用了 HW Header Translation（`HW_HDR_TRANS`）：
固件在硬件层面将 802.11 帧头转换为 802.3 以太网帧头，驱动设置
`RX_FLAG_8023`，让 mac80211 走 **8023 fast path**。

fast path 的调用链：

```
mt76_rx_complete()
  → ieee80211_rx_8023()
    → __ieee80211_rx_handle_8023()
      → if (!fast_rx) goto drop   ← 关键点
```

`__ieee80211_rx_handle_8023()` 要求 `fast_rx` 指针非 NULL。
而 `fast_rx` 仅在 STA 状态到达 **IEEE80211_STA_AUTHORIZED** 之后才被设置。

**时序冲突**：

```
Association 完成 (ASSOC 状态)
    ↓
4-way handshake 开始
    ↓
EAPOL Key 1/4 到达 → fast_rx == NULL → mac80211 静默丢弃
    ↓
wpa_supplicant 超时 → 4WAY_HANDSHAKE_TIMEOUT
    ↓
(STA 永远无法到达 AUTHORIZED → fast_rx 永远不会被设置)
```

这是一个经典的鸡生蛋问题：fast path 需要 AUTHORIZED，
而 AUTHORIZED 需要 4-way handshake，而 handshake 需要 fast path 通畅。

### 为什么是 6GHz/320MHz 场景更容易触发

在 2.4GHz/5GHz 下，如果驱动实现了其他 RX 路径（如 raw socket 或直接
`netif_receive_skb()`），EAPOL 可能通过其他方式到达 wpa_supplicant，
从而掩盖这个问题。6GHz 场景下，路径更严格，问题暴露得更明显。

---

## 修复方案

### 核心思路

在将 802.3 帧投递给 mac80211 之前，检测 EAPOL 帧
（EtherType = `0x888E`），通过 `cfg80211_rx_control_port()` 直接
投递给 wpa_supplicant，绕过 mac80211 8023 fast path。

现代 wpa_supplicant（>= 2.9）使用 **nl80211 control port** 接收 EAPOL，
不依赖 raw socket。`cfg80211_rx_control_port()` 正是这条路径的入口。

### 插入位置

在 mt76 驱动的 RX 完成路径中，于调用 `ieee80211_rx_8023()` /
`ieee80211_rx_napi()` 之前插入 EAPOL 检测逻辑。

具体位置因代码版本而异，候选位置：
- `drivers/net/wireless/mediatek/mt76/mt76.h` 中的 `mt76_rx_complete()`
- 或 mt7925 特有的 RX 回调（`mt7925/mac.c` 中的帧分发函数）

### 伪代码示例

```c
/* 在 802.3 帧投递到 mac80211 之前检测 EAPOL */
if (status->flag & RX_FLAG_8023) {
    struct ethhdr *eth = (struct ethhdr *)skb->data;
    if (eth->h_proto == htons(ETH_P_PAE)) {  /* 0x888E */
        /* EAPOL: 通过 nl80211 control port 投递给 wpa_supplicant */
        cfg80211_rx_control_port(dev->ieee80211_ptr->netdev,
                                 skb,
                                 false,   /* unencrypted */
                                 -1);     /* link_id: -1 for non-MLO */
        dev_kfree_skb(skb);   /* cfg80211 不消费 skb，需手动释放 */
        return;
    }
}
/* 非 EAPOL 帧继续走正常路径 */
```

> **注意**: `cfg80211_rx_control_port()` 的函数签名因内核版本略有不同，
> 需根据目标内核版本调整参数。

### 需要确认的前提条件

使用 `cfg80211_rx_control_port()` 需要在关联流程中通过 NL80211 设置
control port：

```c
/* connect 时确认有设置 */
params->control_port = true;
params->control_port_ethertype = cpu_to_be16(ETH_P_PAE);
params->control_port_no_encrypt = false;
```

wpa_supplicant 通过 `NL80211_ATTR_CONTROL_PORT` 标志发起连接时会自动
设置此项，现代版本默认启用。

---

## 预期效果

修复后：
1. 4WAY handshake 期间 EAPOL 帧正确到达 wpa_supplicant ✅
2. PTK/GTK 协商完成，STA 进入 AUTHORIZED 状态 ✅
3. `fast_rx` 被设置，后续数据帧走 fast path ✅
4. 重连周期稳定，无 `4WAY_HANDSHAKE_TIMEOUT` ✅

结合 marcin-fm 的 iteration-1（320MHz RXV decode），理论上可以实现：
- 稳定的 6GHz 320MHz 连接
- 标准 WPA3-SAE 4-way handshake
- 持续的高吞吐（受 PCIe 带宽限制）

---

## 参考

- 本分析基于对 mac80211 `__ieee80211_rx_handle_8023()` 源码的研究
  （`net/mac80211/rx.c`）
- 相同 fix 已在独立的 MT7927 PCIe 驱动项目中实现并验证
  （Session 45，2025 年，基于 2.4GHz 测试）
- 相关 issue: [openwrt/mt76#927](https://github.com/openwrt/mt76/issues/927)
- marcin-fm 的 iteration-1 patch: 见 issue 评论

---

*分析与文档由 Claude Code（Anthropic）生成。结论基于代码静态分析，
未在 6GHz/320MHz 环境下直接验证。建议在提交补丁前进行实际测试。*

本人在中国，无法进行6ghz测试，请见谅
