# RX 帧处理问题: Ring 4 有帧但 Auth-2 未到达 mac80211

**创建**: Session 39 (2026-02-23)
**前置**: Ring 4 DIDX=0 问题已修复 (MDP BNRCFR)

## 问题一句话描述

Ring 4 DMA 投递活跃 (DIDX 每秒增长 50-100)，但 Auth-2 响应帧未到达 mac80211，认证仍然超时。

## 现象

```
band0 BNRCFR BEFORE: CFR0=0x55400154 CFR1=0x09000000
band0 BNRCFR AFTER:  CFR0=0x55400004 CFR1=0x01000000

PRE-AUTH Ring4: BASE=0x4d8fc000 CNT=256 CIDX=98 DIDX=98
send auth to b4:ba:12:5b:63:c8 (try 1/3)
  Band0: TX_OK=30 TX20=30 RX_OK=0
  RX DIDX: R4=92 R6=27 R7=0
send auth to b4:ba:12:5b:63:c8 (try 2/3)
  Band0: TX_OK=30 TX20=30 RX_OK=0
  RX DIDX: R4=199 R6=28 R7=0
send auth to b4:ba:12:5b:63:c8 (try 3/3)
  Band0: TX_OK=30 TX20=30 RX_OK=0
  RX DIDX: R4=55 R6=29 R7=0
authentication with b4:ba:12:5b:63:c8 timed out
AUTH-FAIL Ring 4: BASE=0x4d8fc000 CNT=256 CIDX=184 DIDX=184
```

### 关键观察

| 指标 | 修复前 (S31-S38) | 修复后 (S39) |
|------|-----------------|-------------|
| Ring 4 DIDX | **始终=0** | **活跃: 98→92→199→55→184** |
| Ring 6 DIDX | 正常前进 | 正常前进 |
| Ring 7 DIDX | 0 | 0 |
| MIB TX_OK | 30 | 30 |
| MIB RX_OK | 0 | 0 |
| Auth 结果 | 超时 | 超时 |
| 扫描 BSS | 160 | 163 |

## 已确认正常的部分

### Ring 4 DMA 投递 ✅ (S39)
- BNRCFR 修复后 Ring 4 DIDX 活跃前进
- CIDX = DIDX → NAPI 正在消费帧 (ring 没有溢出)
- 每次 auth 后 DIDX 增长 50-100 (大量帧进入)

### BNRCFR 配置 ✅ (S39)
- BNRCFR0: 0x55400154 → 0x55400004 (MCU_RX_MGMT/CTL 从 WM→HIF)
- BNRCFR1: 0x09000000 → 0x01000000 (RX_DROPPED_UCAST 从 WM→HIF)
- 两次重启后值一致，不是偶然

### TX 全链路 ✅ (S31)
- AR9271 空口抓包: Auth-1 帧在空口
- AP 回了 Auth-2

## 待调查方向

### 方向 1: NAPI poll 是否正确处理 Ring 4 帧 (概率: 高)
CIDX=DIDX 说明 NAPI 在消费帧，但帧可能被丢弃而非投递到 mac80211。
需要在 NAPI poll 中加诊断:
- 每帧的 PKT_TYPE 是什么？
- RXD 解析是否成功？
- `ieee80211_rx_napi()` 是否被调用？
- 帧是否被 mac80211 的 RX filter 丢弃？

### 方向 2: RXD 格式不匹配 (概率: 高)
Ring 4 的帧可能用不同的 RXD 格式 (vs Ring 6 MCU 事件)。
- Ring 6 帧: PKT_TYPE=7 (RX_EVENT), 有 16 字节 UniEvent header
- Ring 4 帧: PKT_TYPE 应该是 2 (NORMAL) — 需要确认
- CONNAC3 NORMAL RXD 有多种长度 (DW0-DW7 或更长)
- 如果 RXD 解析器只处理 RX_EVENT 而不处理 NORMAL → 帧全部丢弃

### 方向 3: MIB RX_OK=0 的含义 (概率: 中)
Ring 4 有大量帧但 MIB RX_OK=0，可能的解释:
1. Ring 4 帧是固件转发帧 (不经过射频 MIB 计数器)
2. MIB read-to-clear 时序问题 (读了 probe 时的 0，auth 时还没来得及更新)
3. 射频确实没收到帧，Ring 4 的帧是其他类型 (firmware events, TXS, etc.)
4. MIB 基地址在 BNRCFR 修复后可能需要重新验证

### 方向 4: RX Header Translation (概率: 中)
MDP_DCR0 BIT(19) = RX_HDR_TRANS_EN = 1 (固件启用)。
如果 RX Header Translation 把 802.11 头转成 802.3 头，
但我们的 RXD 解析器期望 802.11 头 → 帧格式不匹配 → 丢弃或错误处理。

### 方向 5: Ring 4 收到的帧全是 beacon/probe 而非 Auth-2 (概率: 低)
每次 auth 后 DIDX +50-100，数量远大于 auth 帧数。
可能是周围 AP 的 beacon 和 probe response 帧。
Auth-2 可能混在其中但因 RXD 解析错误被丢弃。

## Ring 4 vs Ring 6 帧处理路径

```
Ring 6 (MCU Events) — 已验证工作:
  DMA → NAPI poll → check PKT_TYPE=7(RX_EVENT) → mt7927_mcu_rx_event()
  → 解析 UniEvent header → 分发到 scan/ROC/TXFREE 处理器

Ring 4 (Data/Mgmt RX) — 需要验证:
  DMA → NAPI poll → check PKT_TYPE=2(NORMAL) → ???
  → 解析 RXD (DW0-DW7) → 构造 skb → ieee80211_rx_napi()
  → mac80211 处理 Auth-2 → wpa_supplicant
```

关键问题: Ring 4 的 NAPI 处理函数是否实现了 PKT_TYPE=NORMAL 的完整路径？

## BNRCFR 修复详情 (供参考)

### 寄存器定义 (mt7925/regs.h)
```c
MT_MDP_BNRCFR0(band) = MDP_BASE + 0x090 + (band << 8)
  [5:4]  MCU_RX_MGMT         — 管理帧路由: 0=HIF, 1=WM
  [7:6]  MCU_RX_CTL_NON_BAR  — 控制帧路由: 0=HIF, 1=WM
  [9:8]  MCU_RX_CTL_BAR      — BAR 帧路由: 0=HIF, 1=WM

MT_MDP_BNRCFR1(band) = MDP_BASE + 0x094 + (band << 8)
  [23:22] MCU_RX_BYPASS       — bypass 路由
  [28:27] RX_DROPPED_UCAST    — 丢弃单播路由
  [30:29] RX_DROPPED_MCAST    — 丢弃多播路由

MT_MDP_TO_HIF = 0  (路由到 Host Ring)
MT_MDP_TO_WM  = 1  (路由到 MCU 固件内部)
```

### 修复代码 (src/mt7927_pci.c mac_init_band)
```c
mt7927_rmw(dev, MT_MDP_BNRCFR0(band),
           MT_MDP_RCFR0_MCU_RX_MGMT |
           MT_MDP_RCFR0_MCU_RX_CTL_NON_BAR |
           MT_MDP_RCFR0_MCU_RX_CTL_BAR, 0);
mt7927_rmw(dev, MT_MDP_BNRCFR1(band),
           MT_MDP_RCFR1_MCU_RX_BYPASS |
           MT_MDP_RCFR1_RX_DROPPED_UCAST |
           MT_MDP_RCFR1_RX_DROPPED_MCAST, 0);
```

## 建议下一步

1. **在 NAPI poll 加 Ring 4 帧诊断**: 打印前几帧的 PKT_TYPE、RXD DW0-DW3、帧长度
2. **检查 RXD 解析器**: 确认 mt7927_mac.c 中是否有 NORMAL (PKT_TYPE=2) 帧的处理路径
3. **检查 RX Header Translation**: 如果 HdrTrans 启用，需要处理 802.3 格式帧
4. **对比 Ring 6 NAPI 路径**: Ring 6 工作正常，Ring 4 应该走类似但不同的路径
