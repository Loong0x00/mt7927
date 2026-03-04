# 5GHz 4WAY_HANDSHAKE_TIMEOUT 复现报告

> *测试与报告由 Claude Code（Anthropic）辅助生成，在实际硬件上验证。*

---

## 测试环境

| 项目 | 详情 |
|------|------|
| 硬件 | MT7927（PCI ID `14c3:6639`），AMD 800 系主板 |
| 驱动 | `mediatek-mt7927-dkms` v2.1，内核 6.18.9-arch1-2（Arch Linux）|
| AP | 中国移动运营商路由器，5GHz WPA2-PSK，ch149（5805 MHz）|
| 工具 | `wpa_supplicant` 手动运行，debug 模式 |

---

## 测试结果

```
wlp9s0: CTRL-EVENT-DISCONNECTED bssid=b4:ba:12:5b:63:c9 reason=15
wlp9s0: WPA: 4-Way Handshake failed - pre-shared key may be incorrect
wlp9s0: CTRL-EVENT-CONNECTED - Connection to b4:ba:12:5b:63:c9 completed
```

**第一次连接尝试**：以 reason=15（`4WAY_HANDSHAKE_TIMEOUT`）失败。

**wpa_supplicant 自动重试**：第二次连接成功。

---

## 分析

这与 marcin-fm / lmcarneiro / cmspam 的报告一致，确认问题存在于所有
5GHz 路径，与加密模式（WPA2/WPA3）和信道带宽无关。

**为什么在宽松路由器上不易察觉：**
商用 ISP 路由器对 EAPOL 重传更积极，wpa_supplicant 自动重试后第二次
成功，NetworkManager 在后台静默处理，用户不会感知到首次失败。
OpenWrt 等严格配置下则陷入持续循环。

**进一步支持 EAPOL 投递路径假设：**
marcin-fm 的测试显示切换至开放模式（无加密）后 4 秒超时消失，结合
本次 WPA2 5GHz 复现，确认问题在 EAPOL 帧投递路径，与加密协议本身无关。

MT6639 固件在 5GHz/6GHz 路径上启用 HW Header Translation（802.11→802.3），
EAPOL 帧经转换后进入 mac80211 8023 fast path，而 `fast_rx` 在 4-way
handshake 期间为 NULL（仅 AUTHORIZED 后设置）→ mac80211 静默丢弃 →
AP 超时等待 Key 2/4 → reason=15 deauth。

2.4GHz 路径未受影响，推测固件在该频段不启用或差异化处理 HW_HDR_TRANS。

**建议修复**：见 [`320mhz_handshake_timeout_fix.md`](320mhz_handshake_timeout_fix.md)。

---

*测试地点：中国大陆，无 6GHz 频段使用许可。*
*分析与文档由 Claude Code（Anthropic）生成，需经开发者审查验证。*
