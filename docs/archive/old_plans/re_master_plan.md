# MT7927 Windows 驱动逆向工程总方案

**版本**: 1.0
**日期**: 2026-02-22
**目标**: 通过字节级精确的 Windows 驱动逆向，彻底解决 auth TX 失败问题并为后续开发阶段提供参考数据
**状态**: 规划阶段（未执行）

---

## 零、方案背景与定位

### 为什么需要这个方案

MT7927 WiFi 驱动开发已进行 25+ session，auth TX 帧始终失败（TXFREE stat=1, MIB TX=0）。
现有逆向工作的根本问题：

1. **结构级逆向 vs 字节级逆向**：知道"BSS_INFO 有 14 个 TLV"但不知道每个字段的精确字节值
2. **文档互相冲突**：60+ 文档中至少 7 对冲突（见 `docs/re_audit_report.md`）
3. **推测当事实**：5+ 个"致命 Bug"推测全部被测试证伪
4. **缺少基准数据**：没有 Windows 实际发送的 hex dump 对比，只有反汇编推导

### 方案核心原则

- **字节级精确**：每个逆向任务的产出必须是精确到每个字节偏移的字段映射
- **可验证**：每个产出必须有验证方法（反汇编交叉引用、hex dump 对比、或测试结果）
- **消除推测**：明确区分"汇编验证的事实"和"推测性假设"，后者必须标注

### 逆向环境

| 项目 | 值 |
|------|-----|
| Ghidra 项目 | `tmp/ghidra_project/mt7927_re` |
| Windows 驱动 | `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` |
| ImageBase | `0x140000000` |
| 反汇编工具 | `tools/disasm_helper.py`（pefile + capstone） |
| BSS/STA 反汇编工具 | `tools/disasm_bss_sta.py` |
| dispatch table (CID) | `0x1402507e0`（58 条目，每条 13 字节） |
| BSS_INFO TLV table | `0x1402505b0`（14 条目，每条 16 字节） |
| STA_REC TLV table | `0x140250710`（13 条目，每条 16 字节） |

---

## 一、Phase 1 — Auth TX 阻塞问题（最高优先级）

### 目标

产出 Windows 连接流程中**每条 MCU 命令的完整 wire-format payload**，逐字节与我们的代码对比，
定位导致 TXFREE stat=1 的精确差异。

### 已完成的基础工作

以下部分已在 Session 24 完成结构级分析，本方案要求升级到**字节级精确**：

- BSS_INFO 14 TLV dispatch table 已提取（`docs/re/win_re_bss_info_all_tlvs.md`）
- STA_REC 13 TLV dispatch table 已提取（`docs/re/win_re_sta_rec_all_tlvs.md`）
- BssActivateCtrl 组合命令已分析（`docs/re/win_re_connect_flow_complete.md`）
- conn_type / extra_info 已汇编验证（`docs/re/win_re_codex_bss_verify.md`）

### 已知关键差异（需字节级确认）

| # | 差异 | 当前文档状态 | 影响等级 |
|---|------|-------------|---------|
| D1 | BSS_INFO_BASIC +0x04 active=bss_idx vs boolean | 推测性（codex_bss_verify） | 🔴 高 |
| D2 | BSS_INFO_BASIC +0x14 bmc_tx_wlan_idx=0 vs 真实值 | 未验证 | 🔴 高 |
| D3 | BSS_INFO_BASIC +0x18 dtim vs phymode 语义冲突 | 推测性 | 🔴 高 |
| D4 | BSS_INFO_BASIC +0x19/+0x1e phymode 编码差异 | 部分确认（phy_mode_from_band 已反汇编） | 🟡 中 |
| D5 | STA_REC conn_state=2(PORT_SECURE) vs Windows 硬编码 1 | 汇编确认（但仅限 BssActivateCtrl 路径） | 🟡 中 |
| D6 | 早期 BSS_INFO(band=255) 污染固件状态 | 未验证 | 🔴 高 |
| D7 | DEV_INFO payload 布局差异（flags=0x000C0000 缺失） | 结构级确认 | 🟡 中 |
| D8 | BssActivateCtrl DEV_INFO+BSS_INFO 原子 vs 分开发送 | 结构级确认 | 🟡 中 |
| D9 | STA_REC_BASIC conn_state: 完整 MtCmdSendStaRecUpdate 路径未确认 | 仅 BssActivateCtrl 内的简化版 | 🔴 高 |
| D10 | ChipConfig 每次连接重发 vs 仅初始化一次 | 结构级确认 | 🟡 中 |
| D11 | HE_BASIC TLV 字段内容 | 已知结构，未逐字段确认 | 🟡 中 |

---

### 任务列表

#### P1-T01: BSS_INFO_BASIC TLV 字节级重建（完整 MtCmdSetBssInfo 路径）

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T01 |
| **目标函数** | `0x14014c610`（nicUniCmdBssInfoTagBasic） |
| **辅助函数** | `0x14014fdfc`（phy_mode_from_band），`0x140151608`（connType） |
| **输入** | 现有反汇编（`docs/re/win_re_bss_info_all_tlvs.md` Entry[0]），`docs/re/win_re_codex_bss_verify.md` |
| **复杂度** | 中等 |
| **依赖** | 无 |

**输出格式**：

```
## BSS_INFO_BASIC TLV (tag=0x0000, len=0x0020, 32 bytes)
## 来源: nicUniCmdBssInfoTagBasic (0x14014c610) — 完整 MtCmdSetBssInfo 路径
## 验证方法: capstone 反汇编逐指令追踪

偏移  大小  字段名称         Windows 值来源             Windows 典型值(5G STA)    我们的值          匹配?
+0x00 u16   tag              硬编码                     0x0000                    0x0000            ✅
+0x02 u16   len              硬编码                     0x0020                    0x0020            ✅
+0x04 u8    active           bss[+0x2e6964] >>7 &1 反   1（BSS已激活）            mvif->bss_idx(0)  ❌?
+0x05 u8    omac_idx         param                      0                         mvif->omac_idx    ✅
...
```

**具体步骤**：

1. 用 `tools/disasm_helper.py disasm 0x14014c610 512` 获取完整反汇编
2. 逐条指令追踪每个字节写入目标（`rdx+N`）和数据来源（`r8+M` 或立即数）
3. 对于来自 `bss_info_ptr` 的字段（`r8+offset`），追踪回 `MtCmdSetBssInfo`（`0x1400cf928`）中如何填充该 flat struct
4. 特别关注 `+0x04`(active)、`+0x14`(bmc_tx_wlan_idx 或其他)、`+0x18`(dtim 或 phymode)、`+0x19`(phymode_lo)、`+0x1e`(phymode_hi) 这 5 个未确认字段
5. 产出上述格式的逐字节映射表，标注每个字段是 "汇编验证" 还是 "推测"

**验证标准**：
- 每个字段必须有对应的反汇编指令地址
- `+0x14`/`+0x18` 字段的语义必须解决（到底是 beacon_int/dtim 还是其他）
- phymode 编码与 `phy_mode_from_band`（`0x14014fdfc`，已完整反汇编）的返回值对齐

---

#### P1-T02: MtCmdSetBssInfo flat input struct 重建

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T02 |
| **目标函数** | `0x1400cf928`（MtCmdSetBssInfo） |
| **辅助函数** | 可能调用多个 adapter/bss 访问器填充 flat struct |
| **输入** | P1-T01 产出（知道 TLV builder 从 flat struct 哪些偏移读取） |
| **复杂度** | 复杂 |
| **依赖** | P1-T01（需要知道 flat struct 的哪些偏移被 TLV builder 使用） |

**目标**：重建 MtCmdSetBssInfo 在调用 14 个 TLV handler 之前，如何从 adapter/bss 对象填充 flat input struct。

**输出格式**：

```
## MtCmdSetBssInfo Flat Input Struct (传给 TLV dispatch table 的 param_3)
## 函数: 0x1400cf928

偏移  大小  来源（adapter/bss 偏移）  含义             对应 TLV 字段
+0x00 u8    bss->bss_idx             bss_idx          BASIC.active
+0x01 u8    bss->phy_idx             phy_idx          BASIC.band_info
...
+0x2c u16   bss->operational_rates   速率集           RATE.operational_rates
+0x2e u16   bss->extended_rates      扩展速率集       RATE.extended_rates
...
```

**具体步骤**：

1. 反汇编 `0x1400cf928` 前 200+ 条指令（`disasm_helper.py disasm 0x1400cf928 1024`）
2. 找到 flat struct 分配（通常在栈上，如 `sub rsp, 0x80` 后的 `lea r8, [rsp+XX]`）
3. 追踪所有写入 flat struct 的指令（`mov [rsp+XX+N], ...`）
4. 将每个写入映射到 adapter/bss 对象的源偏移
5. 与 P1-T01 产出的 TLV 字段映射交叉验证

**验证标准**：
- flat struct 大小必须确认
- 每个写入 flat struct 的偏移必须有反汇编指令地址
- P1-T01 中所有 `r8+offset` 引用必须在此 struct 中找到对应

---

#### P1-T03: BssActivateCtrl DEV_INFO payload 字节级重建

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T03 |
| **目标函数** | `0x140143540`（nicUniCmdBssActivateCtrl），DEV_INFO 部分 |
| **辅助函数** | `0x14014f788`（nicUniCmdBufAlloc，CID=1） |
| **输入** | 现有分析（`docs/re/win_re_connect_flow_complete.md` Section 三） |
| **复杂度** | 简单 |
| **依赖** | 无 |

**目标**：精确重建 BssActivateCtrl 中 DEV_INFO (CID=1) 的 wire payload 每个字节。

**输出格式**：

```
## DEV_INFO payload (BssActivateCtrl 路径, CID=0x01, 16 bytes)
## 函数: 0x140143540 → nicUniCmdBufAlloc(adapter, CID=1, size=0x10)

偏移  大小  字段            Windows 汇编来源                值(STA activate)  我们的值
+0x00 u8    omac_idx        rbp[3]                          0                 ✅ omac_idx
+0x01 u8    active_flag     mode!=4 ? 0xFF : 0xFE           0xFF              ✅ enable
+0x02 u8[2] padding         未写入（zeroed）                0x0000            ✅
+0x04 u32   flags           硬编码                          0x000C0000        ❌ 我们没有此字段!
+0x08 u8    band_idx        rbp[1]                          1 (5GHz)          ✅ band_idx
+0x09 u8    phy_idx         rbp[0xb]                        ?                 ❓
+0x0a u8[6] mac_addr        sta_entry or bss                MAC               ✅
```

**具体步骤**：

1. 用 `disasm_helper.py disasm 0x140143540 768` 反汇编 BssActivateCtrl 完整函数
2. 找到 `nicUniCmdBufAlloc(adapter, 1, 0x10)` 调用（DL=1, R8=0x10）
3. 追踪返回的 buffer 指针（rax→entry, entry->data）上的所有写入
4. 每个写入记录：目标偏移、数据来源、汇编指令地址
5. 特别确认 `flags=0x000C0000` 字段的精确位置和值

**验证标准**：
- `flags=0x000C0000` 的偏移和值必须汇编级确认
- 所有 16 字节必须有来源说明
- 与我们的 `struct dev_info_hdr + struct dev_info_active_tlv` 逐字节对比

---

#### P1-T04: BssActivateCtrl BSS_INFO(BASIC+MLD) payload 字节级重建

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T04 |
| **目标函数** | `0x140143540`（nicUniCmdBssActivateCtrl），BSS_INFO 部分 |
| **辅助函数** | `0x14014fad0`（nicUniCmdBssInfoMld） |
| **输入** | 现有分析（`docs/re/win_re_connect_flow_complete.md` Section 三），P1-T03 |
| **复杂度** | 中等 |
| **依赖** | 无（可与 P1-T03 同时做） |

**目标**：精确重建 BssActivateCtrl 中 BSS_INFO (CID=2) 的 wire payload（BASIC 32B + MLD 20B = 56B when activate）。

**输出格式**：

```
## BSS_INFO payload (BssActivateCtrl 路径, CID=0x02, 56 bytes when activate)
## 函数: 0x140143540

--- bss_req_hdr (4 bytes) ---
偏移  大小  字段            Windows 汇编来源             值(STA activate)  我们的值
+0x00 u8    bss_idx         rbp[0]                       0                 ✅

--- BASIC TLV (tag=0x0000, len=0x0020, 32 bytes) ---
+0x04 u32   tag_len         硬编码                       0x00200000        ✅
+0x08 u8    bss_idx_dup     rbp[0]                       0                 ?
...

--- MLD TLV (tag=0x001A, len=0x0014, 20 bytes) ---
+0x24 u32   tag_len         硬编码 by nicUniCmdBssInfoMld 0x0014001A       ✅
+0x28 u8    link_id         0xFF (non-MLD)                0xFF             ✅
...
```

**具体步骤**：

1. 从 P1-T03 的反汇编继续，找到第二个 `nicUniCmdBufAlloc(adapter, 2, size)` 调用
2. size = activate ? 0x38(56B) : 0x24(36B) — 确认此计算
3. 追踪 buffer 上 [+0x00] 到 [+0x23] 的 BASIC TLV 每个字节写入
4. 追踪 `call 0x14014fad0` (MLD TLV builder) 的写入，buffer 偏移 [+0x24] 到 [+0x37]
5. 特别对比 BssActivateCtrl BASIC 与 full BSS_INFO BASIC 的字段差异

**验证标准**：
- 56 字节每个字节都有来源说明
- conn_state=1（activate）在 BASIC TLV 中的精确偏移确认
- MLD TLV 中 group_mld_id 的 BssActivateCtrl 路径值（bss_idx 还是 0xFF）

---

#### P1-T05: PM_DISABLE payload 字节级确认

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T05 |
| **目标函数** | `0x1400caefc`（nicUniCmdPmDisable） |
| **输入** | 现有分析（`docs/re/win_re_connect_flow_complete.md` Section 五 步骤 3） |
| **复杂度** | 简单 |
| **依赖** | 无 |

**目标**：确认 PM_DISABLE 是否真的只有 `bss_idx(1) + pad(3) + tag(2) + len(2)` = 8 字节。

**具体步骤**：

1. `disasm_helper.py disasm 0x1400caefc 256`
2. 找到 `nicUniCmdBufAlloc(adapter, CID=2, size=?)` 调用，确认 size 参数
3. 追踪所有字节写入，确认 tag=0x1B、len 值、有无额外 payload

**输出**：PM_DISABLE wire payload hex dump 模板 + 每字节说明

**验证标准**：
- 总 payload 大小（含 bss_req_hdr）精确确认
- tag=0x1B 的位置和 len 值精确确认
- 与我们的 `mt7927_mcu_bss_pm_disable()` 逐字节对比

---

#### P1-T06: DEV_INFO 独立命令 payload 字节级重建（非 BssActivateCtrl 路径）

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T06 |
| **目标函数** | 需要先定位 — 从 `MtCmdActivateDeactivateNetwork` 回溯 |
| **辅助函数** | dispatch entry[9] handler `0x140143540` 就是 BssActivateCtrl |
| **输入** | P1-T03 产出 |
| **复杂度** | 简单 |
| **依赖** | P1-T03 |

**目标**：确认 Windows 是否有**独立的** DEV_INFO 命令（不通过 BssActivateCtrl），如果有，其 payload 与 BssActivateCtrl 内的 DEV_INFO 有何差异。

**具体步骤**：

1. 分析 dispatch entry[9]（outer=0x11, handler=`0x140143540`）—— 这就是 BssActivateCtrl
2. **关键发现**：Windows 中**所有** outer_tag=0x11 的命令都走 BssActivateCtrl，不存在独立 DEV_INFO
3. 确认我们分开发送 DEV_INFO 的方式是否等价于 BssActivateCtrl 的 DEV_INFO 部分

**输出**：确认报告 — DEV_INFO 是否有独立路径，以及分开发送是否等价

---

#### P1-T07: STA_REC 完整路径 MtCmdSendStaRecUpdate 字节级重建

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T07 |
| **目标函数** | `0x1400cdea0`（MtCmdSendStaRecUpdate） |
| **辅助函数** | `0x1401446d0`（nicUniCmdUpdateStaRec，dispatch entry[22]） |
| **输入** | 现有 STA_REC TLV 分析（`docs/re/win_re_sta_rec_all_tlvs.md`） |
| **复杂度** | 复杂 |
| **依赖** | 无 |

**目标**：重建 `MtCmdSendStaRecUpdate` 如何构建 0xEC 字节 flat input struct，以及 `nicUniCmdUpdateStaRec` 如何用它生成 wire payload。

**重点确认**：

1. **STA_REC_BASIC conn_state** —— `0x14014d6d0` 是 BssActivateCtrl 简化版（硬编码 1），完整 MtCmdSendStaRecUpdate 用的 builder 是否相同函数？如果不同，conn_state 从哪里来？
2. **STA_REC 请求头**（`sta_req_hdr`）—— bss_idx, wlan_idx_lo/hi, muar_idx, is_tlv_append, tlv_num 的精确值
3. **flat struct 偏移 0x14**（state）的填充来源 —— 是来自 MlmeCntlWaitJoinProc 的参数还是 bss 对象？

**具体步骤**：

1. `disasm_helper.py disasm 0x1400cdea0 1024` — 反汇编 MtCmdSendStaRecUpdate
2. 找到 flat struct（0xEC 字节）的栈分配和填充指令
3. 追踪每个 `[rsp+XX+N] = ...` 写入，映射到 adapter/bss/sta 对象偏移
4. 然后看 `FUN_1400cdc4c(bss, 0x13, 0xed, 0)` 如何将 flat struct 传给 dispatcher
5. 追踪 `nicUniCmdUpdateStaRec`（`0x1401446d0`）如何读取 flat struct 并遍历 13 TLV dispatch table

**输出格式**：

```
## STA_REC Wire Payload (完整连接路径)
## MtCmdSendStaRecUpdate (0x1400cdea0) → nicUniCmdUpdateStaRec (0x1401446d0)

--- sta_req_hdr ---
+0x00 u8  bss_idx        MtCmd 参数          0                  ✅
+0x01 u8  wlan_idx_lo    sta_rec field        1                  ✅
+0x02 u8  wlan_idx_hi    0                    0                  ✅
+0x03 u8  muar_idx       omac_idx             0                  ✅
+0x04 u8  is_tlv_append  硬编码               1                  ✅
+0x06 u16 tlv_num        dispatch count       13(或条件性更少)   我们=9

--- Flat Input Struct (0xEC bytes) 关键偏移 ---
+0x01  bss_idx      bss->idx                   0
+0x02  peer_addr[6] sta->addr                  AP MAC
+0x08  aid          sta->aid                   0 (auth前)
+0x0d  phy_type     bss->phy_capability        取决于 AP
+0x12  qos          bss->wmm_enabled           取决于 AP
+0x14  state        ⚠️ 关键: 从哪里来?          ???
...
```

**验证标准**：
- flat struct 中 state（偏移 0x14）的填充来源必须汇编级确认
- 所有 13 个 TLV handler 是否都被调用，还是某些条件性跳过（如 HT_INFO 当 ht_cap=0 时返回 0）
- tlv_num 是否动态计算（根据实际写入的 TLV 数量）还是固定值

---

#### P1-T08: STA_REC 各 TLV 字节级与驱动代码逐项对比

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T08 |
| **目标函数** | 13 个 TLV builder（见 dispatch table `0x140250710`） |
| **输入** | P1-T07 产出，现有 TLV 分析（`docs/re/win_re_sta_rec_all_tlvs.md`） |
| **复杂度** | 中等（大部分 TLV 已有反汇编，需逐字节对比） |
| **依赖** | P1-T07 |

**目标**：将 Windows 的 13 个 STA_REC TLV 与我们的 9 个 TLV 逐字节对比，找出所有差异。

**需要特别关注的 TLV**：

| TLV | 我们有? | 需验证内容 |
|-----|---------|-----------|
| BASIC (0x00) | ✅ | conn_state 值（完整路径 vs BssActivateCtrl 简化版） |
| RA_INFO (0x01) | ✅ | legacy 速率编码是否匹配，rx_mcs_bitmask 大小 |
| STATE_INFO (0x07) | ✅ | state=0/1/2 的精确含义，flags 字段内容 |
| HT_INFO (0x09) | ✅ | 条件性：auth 前 AP HT cap 是否可用？ |
| VHT_INFO (0x0a) | ✅ | 条件性：auth 前 AP VHT cap 是否可用？ |
| PHY_INFO (0x15) | ✅ | basic_rate bitmap 编码，phy_type 值 |
| BA_OFFLOAD (0x16) | ✅ | auth 阶段值应全零 |
| HE_BASIC (0x19) | ✅ | **需要完整反汇编**（`0x14014d810`），HE 字段精确值 |
| UAPSD (0x24) | ✅ | auth 阶段值应全零 |
| HE_6G_CAP (0x17) | ❌ | 确认 5GHz 是否跳过 |
| EHT_INFO (0x22) | ❌ | 确认是否需要 |
| EHT_MLD (0x21) | ❌ | 确认是否需要 |
| MLD_SETUP (0x20) | ❌ | 确认是否需要 |

**具体步骤**：

1. 对每个已有的 TLV builder，用 `disasm_helper.py disasm <addr> 256` 获取反汇编
2. 逐字段对比 Windows 写入值 vs 我们的 `mt7927_mcu_sta_update()` 写入值
3. 对 HE_BASIC（`0x14014d810`，28 字节 TLV），做完整反汇编并记录每个字段
4. 确认 HE_6G_CAP / EHT / MLD 系列 TLV 在 5GHz 非 MLO 连接时是否被跳过

**输出**：13 TLV 逐字节对比表，每个差异标注 "确认匹配" / "确认不匹配" / "需进一步验证"

---

#### P1-T09: BSS_INFO 各 TLV 字节级与驱动代码逐项对比

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T09 |
| **目标函数** | 14 个 TLV builder（见 dispatch table `0x1402505b0`） |
| **输入** | P1-T01/T02 产出，现有 TLV 分析（`docs/re/win_re_bss_info_all_tlvs.md`） |
| **复杂度** | 中等 |
| **依赖** | P1-T01, P1-T02 |

**目标**：将 Windows 的 14+3 个 BSS_INFO TLV（entry[1] 输出 RLM+PROTECT+IFS_TIME 3 合 1）与我们的 TLV 逐字节对比。

**需要特别关注的 TLV**：

| TLV | 我们有? | 需验证内容 |
|-----|---------|-----------|
| BASIC (0x00) | ✅ | 见 P1-T01，5 个不确定字段 |
| RLM (0x02) | ✅ | bw_mode 转换逻辑（switch 0→0/1, 1→2, 2→3, 3→6, 4→7） |
| PROTECT (0x03) | ✅ | 5GHz 应为 flags=0，但我们的 struct 是否多了字段？ |
| IFS_TIME (0x17) | ✅ | slot_valid vs sifs：Windows 只设 sifs(9/20)，不设 slot_valid |
| RATE (0x0B) | ✅ | operational_rates / extended_rates 的精确 bitmap 编码 |
| SEC (0x10) | ✅ | open auth 时 cipher_type=0, group_cipher=0 |
| QBSS (0x0F) | ✅ | is_qbss=1 是否正确？Windows 用 bss[+0x2a] |
| SAP (0x0D) | ✅ | STA 模式全零是否正确 |
| P2P (0x0E) | ✅ | STA 模式全零是否正确 |
| HE (0x05) | ✅ | `0x14014cd50` 需完整反汇编，16 字节精确内容 |
| BSS_COLOR (0x04) | ✅ | `0x14014d010` 需完整反汇编，8 字节精确内容 |
| EHT (0x1E) | ✅ | `0x14014d150` 需完整反汇编，16 字节精确内容 |
| MBSSID (0x06) | ✅ | STA 模式全零是否正确 |
| 0x0C (未知) | ✅ | bss[+0x38] 的含义 |
| IoT (0x18) | ✅ | `0x14014d350` 需完整反汇编 |
| MLD (0x1A) | ✅ | 非 MLD 默认值已确认 |

**具体步骤**：

1. 对尚未完整反汇编的 TLV builder（HE, BSS_COLOR, EHT, IoT），各用 `disasm_helper.py disasm <addr> 256`
2. 逐字段对比每个 TLV 的 Windows 值 vs 我们的值
3. **重点**：RLM TLV 的 bw_mode 转换逻辑 —— Windows 的 switch 是 `0→(ht_mode?1:0), 1→2, 2→3, 3→6, 4→7`，我们直接写 `bw=0`（20MHz）是否正确
4. **重点**：IFS_TIME 的反汇编确认 `valid=1` 对应的是 slot_valid 还是 sifs_valid —— 现有文档与代码可能不一致

**输出**：17 TLV（14 entry，entry[1] 拆为 3 个）逐字节对比表

---

#### P1-T10: ChPrivilege/ROC payload 字节级重建

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T10 |
| **目标函数** | `0x140144820`（nicUniCmdChReqPrivilege，dispatch entry[12]） |
| **辅助函数** | `0x14014fe60`（acquire TLV builder） |
| **输入** | 现有分析（`docs/re/win_re_cid_mapping.md` Section 9） |
| **复杂度** | 中等 |
| **依赖** | 无 |

**目标**：精确重建 ChPrivilege acquire 命令的 wire payload。

**重点确认**：

1. CID=0x27 的 payload 精确格式（alloc size、TLV 结构）
2. channel / band / bandwidth 的编码方式
3. timeout / token 的位置
4. Windows MlmeCntlWaitJoinProc 中 ChPrivilege 的参数来源

**具体步骤**：

1. `disasm_helper.py disasm 0x140144820 512` — dispatch handler
2. `disasm_helper.py disasm 0x14014fe60 512` — acquire TLV builder
3. 追踪 payload 每个字节的来源
4. 与我们的 ROC acquire 实现对比

**输出**：ChPrivilege acquire wire payload hex dump 模板

---

#### P1-T11: ChipConfig 命令 payload 分析

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T11 |
| **目标函数** | `0x1400484c0`（ChipConfig 调用点），`0x140143a70`（dispatch handler，entry[4]） |
| **输入** | 现有分析（`docs/re/win_re_connect_flow_complete.md` 步骤 1） |
| **复杂度** | 复杂（328B payload） |
| **依赖** | 无 |

**目标**：确认 ChipConfig (CMD_ID=0xca, CID=0x0e) 的 328B payload 内容。
Windows **每次 connect 都重发**此命令，我们只在 init 发一次。

**重点确认**：

1. 328B payload 是否包含信道/频段相关配置
2. 是否包含 auth 所需的射频/PHY 设置
3. 如果是纯配置命令（与连接无关），可暂缓；如果包含频段设置，则必须在连接时重发

**具体步骤**：

1. `disasm_helper.py disasm 0x140143a70 1024` — dispatch handler
2. 分析 handler 如何构建 328B payload
3. 确认 payload 中是否有信道/频段/功率相关字段

**输出**：ChipConfig payload 结构概览（不要求逐字节，但关键字段必须精确）

---

#### P1-T12: UniCmd 帧封装格式精确重建

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T12 |
| **目标函数** | `0x14014eb0c`（nicUniCmdSendCmd） |
| **辅助函数** | `0x1401cc290`（header overhead 计算），`0x1400ca864`（option decode） |
| **输入** | 现有分析（`docs/re/win_re_connect_flow_complete.md` 附录 B） |
| **复杂度** | 中等 |
| **依赖** | 无 |

**目标**：精确重建 UniCmd 帧的 wire format —— TXD(32B) + UniCmd Header + Payload。

**重点确认**：

1. UniCmd Header 的精确布局（len、CID、seq、S2D、option flag 的偏移）
2. option=0xed 解码后在 header 中的表现方式
3. header overhead 是 0x30 还是 0x40（取决于 `FUN_1401cc290` 返回值）
4. payload 复制到 `[r14 + 0x30]` 还是其他偏移

**具体步骤**：

1. `disasm_helper.py disasm 0x14014eb0c 512`
2. 追踪 TXD DW0（`0x41000000 | len`）的构建
3. 追踪 UniCmd Header 每个字段的写入
4. 确认 header overhead 对于 BSS_INFO / STA_REC / DEV_INFO 各是多少
5. 与我们的 `mt7927_mcu_send_unicmd()` 实现对比

**输出**：UniCmd 帧 wire format 字节级布局图

---

#### P1-T13: 连接流程完整 hex dump 模板生成

| 属性 | 值 |
|------|-----|
| **任务 ID** | P1-T13 |
| **目标函数** | 综合 P1-T01 到 P1-T12 的产出 |
| **复杂度** | 中等（综合已有结果） |
| **依赖** | P1-T01 到 P1-T12 全部完成 |

**目标**：基于所有子任务产出，生成 Windows 连接流程中每条命令的**预期 hex dump**，
作为我们驱动输出的对照基准。

**输出格式**：

```
## Windows 连接命令序列 — 预期 Wire Format

### [1] ChipConfig (CMD_ID=0xca, option=0xed)
总长: TXD(32) + Header(16) + Payload(328) = 376 bytes
TXD:     41 00 01 78 ...   (DW0 = 0x41000178)
Header:  78 01 0e ca a0 00 ...
Payload: XX XX XX XX ...

### [2] BssActivateCtrl — DEV_INFO (CID=0x01)
总长: TXD(32) + Header(16) + Payload(16) = 64 bytes
TXD:     41 00 00 40 ...
Header:  40 00 01 00 a0 00 ...
Payload: 00 FF 00 00 00 00 0C 00 01 00 XX XX XX XX XX XX

### [2] BssActivateCtrl — BSS_INFO (CID=0x02, BASIC+MLD)
总长: TXD(32) + Header(16) + Payload(56) = 104 bytes
...

### [3] PM_DISABLE (CID=0x02, tag=0x1B)
...

### [4] Full BSS_INFO (CID=0x02, 14 TLVs)
...

### [5] STA_REC (CID=0x03, 13 TLVs)
...

### [6] Auth TX (Ring 0 CT mode)
TXD: DW0=0x2000003e DW1=0x800c9001 DW2=0x001e000b ...
```

**验证标准**：
- 每条命令的 hex dump 必须与汇编分析一致
- 每个字节必须有来源注释
- 将此模板与 dmesg 实际输出对比，找出**精确差异**

---

### Phase 1 任务依赖图

```
                    ┌─────────┐
                    │ P1-T12  │  UniCmd 帧封装
                    │ (独立)  │
                    └────┬────┘
                         │
    ┌─────────┐     ┌────┴────┐     ┌─────────┐     ┌─────────┐
    │ P1-T03  │     │ P1-T01  │     │ P1-T07  │     │ P1-T10  │
    │DEV_INFO │     │BSS BASIC│     │STA_REC  │     │ChPriv   │
    │BssActCtrl│    │(full)   │     │MtCmdSend│     │ROC      │
    └────┬────┘     └────┬────┘     └────┬────┘     └─────────┘
         │               │               │
    ┌────┴────┐     ┌────┴────┐     ┌────┴────┐     ┌─────────┐
    │ P1-T04  │     │ P1-T02  │     │ P1-T08  │     │ P1-T05  │
    │BSS BASIC│     │MtCmdSet │     │STA TLV  │     │PM_DIS   │
    │BssActCtrl│    │BSS flat │     │对比     │     │         │
    └────┬────┘     └────┬────┘     └─────────┘     └─────────┘
         │               │
    ┌────┴────┐     ┌────┴────┐     ┌─────────┐
    │ P1-T06  │     │ P1-T09  │     │ P1-T11  │
    │DEV_INFO │     │BSS TLV  │     │ChipCfg  │
    │独立?    │     │对比     │     │328B     │
    └─────────┘     └─────────┘     └─────────┘
                         │
                    ┌────┴────┐
                    │ P1-T13  │  综合 hex dump 模板
                    │依赖全部 │
                    └─────────┘
```

**可并行组**：

- **组 A**（BSS_INFO 方向）: P1-T01 → P1-T02 → P1-T09
- **组 B**（STA_REC 方向）: P1-T07 → P1-T08
- **组 C**（BssActivateCtrl）: P1-T03 + P1-T04（并行）→ P1-T06
- **组 D**（独立小任务）: P1-T05, P1-T10, P1-T11, P1-T12（均可并行）

---

## 二、Phase 2 — TX 路径（如果 Phase 1 不够）

### 目标

如果修复了所有 MCU 命令差异后 auth 仍失败，则需要深入 TX 路径本身。

---

#### P2-T01: Ring 0 CT mode TXD + TXP 完整构建路径（XmitWriteTxDv1）

| 属性 | 值 |
|------|-----|
| **任务 ID** | P2-T01 |
| **目标函数** | `0x1401a2ca4`（XmitWriteTxDv1） |
| **辅助函数** | `0x1401a2c8c`（caller），`0x14005d6d8`（N6PciUpdateAppendTxD / TXP builder） |
| **输入** | 现有 TXD 分析（`docs/re/win_re_dw2_dw6_verified.md`），`MEMORY.md` TXD 值 |
| **复杂度** | 已大部分完成，需补充 TXP 部分 |
| **依赖** | 无 |

**目标**：
1. 确认 Ring 0 CT mode 的 TXP（TX Payload descriptor）精确格式
2. `N6PciUpdateAppendTxD`（`0x14005d6d8`）从未被反编译 —— 需完整分析
3. DMA 描述符格式在 CT mode 下的精确布局

**具体步骤**：

1. `disasm_helper.py disasm 0x14005d6d8 768` — TXP builder
2. 追踪 TXP 结构中 msdu_id、buf0/len0/buf1/len1 的精确编码
3. 确认 LAST bit 位置（BIT(15) of len0？）
4. 确认 VALID bit 位置（BIT(15) of msdu_id？）

**输出**：TXP wire format 字节级布局 + 与我们的 `struct mt7927_hw_txp` 对比

---

#### P2-T02: Ring 2 SF mode 完整路径分析

| 属性 | 值 |
|------|-----|
| **任务 ID** | P2-T02 |
| **目标函数** | `0x14005d1a4`（N6PciTxSendPkt） |
| **输入** | 现有分析（`docs/re/win_re_ring2_analysis.md`），`docs/re/win_re_full_txd_dma_path.md` |
| **复杂度** | 中等 |
| **依赖** | P2-T01（了解 TXD 通用构建后再看 Ring 2 差异） |

**目标**：确认 Ring 2 SF mode 下 TXD + 802.11 帧是否有特殊处理（如额外 padding、DW0 Q_IDX 编码）。

**重点确认**：

1. SF mode 下 TXD DW0 的 Q_IDX 编码（Ring 2 对应 Q_IDX=什么？）
2. SF mode 下 802.11 帧头是否直接附在 TXD 后面（无 TXP）
3. DMA descriptor 在 SF mode 下的 format（与 CT mode 有何不同）

**具体步骤**：

1. 在 `N6PciTxSendPkt` 中找到 `param_3=2`（Ring 2）的分支
2. 追踪 DMA descriptor 构建：base_addr、buf_addr、buf_len 的写入
3. 确认 SF mode 不写 TXP，TXD 直接跟帧 payload

---

#### P2-T03: WTBL 创建和 BAND 字段赋值逻辑

| 属性 | 值 |
|------|-----|
| **任务 ID** | P2-T03 |
| **目标函数** | 需要定位 — 固件内部函数，无法从 Windows 驱动反汇编 |
| **替代方法** | 从 MT6639 Android 源码 `wf_ds_lwtbl.h` + 驱动寄存器观察 |
| **复杂度** | 复杂 |
| **依赖** | P1-T07（STA_REC 精确 payload） |

**目标**：理解 WTBL DW0 bits[27:26]（BAND 字段）到底由什么控制。

**当前问题**：无论 BSS_INFO/STA_REC 传什么 band_idx，WTBL BAND 始终为 0。

**方法**：

1. **方法 A**：从 MT6639 Android 源码搜索 WTBL BAND 赋值路径（`wlanTableInsertRow` 或类似函数）
2. **方法 B**：用 AR9271 monitor mode 抓包，确认 auth 帧是否实际在空中发射（如果有信号说明 BAND=0 不是问题）
3. **方法 C**：读取 Windows 运行状态下的 WTBL 寄存器（需要 Windows 机器 + WinDbg）—— 可能不可行

**输出**：WTBL BAND 赋值机制说明 + 是否真的影响 TX

---

#### P2-T04: TXFREE 解码（stat 字段精确含义）

| 属性 | 值 |
|------|-----|
| **任务 ID** | P2-T04 |
| **目标函数** | 需要定位 — Windows TXFREE 处理函数 |
| **输入** | `MEMORY.md` 中的 TXFREE DW0-DW4 dump |
| **复杂度** | 中等 |
| **依赖** | 无 |

**目标**：确认 TXFREE stat=0/1/2 的精确含义（成功/失败/其他）。

**方法**：

1. 在 Windows 驱动中搜索 TXFREE 处理函数（通常在 RX event handler 中）
2. 追踪 stat 字段的解码和错误处理逻辑
3. 确认 DW1 byte2=0x05 是否是 tx_count

**具体步骤**：

1. 在 `mtkwecx.sys` 中搜索 TXFREE 处理 —— 通常在 RX event dispatch 中匹配 PKT_TYPE
2. `grep` 常量 0x2D（TX_DONE event ID）或 stat 字段解码逻辑
3. 反汇编 TXFREE 解码函数

---

## 三、Phase 3 — 后续阶段准备

### 目标

为 assoc / key exchange / 数据帧 TX/RX 提供逆向参考数据。
仅在 auth 成功后启动。

---

#### P3-T01: Assoc 请求路径

| 属性 | 值 |
|------|-----|
| **任务 ID** | P3-T01 |
| **目标函数** | 需要定位 — 从 `MlmeCntlWaitJoinProc` 回溯 assoc 帧发送 |
| **复杂度** | 中等 |
| **依赖** | Auth 成功 |

**目标**：
- Assoc 请求帧的 TXD 构建（与 auth 有何不同？）
- Assoc 后的 STA_REC 更新（state 从 2 变到什么？）
- BSS_INFO 是否在 assoc 后再次更新

---

#### P3-T02: Key Exchange (EAPOL) 路径

| 属性 | 值 |
|------|-----|
| **任务 ID** | P3-T02 |
| **目标函数** | 需要定位 |
| **复杂度** | 复杂 |
| **依赖** | P3-T01 |

**目标**：
- EAPOL 帧是否走特殊 Ring（Ring 2 还是 Ring 0？）
- 4-way handshake 中的 STA_REC 更新（PORT_SECURE 何时设置？）
- 密钥安装（STA_REC SEC TLV 或独立命令？）

---

#### P3-T03: 数据帧 TX/RX 路径

| 属性 | 值 |
|------|-----|
| **任务 ID** | P3-T03 |
| **目标函数** | N6PciTxSendPkt（Ring 0 数据帧路径） |
| **复杂度** | 中等 |
| **依赖** | P3-T02 |

**目标**：
- 数据帧 TXD 格式（与管理帧的 TXD 差异）
- 数据帧 TXP 格式（scatter-gather 多 buffer 场景）
- RX 数据帧解析（CONNAC3 RXD 格式）

---

## 四、任务总览表

| 任务 ID | 名称 | 目标函数 | 复杂度 | 依赖 | Phase |
|---------|------|---------|--------|------|-------|
| P1-T01 | BSS_INFO_BASIC TLV 字节级重建 | `0x14014c610` | 中等 | 无 | 1 |
| P1-T02 | MtCmdSetBssInfo flat struct | `0x1400cf928` | 复杂 | T01 | 1 |
| P1-T03 | BssActivateCtrl DEV_INFO | `0x140143540` | 简单 | 无 | 1 |
| P1-T04 | BssActivateCtrl BSS_INFO | `0x140143540` + `0x14014fad0` | 中等 | 无 | 1 |
| P1-T05 | PM_DISABLE | `0x1400caefc` | 简单 | 无 | 1 |
| P1-T06 | DEV_INFO 独立路径确认 | 分析结果 | 简单 | T03 | 1 |
| P1-T07 | STA_REC MtCmdSendStaRecUpdate | `0x1400cdea0` + `0x1401446d0` | 复杂 | 无 | 1 |
| P1-T08 | STA_REC 13 TLV 对比 | 13 TLV builders | 中等 | T07 | 1 |
| P1-T09 | BSS_INFO 14 TLV 对比 | 14 TLV builders | 中等 | T01,T02 | 1 |
| P1-T10 | ChPrivilege/ROC | `0x140144820` + `0x14014fe60` | 中等 | 无 | 1 |
| P1-T11 | ChipConfig 328B | `0x1400484c0` + `0x140143a70` | 复杂 | 无 | 1 |
| P1-T12 | UniCmd 帧封装 | `0x14014eb0c` | 中等 | 无 | 1 |
| P1-T13 | 综合 hex dump 模板 | 综合 | 中等 | 全部 | 1 |
| P2-T01 | Ring 0 CT TXD+TXP | `0x1401a2ca4` + `0x14005d6d8` | 中等 | 无 | 2 |
| P2-T02 | Ring 2 SF mode | `0x14005d1a4` | 中等 | T01 | 2 |
| P2-T03 | WTBL BAND 赋值 | MT6639 源码 / 寄存器观察 | 复杂 | P1-T07 | 2 |
| P2-T04 | TXFREE 解码 | 需定位 | 中等 | 无 | 2 |
| P3-T01 | Assoc 路径 | 需定位 | 中等 | Auth 成功 | 3 |
| P3-T02 | EAPOL 路径 | 需定位 | 复杂 | T01 | 3 |
| P3-T03 | 数据帧 TX/RX | `0x14005d1a4` | 中等 | T02 | 3 |

---

## 五、关键函数地址参考表

### 连接流程主函数

| 函数名 | VA | 用途 | 分析状态 |
|--------|-----|------|---------|
| WdiTaskConnect | `0x140065be0` | 连接入口 | 结构级 |
| MlmeCntlOidConnectProc | `0x140123588` | 连接流程主体 | 结构级 |
| MlmeCntlWaitJoinProc | `0x1401273a8` | Join 后处理 | 结构级 |
| MtCmdActivateDeactivateNetwork | `0x1400c558c` | BSS 激活/停用 | 结构级 |
| MtCmdSetBssInfo | `0x1400cf928` | 完整 BSS_INFO 发送 | **需字节级** |
| MtCmdSendStaRecUpdate | `0x1400cdea0` | STA_REC 发送 | **需字节级** |
| MtCmdChPrivilage | `0x1400c5e08` | 信道请求 | 需分析 |
| FUN_1400ac6c8 | `0x1400ac6c8` | Auth TX 入队 | 结构级 |
| ChipConfig | `0x1400484c0` | 芯片配置 | 需分析 |

### UniCmd 基础设施

| 函数名 | VA | 用途 | 分析状态 |
|--------|-----|------|---------|
| nicUniCmdAllocEntry | `0x1400cdc4c` | dispatch stub | 结构级 |
| nicUniCmdDispatch | `0x14014e644` | UniCmd 路由 | 结构级 |
| nicUniCmdSendCmd | `0x14014eb0c` | 帧发送 | 需字节级 |
| nicUniCmdBufAlloc | `0x14014f788` | buffer 分配 | 结构级 |
| tag→index converter | `0x14014f720` | dispatch table 查找 | 已完成 |
| option decode | `0x1400ca864` | 0xed→0x8000 | 已完成 |

### Dispatch Handlers — BSS_INFO

| Handler | VA | TLV | 分析状态 |
|---------|-----|-----|---------|
| nicUniCmdBssActivateCtrl | `0x140143540` | DEV+BSS(BASIC+MLD) | 结构级，**需字节级** |
| nicUniCmdSetBssInfo | `0x1401444a0` | 14 TLV dispatch | 结构级 |
| nicUniCmdPmDisable | `0x1400caefc` | PM tag=0x1B | 需确认 |
| BSS_BASIC builder | `0x14014c610` | tag=0x0000 | **需字节级** |
| RLM+PROTECT+IFS builder | `0x14014cc80`→`0x140150edc` | tag=0x0002+0x0003+0x0017 | 已有结构 |
| RATE builder | `0x14014cc90` | tag=0x000B | 已有反汇编 |
| SEC builder | `0x14014ccb0` | tag=0x0010 | 已有反汇编 |
| QBSS builder | `0x14014ccd0` | tag=0x000F | 已有反汇编 |
| SAP builder | `0x14014ccf0` | tag=0x000D | 已有反汇编 |
| P2P builder | `0x14014cd30` | tag=0x000E | 已有反汇编 |
| HE builder | `0x14014cd50` | tag=0x0005 | **需反汇编** |
| BSS_COLOR builder | `0x14014d010` | tag=0x0004 | **需反汇编** |
| EHT builder | `0x14014d150` | tag=0x001E | **需反汇编** |
| MBSSID builder | `0x14014d300` | tag=0x0006 | 已有反汇编 |
| UNKNOWN_0C builder | `0x14014d320` | tag=0x000C | 已有反汇编 |
| MLD builder | `0x14014d340`→`0x14014fad0` | tag=0x001A | 结构级，需确认 |
| IoT builder | `0x14014d350` | tag=0x0018 | **需反汇编** |

### Dispatch Handlers — STA_REC

| Handler | VA | TLV | 分析状态 |
|---------|-----|-----|---------|
| nicUniCmdUpdateStaRec | `0x1401446d0` | 13 TLV dispatch | 结构级 |
| BASIC builder | `0x14014d6d0` | tag=0x0000 | 已有反汇编（**BssActivateCtrl 简化版**） |
| HT_INFO builder | `0x14014d7a0` | tag=0x0009 | 已有反汇编 |
| VHT_INFO builder | `0x14014d7e0` | tag=0x000A | 已有反汇编 |
| HE_BASIC builder | `0x14014d810` | tag=0x0019 | **需反汇编** |
| HE_6G_CAP builder | `0x14014dae0` | tag=0x0017 | 需确认跳过条件 |
| STATE_INFO builder | `0x14014d730` | tag=0x0007 | 已有反汇编 |
| PHY_INFO builder | `0x14014d760` | tag=0x0015 | 已有反汇编 |
| RA_INFO builder | `0x14014e570` | tag=0x0001 | 已有反汇编 |
| BA_OFFLOAD builder | `0x14014e5b0` | tag=0x0016 | 已有反汇编 |
| UAPSD builder | `0x14014e620` | tag=0x0024 | 已有反汇编 |
| EHT_INFO builder | `0x14014db80` | tag=0x0022 | 需确认跳过条件 |
| EHT_MLD builder | `0x14014e2a0` | tag=0x0021 | 需确认跳过条件 |
| MLD_SETUP builder | `0x14014ddc0` | tag=0x0020 | 需确认跳过条件 |

### 辅助函数

| 函数名 | VA | 用途 | 分析状态 |
|--------|-----|------|---------|
| phy_mode_from_band | `0x14014fdfc` | PHY mode 位图转换 | 已完整反汇编 ✅ |
| connType helper | `0x140151608` | conn_type 查找 | 已完整反汇编 ✅ |
| nicUniCmdBssInfoMld | `0x14014fad0` | MLD TLV builder | 结构级 |
| nicUniCmdBssInfoConnType | 需定位 | BSS conn_type | 已知返回 0x10001 |
| XmitWriteTxDv1 | `0x1401a2ca4` | TXD 构建 | 已汇编验证（DW0-DW7） |
| N6PciTxSendPkt | `0x14005d1a4` | DMA 提交 | 结构级 |
| N6PciUpdateAppendTxD | `0x14005d6d8` | TXP builder | **需反汇编** |

---

## 六、Ghidra 分析方法指南

### 通用工作流

对每个逆向任务，推荐以下步骤：

1. **快速反汇编**（首选 `tools/disasm_helper.py`）
   ```bash
   cd /home/user/mt7927
   python3 tools/disasm_helper.py disasm 0x<VA> <size_bytes>
   ```
   优点：快速、可脚本化、输出干净
   缺点：无类型信息、无交叉引用

2. **Ghidra 反编译**（补充细节）
   ```
   tmp/ghidra_project/mt7927_re
   ```
   优点：有伪 C 代码、交叉引用、数据流分析
   缺点：反编译可能不准确，需要与汇编对比验证

3. **数据表读取**
   ```bash
   python3 tools/disasm_helper.py read 0x<VA> <size_bytes>
   python3 tools/disasm_helper.py table 0x<VA> <num_entries> <entry_size>
   ```
   用于读取 dispatch table、常量池等数据区域

### 特定分析技巧

#### 追踪 flat struct 填充

```
目标: 找到 MtCmdSetBssInfo 如何填充传给 TLV dispatch 的 flat struct

方法:
1. 反汇编 MtCmdSetBssInfo (0x1400cf928) 前 200 条指令
2. 找到 `sub rsp, 0x??` — flat struct 在栈上分配
3. 找到 `lea r8, [rsp+XX]` — r8 通常指向 flat struct (因为 TLV builder 的第三个参数是 r8)
4. 追踪所有 `mov [rsp+XX+N], reg/imm` — 这些是 flat struct 的字段写入
5. 对于从 adapter/bss 对象读取的值 (如 `mov eax, [rcx+0x5c3090]`),
   记录 adapter/bss 偏移但不需要追踪到最终值
```

#### 确认 TLV 是否条件性跳过

```
目标: 确认 HT_INFO TLV 在 auth 前是否被跳过

方法:
1. 反汇编 HT_INFO builder (0x14014d7a0)
2. 找到 `cmp [r8+0x28], 0` — 检查 HT cap 是否非零
3. 如果 HT cap 为 0, 函数返回 eax=0 (跳过)
4. 在 auth 阶段, HT cap 是否已被填充? → 取决于 MtCmdSendStaRecUpdate 是否从 beacon 解析
```

#### 追踪 option/flag 传播

```
目标: 确认 option=0xed 在 wire format 中如何表现

方法:
1. 从 nicUniCmdSendCmd (0x14014eb0c) 开始
2. 找到 `call 0x1400ca864` (option decode) → 返回 0x8000
3. 追踪 0x8000 被写到 frame 结构的哪个偏移
4. 追踪该偏移如何映射到 UniCmd wire header
```

---

## 七、快速启动指南

### 如果只有 2 个 agent 可用

**Agent 1（BSS_INFO 方向）**：

1. P1-T01: BSS_INFO_BASIC 字节级重建 → **最高优先级**
   - 产出 +0x04/+0x14/+0x18/+0x19/+0x1e 五个不确定字段的精确值
2. P1-T04: BssActivateCtrl BSS_INFO 字节级重建
3. P1-T09: BSS_INFO 14 TLV 逐字节对比

**Agent 2（STA_REC 方向）**：

1. P1-T07: STA_REC MtCmdSendStaRecUpdate 字节级重建 → **最高优先级**
   - 确认 conn_state 的完整路径值
   - 确认 flat struct 0xEC 字节的填充
2. P1-T08: STA_REC 13 TLV 逐字节对比
3. P1-T03: BssActivateCtrl DEV_INFO 字节级重建

### 如果只有 3 个 agent 可用

在上述基础上，增加：

**Agent 3（独立任务 + 综合）**：

1. P1-T05: PM_DISABLE 确认（30 分钟）
2. P1-T10: ChPrivilege/ROC payload（1 小时）
3. P1-T12: UniCmd 帧封装格式（1 小时）
4. P1-T13: 综合 hex dump 模板（等 Agent 1、2 完成后）

### 预计时间线

| 阶段 | 2 agent | 3 agent |
|------|---------|---------|
| Phase 1 核心（T01-T09） | 6-8 小时 | 4-5 小时 |
| Phase 1 完整（含 T10-T13） | 10-12 小时 | 6-8 小时 |
| Phase 2（如果需要） | +4-6 小时 | +3-4 小时 |
| Phase 3（auth 成功后） | +6-8 小时 | +4-5 小时 |

---

## 八、质量标准

### 每个任务产出必须包含

1. **反汇编证据**：每个字段声称的值必须附带反汇编指令地址
2. **可信度标注**：
   - `✅ 汇编验证` — 直接从反汇编指令确认
   - `🟡 高置信推测` — 从上下文和交叉引用推导，但无直接指令确认
   - `❓ 未确认` — 需要更多分析
3. **对比表**：Windows 值 vs 我们的值，每行标注 ✅/❌/❓
4. **差异修复建议**：对每个 ❌ 差异，给出精确的代码修改建议

### 避免的反模式

1. **不要推测根因** —— 只记录事实差异，不做因果推导
2. **不要引用低可信度文档** —— 优先使用汇编级验证的数据
3. **不要假设 MT6639 Android 值** —— 即使字段名相同，值可能不同
4. **不要混淆 outer_tag / inner_CID / TLV tag** —— 三者是不同概念

---

## 附录 A: 现有文档可信度分级（引用指南）

### 高可信度（可直接引用）

| 文档 | 内容 |
|------|------|
| `docs/re/win_re_dw2_dw6_verified.md` | TXD DW2/5/6/7 汇编级验证 |
| `docs/re/win_re_bss_info_all_tlvs.md` | BSS_INFO 14 TLV 结构 |
| `docs/re/win_re_sta_rec_all_tlvs.md` | STA_REC 13 TLV 结构 |
| `docs/re/win_re_connect_flow_complete.md` | 连接流程 + BssActivateCtrl |
| `docs/re/win_re_codex_bss_verify.md` | BSS/STA BASIC TLV 验证 |
| `docs/re/win_re_payload_formats_detailed.md` | SET_DOMAIN/BAND_CONFIG payload |
| `docs/re/win_re_dma_descriptor_format.md` | DMA 描述符格式 |

### 中可信度（需交叉验证后引用）

| 文档 | 注意事项 |
|------|---------|
| `docs/re/win_re_cid_mapping.md` | dispatch table 数据正确，命名和推测部分需谨慎 |
| `docs/re/win_re_full_txd_dma_path.md` | DW5=0 错误，其余正确 |
| `docs/re/win_re_ring2_analysis.md` | "Ring 2 不是根因" 结论需更新 |

### 低可信度（已废弃，不要引用）

| 文档 | 替代 |
|------|------|
| `docs/re/win_re_txd_dw0_dw1_precise.md` | → `win_re_dw2_dw6_verified.md` |
| `docs/archive/low_trust/win_re_tx_mgmt_path.md` | → `win_re_full_txd_dma_path.md` |

---

## 附录 B: 已知 Windows 连接流程（当前最佳理解）

```
WdiTaskConnect
  │
  ├─[1] ChipConfig (CID=0x0e, 328B)              ← 每次 connect 重发
  │
  ├─[2] MtCmdActivateDeactivateNetwork
  │       → BssActivateCtrl (dispatch 0x11→entry[9])
  │       → 原子发送: DEV_INFO(CID=1) + BSS_INFO(CID=2, BASIC+MLD)
  │
  └─ MlmeCntlOidConnectProc
        │
        ├─[3] PM_DISABLE (BSS_INFO CID=2, tag=0x1B)
        │
        ├─[4] MtCmdSetBssInfo (dispatch 0x12→entry[15])
        │       → BSS_INFO CID=2, 14 TLV dispatch
        │
        └─ MlmeCntlWaitJoinProc
              │
              ├─[5] SCAN_CANCEL
              │
              ├─[6] MtCmdChPrivilage → ChPrivilege (CID=0x27)
              │
              ├─[7] MtCmdSendStaRecUpdate (dispatch 0x13→entry[22])
              │       → STA_REC CID=3, 13 TLV dispatch
              │
              └─[8] Auth TX 入队 (FUN_1400ac6c8)
```

---

*方案生成于 2026-02-22，基于对 60+ 逆向文档、审计报告、25+ session 调试记录的综合分析。*
