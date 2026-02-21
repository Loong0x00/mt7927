# Windows mtkwecx.sys — Auth 前完整 MCU 命令序列

**分析目标**: AMD v5.7.0.5275 (`WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys`)
**分析方法**: pefile + capstone 汇编级逆向
**日期**: 2026-02-21

---

## 一、完整命令序列（按执行顺序）

### Phase 1: WdiTaskConnect (0x140065be0)

| 顺序 | 调用地址 | 函数 | 命令 | CID |
|------|---------|------|------|-----|
| 1 | 0x1400484c0 | ChipConfig | CMD_ID_CHIP_CONFIG (0xca) → UNI_CMD_CHIP_CONFIG | 0x0E |
| 2 | 0x1400c558c | MtCmdActivateDeactivateNetwork | CMD_ID_BSS_ACTIVATE_CTRL (0x11) → DEV_INFO | 0x01 |

**发现细节**:
- `0x1400484c0` 在 WdiTaskConnect 调用，分配 **0x148字节** 的 CHIP_CONFIG 结构体
- 汇编：`mov r15d, 0x148` → `mov dl, 0xca` → `call 0x1400cdc4c` (MtCmd dispatcher)
- CHIP_CONFIG (legacy 0xca → UniCmd CID=0x0E) = 自定义芯片配置，载入固件运行参数

### Phase 2: MlmeCntlOidConnectProc (0x140123588)

| 顺序 | 调用地址 | 函数 | 命令 | CID | 说明 |
|------|---------|------|------|-----|------|
| 3 | 0x14012377f | `0x1400caefc` | CMD_ID_INDICATE_PM_BSS_ABORT (0x17) → **BSS_INFO PM_DISABLE** | **0x02** | **关键！** |
| 4 | 0x140123790 | MtCmdSetBssInfo | CMD_ID_SET_BSS_INFO (0x12) → BSS_INFO full | 0x02 | 14个TLV |

**调用顺序**（从 OidConnectProc 反汇编直接确认）：
```
0x14012377f: CALL 0x1400caefc   ← PM_DISABLE (必须先于BSS_INFO!)
0x140123790: CALL 0x1400cf928   ← BSS_INFO full config
```

### Phase 3: MlmeCntlWaitJoinProc (0x1401273a8)

| 顺序 | 调用地址 | 函数 | 命令 | CID |
|------|---------|------|------|-----|
| 5 | 0x140127475 | `0x1400cb7a4` | CMD_ID_SCAN_CANCEL (0x1b) → 停止扫描 | — |
| 6 | 0x14012757b | MtCmdChPrivilage | CMD_ID_CH_PRIVILEGE (0x1c) → Channel Request | 0x27 |
| 7 | 0x14012758c | MtCmdSendStaRecUpdate | CMD_ID_UPDATE_STA_RECORD (0x13) → STA_REC | 0x03 |
| 8 | 0x1401276ac | **auth frame TX** | — | — |

---

## 二、关键发现：PM_DISABLE 命令（BSS_INFO tag=0x1B）

### 2.1 Windows 行为

函数 `0x1400caefc` 对应 MT6639 中的 `nicUniCmdPmDisable`（legacy CMD_ID 0x17 = CMD_INDICATE_PM_BSS_ABORT）：

```c
// MT6639 nic_uni_cmd_event.c (实现相同逻辑)
entry = nicUniCmdAllocEntry(ad, UNI_CMD_ID_BSSINFO, max_cmd_len, NULL, NULL);
uni_cmd->ucBssInfoIdx = cmd->ucBssIndex;  // BSS index
tag->u2Tag    = UNI_CMD_BSSINFO_TAG_PM_DISABLE;  // = 0x001B
tag->u2Length = sizeof(*tag);                    // = 4 bytes
```

**发送的帧结构**：
```
UniCmd Header (16 bytes)
  + UNI_CMD_BSSINFO Header (4 bytes):
      ucBssInfoIdx = 0  (BSS 0)
      aucPadding[3] = 0
  + TLV tag=0x001B, len=4
```
总 payload = 24 bytes（16 hdr + 4 bssinfo + 4 TLV）

### 2.2 含义

PM_DISABLE 命令告知固件：
- **禁用该 BSS 的省电模式**
- 让固件的 TX 队列开始处理管理帧
- **必须在 BSS_INFO full setup 之前发送**，否则固件处于 power-save 模式，TX 静默丢弃

### 2.3 我们的 Linux 驱动现状

**完全缺失**。我们的 `mt7927_set_bss_info()` 直接发送 BSS_INFO full config，**从未发送 PM_DISABLE**。

---

## 三、CHIP_CONFIG in WdiTaskConnect

### Windows 行为

`0x1400484c0` 函数：
1. 分配 `0x148` (328字节) 的本地缓冲区
2. 设置 `mov r15d, 0x148` 为 payload size
3. 之后 `mov dl, 0xca` → `call 0x1400cdc4c` (MtCmdDispatch with CMD_ID_CHIP_CONFIG)

CMD_ID_CHIP_CONFIG (0xca) → `nicUniCmdChipCfg` → **UNI_CMD_ID_CHIP_CONFIG (0x0E)**

这是每次连接时重发一次芯片配置（可能包含频段/国家码/功率表）。

### 我们的现状

我们在 `PostFwDownloadInit` 中已经发送过 CHIP_CONFIG。但 Windows **每次 connect 都重发**，说明可能有连接特定的参数。这是次要问题，暂不优先。

---

## 四、与 Linux 驱动的命令对比

| 顺序 | Windows 命令 | 我们是否发送 | 差异 |
|------|-------------|-------------|------|
| 1 | CHIP_CONFIG (0x0E, 每次connect) | 只在init发送 | 次要 |
| 2 | DEV_INFO (0x01) | ✅ 发送 | OK |
| **3** | **BSS_INFO PM_DISABLE (0x02, tag=0x1B)** | **❌ 完全缺失** | **🔴 CRITICAL** |
| 4 | BSS_INFO full (0x02, 14 TLVs) | 只发3个TLV | 🔴 高 |
| 5 | SCAN_CANCEL | ✅ 发送 | OK |
| 6 | Channel Request (0x27) | ✅ ROC 已实现 | OK |
| 7 | STA_REC (0x03, 10 TLVs) | 发5个TLV | 🟡 中 |
| 8 | Auth TX | ✅ DMA提交 | DMA OK但固件静默 |

**未在 connect flow 中出现的命令**（排除）:
- EDCA (CMD_UPDATE_WMM_PARMS 0x1d) — 在 AssocResp 处理中更新，**不在 connect 流中**
- BA_OFFLOAD — STA_REC 中的 TLV，通过 STA_REC 发送

---

## 五、遗漏命令列表 + 修复建议

### 🔴 修复 1：发送 BSS_INFO PM_DISABLE（最高优先级）

**必须在 BSS_INFO full config 之前发送**。

```c
// 在 mt7927_pci.c 的 mt7927_set_bss_info() 函数中，在发送 BSS_INFO 之前插入：
static int mt7927_send_pm_disable(struct mt7927_dev *dev, u8 bss_idx)
{
    struct sk_buff *skb;
    struct mt7927_uni_cmd_hdr *hdr;

    // Total: 16 (UniCmd hdr) + 4 (bssinfo base) + 4 (PM_DISABLE TLV) = 24 bytes
    skb = alloc_skb(24 + 4, GFP_KERNEL);
    if (!skb) return -ENOMEM;

    // Unicmd header
    hdr = skb_put(skb, sizeof(*hdr));
    hdr->cid = 0x02;           // UNI_CMD_ID_BSSINFO
    hdr->option = 0x06;        // fire-and-forget (set)
    hdr->len = cpu_to_le16(24 - 4);  // plen + 16 - 4 = payload after hdr
    // ... fill rest of UniCmd header ...

    // BSS_INFO base (4 bytes)
    u8 *base = skb_put(skb, 4);
    base[0] = bss_idx;  // ucBssInfoIdx
    memset(base + 1, 0, 3);

    // PM_DISABLE TLV (tag=0x1B, len=4)
    __le16 *tlv = skb_put(skb, 4);
    tlv[0] = cpu_to_le16(0x1B);  // UNI_CMD_BSSINFO_TAG_PM_DISABLE
    tlv[1] = cpu_to_le16(4);     // sizeof TLV

    return mt7927_mcu_send_msg(dev, skb, false);
}
```

调用位置：`mt7927_bss_info_assoc()` / `mt7927_set_bss_info()` **之前**。

### 🔴 修复 2：BSS_INFO 补充 TLV

Windows 发14个 TLV，我们只发3个(BASIC+RLM+MLD)。关键缺失：
- **RATE TLV** (tag=0x05?) — 告知固件此BSS的合法速率集
- **PROTECT TLV** (tag=?) — 保护模式配置
- **IFS_TIME TLV** (tag=?) — 帧间隔时间配置

参考：`docs/win_re_bss_info_tlv_dispatch.md`（Windows 14-entry dispatch table）

### 🟡 修复 3：STA_REC 补充 TLV

Windows 发10个 TLV，我们发5个。缺失：
- **BA_OFFLOAD** (tag=0x0F) — 硬件聚合卸载
- **UAPSD** (tag=?) — 省电队列
- **HE_6G_CAP** (如适用)

---

## 六、根因假设

**假设**: 固件在 BSS_INFO 未收到 PM_DISABLE 的情况下，TX 队列可能仍处于 suspend/power-save 状态，导致即使 DMA 描述符被消费，帧也在 PLE 层被丢弃（不生成 TX_DONE 事件）。

**验证方法**:
1. 在 BSS_INFO 之前发送 PM_DISABLE
2. 检查 `PLE_QUEUE_EMPTY (0x820c0360)` 和 `PSE_QUEUE_EMPTY (0x820c80b0)` 确认帧流向
3. 观察是否出现 TX_DONE (eid=0x2D) 回调

---

## 七、函数地址参考 (AMD v5705275)

| 函数 | 地址 | 用途 |
|------|------|------|
| WdiTaskConnect | 0x140065be0 | 顶层连接入口 |
| MlmeCntlOidConnectProc | 0x140123588 | BSS_INFO 设置 |
| MlmeCntlWaitJoinProc | 0x1401273a8 | Join 序列 |
| PM_DISABLE sender | 0x1400caefc | nicUniCmdPmDisable |
| MtCmdSetBssInfo | 0x1400cf928 | BSS_INFO full |
| MtCmdChPrivilage | 0x1400c5e08 | CH_PRIVILEGE |
| MtCmdSendStaRecUpdate | 0x1400cdea0 | STA_REC |
| ChipConfig sender | 0x1400484c0 | CHIP_CONFIG(0xca) |
| MtCmdDispatch | 0x1400cdc4c | legacy ID dispatcher |
| auth frame trigger | 0x1400ac6c8 | auth TX 触发 |

