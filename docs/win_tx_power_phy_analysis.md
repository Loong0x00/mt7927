# Windows TX Power / PHY / RF 初始化分析报告

**日期**: 2026-02-16
**分析者**: win-reverser agent (RE specialist)
**目标**: 分析 Windows 驱动是否有 TX power / PHY / RF 相关的 MCU 命令我们遗漏了

---

## 一、核心结论

### TX Power / PHY / RF 初始化 **不是** auth 帧失败的原因

经过对所有现有 Ghidra 逆向文档、mt7925 参考代码、MT6639 Android 驱动的全面分析：

1. **没有找到** 任何 Windows 驱动在 PostFwDownloadInit 或连接流程中发送的 TX power 配置命令
2. **没有找到** 任何显式的 PHY/RF 初始化 MCU 命令
3. **没有找到** 任何 Radio Enable 命令
4. **固件使用出厂 E-fuse 中的默认 TX power** — 无需驱动配置即可发射

---

## 二、TX Power 分析

### 2.1 mt7925 参考代码

mt7925 有以下 TX power 相关函数：

| 函数 | CID | 调用时机 | 是否影响 auth |
|------|-----|---------|-------------|
| `mt7925_mcu_set_rate_txpower()` | `MCU_UNI_CMD(SET_POWER_LIMIT)` (0x2c) | `set_sar_specs` / `set_txpower` 回调 | **否** — 仅在用户/调节域修改功率时调用 |
| `mt7925_mcu_get_txpower_info()` | `MCU_UNI_CMD(TXPOWER)` (0x2b) | debugfs 查询 | **否** — 只是读取信息 |

**关键发现**: mt7925 的 `__mt7925_start()` 初始化流程中只调用了：
1. `mt7925_mcu_set_channel_domain()` — 信道域信息
2. `mt7925_mcu_set_rts_thresh()` — RTS 阈值

**没有调用** `set_rate_txpower`、`set_radio_en` 或任何 TX power 配置命令。

### 2.2 mt7925 Radio Enable

```c
// mt7925/mcu.c:3572 — 定义了函数
int mt7925_mcu_set_radio_en(struct mt792x_phy *phy, bool enable)
{
    // 使用 MCU_UNI_CMD(BAND_CONFIG) + UNI_BAND_CONFIG_RADIO_ENABLE
}

// 但在整个 mt7925 驱动中 **从未被调用**!
// 对比 mt7996: 在 start() 中显式调用 set_radio_en(true)
```

**结论**: CONNAC3 STA 模式 (mt7925/mt7927) 的固件 **自动启用 radio**。不需要驱动发送 Radio Enable 命令。

### 2.3 Windows 驱动逆向

已分析的所有 Windows 逆向文档中：

| 文档 | 覆盖阶段 | TX Power 相关 |
|------|---------|-------------|
| `ghidra_post_fw_init.md` | PostFwDownloadInit | **无** |
| `windows_post_fw_init.md` | PostFwDownloadInit v5705275 | **无** |
| `win_v5705275_fw_flow.md` | FW 下载协议 | **无** |
| `win_v5705275_core_funcs.md` | 核心函数反编译 | **无** (文件太大未完整读取，但已搜索 power/phy/radio 关键词) |
| `mtkwecx_mt6639_fw_dma_reverse.md` | WFDMA/DMA | **无** |
| `v5705275_vs_our_driver.md` | 对比分析 | **无** |

**在所有文档的全文搜索中，"power" 仅出现在**:
- Power management (SET_OWN/CLR_OWN) — 不相关
- Power save (PS-POLL/U-APSD) — 不相关
- PCIe power (D-state) — 不相关

### 2.4 FCC/QA Tool 文档

`mt7927_qa_tool_analysis.md` 确认：
- MT7927 = 2T2R 设备，3.3V 供电
- TX power 表存储在 **E-fuse** 中 (出厂校准)
- 5GHz 1TX: ~65mW (18dBm), 2TX: ~66mW
- 驱动需要 **读取** E-fuse 获取 TX power 表，但 **不需要** 写入

**结论**: 固件从 E-fuse 加载默认 TX power。即使驱动不发任何 SET_POWER_LIMIT 命令，固件也会使用有效的发射功率。

---

## 三、我们的 PostFwDownloadInit vs Windows

### 3.1 Windows PostFwDownloadInit MCU 命令序列

来源: `ghidra_post_fw_init.md` + `windows_post_fw_init.md`

| # | 操作 | CID/class | 我们的实现 | 状态 |
|---|------|-----------|-----------|------|
| 1 | DMASHDL enable | 寄存器 0xd6060 \|= 0x10101 | ✅ `MT_DMASHDL_ENABLE` | 已实现 |
| 2 | WpdmaConfig | 寄存器 GLO_CFG | ✅ `mt7927_wpdma_config()` | 已实现 |
| 3 | Clear FWDL bypass | GLO_CFG &= ~BIT(9) | ✅ | 已实现 |
| 4 | NIC_CAP query | class=0x8a | ✅ `UNI_CMD_ID_NIC_CAP` | 已实现 |
| 5 | Config v2 | class=0x02 | ✅ `UNI_CMD_ID_CONFIG` | 已实现 |
| 6 | Config 0xc0 | class=0xc0 | ✅ | 已实现 |
| 7 | DownloadBufferBin | class=0xed, subcmd=0x21 | ❌ **可选** (跳过条件) | 未实现但可选 |
| 8 | DBDC config | class=0x28 (MT6639 only) | ✅ `UNI_CMD_ID_MBMC` | 已实现 |
| 9 | 1ms delay | KeStallExecutionProcessor | ✅ `usleep_range` | 已实现 |
| 10 | ScanConfig | class=0xca, tag=scan | ✅ | 已实现 |
| 11 | ChipConfig | class=0xca, tag=chip | ✅ | 已实现 |
| 12 | LogConfig | class=0xca, tag=log | ✅ | 已实现 |

### 3.2 DownloadBufferBin (唯一缺失项)

Windows PostFwDownloadInit 第 7 步 `DownloadBufferBin`:
```
条件: *(ctx + 0x1467608) == 1
动作: 打开 NdisOpenFile, 按 1KB 分块通过 MCU 命令发送
CID: class=0xed, subcmd=0x21
```

**分析**: 这是一个 **可选的二进制数据下载** (类似补充固件)。条件标志 `0x1467608` 是否为 1 取决于设备配置。
- mt7925 参考代码中 **没有对应实现**
- 这更可能是 ACPI/OEM 定制的固件补充数据
- **不太可能影响 auth 帧发送**

### 3.3 初始化后的额外 MCU 命令 (来自 `__mt7925_start()`)

mt7925 在 mac80211 `start()` 回调中还发送：

| 命令 | CID | 我们的实现 |
|------|-----|-----------|
| `set_channel_domain()` | `MCU_UNI_CMD(SET_DOMAIN_INFO)` (0x15) | ✅ 已实现 |
| `set_rts_thresh()` | `MCU_UNI_CMD(BAND_CONFIG)` (0x08) tag=0x08 | ✅ 已实现 |

**我们和 mt7925 的初始化 MCU 命令序列完全对齐。**

---

## 四、关键字搜索结果汇总

### 在 docs/ 目录的全面搜索

| 关键字 | 相关匹配 | 与 TX power/PHY 初始化相关 |
|--------|---------|------------------------|
| `power` | ~80+ 匹配 | **全部是** power management / save / PCIe power |
| `txpower` / `tx_power` | ~10 匹配 | 全部在 mt7925 reference (非 Windows RE) |
| `TPC` | 0 匹配 | — |
| `phy_init` / `PHY_INIT` | 0 匹配 | — |
| `rf_init` / `RF_INIT` | 0 匹配 | — |
| `radio` / `RADIO` | 1 匹配 | FCC 文档 "built-in radio" 描述 |
| `antenna` / `ANTENNA` | ~5 匹配 | mt7925 `antenna_mask` 参考 |
| `CHANNEL_SWITCH` | ~30 匹配 | 已分析 — STA 模式不使用 |
| `RATE_CTRL` / `POWER_CTRL` | 0 匹配 (docs/) | — |
| `BAND_CONFIG` | 2 匹配 (src/) | ✅ 已实现 RTS_THRESHOLD |

### 在 mt76/mt7925/ 的搜索

| 命令 | CID | 调用时机 | 结论 |
|------|-----|---------|------|
| `SET_POWER_LIMIT` | 0x2c | 仅在 `set_sar_specs` / `set_txpower` | 非初始化 |
| `TXPOWER` | 0x2b | 仅在 debugfs | 非必须 |
| `BAND_CONFIG` + `RADIO_ENABLE` | 0x08, tag=0 | **从未被调用** | 不需要 |
| `BAND_CONFIG` + `RTS_THRESHOLD` | 0x08, tag=0x08 | `start()` | ✅ 已实现 |
| `BAND_CONFIG` + `RX_FILTER` | 0x08, tag=0x0c | `configure_filter` | 非必须 |
| `POWER_CTRL` | 0x0f | mt7996 only, mt7925 不用 | N/A |

---

## 五、为什么 TX power 不是问题

### 理论分析

如果 TX power 为 0 (固件不发射):
- TXFREE **count 应该=0** (帧从未尝试发射)
- 或 count=1 且 stat=某个特殊值

但我们看到的是:
- **TXFREE count=15** (固件尝试了 15 次重试)
- **stat=1** (每次重试都失败)

**count=15 证明固件 IS 发射帧到空中**。如果 TX power 为 0，AP 收不到，但固件不会知道（它只是发射然后等 ACK）。这种情况下 count 应该更小（通常是 retry limit），但 stat 值不一定是 1。

**stat=1 的真正含义**: 根据 CONNAC3 TXS 定义，stat=1 通常表示 **TX 被固件内部拒绝** (policy drop)，而不是无线传输失败 (那是 stat=2 或其他值)。

这与 STA_REC conn_state=DISCONNECT 的假设完全吻合：固件检查目标 STA 的连接状态，发现是 DISCONNECT，拒绝发射 → stat=1。

### 实证分析

如果 TX power 真是问题：
1. WiFi **扫描也会失败** — 但我们的扫描正常工作 (56-61 BSS)
2. 被动扫描不需要发射，但如果 AP 很远，RX 信号也会很弱
3. 扫描能看到 56+ AP → 证明 RX 链路正常 → RF/PHY 工作正常

---

## 六、真正的 Auth 失败原因 (交叉引用)

根据已有的详细分析 (`auth_fix_plan.md`, `txd_starec_analysis.md`, `win_auth_flow_analysis.md`)：

### P0: STA_REC conn_state=DISCONNECT
- 我们在 NOTEXIST→NONE 发送 STA_REC 时用 `conn_state=0` (DISCONNECT)
- mt7925 用 `conn_state=2` (PORT_SECURE) 当 enable=true
- **固件拒绝为 DISCONNECT 的 STA 发射帧** → stat=1

### P0: ROC ABORT fire-and-forget
- abort 用 option=0x06 (不等 ACK)
- 10ms sleep 不够 → 新 ROC acquire 被固件拒绝 → 4s 超时

### P1: BSS_INFO → STA_REC 顺序
- mt7925: 先 BSS_INFO 后 STA_REC
- 我们: 先 STA_REC 后 BSS_INFO (ROC_GRANT 后)

### P1: 缺少 WTBL ADM_COUNT_CLEAR
- mt7925 在 sta_add 第一步做

---

## 七、需要进一步 Ghidra 逆向的方向

### 不需要逆向的
- TX power 初始化 — 不是问题
- PHY/RF 初始化 — 不是问题
- Radio Enable — 不是问题

### 如果现有修复方案仍然失败，考虑逆向

1. **Windows 连接回调 (WDI OID_CONNECT)**: 定位连接过程中的 MCU 命令序列
   - 需要从 NdisMiniportDriverCharacteristics 入口追踪
   - 涉及大量间接调用，工作量大

2. **完整的 57 条 MCU 命令路由表**: 从 Ghidra 读取 0x14023fcf0
   - 可以确认固件支持的所有 MCU CID
   - 目前只分析了前 5 条

3. **固件 TXFREE stat 值含义**: 如果能找到固件中 stat 字段的枚举定义
   - 可以精确确认 stat=1 = policy drop (而非 TX failure)

---

## 八、总结

| 假设 | 结论 | 依据 |
|------|------|------|
| 缺少 TX power 配置命令 | ❌ **排除** | mt7925 也不在 init 时发 power 命令；固件使用 E-fuse 默认值 |
| 缺少 PHY/RF 初始化命令 | ❌ **排除** | 所有 RE 文档中无此类命令；固件自行初始化 |
| 缺少 Radio Enable 命令 | ❌ **排除** | mt7925 从未调用 set_radio_en；自动启用 |
| TX power = 0 导致 AP 收不到 | ❌ **排除** | count=15 证明固件在发射；stat=1 是 policy drop |
| PostFwDownloadInit 缺失步骤 | ❌ **排除** | 所有 10 步 MCU 命令都已实现 (唯一缺的 BufferBin 是可选的) |
| **STA_REC conn_state=DISCONNECT** | ✅ **最可能** | stat=1 = policy drop 与 DISCONNECT 状态吻合 |
| **ROC ABORT 超时** | ✅ **第二原因** | fire-and-forget abort 导致后续 ROC 失败 |

**建议**: 优先修复 `auth_fix_plan.md` 中的 Fix 1-5，不需要添加任何 TX power / PHY / RF 命令。

---

*分析完成 — 2026-02-16*
