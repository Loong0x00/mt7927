# TXD/STA_REC 分析报告 — Auth Frame stat=1 Root Cause

## 1. TXD DW0-DW7 逐字段解析

测试 dump:
```
DW0: 0x2000003e  DW1: 0x800c9001  DW2: 0x0000000b  DW3: 0x10007800
DW4: 0x00000000  DW5: 0x00000000  DW6: 0x000f001c  DW7: 0x00000000
```

### DW0: 0x2000003e
| 字段 | 位域 | 值 | 含义 | 正确? |
|------|------|-----|------|-------|
| TX_BYTES | [15:0] | 0x003e = 62 | TXD(32) + auth帧(30) = 62 | ✅ |
| PKT_FMT | [24:23] | 0 | MT_TX_TYPE_CT (Cut-Through) | ✅ |
| Q_IDX | [31:25] | 0x10 | MT_LMAC_ALTX0 (管理帧队列) | ✅ |

### DW1: 0x800c9001
| 字段 | 位域 | 值 | 含义 | 正确? |
|------|------|-----|------|-------|
| WLAN_IDX | [11:0] | 1 | STA WCID=1 | ✅ |
| **TGID** | **[13:12]** | **1** | **band_idx=1 (5GHz DBDC)** | **✅** |
| HDR_FORMAT | [15:14] | 2 | MT_HDR_FORMAT_802_11 | ✅ |
| HDR_INFO | [20:16] | 12 | hdr_len=24 bytes (24/2=12) | ✅ |
| TID | [24:21] | 0 | 管理帧 TID=0 | ✅ |
| **OWN_MAC** | **[30:25]** | **0** | **omac_idx=0** | **✅** |
| FIXED_RATE | [31] | 1 | 管理帧固定速率 | ✅ |

### DW2: 0x0000000b
| 字段 | 位域 | 值 | 含义 | 正确? |
|------|------|-----|------|-------|
| SUB_TYPE | [3:0] | 0xB | Auth (0x00B0 >> 4 = 0xB) | ✅ |
| FRAME_TYPE | [5:4] | 0 | Management (0x0000 >> 2 = 0) | ✅ |
| HDR_PAD | [11:10] | 0 | 无加密,无填充 | ✅ |

### DW3: 0x10007800
| 字段 | 位域 | 值 | 含义 | 正确? |
|------|------|-----|------|-------|
| NO_ACK | [0] | 0 | Auth 需要 ACK | ✅ |
| PROTECT_FRAME | [1] | 0 | 无加密 | ✅ |
| BCM | [4] | 0 | 单播帧 | ✅ |
| **REM_TX_COUNT** | **[15:11]** | **15** | **最多重试15次** | **✅** |
| BA_DISABLE | [28] | 1 | 固定速率禁用BA | ✅ |

### DW4: 0x00000000 — 无 PN (未加密) ✅
### DW5: 0x00000000 — PID=0, 无 TX status 请求 ✅
### DW6: 0x000f001c
| 字段 | 位域 | 值 | 含义 | 正确? |
|------|------|-----|------|-------|
| DAS | [2] | 1 | 地址搜索 | ✅ |
| DIS_MAT | [3] | 1 | 禁用 MAT | ✅ |
| MSDU_CNT | [9:4] | 1 | 单个 MSDU | ✅ |
| **TX_RATE** | **[21:16]** | **15** | **速率表索引15=OFDM 6Mbps** | **✅** |

### DW7: 0x00000000 — 空 ✅

### TXD 结论: **所有字段正确, TXD 不是 stat=1 的原因**

---

## 2. 与 mt7925 TXD 代码对比

### 关键差异

| 方面 | mt7925 | 我们的代码 | 影响 |
|------|--------|-----------|------|
| FIXED_RATE 条件 | `!is_data \|\| multicast \|\| USE_MINRATE` | `!is_data` | 无影响 (auth是mgmt) |
| TX_RATE 来源 | `mvif->basic_rates_idx` (动态) | 硬编码 11/15 | 无影响 (我们有正确的rate table) |
| 802.11 TID | ADDBA=MT_TX_ADDBA, mgmt=MT_TX_NORMAL | 0 for non-QoS data | 无影响 |
| BCM 设置位置 | 在 80211 子函数中 | 在主函数中 | 无影响 |

### 结论: TXD 构建代码**基本正确**, 与 mt7925 功能等价。

---

## 3. STA_REC 分析 — ⚠️ 发现关键 Bug

### Bug #1 (严重度: **致命**): conn_state = DISCONNECT 而非 PORT_SECURE

**mt7925 (正确)**:
```c
// mt7925_mcu_sta_cmd() line 1974:
conn_state = info->enable ? CONN_STATE_PORT_SECURE : CONN_STATE_DISCONNECT;
// → enable=true 时永远发送 PORT_SECURE(2)
```

**我们的代码 (错误)**:
```c
// sta_state: NOTEXIST → NONE:
mt7927_mcu_sta_update(dev, vif, sta, true, CONN_STATE_DISCONNECT);
// → 发送 conn_state=DISCONNECT(0)
```

**影响**: 固件创建了 WTBL 条目, 但将 STA 标记为 "断开连接"。当 TX auth 帧时, 固件因为目标 STA 处于 DISCONNECT 状态, **拒绝发射帧** → TXFREE stat=1!

**修复**:
```c
// NOTEXIST → NONE:
mt7927_mcu_sta_update(dev, vif, sta, true, CONN_STATE_PORT_SECURE);
```

### Bug #2 (严重度: **高**): EXTRA_INFO_NEW 逻辑与 mt7925 不同

**mt7925** (`mt76_connac_mcu_sta_basic_tlv`):
```c
if (newly && conn_state != CONN_STATE_DISCONNECT)
    basic->extra_info |= cpu_to_le16(EXTRA_INFO_NEW);
basic->conn_state = conn_state;
// → newly=true + PORT_SECURE → NEW 被设置
```

**我们的代码**:
```c
if (conn_state == CONN_STATE_DISCONNECT)
    req.basic.extra_info |= cpu_to_le16(EXTRA_INFO_NEW);
// → DISCONNECT → NEW 被设置 (条件逻辑完全不同!)
```

**当前行为**: conn_state=DISCONNECT 时设 NEW → WTBL 条目被创建, 但 conn_state=0。
**mt7925 行为**: conn_state=PORT_SECURE 且 newly=true 时设 NEW → WTBL 条目被创建, conn_state=2。

最终 NEW 位在两种实现中都设置了 (因为触发条件刚好覆盖), 但 conn_state 值不同! **conn_state=0 vs conn_state=2 是关键差异。**

**修复**: 改为 mt7925 逻辑 — 使用 `newly` 参数:
```c
// 传入 newly 参数
if (newly && conn_state != CONN_STATE_DISCONNECT)
    req.basic.extra_info |= cpu_to_le16(EXTRA_INFO_NEW);
```

### Bug #3 (严重度: 中): 缺少 STA_REC_PHY TLV

mt7925 发送 **STA_REC_PHY** 包含:
- `phy_type` — 对端 PHY 模式 (OFDM/HT/VHT/HE)
- `basic_rate` — 基本速率位图
- `ampdu` — AMPDU 参数

我们完全缺少这个 TLV。固件可能需要它来初始化内部速率选择。

### Bug #4 (严重度: 低): STA_REC_STATE 映射不准确

我们的映射基于 conn_state, mt7925 直接使用 `mt76_sta_info_state`:
- 当前: conn_state=DISCONNECT → state=0 (NONE) ← 对 NOTEXIST→NONE 碰巧正确
- 后续: conn_state=CONNECT → state=2 (ASSOC) ← 对 AUTH→ASSOC 正确
- 但: 缺少 state=1 (AUTH) 的路径

### TLV 对比

| TLV | mt7925 | 我们 | 必要性 |
|-----|--------|------|--------|
| STA_REC_BASIC (0x00) | ✅ | ✅ | 必须 |
| STA_REC_RA (0x01) | ✅ | ✅ | 必须 |
| **STA_REC_PHY (0x02)** | **✅** | **❌** | **建议** |
| STA_REC_HT (0x03) | ✅ | ❌ | 可选 (auth阶段) |
| STA_REC_VHT (0x04) | ✅ | ❌ | 可选 |
| STA_REC_UAPSD (0x06) | ✅ | ❌ | 可选 |
| STA_REC_STATE (0x07) | ✅ | ✅ | 必须 |
| STA_REC_HE (0x09) | ✅ | ❌ | 可选 |
| STA_REC_HW_AMSDU (0x0A) | ✅ | ❌ | 可选 |
| STA_REC_HDR_TRANS (0x2B) | ✅ | ✅ | 推荐 |

---

## 4. BSS_INFO 分析

### conn_state = !enable 逻辑

```c
req.basic.conn_state = !enable;
// enable=true → conn_state=0 (DISCONNECT)
// enable=false → conn_state=1 (CONNECT)
```

这个映射看起来反直觉, 但考虑到 CONN_STATE 定义:
- CONN_STATE_DISCONNECT = 0
- CONN_STATE_CONNECT = 1

对于 BSS_INFO, mt76 通用代码中 conn_state 的语义可能与 STA_REC 不同。
BSS active 时 conn_state=0, inactive 时 conn_state=1 — 在多处 mt76 代码中有此模式。
**暂不认为是 bug, 但需留意。**

### phymode 0x31 (5GHz)

0x31 = PHY_MODE_A | PHY_MODE_AN | PHY_MODE_AC
对于 5GHz WiFi 5 路由器, 这是合理的。✅

---

## 5. 结论与修复优先级

### 问题排序 (按导致 stat=1 的可能性)

| 优先级 | 问题 | 修复方案 |
|--------|------|----------|
| **P0** | **STA_REC conn_state=DISCONNECT** | NOTEXIST→NONE 改为 `PORT_SECURE` |
| **P0** | **EXTRA_INFO_NEW 逻辑** | 用 `newly` 参数, 匹配 mt7925 |
| P1 | 缺少 STA_REC_PHY TLV | 添加 PHY TLV (phy_type + basic_rate) |
| P2 | STA_REC_STATE 映射 | 使用独立 state 参数 |
| P3 | 缺少 HT/VHT/HE TLV | ASSOC 后再加 |

### 最可能的 stat=1 根因

**STA_REC_BASIC.conn_state = DISCONNECT(0)** — 固件将目标 STA 视为断开连接,
拒绝通过空口发送任何帧给它。改为 **PORT_SECURE(2)** 应该能解决 auth 帧被丢弃的问题。

这与 TXFREE stat=1 + count=15 (重试15次全部失败) 完全一致:
固件内部状态不允许 TX → 直接丢弃, 重试也没用。
