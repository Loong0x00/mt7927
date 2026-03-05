# MT6639/MT7927 WiFi 7 完整功能测试报告

**测试日期**: 2026-03-05
**测试工程师**: Claude Opus 4.6 (自动化测试)

## 测试环境

| 项目 | 详细信息 |
|------|----------|
| **网卡** | MediaTek MT7927 (芯片 MT6639, PCI ID 14c3:6639) |
| **驱动** | mediatek-mt7927-dkms v2.1-15 (AUR, 基于 mt7925e + MLO 补丁) |
| **内核** | 6.18.13-arch1-1 (Arch Linux) |
| **AP** | ASUS RT-BE92U BE9700 (WiFi 7 三频, 美版, 固件最新) |
| **WiFi 接口** | wlp9s0 (wiphy phy15) |
| **天线配置** | 2T2R (TX 0x3 RX 0x3) |
| **Regulatory** | US (FCC) |
| **wpa_supplicant** | 系统默认版本 |

---

## 功能测试结果

### 1. 基础连接

#### 1.1 2.4GHz (WPA2-PSK)

| 指标 | 结果 |
|------|------|
| **认证/关联** | COMPLETED |
| **SSID** | asus_SSID |
| **BSSID** | b2:cf:84:fd:2d:a1 |
| **频率** | 2412 MHz (ch1) |
| **wifi_generation** | 7 (EHT) |
| **密钥管理** | WPA2-PSK |
| **信号** | -19 dBm [-21, -22] |
| **TX 速率** | 6.0 MBit/s (legacy, 异常) |
| **RX 速率** | 1.0 MBit/s (legacy, 异常) |
| **DHCP** | FAIL (超时, 数据平面不通) |
| **Ping** | FAIL (目标主机不可达, ARP 不通) |

#### 1.2 5GHz (WPA2-PSK)

| 指标 | 结果 |
|------|------|
| **认证/关联** | COMPLETED |
| **SSID** | asus_SSID_5G |
| **BSSID** | d2:cf:84:fd:2d:a5 |
| **频率** | 5785 MHz (ch157) |
| **wifi_generation** | 7 (EHT) |
| **密钥管理** | WPA2-PSK |
| **信号** | -27 dBm |
| **TX 速率** | 6.0 MBit/s (legacy, 异常) |
| **RX 速率** | 6.0 MBit/s (legacy, 异常) |
| **DHCP** | FAIL (超时, 数据平面不通) |

#### 1.3 6GHz (WPA3-SAE)

| 指标 | 结果 |
|------|------|
| **认证/关联** | COMPLETED |
| **SSID** | asus_SSID_6G |
| **BSSID** | 60:cf:84:fd:2d:a9 |
| **频率** | 6615 MHz (ch133) |
| **wifi_generation** | 7 (EHT) |
| **密钥管理** | SAE + PMF |
| **BW** | 320 MHz |
| **信号** | -32 dBm |
| **TX 速率** | 6.0 MBit/s (legacy, 异常) |
| **RX 速率** | 6.0 MBit/s 320MHz |
| **DHCP** | FAIL (超时, 数据平面不通) |

**重要发现**: 6GHz 非 MLO SSID 连接时, 驱动自动进入 MLD 路径 (`change_vif_links` 被调用, `is_mld=1`), 因为 ASUS AP 在 beacon 中广播 MLD capabilities。

### 2. WiFi 7 特性 (EHT/320MHz/4K-QAM)

#### 2.1 wiphy 硬件能力

| 特性 | 支持状态 |
|------|----------|
| **EHT (802.11be)** | 支持 (Band 1/2/4) |
| **320MHz BW** | 支持 (仅 6GHz Band 4) |
| **4096-QAM** | 支持 (Rx 4096-QAM In Wider BW DL OFDMA) |
| **MCS 0-13** | 支持 (2 streams) |
| **MCS 15** | 支持 |
| **LDPC** | 支持 (RX LDPC) |
| **STBC** | 支持 (TX/RX <= 80MHz) |
| **SU Beamformee** | 支持 (80/160/320MHz) |
| **MU Beamformer** | 支持 (80/160/320MHz) |
| **UL MU-MIMO** | 支持 (Non-OFDMA, 80/160/320MHz) |
| **HE (802.11ax)** | 支持 (全频段) |
| **VHT (802.11ac)** | 支持 (160MHz) |
| **HT (802.11n)** | 支持 (HT40) |
| **1024-QAM** | 支持 (HE TX/RX) |
| **最大 AMPDU** | 65535 bytes |
| **最大 AMSDU** | 7935 bytes |

#### 2.2 MCS/NSS 能力

| 标准 | BW | NSS | MCS 范围 |
|------|----|-----|----------|
| EHT | <= 80MHz | 2 | 0-13 |
| EHT | 160MHz | 2 | 0-13 |
| EHT | 320MHz | 2 | 0-13 |
| HE | <= 80MHz | 2 | 0-11 |
| HE | 160MHz | 2 | 0-11 |
| VHT | <= 80MHz | 2 | 0-9 |
| HT | 40MHz | 2 | 0-15 |

#### 2.3 实测 PHY 速率 (MLO 模式)

| 方向 | 速率 | MCS | NSS | GI | 备注 |
|------|------|-----|-----|----|------|
| **RX** | 206.5 MBit/s | EHT-MCS 8 | 2 | 0 | 20MHz BW (2.4GHz link) |
| **RX** | 172.0 MBit/s | EHT-MCS 7 | 2 | 0 | 20MHz BW |
| **TX** | 6.0 MBit/s | legacy | - | - | 异常低速 (已知问题) |

**理论最高速率** (基于硬件能力):
- 6GHz 320MHz EHT-MCS 13 2SS: ~5765 Mbps
- 5GHz 160MHz EHT-MCS 13 2SS: ~2882 Mbps
- 2.4GHz 40MHz EHT-MCS 13 2SS: ~720 Mbps

### 3. MLO (Multi-Link Operation)

#### 3.1 单链路 MLO (之前测试结果, 2026-03-04)

| 指标 | 结果 |
|------|------|
| **SSID** | asus_MLO |
| **MLD AP** | 60:cf:84:fd:2d:aa |
| **活跃链路** | link_id=2 (2.4GHz, ch1) |
| **valid_links** | 0x4 |
| **DHCP** | 成功 (192.168.52.x 子网) |
| **Ping** | 50/50 成功, 0% 丢包, avg 7.0ms |
| **大包 Ping** (1400B) | 91/100, 9% 丢包, avg 7.1ms |
| **状态** | 数据平面完全工作 |

#### 3.2 双链路 MLO (本次测试, 2026-03-05)

| 指标 | 结果 |
|------|------|
| **SSID** | asus_MLO |
| **MLD AP** | 60:cf:84:fd:2d:aa |
| **Link 0** | 6GHz (ch133, 6615 MHz), 320MHz BW |
| **Link 2** | 2.4GHz (ch1, 2412 MHz), 20MHz BW |
| **valid_links** | 0x5 (link 0 + link 2) |
| **link_num** | 2 |
| **STA_MLD** | primary=1 secondary=2, wlan_id=1/2, bss_idx=0/1 |
| **DHCP** | FAIL (超时) |
| **Ping** | FAIL (ARP 不通) |
| **状态** | 控制平面完整, 数据平面不通 |

**关键 dmesg 日志** (双链路建立过程):
```
mac_set_links: deflink=0 sel=0x5 active=0x1 valid=0x5 band=3 is_mld=1
mac_set_links: secondary=2 sec_band=0 pri_band=3 -> roc=0
mac_set_links: sending MLO ROC for sel=0x5
change_vif_links: old=0x1 new=0x5 add=0x4 rem=0x0
link_bss_add: link_id=2 idx=1 omac=1 band_idx=0 wmm=0 link_idx=1 wcid=18 mld=1
STA_MLD: mac=60:cf:84:fd:2d:aa primary=1 secondary=2 wlan_id=1 link_num=2 valid=0x5
STA_MLD: link[0] wlan_id=1 bss_idx=0
STA_MLD: link[1] wlan_id=2 bss_idx=1
```

#### 3.3 MLO 回归分析

**重要发现**: 本次测试中, MLO 连接稳定进入双链路模式 (之前测试为单链路)。双链路模式下数据平面不通, 单链路模式下工作。可能原因:

1. `mac_set_links` 中 `active=0x1` 但 `valid=0x5`, 说明只有 link 0 (6GHz) 是活跃的, link 2 (2.4GHz) 尚未完全激活
2. ROC (Remain on Channel) 机制 (`roc=0`) 可能未正确完成第二链路的信道切换
3. 之前单链路成功可能因为 AP 侧配置变化或 wpa_supplicant scan 时序导致只发现了一个频段

### 4. AP 模式

| 测试项 | 结果 |
|--------|------|
| **wiphy 声明** | 支持 (Supported interface modes 包含 AP, AP/VLAN) |
| **创建 AP 接口** | 成功 (`iw dev wlp9s0 interface add wlp9s0ap type __ap`) |
| **接口 type** | AP |
| **独立 MAC** | f8:3d:c6:ec:a4:4c |
| **HE AP caps** | 支持 (BSR, BQR, OM Control, 1024-QAM TX/RX) |
| **EHT AP caps** | 支持 (EPCS, OM Control, SU/MU Beamformer, 320MHz) |
| **实际 AP 运行** | 未测试 (仅验证接口创建) |

**接口组合限制**:
- 组合1: managed/P2P-client <= 2 + P2P-GO <= 1 + P2P-device <= 1, total <= 3, channels <= 2
- 组合2: managed/P2P-client <= 2 + AP <= 1 + P2P-device <= 1, total <= 3, channels <= 1

### 5. Monitor 模式

| 测试项 | 结果 |
|--------|------|
| **wiphy 声明** | 支持 (software interface mode) |
| **切换到 Monitor** | 成功 |
| **接口 type** | monitor |
| **Active Monitor** | 支持 (will ACK incoming frames) |
| **恢复到 Managed** | 成功 |

### 6. P2P / Wi-Fi Direct

| 测试项 | 结果 |
|--------|------|
| **P2P-client** | 支持 |
| **P2P-GO** | 支持 (CT window, opportunistic powersave) |
| **P2P-device** | 支持 |
| **start_p2p_device** | 支持 (在 Supported commands 中) |

### 7. 安全协议 (WPA2/WPA3/SAE)

#### 7.1 支持的密码套件

| 密码 | OUI | 状态 |
|------|-----|------|
| WEP40 | 00-0f-ac:1 | 支持 |
| WEP104 | 00-0f-ac:5 | 支持 |
| TKIP | 00-0f-ac:2 | 支持 |
| **CCMP-128** | 00-0f-ac:4 | 支持 |
| **CCMP-256** | 00-0f-ac:10 | 支持 |
| **GCMP-128** | 00-0f-ac:8 | 支持 |
| **GCMP-256** | 00-0f-ac:9 | 支持 |
| CMAC | 00-0f-ac:6 | 支持 (PMF BIP) |
| CMAC-256 | 00-0f-ac:13 | 支持 |
| GMAC-128 | 00-0f-ac:11 | 支持 |
| GMAC-256 | 00-0f-ac:12 | 支持 |

#### 7.2 实测安全协议

| 协议 | 测试结果 |
|------|----------|
| **WPA2-PSK (CCMP)** | 认证成功, 数据平面 FAIL |
| **WPA3-SAE + PMF** | 认证成功, 数据平面 FAIL (非 MLO) |
| **WPA3-SAE + PMF (MLO 单链路)** | 完全工作 |
| **WPA3-SAE + PMF (MLO 双链路)** | 认证成功, 数据平面 FAIL |
| **SAE with AUTHENTICATE** | 支持 (Device supports SAE with AUTHENTICATE command) |
| **FILS** | 支持 (ext feature: FILS_STA) |

### 8. 电源管理 / 休眠恢复

#### 8.1 WoWLAN 支持

| 功能 | 状态 |
|------|------|
| **wake up on disconnect** | 支持 |
| **wake up on magic packet** | 支持 |
| **wake up on pattern match** | 支持 (最多 1 pattern, 1-128 bytes) |
| **GTK rekeying** | 支持 |
| **wake up on network detection** | 支持 (最多 10 match sets) |

#### 8.2 休眠恢复测试

**状态: 需要手动测试**

测试方法 (供手动执行):
```bash
# 1. 连接 WiFi 并确认数据平面工作
# 2. 记录连接状态
iw dev wlp9s0 link > /tmp/pre_suspend.txt
# 3. 挂起系统
systemctl suspend
# 4. 唤醒后检查
iw dev wlp9s0 link
ping -I <ip> -c 5 <router>
dmesg | grep -i "mt7925\|wlp9s0" | tail -30
```

### 9. 硬件 Reset

| 项目 | 结果 |
|------|------|
| **PCI Reset 文件** | `/sys/bus/pci/devices/0000:09:00.0/reset` (存在) |
| **Reset 方法** | `flr bus` (Function Level Reset + Bus Reset) |
| **FLR 支持** | 是 (但根据项目经验, FLR 可能导致设备不可恢复!) |
| **Bus Reset** | 支持 |

**警告**: 根据项目历史记录, `pcie_flr()` 可能导致设备进入 D3cold 且永不恢复。不建议在正常操作中使用 FLR。

### 10. TX 重传 / 吞吐量

#### 10.1 Station 统计 (MLO 单链路, 2.4GHz)

| 指标 | 值 |
|------|-----|
| **TX retries** | 0 |
| **TX failed** | 47 (异常高) |
| **RX drop misc** | 5 |
| **信号** | -22 dBm (非常强) |
| **信号均值** | -19 dBm |
| **TX bitrate** | 6.0 MBit/s (legacy, 异常低) |
| **RX bitrate** | 172.0 MBit/s (EHT-MCS 7, 2NSS) |
| **last ack signal** | -24 dBm |
| **connected time** | 46 seconds |

#### 10.2 Ping 吞吐 (MLO 单链路)

**小包 (56 bytes, 50 次, 100ms 间隔):**
- 发送: 50, 接收: 50, 丢包: 0%
- RTT min/avg/max/mdev = 2.147/7.048/36.340/6.311 ms

**大包 (1400 bytes, 100 次, 10ms 间隔):**
- 发送: 100, 接收: 91, 丢包: 9%
- RTT min/avg/max/mdev = 2.071/7.074/31.907/6.382 ms

#### 10.3 吞吐量分析

TX bitrate 锁定在 legacy 6.0 MBit/s 是一个已知问题。RX 方向可以达到 EHT 速率 (206.5 Mbps, MCS 8, 2NSS), 但受限于 2.4GHz 20MHz BW。TX 方向的速率协商似乎有 bug, 导致 TX failed 计数异常高。

### 11. wiphy 能力汇总

#### 11.1 支持的接口模式

- managed (station)
- AP
- AP/VLAN
- monitor (active monitor, software mode)
- P2P-client
- P2P-GO
- P2P-device

#### 11.2 频段覆盖

| 频段 | 信道 | 最大 TX 功率 | 备注 |
|------|------|-------------|------|
| 2.4GHz | 1-11 | 30 dBm | ch12-14 disabled (US reg) |
| 5GHz | 36-177 | 23-30 dBm | DFS ch52-144, UNII-3 ch149-165 无 DFS |
| 6GHz | 1-233 | 12 dBm | 全频段 no-IR (需 AFC 或 AP 广播) |

#### 11.3 支持的命令

new_interface, set_interface, new_key, start_ap, new_station, new_mpath, set_mesh_config, set_bss, authenticate, associate, deauthenticate, disassociate, join_ibss, join_mesh, remain_on_channel, set_tx_bitrate_mask, frame, frame_wait_cancel, set_wiphy_netns, set_channel, tdls_mgmt, tdls_oper, start_sched_scan, probe_client, set_noack_map, register_beacons, start_p2p_device, set_mcast_rate, connect, disconnect, channel_switch, set_qos_map, set_multicast_to_unicast, set_sar_specs, assoc_mlo_reconf

#### 11.4 扩展特性

- RRM
- SET_SCAN_DWELL
- BEACON_RATE (Legacy/HT/VHT/HE)
- FILS_STA
- CQM_RSSI_LIST
- CONTROL_PORT_OVER_NL80211 (含 TX_STATUS, NO_PREAUTH)
- ACK_SIGNAL_SUPPORT
- TXQS (FQ-CoDel)
- CAN_REPLACE_PTK0
- AIRTIME_FAIRNESS
- AQL (Airtime Queue Limits)
- SCAN_FREQ_KHZ
- POWERED_ADDR_CHANGE

#### 11.5 其他特性

- T-DLS 支持
- AP-side u-APSD
- TX status socket option
- HT-IBSS
- SAE with AUTHENTICATE
- Scan flush
- Per-vif TX power
- P2P GO CT window / opportunistic powersave
- vdev MAC-addr on create
- Randomizing MAC in scans / sched scans

---

## 已知问题和 Bug

### BUG-1: 非 MLO SSID 数据平面完全不通 (严重)

- **影响**: 所有三个频段 (2.4G/5G/6G) 的非 MLO SSID
- **症状**: 认证/关联成功, wpa_state=COMPLETED, 但 DHCP 超时, ARP 不可达
- **dmesg 线索**: 非 MLO 连接 `is_mld=0`, `link_num=0`, `valid=0x0`; 无 `RX 8023` 日志
- **可能原因**:
  1. 驱动在 MLO 补丁后, 非 MLD 模式的 RX 数据路径可能被破坏
  2. WCID 查找对非 MLD STA 可能返回错误
  3. 之前 (2026-03-04) 这些 SSID 工作的, 可能有驱动/固件状态残留
- **严重性**: Critical - 非 MLO 连接完全不可用

### BUG-2: MLO 双链路数据平面不通 (严重)

- **影响**: MLO 双链路模式 (valid_links=0x5)
- **症状**: 双链路建立成功 (link_num=2), 但 DHCP 和 ping 都不通
- **dmesg 线索**: `mac_set_links: active=0x1` (只有 link 0 活跃), `roc=0`
- **可能原因**:
  1. `ieee80211_set_active_links_async` 后第二链路未完全激活
  2. ROC 机制未正确处理双频段切换
  3. 需要 marcin-fm 的 STR MLO 补丁
- **严重性**: Critical - 双链路 MLO 不可用

### BUG-3: TX 速率锁定在 legacy 6.0 MBit/s (中等)

- **影响**: 所有模式 (包括工作的 MLO 单链路)
- **症状**: TX bitrate 始终报告 6.0 MBit/s, 不使用 HT/VHT/HE/EHT 速率
- **RX 正常**: RX 可达 EHT-MCS 8 (206.5 Mbps)
- **可能原因**:
  1. TX rate control 未正确初始化
  2. STA_REC 中 RA (Rate Adaptation) TLV 配置不正确
  3. 可能是 MT6639 特有的 TX rate 问题
- **严重性**: High - 严重限制上行性能

### BUG-4: TX failed 计数异常高 (中等)

- **影响**: MLO 模式
- **症状**: TX failed=47 (46s 内), TX retries=0, 信号 -19 dBm
- **可能原因**: 与 BUG-3 相关, legacy rate 的 TX 成功率低
- **严重性**: Medium

---

## 功能完整度评估

| 功能类别 | 完整度 | 状态 |
|----------|--------|------|
| **硬件检测/初始化** | 100% | 完全正常 |
| **wiphy 能力注册** | 100% | EHT/HE/VHT/HT 全部正确 |
| **扫描** | 100% | 三频段扫描正常 |
| **WPA2-PSK 认证** | 100% | 全频段 COMPLETED |
| **WPA3-SAE 认证** | 100% | 6GHz/MLO COMPLETED |
| **AP 接口创建** | 50% | 接口创建成功, 未测试实际运行 |
| **Monitor 模式** | 100% | 切换和恢复均正常 |
| **P2P** | 50% | wiphy 声明支持, 未实际测试 |
| **非 MLO 数据平面** | 0% | 完全不通 (BUG-1) |
| **MLO 单链路数据平面** | 90% | 工作但 TX 速率异常 |
| **MLO 双链路数据平面** | 0% | 不通 (BUG-2) |
| **TX Rate Adaptation** | 10% | legacy 6.0 Mbps 锁定 (BUG-3) |
| **RX Rate Adaptation** | 80% | EHT-MCS 8 正常 |
| **WoWLAN** | 未测试 | 需手动测试 |
| **休眠恢复** | 未测试 | 需手动测试 |

**综合评分**: 约 40% 功能可用 (控制平面基本完整, 数据平面严重受限)

---

## 与 Intel BE200 对比 (参考)

| 功能 | MT6639 (mt7925e v2.1) | Intel BE200 (iwlwifi) |
|------|----------------------|----------------------|
| **驱动成熟度** | Alpha/实验性 (社区补丁) | 稳定 (Intel 官方) |
| **2.4G/5G 数据平面** | 不通 (BUG-1) | 稳定工作 |
| **6G 数据平面** | 仅 MLO 单链路可用 | 稳定工作 |
| **EHT 320MHz** | wiphy 声明支持, 未达到 PHY 速率 | 工作, 实测 5+ Gbps |
| **MLO** | 单链路部分工作 | 双链路工作 (上游进行中) |
| **AP 模式** | 接口创建成功 | 完整支持 |
| **Monitor 模式** | 工作 | 工作 |
| **TX Rate Control** | 锁定 legacy 6 Mbps | 正常 EHT 速率 |
| **WoWLAN** | 硬件支持, 未测试 | 完整支持 |
| **上游内核支持** | 无 (仅 AUR DKMS) | 是 (iwlwifi) |

---

## 结论

MT6639/MT7927 在 AUR 驱动 v2.1 下的功能状态:

1. **控制平面** (扫描, 认证, 关联, 密钥协商) 在所有模式下工作良好
2. **数据平面** 仅在 MLO 单链路模式下工作, 非 MLO 和 MLO 双链路模式均不通
3. **TX 速率适配** 存在严重 bug, 锁定在 legacy 6 Mbps
4. **硬件能力** 完整 (EHT, 320MHz, 4K-QAM, 2T2R), 但驱动未能充分利用
5. **距离日常可用仍有较大差距**, 主要阻碍是数据平面稳定性和 TX rate control

建议关注 marcin-fm 的后续补丁, 特别是 band_idx/ROC 修复和 STR MLO 支持。
