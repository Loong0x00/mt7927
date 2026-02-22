# MT7927 WiFi 驱动当前问题全景分析

**日期**: 2026-02-22 (Session 24 结束)
**驱动版本**: 自研 Linux PCIe WiFi 7 驱动 (mt7927.ko)
**芯片**: MT7927 = MT6639 PCIe 封装, PCI ID `14c3:6639`
**逆向参考**: Windows mtkwecx.sys v5705275 (Ghidra + pefile/capstone)

---

## 一、核心阻塞问题

### 1.1 Auth 帧从未到达空中

**阻塞 24 个 session，是驱动开发的最大障碍。**

WiFi 扫描已正常工作（28 BSS），但 wpa_supplicant 发送 auth 帧时，帧被 DMA 硬件消费后，从未实际发射到空中。

**证据链**:
- Session 22: AR9271 USB WiFi (Qualcomm Atheros, 0cf3:9271) 在 monitor mode 下抓包 344 packets，来自 MT7927 的帧数为 **零**
- Session 22: TX_DONE 返回 MPDU_ERR status=3, cnt=30 (固件重试 30 次全部失败)
- Session 23: TX_DONE **完全消失** (STATE_INFO 修正后固件可能进入不同处理路径)
- DMA 描述符 DIDX 前进，说明硬件确实消费了帧，但固件内部处理失败

### 1.2 PLE STATION_PAUSE0 = 0xf133ffff

PLE (Packet Level Engine) 的 STATION_PAUSE0 寄存器持续读到 `0xf133ffff`，表示 **所有 WCID 的 TX 被固件暂停**。

**关键事实**:
- 直接写 PLE 寄存器无法清除此标志 (Session 22 实测)
- STA_REC STATE_INFO state=2 (ASSOC) 理论上应清除 PLE PAUSE，但实际未生效
- Windows 在 auth 之前也发送 state=0，说明 **state=2 可能不是清除 PAUSE 的触发条件**
- 此标志可能由固件内部的 BSS/STA 配置不完整导致

### 1.3 Ring 15 固件崩溃

auth 超时后系统发送 BSS_INFO disconnect，随后 Ring 15 (MCU TX ring) 的 DIDX 停止前进，表明固件已挂死。

**崩溃时序**:
1. wpa_supplicant 发起 auth → Ring 0 提交 auth 帧
2. 帧被 DMA 消费但未发射 → auth 超时 (约 5 秒)
3. mac80211 发送 BSS_INFO disconnect → Ring 15 提交
4. Ring 15 DIDX 停止前进 → 固件死亡
5. `hw_stopped=true` 在 100ms 内检测到，禁止所有 MMIO 读操作
6. 系统不再冻死 (Session 23 修复)

**Session 23 之前**: 固件死后 IRQ handler/NAPI 仍在读设备寄存器 → PCIe completion timeout → CPU 永久卡死 → 系统冻死 (20-60 秒)

---

## 二、BSS_INFO BASIC TLV 字段布局冲突

### 2.1 问题背景

BSS_INFO BASIC TLV (tag=0, 32 字节) 是最关键的配置命令之一。mt7925 上游驱动定义的 struct 字段名，与 Windows RE 反汇编 (0x14014c610) 揭示的实际固件读取偏移 **存在严重不匹配**。

### 2.2 逐字段对比

| TLV偏移 | mt7925 字段名 | Windows 字段名 | 我们赋的值 | Windows 赋的值 | 值是否一致 |
|---------|-------------|--------------|---------|-------------|----------|
| +4 | `active` | `bss_idx` | 1 (enable) | bss_idx (0) | **不一致** |
| +5 | `omac_idx` | `ownmac_idx` | omac_idx | omac_idx | 一致 |
| +6 | `hw_bss_idx` | `ownmac(dup)` | bss_idx | omac_idx | 存疑 |
| +7 | `band_idx` | `sco` | **band_idx (0或1)** | **0xFF** | **危险不一致** |
| +8..+B | `conn_type` | `conn_type` | 0x10001 | 0x10001 | 一致 |
| +C | `conn_state` | `active` | 1 | 1 | 值碰巧一致 |
| +D | `wmm_idx` | `network_type` | 0 | network_type | 存疑 |
| +E..+13 | `bssid[6]` | `bssid[6]` | AP BSSID | AP BSSID | 一致 |
| +14..+15 | `bmc_tx_wlan_idx` | `sta_type` | wcid.idx | 0 (STA) | **不一致** |
| +16..+17 | `bcn_interval` | `bcn_interval` | beacon_int | beacon_int | 一致 |
| +18 | `dtim_period` | `dtim` | dtim | dtim | 一致 |
| +19 | `phymode` | `phy_mode_lo` | 0x31 (5G) | phy_mode | 需验证 |
| +1A..+1B | `sta_idx` | `mbss_flags` | 0xFFFE | 0x00FE | **不一致** |
| +1C..+1D | `nonht_basic_phy` | `wlan_idx` | 3 (PHY_OFDM) | wcid.idx | **不一致** |
| +1E | `phymode_ext` | `phy_mode_hi` | 0 | phy_mode byte1 | 存疑 |
| +1F | `link_idx` | `band_info` | 0 | **band_idx** | **危险不一致** |

### 2.3 最危险的字段冲突

#### band_idx 位置错误 (偏移 +7 vs +1F)

**这是最关键的冲突。** mt7925 把 `band_idx` 放在偏移 +7，但 Windows 固件从偏移 +1F 读取 `band_info`。

- 我们在 +7 写入 `band_idx=1` (5GHz) → 固件读作 `sco` (secondary channel offset)
- 我们在 +1F 写入 `link_idx=0` → 固件读作 `band_info=0` (2.4GHz!)
- **结果**: 固件一直不知道我们在 5GHz band 上操作

#### wlan_idx 位置错误 (偏移 +1C vs +14)

- Windows 在 +1C 写 `wlan_idx` = wcid.idx (用于 WTBL 查找)
- 我们在 +1C 写 `nonht_basic_phy` = 3 → 固件读到错误的 WTBL 索引
- 我们在 +14 写 `bmc_tx_wlan_idx` = wcid.idx → 固件读作 `sta_type`

### 2.4 测试结果: 应用 Windows 布局后的副作用

Session 24 反汇编确认了上述字段差异。然而，尝试切换到 Windows 布局后发现:

- LWTBL BAND 从 1 (5GHz, 正确) 变为 **0** (2.4GHz, 错误!)
- 这可能是因为 Windows 把 band_info 放在 +1F，但完整的布局切换引入了其他字段错位

### 2.5 当前状态

**已回退到 mt7925 布局**，因为:
- mt7925 布局下 LWTBL BAND=1 (5GHz, 正确)
- 扫描正常工作 (28 BSS)
- 切换到 Windows 布局导致 BAND 变 0，可能引发更多问题

**遗留风险**: mt7925 布局下固件从 +7 读到的 `sco` 是我们的 `band_idx`，可能导致固件使用错误的 secondary channel offset。从 +1F 读到的 `band_info` 是我们的 `link_idx=0`，可能导致固件认为在 2.4GHz band。

---

## 三、鸡生蛋问题: STA_REC STATE 与 STATION_PAUSE

### 3.1 理论模型

根据 MT6639 参考代码的理解:
- STA_REC STATE_INFO state=2 (STA_STATE_3, post-assoc) 应当触发固件清除 PLE STATION_PAUSE
- 清除 STATION_PAUSE 后，TX 队列才允许发送帧
- auth 帧发送需要 TX 队列畅通

### 3.2 矛盾

```
auth 帧需要发送
  → 需要 STATION_PAUSE 被清除
    → 需要 STA_REC state=2 (post-assoc)
      → 需要先完成 auth + assoc
        → 需要先发送 auth 帧
          → 回到开头: 死循环!
```

### 3.3 Windows 驱动的行为

Windows 连接流程 (从 `docs/win_re_connect_flow.md` 和 `docs/win_re_connect_flow_complete.md`):

1. ChipConfig (每次 connect 重发)
2. DEV_INFO (activate) + BSS_INFO(BASIC+MLD) — BssActivateCtrl 组合
3. BSS_INFO PM_DISABLE (tag=0x1B)
4. BSS_INFO full (14 TLVs)
5. SCAN_CANCEL
6. Channel Request (CID=0x27)
7. **STA_REC (13 TLVs, state=0)** — auth **之前** 发送, state=0 (pre-auth)!
8. auth 帧 TX

**关键**: Windows 在 auth 之前发送 STA_REC 时 state=0 (STA_STATE_1, pre-auth)，不是 state=2。这说明:

- auth 帧可能走特殊路径绕过 PLE STATION_PAUSE
- 或者 PLE STATION_PAUSE 不是由 STA_REC state 控制的
- 或者 STATION_PAUSE 的清除由其他 MCU 命令触发 (如 BSS_INFO 的某些 TLV 组合)
- 或者 `0xf133ffff` 并非完全锁定 TX (某些 WCID 可能例外)

### 3.4 我们的行为

在 `mt7927_sta_state()` (NOTEXIST->NONE 转换) 中:

```c
// 我们发送 STA_REC 时 state=0, conn_state=CONNECT
mt7927_mcu_sta_update(dev, vif, sta, true, CONN_STATE_CONNECT, 0);
```

这与 Windows 行为一致 (auth 前 state=0)。所以 **STA_REC state 不是问题所在**。

### 3.5 真正可能的根因

STATION_PAUSE 持续存在的原因更可能是:
1. **BSS_INFO 配置不完整或布局错误** — 固件未正确初始化 BSS 上下文
2. **BSS_INFO BASIC TLV 字段错位** — 固件读到错误的 band/wlan_idx/sco
3. **缺少 BssActivateCtrl 的早期 BSS_INFO** — 固件内部状态机未正确启动 (Session 24 已添加)
4. **STA_REC option 参数差异** — 0x07 vs Windows 0xed

---

## 四、代码 vs Windows RE 具体冲突清单

### 4.1 BSS_INFO TLVs

我们发送 **16 个 TLV** (Session 24), Windows dispatch table 有 **14 个 entry** 输出 **17 个 TLV** (entry[1] 输出 3 个: RLM+PROTECT+IFS_TIME)。

| Tag | TLV 名称 | Windows 发 | 我们发 | 结构体匹配 | 状态 |
|-----|---------|-----------|-------|-----------|------|
| 0x00 | BASIC | 32B | 32B | **存疑** (字段偏移冲突, 见第二章) | 已回退 mt7925 布局 |
| 0x02 | RLM | 16B | 16B | 基本匹配 | 已修正 (Session 24) |
| 0x03 | PROTECT | 8B | 8B | 匹配 | 已修正 (Session 24) |
| 0x04 | BSS_COLOR | 8B | 8B | 匹配 | 已修正 (Session 24) |
| 0x05 | HE | 16B | 16B | 匹配 | 已修正 (Session 24) |
| 0x06 | 11V_MBSSID | 8B | 8B | 匹配 | 已添加 (Session 24) |
| 0x0B | RATE | 16B | 16B | 匹配 | 已修正 (Session 24) |
| 0x0C | UNKNOWN_0C | 8B | 8B | 匹配 | 已添加 (Session 24) |
| 0x0D | SAP | 40B | 40B | 匹配 (STA 模式全零) | 已修正 (Session 24) |
| 0x0E | P2P | 8B | 8B | 匹配 (STA 模式全零) | 已修正 (Session 24) |
| 0x0F | QBSS | 8B | 8B | 匹配 | 已修正 (Session 24) |
| 0x10 | SEC | 8B | 8B | 匹配 | 已修正 (Session 24) |
| 0x17 | IFS_TIME | 20B | 20B | 匹配 | 已修正 (Session 24) |
| 0x18 | STAIoT | 8B | 8B | 匹配 | 已修正 (Session 24) |
| 0x1A | MLD | 20B | 20B | 匹配 | 已修正 (Session 24) |
| 0x1E | EHT | 16B | 16B | 匹配 (auth 阶段全零) | 已添加 (Session 24) |

**差异说明**:
- 我们不发 BSS_RA (tag=0x01): Windows dispatch table 无此条目, Session 24 已移除
- BASIC TLV 字段布局存疑: 虽然大小 (32B) 匹配, 但内部字段偏移可能不一致

### 4.2 STA_REC TLVs

我们发送 **12 个 TLV** (Session 24), Windows dispatch table 有 **13 个 entry**。

| Tag | TLV 名称 | Windows 发 | 我们发 | 结构体匹配 | 状态 |
|-----|---------|-----------|-------|-----------|------|
| 0x00 | BASIC | 20B | 20B | 匹配, extra_info=3 硬编码 | 已修正 (Session 24) |
| 0x01 | RA_INFO | 16B | 16B | 匹配 | 已实现 |
| 0x07 | STATE_INFO | 16B | 16B | **已修正**: state@+4, flags@+8 | 已修正 (Session 24) |
| 0x09 | HT_INFO | 8B (条件) | 8B | 匹配 | 已实现 |
| 0x0A | VHT_INFO | 16B (条件) | 16B | 匹配 | 已实现 |
| 0x15 | PHY_INFO | 12B | 12B | **已修正**: rcpi@+7, bw@+8, nss@+9 | 已修正 (Session 24) |
| 0x16 | BA_OFFLOAD | 16B | 16B | **已修正**: 完整 16B 布局匹配 | 已修正 (Session 24) |
| 0x17 | HE_6G_CAP | 8B (条件) | **未发送** | 仅 6GHz band 需要 | 未实现 (不影响 5GHz) |
| 0x19 | HE_BASIC | 28B | 28B | 匹配 | 已实现 |
| 0x20 | MLD_SETUP | 32B | 32B | 匹配 (auth 阶段全零) | 已实现 |
| 0x21 | EHT_MLD | 16B | 16B | 匹配 (auth 阶段全零) | 已实现 |
| 0x22 | EHT_INFO | 40B | 40B | 匹配 (auth 阶段全零) | 已实现 |
| 0x24 | UAPSD | 8B | 8B | **已修正**: 完整 3 字段布局匹配 | 已修正 (Session 24) |

**差异说明**:
- HDR_TRANS (tag=0x2B): 我们之前多发, Windows 不发此 TLV, Session 24 已移除
- HE_6G_CAP (tag=0x17): 仅 6GHz band 使用, 5GHz 连接不需要, 暂不影响

### 4.3 BssActivateCtrl 组合命令

| 项目 | Windows | 我们 | 状态 |
|------|---------|------|------|
| 实现方式 | DEV_INFO + BSS_INFO(BASIC+MLD) 原子组合 | DEV_INFO 后单独调用 BSS_INFO(BASIC+MLD) | 已修正 (Session 24) |
| BSS_INFO 发送次数 | 3 次 (早期+PM_DISABLE+完整) | 3 次 (早期+PM_DISABLE+完整) | 匹配 |
| activate 顺序 | DEV_INFO 先, BSS_INFO 后 | DEV_INFO 先, BSS_INFO 后 | 匹配 |
| deactivate 顺序 | BSS_INFO 先, DEV_INFO 后 | BSS_INFO 先, DEV_INFO 后 | 匹配 |

**Session 24 新增** `mt7927_mcu_bss_activate_early()`: 在 `mt7927_mcu_uni_add_dev()` 内, DEV_INFO 发送成功后立即发送简化 BSS_INFO(BASIC+MLD)。**尚未测试。**

### 4.4 CID 使用差异

| 命令 | 我们的 CID | Windows inner CID | 测试结果 | 策略 |
|------|-----------|------------------|---------|------|
| DEV_INFO | 0x01 | 0x01 | 工作 | 匹配 |
| BSS_INFO | 0x02 | 0x02 | 工作 | 匹配 |
| STA_REC | 0x03 | 0x25 | 0x03 工作, 0x25 未测试 | 用 mt7925 |
| NIC_CAP | 0x8a | 0x0e | 0x8a 工作 | 用 mt7925 |
| SET_DOMAIN | 0x15 | 0x03 | **0x03 测试失败!** | 用 mt7925 |
| BAND_CONFIG | 0x08 | 0x49 | **0x49 测试失败!** | 用 mt7925 |
| SCAN_CFG | 0xca | 0x0e | 0xca 工作 | 用 mt7925 |
| EFUSE_CTRL | 0x2d | 0x05 | 0x2d 工作 | 用 mt7925 |

**教训 (Session 20)**: Windows RE dispatch table 的 inner CID 不等于 UniCmd header CID! Session 19 盲目改用 Windows inner CID 导致 scan 从 61 BSS 回归到 0。MT7927 固件的 CID routing 可能与 Windows dispatch table 不同。**当前策略: 用已验证工作的 mt7925 CID。**

### 4.5 STA_REC option 差异

| 参数 | Windows | 我们 | 语义 |
|------|---------|------|------|
| option (BSS_INFO) | 0xed | 0x06 (UNI_CMD_OPT_SET) | 语义等价: fire-and-forget |
| option (STA_REC) | 0xed | 0x06 (UNI_CMD_OPT_SET) | 语义等价: fire-and-forget |
| option (查询型) | 0xee | 0x07 (UNI_CMD_OPT_SET_ACK) | 语义等价: wait-for-response |

Windows 的 option 解码函数 (0x1400ca864):
- `0xed` → 内部 flag `0x8000` (不等待响应)
- `0xee` → 内部 flag `0xc000` (等待响应)

**结论**: 我们的 0x06 与 Windows 的 0xed 语义等价, 不是问题根因。但 Windows dispatch table entry 40 中 STA_REC 的 `filter=0xa8` 和 `R9=0xa8` 参数含义尚未完全理解, 可能存在未知的二级路由差异。

### 4.6 conn_state 双枚举差异

两套不同的 "连接状态" 枚举在不同命令中使用, 极易混淆:

| 命令 | 枚举来源 | CONNECTED 值 | DISCONNECTED 值 | 我们的使用 |
|------|---------|-------------|-----------------|----------|
| BSS_INFO | wsys_cmd_handler_fw.h | **0** | 1 | `conn_state = enable ? 1 : 0` |
| STA_REC | wlan_def.h | 1 | **0** | `conn_state = CONN_STATE_CONNECT (1)` |

**潜在问题**: BSS_INFO 的 `conn_state` 我们设为 `1` (enable 时), 但如果固件按 MEDIA_STATE 枚举理解, `1` = DISCONNECTED! 不过 Windows 也在 activate 时设 `active=1`, 偏移 +C 值恰好也是 1, 所以实际可能无影响。

### 4.7 其他差异

| 项目 | Windows | 我们 | 影响评估 |
|------|---------|------|---------|
| ChipConfig 频率 | 每次 connect 重发 | 只在 init 发一次 | 低 (仅日志/诊断配置) |
| Config 命令 (CID=0x0b) | PostFwDownloadInit step3 发送 | Session 20 跳过 (CID 冲突) | 中 (可能影响固件内部状态) |
| Per-ring EXT_CTRL | **不写** (仅用 packed prefetch) | 写 `PREFETCH(0x02c0, 0x4)` | 中 (可能导致 Ring 2 crash) |
| Ring 2 初始化时机 | Pre-FWDL | Post-FWDL | 低 |
| 中断掩码 | 0x2600f000 (无 TX 完成位) | 0x2600f050 (含 BIT(4)+BIT(6)) | 低 (额外中断无害) |

---

## 五、Session 24 修改汇总 (纯代码修改, 尚未测试)

### 5.1 BSS_INFO 重构

- 从 3 个 TLV (BASIC+RLM+MLD) 扩展到 16 个 TLV
- 移除 BSS_RA (tag=0x01, Windows 无此条目)
- 新增: PROTECT, IFS_TIME, RATE, SEC, QBSS, SAP, P2P, HE, BSS_COLOR, 11V_MBSSID, UNKNOWN_0C, STAIoT, EHT
- 所有结构体布局匹配 Windows RE 反汇编

### 5.2 STA_REC 重构

- 从 13 个 TLV 减少到 12 个 (移除 HDR_TRANS)
- 修正 STATE_INFO: state@+4, flags@+8 (之前 state 和 flags 反转, 固件读到 state=0)
- 修正 PHY_INFO: rcpi@+7, channel_bw@+8, nss@+9
- 修正 BA_OFFLOAD: 完整 16B 布局匹配 Windows RE
- 修正 UAPSD: 3 字段布局匹配 Windows RE
- BASIC extra_info 硬编码为 3 (Windows RE 确认)

### 5.3 BssActivateCtrl 组合命令

- 新函数 `mt7927_mcu_bss_activate_early()`
- 在 `mt7927_mcu_uni_add_dev()` 的 DEV_INFO 后自动调用
- 发送简化 BSS_INFO(BASIC+MLD), 匹配 Windows BssActivateCtrl 行为

---

## 六、待调查方向 (按优先级)

### P0 — 立即测试

| # | 方向 | 预期效果 | 依据 |
|---|------|---------|------|
| 1 | **测试 Session 24 全部修改** | BSS_INFO/STA_REC 完整 TLV + BssActivateCtrl 可能解除 PLE PAUSE | Session 24 大量修正, 核心是 STATE_INFO 布局 |
| 2 | **观察 TX_DONE 变化** | 修正后固件可能恢复 TX_DONE 报告 | Session 23 TX_DONE 消失可能与 state 字段反转有关 |

### P1 — 高优先级

| # | 方向 | 理由 |
|---|------|------|
| 3 | **BSS_INFO conn_state 枚举** | CONNECTED=0 (Windows) vs 1 (我们), 可能发反了 |
| 4 | **Ring 2 恢复: 去掉 per-ring EXT_CTRL** | Windows 不写 EXT_CTRL, 这可能是 Ring 2 crash 的根因 |
| 5 | **Config 命令恢复** | CID=0x0b, payload={1,0,0x70000}, Session 20 因 CID 冲突跳过 |
| 6 | **BSS_INFO BASIC 字段布局深入验证** | 用 Ghidra 逐字节确认固件读取的每个偏移 |

### P2 — 中优先级

| # | 方向 | 理由 |
|---|------|------|
| 7 | **STA_REC option 0x06 vs Windows 0xed** | 虽然语义等价, 但 dispatch table entry 40 的 filter=0xa8 可能有未知含义 |
| 8 | **WTBL BAND 值修复** | BAND=0 (2.4G) 但连接 5G AP, 可能是 BSS_INFO BASIC +1F (band_info) 未正确设置 |
| 9 | **ChipConfig 每次 connect 重发** | Windows 每次 connect 都重发, 我们只发一次 |
| 10 | **HE_6G_CAP TLV 添加** | 虽然 5GHz 不需要, 但缺失可能导致固件 TLV 计数不匹配 |

### P3 — 低优先级 / 长期

| # | 方向 | 理由 |
|---|------|------|
| 11 | BSS_INFO_PROTECT 独立 CID (0x4a) | Windows dispatch table entry 53 显示 PROTECT 用独立 CID=0x4a, 不是 0x02 |
| 12 | BSS_INFO_HE 独立 CID (0x08) | Windows dispatch table entry 27 显示 HE 用 CID=0x08 |
| 13 | Ring 2 SF mode 管理帧 TX | 解决 EXT_CTRL 问题后可重试, Windows 用 Ring 2 发管理帧 |

---

## 七、已排除的调查方向

以下方向在过去 24 个 session 中已被排除:

| 方向 | 排除原因 | Session |
|------|---------|---------|
| CHIP_CONFIG 命令 | 仅日志配置, 不影响 TX | Session 21 |
| PM_DISABLE 缺失 | 已实现 `mt7927_mcu_bss_pm_disable()` | Session 22 |
| DEV_INFO band_idx | WTBL BAND 不因 DEV_INFO 变化 | Session 22 |
| Ring 2 SF mode | 固件崩溃 (DMA 消费后固件挂死) | Session 22 |
| SCAN_CANCEL | 2.4G 和 5G 同样 MPDU_ERR, SCAN_CANCEL 不改善 | Session 21 |
| Ring 0 CT TXD | DW0-DW7 已完全匹配 Windows RE (7 个 DW 汇编级验证) | Session 22 |
| DMA 描述符格式 | Ring 0/Ring 2 格式均匹配 Windows | Session 22 |
| NAPI/IRQ 竞争 | 已修复, napi_disable/enable 保护 | Session 18 |
| FWDL 流程 | 固件下载成功 (fw_sync=0x3) | Session 初期 |
| mac80211 注册 | 正常工作 (wlp9s0 出现, 扫描成功) | Session 15+ |

---

## 八、关键数据点参考

### 8.1 Windows 完整连接命令序列

```
WdiTaskConnect
  [1] ChipConfig (CID=0xca, payload=328B)
  [2] BssActivateCtrl (CID=0x01): DEV_INFO + BSS_INFO(BASIC+MLD)
MlmeCntlOidConnectProc
  [3] BSS_INFO PM_DISABLE (CID=0x02, tag=0x1B)
  [4] BSS_INFO full (CID=0x02, 14 entries → 17 TLVs)
MlmeCntlWaitJoinProc
  [5] SCAN_CANCEL (CID=0x16)
  [6] Channel Request (CID=0x27)
  [7] STA_REC (CID=0x03, 13 TLVs, state=0)
  [8] Auth frame TX (DMA 入队)
```

### 8.2 BSS_INFO dispatch table 原始地址 (0x1402505b0)

| Idx | Handler VA | Tag(s) | 名称 |
|-----|-----------|--------|------|
| 0 | 0x14014c610 | 0x00 | BASIC |
| 1 | 0x14014cc80→0x140150edc | 0x02+0x03+0x17 | RLM+PROTECT+IFS_TIME |
| 2 | 0x14014cc90 | 0x0B | RATE |
| 3 | 0x14014ccb0 | 0x10 | SEC |
| 4 | 0x14014ccd0 | 0x0F | QBSS |
| 5 | 0x14014ccf0 | 0x0D | SAP |
| 6 | 0x14014cd30 | 0x0E | P2P |
| 7 | 0x14014cd50 | 0x05 | HE |
| 8 | 0x14014d010 | 0x04 | BSS_COLOR |
| 9 | 0x14014d150 | 0x1E | EHT |
| 10 | 0x14014d300 | 0x06 | 11V_MBSSID |
| 11 | 0x14014d320 | 0x0C | UNKNOWN_0C |
| 12 | 0x14014d340→0x14014fad0 | 0x1A | MLD |
| 13 | 0x14014d350 | 0x18 | STAIoT |

### 8.3 STA_REC dispatch table 原始地址 (0x140250710)

| Idx | Handler VA | Tag | 名称 |
|-----|-----------|-----|------|
| 0 | 0x14014d6d0 | 0x00 | BASIC |
| 1 | 0x14014d7a0 | 0x09 | HT_INFO |
| 2 | 0x14014d7e0 | 0x0A | VHT_INFO |
| 3 | 0x14014d810 | 0x19 | HE_BASIC |
| 4 | 0x14014dae0 | 0x17 | HE_6G_CAP |
| 5 | 0x14014d730 | 0x07 | STATE_INFO |
| 6 | 0x14014d760 | 0x15 | PHY_INFO |
| 7 | 0x14014e570 | 0x01 | RA_INFO |
| 8 | 0x14014e5b0 | 0x16 | BA_OFFLOAD |
| 9 | 0x14014e620 | 0x24 | UAPSD |
| 10 | 0x14014db80 | 0x22 | EHT_INFO |
| 11 | 0x14014e2a0 | 0x21 | EHT_MLD |
| 12 | 0x14014ddc0 | 0x20 | MLD_SETUP |

### 8.4 代码文件位置

| 文件 | 行数 | 职责 |
|------|------|------|
| `src/mt7927_pci.c` | ~4100 | 初始化、MCU 通信、mac80211 回调 |
| `src/mt7927_pci.h` | ~1780 | 寄存器、结构体、宏定义 |
| `src/mt7927_mac.c` | ~1100 | TXD 构建、RXD 解析 |
| `src/mt7927_dma.c` | ~800 | 中断、NAPI、TX/RX DMA |

### 8.5 关键函数位置 (src/mt7927_pci.c)

| 函数 | 行号 | 职责 |
|------|------|------|
| `mt7927_mcu_bss_activate_early()` | 2433 | BssActivateCtrl 简化 BSS_INFO |
| `mt7927_mcu_uni_add_dev()` | 2483 | DEV_INFO + 早期 BSS_INFO |
| `mt7927_mcu_bss_pm_disable()` | 2526 | PM_DISABLE (tag=0x1B) |
| `mt7927_mcu_add_bss_info()` | 2545 | 完整 BSS_INFO (16 TLVs) |
| `mt7927_mcu_sta_update()` | 2796 | STA_REC (12 TLVs) |
| `mt7927_sta_state()` | 3576 | mac80211 STA 状态机 |

---

## 九、A/B 测试结果记录

### Test 1: conn_state 枚举翻转 (2026-02-22, Session 25)

**改动**: `mt7927_mcu_add_bss_info()` 中 `conn_state = enable ? 0 : 1` (之前 `enable ? 1 : 0`)

**原理**: BSS_INFO 枚举 MEDIA_STATE_CONNECTED=0, MEDIA_STATE_DISCONNECTED=1。之前 enable 时写 1 实际上告诉固件"未连接"。

**结果: 重大突破！**

| 指标 | 之前 (conn_state=1) | 之后 (conn_state=0) |
|------|---------------------|---------------------|
| PLE_STA0 | 0xf133ffff (全暂停) | **0x0000000a** (基本不暂停) |
| TX_DONE | 完全消失 (Session 23) | **恢复! MPDU_ERR status=3 cnt=30** |
| Ring 15 | auth 后卡死→固件死亡 | **正常运行，未卡死** |
| 系统稳定性 | 20-60秒后冻死 | **20秒超时正常退出** |
| Auth | 超时 | 仍然超时，但固件在尝试发帧 |

**关键日志**:
```
BSS_INFO bss=0 active=1 conn_state=0 phymode=0x31 bssid=b4:ba:12:5b:63:c9 ch=161 band=2 band_idx=1
STA_REC wcid=1 conn_state=1 state=2 enable=1
TX_DONE(UNI): PID=1 status=3(MPDU_ERR) SN=160 WIDX=1 cnt=30 rate=0x004b
PLE_STA0=0x0000000a PLE_STA1=0x00000005
```

**发现的新问题**:
- 初始 BSS_INFO 有 `band_idx=255`（add_interface 阶段 band 未确定），后续 ROC_GRANT 后变为 `band_idx=1`
- MPDU_ERR status=3 cnt=30: 固件重试 30 次全部失败，帧可能仍未到达空中
- rate=0x004b (OFDM 6Mbps) 看起来正确

**结论**: conn_state=0 (CONNECTED) 是正确的。PLE STATION_PAUSE 问题由此解除。auth 仍失败需要继续排查。

### Test 2: 延迟 BSS 激活 (2026-02-22, Session 25)

**改动**: 将 `mt7927_mcu_uni_add_dev()` 从 `add_interface` 移到 `sta_state(NOTEXIST→NONE)`

**原理**: add_interface 时 band_idx=255、BSSID 未知，Windows 在 connect 流程才激活。

**结果: 无额外改善**

| 指标 | Test 1 baseline | Test 2 (+延迟激活) |
|------|-----------------|---------------------|
| PLE_STA0 | 0x0000000a | 0x0000000a (相同) |
| TX_DONE | MPDU_ERR status=3 | MPDU_ERR status=3 (相同) |
| 初始 band_idx | 255 | 255 (相同，ROC_GRANT 后变 1) |

**结论**: 延迟激活时序不是关键瓶颈。add_interface 阶段的 BSS_INFO 会被 ROC_GRANT 后的重发覆盖。已回退此改动。

### Test 3: 逐命令 STATION_PAUSE 快照 (2026-02-22, Session 25)

**改动**: 6 个诊断点读取 PLE_STA0/STA1 (DEV_INFO, PM_DISABLE, BSS_INFO, STA_REC, sta_NONE, TX_AUTH)

**结果: PLE_STA0 全程稳定**

```
DIAG[DEV_INFO]:   PLE_STA0=0x0000000a PLE_STA1=0x00000005
DIAG[PM_DISABLE]: PLE_STA0=0x0000000a PLE_STA1=0x00000005
DIAG[BSS_INFO]:   PLE_STA0=0x0000000a PLE_STA1=0x00000005
DIAG[STA_REC]:    PLE_STA0=0x0000000a PLE_STA1=0x00000005
DIAG[sta_NONE]:   PLE_STA0=0x0000000a PLE_STA1=0x00000005
DIAG[TX_AUTH]:    PLE_STA0=0x0000000a PLE_STA1=0x00000005
(auth try 1/3, 2/3, 3/3 全部相同)
```

**结论**: PLE STATION_PAUSE 已被 Test 1 (conn_state=0) 彻底解决。`0x0000000a` 是正常值。MPDU_ERR status=3 的根因不在 PLE PAUSE。已回退诊断代码。

## 三个 A/B 测试总结

| Test | 改动 | 结果 | 保留 |
|------|------|------|------|
| 1. conn_state 翻转 | `enable ? 0 : 1` | **突破!** PLE 0xf133ffff→0xa, TX_DONE 恢复 | **是** |
| 2. 延迟 BSS 激活 | DEV_INFO 移到 sta_state | 无额外改善 | 否 |
| 3. 逐命令 PAUSE 快照 | 6 个诊断点 | PLE 全程稳定 0xa | 否 |

### Test 4: STA_REC state=0 pre-auth (2026-02-22, Session 25)

**改动**: `req.state.state = enable ? 0 : 0` (之前 `enable ? 2 : 0`)

**原理**: Windows 在 auth 前发 state=0，我们发 state=2 可能导致固件状态机混乱。

**结果: 无改善** — MPDU_ERR status=3 cnt=30 完全相同。已回退。

## 四个 A/B 测试总结

| Test | 改动 | 结果 | 保留 |
|------|------|------|------|
| 1. conn_state 翻转 | `enable ? 0 : 1` | **突破!** PLE 解除, TX_DONE 恢复 | **是** |
| 2. 延迟 BSS 激活 | DEV_INFO 移到 sta_state | 无改善 | 否 |
| 3. 逐命令 PAUSE 快照 | 6 个诊断点 | PLE 全程稳定 0xa | 否 |
| 4. STA_REC state=0 | auth 前 state=0 | 无改善 | 否 |

### Test 5-7: BSS_INFO BASIC 字段逐个测试 (2026-02-22, Session 25)

| Test | 字段 | 改动 | 结果 |
|------|------|------|------|
| 5 | +1F link_idx→band_info | `req.basic.link_idx = mvif->band_idx` | 无改善 |
| 6 | +7 band_idx→sco=0xFF + +1F | `band_idx=0xFF` + `link_idx=band_idx` | 无改善 |
| 7 | +1C nonht_basic_phy→wlan_idx | `nonht_basic_phy = wcid.idx` | 无改善 |

**结论**: BSS_INFO BASIC 的三个争议字段 (+7/+1C/+1F) 单独或组合修改均不影响 MPDU_ERR。问题不在 BASIC TLV 字段布局。

**下一步**: MPDU_ERR status=3 cnt=30 根因仍未找到。
- 需解码 TXS 错误位 (ACK_TIMEOUT/RTS_TIMEOUT/QUEUE_TIMEOUT) 确认失败类型
- 考虑管理帧 TX 路径 (Ring 0 CT vs Ring 2 SF)
- 考虑射频/信道配置问题

---

*文档生成时间: 2026-02-22, 基于 CLAUDE.md + Windows RE 文档 + 源代码分析*
