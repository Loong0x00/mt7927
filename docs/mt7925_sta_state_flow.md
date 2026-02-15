# MT7925 STA_REC Update Flow at sta_state Transitions

## 概述

本文档详细记录 mt7925 驱动在各个 `sta_state` 转换阶段发送的 STA_REC_UPDATE MCU 命令的具体参数。

## sta_state 转换流程 (mac80211 回调)

```
NOTEXIST → NONE       mt76_sta_add() → mt7925_mac_sta_add()
NONE → AUTH           无操作 (sta_event 不触发)
AUTH → ASSOC          mt7925_mac_sta_event(MT76_STA_EVENT_ASSOC)
ASSOC → AUTHORIZED    无操作 (mt7925 忽略此事件)
```

### 1. NOTEXIST → NONE (sta_add)

**调用路径**: `mt76_sta_state()` → `mt76_sta_add()` → `mt7925_mac_sta_add()` → `mt7925_mac_link_sta_add()`

**操作顺序**:
1. 分配 WCID (WTBL index)
2. 初始化 wcid 结构体
3. `mt7925_mac_wtbl_update(idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR)` — 清空 WTBL 条目
4. **BSS_INFO_UPDATE** (仅 STATION 模式): `mt7925_mcu_add_bss_info(link_sta, false)` — 关联前更新 BSS 信息
5. **STA_REC_UPDATE**: `mt7925_mcu_sta_update(link_sta, vif, enable=true, MT76_STA_INFO_STATE_NONE)`

**STA_REC_UPDATE 参数** (non-MLD 场景):
```c
enable = true
state = MT76_STA_INFO_STATE_NONE  // 0
newly = true  // (state != ASSOC)
conn_state = CONN_STATE_PORT_SECURE  // 2 (enable=true)
```

**TLVs 包含** (按添加顺序):
1. **STA_REC_BASIC** (tag=0x00):
   - `conn_state = CONN_STATE_PORT_SECURE (2)`
   - `extra_info = EXTRA_INFO_VER | EXTRA_INFO_NEW (0x03)` — 因为 newly=true
   - `conn_type = CONNECTION_INFRA_AP (1)` — STA 模式
   - `peer_addr = AP MAC`
   - `aid = vif->cfg.aid`
   - `qos = 1`

2. **STA_REC_PHY** (tag=0x15): 带宽、NSS、LDPC 等物理层参数

3. **STA_REC_HT** (tag=0x02): HT capabilities (如果支持)

4. **STA_REC_VHT** (tag=0x03): VHT capabilities (如果支持)

5. **STA_REC_APPS** (tag=0x04): UAPSD 配置 (如果 AP 模式 + STA->wme)

6. **STA_REC_AMSDU** (tag=未知): AMSDU 参数

7. **STA_REC_HE** (tag=0x11): HE capabilities (如果支持)

8. **STA_REC_HE_6G** (tag=0x17): 6GHz HE capabilities (如果支持)

9. **STA_REC_EHT** (tag=0x22): EHT capabilities (如果支持)

10. **STA_REC_RA_INFO** (tag=未知): 速率控制信息

11. **STA_REC_STATE** (tag=未知):
    - `state = MT76_STA_INFO_STATE_NONE (0)`
    - `vht_opmode = bandwidth | (rx_nss << 4)`

12. **STA_REC_HDR_TRANS** (tag=未知):
    - `dis_rx_hdr_tran = true`
    - `to_ds = true` (STA 模式)
    - `from_ds = false`

**注意**: mt7925 在 STA_REC_UPDATE 中**不包含 STA_REC_WTBL** TLV！WTBL 通过独立的 `mt7925_mac_wtbl_update()` 操作更新。

**UniCmd 参数**:
```c
cmd = MCU_UNI_CMD(STA_REC_UPDATE)  // 0x19
option = 0x01 (wait_resp=true)  // mt76_mcu_skb_send_msg(..., true)
```

### 2. NONE → AUTH

**无操作** — mt7925 不在此转换发送任何 MCU 命令。

### 3. AUTH → ASSOC (sta_assoc)

**调用路径**: `mt76_sta_state()` → `dev->drv->sta_event(MT76_STA_EVENT_ASSOC)` → `mt7925_mac_sta_event()` → `mt7925_mac_link_sta_assoc()`

**操作顺序**:
1. **BSS_INFO_UPDATE** (仅 STATION 模式): `mt7925_mcu_add_bss_info(link_sta, true)` — 关联后更新 BSS 信息
2. `mt7925_mac_wtbl_update(idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR)` — 清空 admission count
3. **STA_REC_UPDATE**: `mt7925_mcu_sta_update(link_sta, vif, enable=true, MT76_STA_INFO_STATE_ASSOC)`

**STA_REC_UPDATE 参数**:
```c
enable = true
state = MT76_STA_INFO_STATE_ASSOC  // 2
newly = false  // (state != ASSOC) → false!
conn_state = CONN_STATE_PORT_SECURE  // 2
```

**TLVs 包含** (同 sta_add，但关键差异):
- **STA_REC_BASIC**:
  - `extra_info = EXTRA_INFO_VER (0x01)` — **不含 EXTRA_INFO_NEW** (newly=false)
  - 其余字段同 sta_add

- **STA_REC_STATE**:
  - `state = MT76_STA_INFO_STATE_ASSOC (2)` — **关键变化!**

- **新增 TLVs** (仅当 state != NONE):
  - **STA_REC_MLD** (tag=0x20): MLO 相关信息 (仅 MLD 模式)
  - **STA_REC_EHT_MLD** (tag=0x21): EHT MLO 信息 (仅 MLD 模式)

### 4. ASSOC → AUTHORIZED

**无操作** — mt7925_mac_sta_event() 只处理 `MT76_STA_EVENT_ASSOC`，忽略 `MT76_STA_EVENT_AUTHORIZE`。

### 5. sta_remove (NONE → NOTEXIST)

**调用路径**: `mt76_sta_state()` → `mt76_sta_remove()` → `mt7925_mac_sta_remove()` → `mt7925_mac_link_sta_remove()`

**操作顺序**:
1. `mt7925_roc_abort_sync()` — 取消 ROC
2. `mt76_connac_free_pending_tx_skbs()` — 释放待发送的 SKB
3. **STA_REC_UPDATE**: `mt7925_mcu_sta_update(link_sta, vif, enable=false, MT76_STA_INFO_STATE_NONE)`
4. `mt7925_mac_wtbl_update(idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR)`
5. **BSS_INFO_UPDATE**: `mt7925_mcu_add_bss_info(link_sta, false)` — 移除 BSS 信息

**STA_REC_UPDATE 参数**:
```c
enable = false  // ← 关键!
state = MT76_STA_INFO_STATE_NONE
conn_state = CONN_STATE_DISCONNECT  // 0 (enable=false)
```

**TLVs 包含** (极简):
- **STA_REC_REMOVE** (tag=0x25): `action=0`
- **STA_REC_MLD_OFF** (tag=未知): 空 TLV

**注意**: 删除时**不包含 STA_REC_BASIC、PHY、HT/VHT/HE 等 TLVs**，只发送 REMOVE 和 MLD_OFF。

## BSS_INFO_UPDATE 与 STA_REC 的关系

| sta_state 转换 | BSS_INFO_UPDATE 时机 | enable 参数 |
|----------------|----------------------|------------|
| NOTEXIST→NONE (sta_add) | STA_REC **之前** | false (关联前) |
| AUTH→ASSOC (sta_assoc) | STA_REC **之前** | true (关联后) |
| NONE→NOTEXIST (sta_remove) | STA_REC **之后** | false (移除) |

**规律**: `BSS_INFO_UPDATE` 始终在 STATION 模式下发送，且在 STA_REC_UPDATE 之前 (添加/更新) 或之后 (移除)。

## UniCmd option 参数

**所有 STA_REC_UPDATE 命令使用**:
```c
mt76_mcu_skb_send_msg(dev, skb, MCU_UNI_CMD(STA_REC_UPDATE), wait_resp=true)
```

这对应 **option=0x01** (等待响应)，**不是** 0x06 (SET) 或 0x07 (SET_ACK)。mt76 UniCmd 框架会自动处理 option 字段。

## 关键发现 (vs. MT7927 当前实现)

1. **mt7925 不使用 STA_REC_WTBL**:
   - mt7925 没有在 STA_REC_UPDATE 中嵌套 WTBL TLV
   - WTBL 通过独立的 `mt7925_mac_wtbl_update()` 寄存器操作更新
   - **MT7927 可能不需要在 STA_REC 中包含 WTBL**

2. **conn_state 始终是 PORT_SECURE 或 DISCONNECT**:
   - sta_add/sta_assoc: `conn_state=CONN_STATE_PORT_SECURE (2)`
   - sta_remove: `conn_state=CONN_STATE_DISCONNECT (0)`
   - **从不使用 CONN_STATE_CONNECT (1)** — 这是 AP 模式或其他芯片的行为

3. **extra_info 的 EXTRA_INFO_NEW 标志**:
   - 仅在 `newly=true` 时设置 (即 state != ASSOC)
   - sta_add: `extra_info=0x03` (VER+NEW)
   - sta_assoc: `extra_info=0x01` (仅 VER)

4. **state 字段的作用**:
   - `MT76_STA_INFO_STATE_NONE (0)`: 初始添加、删除
   - `MT76_STA_INFO_STATE_ASSOC (2)`: 关联后
   - `MT76_STA_INFO_STATE_AUTH (1)`: **mt7925 从不使用**

5. **BSS_INFO_UPDATE 必须在 STA_REC 之前** (添加/关联时):
   - 固件需要先知道 BSS 上下文才能处理 STA_REC
   - 这可能是 MT7927 当前实现缺失的关键步骤

6. **MLD TLVs 仅在 state=ASSOC 时发送**:
   - `STA_REC_MLD` 和 `STA_REC_EHT_MLD` 只在 state != NONE 时添加
   - 单链路模式 (non-MLO) 不需要这些 TLV

## MT7927 实现建议

基于 mt7925 分析，MT7927 驱动应在以下阶段调用 STA_REC_UPDATE:

1. **sta_add (NOTEXIST→NONE)**:
   ```c
   mt7925_mcu_add_bss_info(vif, NULL, true);  // 先更新 BSS
   mt7925_mcu_sta_update(NULL, vif, true, MT76_STA_INFO_STATE_NONE);
   ```
   - `link_sta=NULL` → 使用 mvif->sta (broadcast WCID)
   - `conn_state=PORT_SECURE`, `extra_info=VER+NEW`, `state=0`
   - TLVs: BASIC (broadcast addr) + HDR_TRANS

2. **bss_info_changed (BSS_CHANGED_ASSOC)**:
   ```c
   if (vif->cfg.assoc)
       mt7925_mcu_sta_update(NULL, vif, true, MT76_STA_INFO_STATE_ASSOC);
   ```
   - 关联成功后更新 state=ASSOC
   - **这是 mt7925 发送 STA_REC 的主要时机**

3. **可选: mgd_prepare_tx (ROC for auth/assoc)**:
   - mt7925 在某些场景下也会调用 STA_REC_UPDATE
   - 但对于基本连接流程不是必须的

**注意**: MT7927 当前缺少 `BSS_INFO_UPDATE` 和 `STA_REC_UPDATE` (broadcast WCID)，这可能是认证帧发不出去的根本原因 — 固件不知道我们要连接哪个 BSS，也没有为 AP 创建 WTBL 条目。

## 参考代码位置

- **sta_state 回调**: `mt76/mac80211.c` — `mt76_sta_state()`
- **sta_event 分发**: `mt76/mt7925/main.c` — `mt7925_mac_sta_event()`
- **sta_add 实现**: `mt76/mt7925/main.c` — `mt7925_mac_link_sta_add()`
- **sta_assoc 实现**: `mt76/mt7925/main.c` — `mt7925_mac_link_sta_assoc()`
- **STA_REC 命令构建**: `mt76/mt7925/mcu.c` — `mt7925_mcu_sta_cmd()`
- **BASIC TLV**: `mt76/mt76_connac_mcu.c` — `mt76_connac_mcu_sta_basic_tlv()`

## 附录: conn_state 定义

```c
#define CONN_STATE_DISCONNECT    0  // 删除 STA
#define CONN_STATE_CONNECT       1  // AP 模式或其他芯片
#define CONN_STATE_PORT_SECURE   2  // STA 模式 (mt7925 始终用此)
```

## 附录: MT76_STA_INFO_STATE 定义

```c
enum {
    MT76_STA_INFO_STATE_NONE = 0,   // 初始添加、删除
    MT76_STA_INFO_STATE_AUTH = 1,   // mt7925 不使用
    MT76_STA_INFO_STATE_ASSOC = 2   // 关联成功
};
```

## 附录: EXTRA_INFO 标志

```c
#define EXTRA_INFO_VER   BIT(0)  // 始终设置
#define EXTRA_INFO_NEW   BIT(1)  // newly=true 时设置 (state != ASSOC)
```
