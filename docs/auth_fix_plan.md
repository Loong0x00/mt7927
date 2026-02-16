# Auth Frame 修复计划 — TXFREE stat=1 + ROC Abort 超时

**日期**: 2026-02-16
**汇总**: code-fixer (基于 txd-analyst + win-reverser + ROC 分析)
**状态**: 等待 team-lead 审批

---

## 一、问题概述

两个独立问题导致 auth 帧无法发出：

1. **TXFREE stat=1** — 固件拒绝发射 auth 帧 (所有 15 次重试均丢弃)
2. **ROC abort 后超时** — 第二次 ROC acquire 永远收不到 GRANT

---

## 二、根因分析

### 根因 #1 (P0): STA_REC conn_state=DISCONNECT

**发现者**: txd-analyst

**问题**: `mt7927_pci.c` 中 `sta_state(NOTEXIST→NONE)` 发送 STA_REC 时用了 `CONN_STATE_DISCONNECT(0)`。

mt7925 参考 (`mt7925_mcu_sta_cmd` line 1974):
```c
conn_state = info->enable ? CONN_STATE_PORT_SECURE : CONN_STATE_DISCONNECT;
// enable=true → 永远用 PORT_SECURE(2)
```

我们的代码:
```c
mt7927_mcu_sta_update(dev, vif, sta, true, CONN_STATE_DISCONNECT);
// → conn_state=0, 固件认为 STA "已断开" → 拒绝发帧
```

**影响**: 这是 stat=1 的直接原因。固件创建了 WTBL 条目但标记为 DISCONNECT，拒绝通过空口发送帧。

### 根因 #2 (P0): ROC ABORT fire-and-forget

**发现者**: code-fixer (ROC 分析)

**问题**: `mt7927_pci.c:3118` abort 用 `option=0x06` (无 ACK)，不等固件确认。

mt7925 参考:
```c
mt76_mcu_send_msg(&dev->mt76, MCU_UNI_CMD(ROC), &req, sizeof(req), true);
// wait=true → 等固件 ACK
```

我们的代码:
```c
mt7927_mcu_send_unicmd(dev, 0x27, UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET, ...);
// option=0x06 → BIT(0) ACK 未设 → mcu_wait_resp() 不会被调用
msleep(10);  // 仅靠猜测等待 10ms
```

**影响**: 10ms 不够固件处理完 abort → 新 acquire 被静默拒绝 → ROC_GRANT 永不到来 → 4s 超时。

---

## 三、完整修复清单 (按优先级)

### Fix 1+2 (P0): 重构 mt7927_mcu_sta_update() — conn_state + EXTRA_INFO_NEW

**文件**: `src/mt7927_pci.c`

**问题**: 两个独立但相关的 bug:
1. conn_state 参数传了 DISCONNECT(0)，mt7925 用 PORT_SECURE(2)
2. EXTRA_INFO_NEW 条件反了（DISCONNECT 时设 NEW，应该是 newly+非DISCONNECT 时设）

**函数签名修改** (参考 win-reverser 建议):
```c
// 旧签名:
static int mt7927_mcu_sta_update(struct mt7927_dev *dev,
    struct ieee80211_vif *vif, struct ieee80211_sta *sta,
    bool enable, u8 conn_state);

// 新签名 — 去掉 conn_state, 加 newly:
static int mt7927_mcu_sta_update(struct mt7927_dev *dev,
    struct ieee80211_vif *vif, struct ieee80211_sta *sta,
    bool enable, bool newly);
```

**函数体修改** (对齐 mt7925 mt7925_mcu_sta_cmd + mt76_connac_mcu_sta_basic_tlv):
```c
// conn_state 由 enable 决定 (mt7925/mcu.c:1974):
u8 conn_state = enable ? CONN_STATE_PORT_SECURE : CONN_STATE_DISCONNECT;
req.basic.conn_state = conn_state;

// EXTRA_INFO_NEW 由 newly 决定 (mt76_connac_mcu.c:385):
req.basic.extra_info = cpu_to_le16(EXTRA_INFO_VER);
if (newly && conn_state != CONN_STATE_DISCONNECT)
    req.basic.extra_info |= cpu_to_le16(EXTRA_INFO_NEW);
```

**调用处修改**:
```c
// NOTEXIST→NONE (首次创建 STA): enable=true, newly=true
mt7927_mcu_sta_update(dev, vif, sta, true, true);
// → PORT_SECURE + VER + NEW → 固件创建 WTBL 并允许 TX

// AUTH→ASSOC (更新已有 STA): enable=true, newly=false
mt7927_mcu_sta_update(dev, vif, sta, true, false);
// → PORT_SECURE + VER (无 NEW) → 固件更新已有 WTBL

// 断开连接: enable=false
mt7927_mcu_sta_update(dev, vif, sta, false, false);
// → DISCONNECT + VER (无 NEW) → 固件释放 WTBL
```

### Fix 3 (P0): ROC ABORT 改用 wait-for-ACK (解决 4s 超时)

**文件**: `src/mt7927_pci.c` — `mt7927_mgd_prepare_tx()` 中 abort 分支

**修改**:
```c
// 旧:
mt7927_mcu_send_unicmd(dev, 0x27,
                       UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,  // 0x06
                       &abort_req, sizeof(abort_req));
msleep(10);

// 新:
mt7927_mcu_send_unicmd(dev, 0x27,
                       UNI_CMD_OPT_SET_ACK,  // 0x07 — 等待固件 ACK
                       &abort_req, sizeof(abort_req));
// 不需要 msleep — mcu_wait_resp() 已经等到固件确认
```

### Fix 4 (P1): mgd_complete_tx 加条件检查

**文件**: `src/mt7927_pci.c` — `mt7927_mgd_complete_tx()`

**修改**:
```c
// 旧: 无条件发 abort
dev->roc_active = false;
mt7927_mcu_send_unicmd(dev, 0x27, ...);

// 新: 检查 roc_active
if (!dev->roc_active)
    return;  // 没有活跃 ROC，不需要 abort

dev->roc_active = false;
mt7927_mcu_send_unicmd(dev, 0x27,
                       UNI_CMD_OPT_SET_ACK,  // 也改为 0x07
                       &req, sizeof(req));
```

### Fix 5 (P1): ROC 超时后发 abort 清理固件状态

**文件**: `src/mt7927_pci.c` — `mt7927_mgd_prepare_tx()` 超时分支

**修改**:
```c
if (!wait_for_completion_timeout(&dev->roc_complete, 4 * HZ)) {
    dev_warn(&dev->pdev->dev,
             "mgd_prepare_tx: ROC grant timeout (4s)!\n");
    // 新增: abort 清理固件状态
    {
        struct { ... } __packed abort_req = {0};
        abort_req.tag = cpu_to_le16(1);
        abort_req.len = cpu_to_le16(sizeof(abort_req) - 4);
        abort_req.bss_idx = mvif->bss_idx;
        abort_req.dbdcband = 0xff;
        mt7927_mcu_send_unicmd(dev, 0x27,
                               UNI_CMD_OPT_SET_ACK,
                               &abort_req, sizeof(abort_req));
    }
    dev->roc_active = false;
}
```

### Fix 6 (P1): BSS_INFO 在 STA_REC 之前发送

**文件**: `src/mt7927_pci.c` — `mt7927_sta_state()` 中 NOTEXIST→NONE 分支

**发现者**: win-reverser

mt7925 (`mt7925_mac_link_sta_add` line ~891):
```c
/* should update bss info before STA add */
mt7925_mcu_add_bss_info(...);
mt7925_mcu_sta_update(...);
```

**修改**: 在 STA_REC 之前加 BSS_INFO (如果还没发过):
```c
// NOTEXIST→NONE:
// 1. 分配 WCID
// 2. mt7927_mcu_add_bss_info(dev, vif, false);  // 先! (用 enable=false 避免重复)
// 3. mt7927_mcu_sta_update(dev, vif, sta, true, CONN_STATE_PORT_SECURE);
```

注意: add_interface 已发过 BSS_INFO (enable=true)，sta_state 再发一次 (enable=false) 是为了确保固件有最新 BSS 上下文。需要验证 enable=false 是否正确，或者是否应该传 true (重复 update)。

### Fix 7 (P2): 添加 WTBL ADM_COUNT_CLEAR

**文件**: `src/mt7927_pci.c` — `mt7927_sta_state()` 中 NOTEXIST→NONE

**发现者**: win-reverser

mt7925 在分配 WCID 后、发 STA_REC 前：
```c
mt7925_mac_wtbl_update(dev, idx, MT_WTBL_UPDATE_ADM_COUNT_CLEAR);
```

**需要**: 找到 MT_WTBL_UPDATE 寄存器偏移 (CONNAC3)，添加写入 + busy 轮询。

---

## 四、不做的事情 (记录为已知限制)

1. **mcu_wait_resp() 消费异步事件** (Bug #3 from ROC analysis) — 需要更大架构改动，当前时序下 ROC_GRANT 通常在 ACK 之后到达，不太会被误消费。记录但暂不修复。

2. **STA_REC_PHY TLV** — 可能需要但非必须。auth 阶段固件可能不需要详细的 PHY 参数。如果 Fix 1-6 解决了 stat=1，则推迟到 assoc 阶段再加。

3. **STA_REC_STATE.state 值** — txd-analyst 指出 state=0 可能需要改为 state=1。但这与 mt7925 行为一致 (state=NONE=0)，优先级低。如果 Fix 1 不够则再尝试。

---

## 五、修复顺序建议

```
第一批 (解决 stat=1):
  Fix 1+2: 重构 sta_update — conn_state=PORT_SECURE + newly 参数 + EXTRA_INFO_NEW

第二批 (解决 ROC 超时):
  Fix 3: abort option → 0x07 (等 ACK)
  Fix 4: complete_tx 条件检查
  Fix 5: 超时后 abort

第三批 (改善稳定性):
  Fix 6: BSS_INFO 顺序
  Fix 7: WTBL ADM_COUNT_CLEAR
```

建议先做第一批 + 第二批，编译测试。Fix 1+2 极可能解决 stat=1，Fix 3-5 解决 ROC 超时。如果 auth 仍失败再做第三批。

---

## 六、验证方法

```bash
# 1. 编译
make driver

# 2. 加载
sudo rmmod mt7927_pci 2>/dev/null; sudo insmod src/mt7927.ko

# 3. 检查接口
iw dev

# 4. 连接测试
sudo wpa_supplicant -i wlp9s0 -c /tmp/wpa_mt7927.conf -d

# 5. 检查结果
sudo dmesg | grep -E "TXFREE|auth|ROC|sta_update|conn_state"

# 成功标志:
# - "ROC_GRANT status=0" (每次 prepare_tx)
# - 无 "TXFREE: stat=1"
# - wpa_supplicant 显示 "Authentication succeeded" 或至少 "auth reply"
```

---

*汇总完成 — 2026-02-16*
