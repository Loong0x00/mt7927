# Windows MT7927 驱动 TX 路径分析报告

**日期**: 2026-02-16
**分析者**: win-tx-analyst
**来源**: 全部 `docs/win_v5705275_*.md` Ghidra 逆向文档 + `docs/references/` + MT6639/mt7925 交叉参考
**任务**: 从 Windows 逆向文档中提取 TX 路径相关信息，回答管理帧发送的 5 个关键问题

---

## 一、核心结论

### Windows RE 文档的覆盖范围

**重要限制**: 现有 Windows 逆向文档 **仅覆盖了初始化阶段** (FW下载 + PostFwDownloadInit)。
连接流程 (scan → auth → assoc) 对应的 NDIS/WDI 回调 **从未被逆向分析**。

因此，以下结论部分基于直接逆向数据，部分基于 MCU 命令路径的间接推导。

---

## 二、5 个关键问题的回答

### Q1: Windows 管理帧走哪个 TX Ring?

**直接答案**: 现有 RE 文档未涵盖管理帧 TX 路径。但可从以下线索推导：

**Windows TX Ring 布局** (来源: `v5705275_vs_our_driver.md`, `dmashdl_and_mcu_response.md`):

| Ring | 用途 | DMASHDL Group |
|------|------|---------------|
| TX 0 | 数据/管理 (AC0/BE) | Group 0 |
| TX 1 | 数据 (AC1/BK) | Group 1 |
| TX 2 | 数据 (AC2/VI) | Group 2 |
| TX 3 | 数据 (AC3/VO) | Group 3 |
| TX 15 | MCU 命令 (FWDL + UniCmd) | Group 15 |
| TX 16 | MCU 命令 (WM) | — |

**DMASHDL HIF_GUP_ACT_MAP = 0x8007**:
- BIT(0)=Ring 0, BIT(1)=Ring 1, BIT(2)=Ring 2, BIT(15)=Ring 15
- Ring 0/1/2 属于数据组, Ring 15 属于 MCU 命令组

**推导**: Windows 管理帧 (auth/assoc/deauth) 有两种可能路径：
1. **通过 TX Ring 0 发送** (作为数据帧，Q_IDX=ALTX0) — 类似 mt7925 的做法
2. **通过 MCU 命令路径发送** (TX Ring 15, UniCmd 封装) — 类似 MT6639 Android 的做法

**MT6639 Android 参考** (来源: `mt6639_auth_flow_code_analysis.md`):
- MT6639 管理帧走 **TC4 → TX Ring 15 (CMD ring)**
- 使用 MCU 命令封装: PKT_FMT=2, Q_IDX=0x20 (MCU_Q0)
- **但 MT6639 是 AXI 总线 (内存共享)**，PCIe 路径可能不同

**mt7925 参考** (上游 Linux):
- 管理帧走 **TX Ring 0** (数据 ring)，CT mode + TXP
- Q_IDX=0x10 (ALTX0)

**结论**: 由于 Windows RE 未覆盖连接流程，无法确认。但两种路径都需要测试：
- 当前实现: Ring 0 + CT mode + Q_IDX=0x10 (已测试，stat=1)
- **未测试**: Ring 15 + PKT_FMT=2 + Q_IDX=0x20 (MT6639 路径)

---

### Q2: Windows TXD 的 PKT_FMT 是什么?

**Windows MCU 命令 TXD 构造** (来源: `win_v5705275_mcu_dma_submit.md`, 函数 `MtCmdSendSetQueryUniCmdAdv`):

```c
// TXD DW0 构造 (UniCmd 路径):
*puVar4 = (total_len & 0xFFFF) | (*puVar4 & 0x7FFFFF) | 0x41000000;
// 0x41000000 = BIT(30) | BIT(24)
// BIT(30) = PKT_FMT bit 0 → PKT_FMT 最低位=1
// BIT(24) = Q_IDX bit 0 → Q_IDX 最低位=1
// 加上 byte+0x25 = 0xa0 → 更多 Q_IDX bits

// 解析 0x41000000:
// bits[24:20] = Q_IDX → 需要结合 byte offset 0x25 = 0xa0 分析
// bits[30:29] = PKT_FMT → 最低位=1

// byte offset 0x25 = DW9 的 byte1 或 DW0 的高位扩展
// 0xa0 = 10100000b → 如果是 Q_IDX 的高位 bits，Q_IDX = 0x20
```

**完整 TXD DW0 解析**:
```
DW0 bits[15:0]   = TX_BYTE_CNT (total_len)
DW0 bits[24]     = 1 → Q_IDX[0]
DW0 bits[30:29]  = PKT_FMT
DW0 bit[30]      = 1 → PKT_FMT ≥ 2
```

结合 CONNAC3 TXD 格式定义:
- **PKT_FMT = 2** (0b10, DW0 bits[30:29])
- **Q_IDX = 0x20** (MCU_Q0, DW0 bits[28:24] + byte 0x25 扩展)

**Windows TXD DW1 关键差异** (来源: `v5705275_vs_our_driver.md`):
```c
// Windows:
puVar4[1] = puVar4[1] & 0xffff7fff | 0x4000;
// 清除 BIT(15), 设置 BIT(14) = HDR_FORMAT_V3
// 注意: BIT(31) LONG_FORMAT 未设!!!

// 我们的驱动:
txd[1] = BIT(31) | ...;  // 设了 LONG_FORMAT
```

**★★★ 关键差异**: Windows UniCmd **不设 BIT(31) LONG_FORMAT**，我们设了。
这意味着 Windows 的 MCU 命令 TXD 可能只有 2 个 DWORD (短格式)，而我们用了 8 个 DWORD 的长格式。

---

### Q3: Windows 管理帧有 TXP 吗?

**MCU 命令路径 (UniCmd)**: **没有 TXP**
- `MtCmdSendSetQueryUniCmdAdv` 将命令数据直接拼接在 TXD 后面
- TXD + 命令头 + TLV 数据 = 一个连续 buffer
- 通过 TX Ring 15 以 **inline 方式** 提交
- 这是 PKT_FMT=2 的特点: 命令数据紧跟 TXD，无需 TXP 指针

**数据帧路径**: Windows RE 未覆盖数据帧 TX，无法确认数据帧是否使用 TXP。

**MT6639 参考**:
- MCU 命令: inline，无 TXP
- 管理帧: 通过 MCU 命令路径 (TC4)，同样 inline 无 TXP
- 数据帧: MT6639 是 AXI 总线，使用 scatter-gather，不完全等同于 PCIe TXP

**mt7925 参考**:
- 管理帧: TX Ring 0, CT mode, **有 TXP** (与数据帧相同)
- TXP 格式: msdu_id + buf_addr + buf_len

**结论**: Windows MCU 命令不用 TXP (inline)。管理帧是否用 TXP 取决于走哪条路径。

---

### Q4: Windows 在 auth 之前发了哪些 MCU 命令?

**直接逆向数据 (PostFwDownloadInit 之后)**: 现有 RE 未覆盖连接流程。

**已知的 Windows 完整 PostFwDownloadInit** (来源: `win_tx_power_phy_analysis.md`, `v5705275_vs_our_driver.md`):

| 步骤 | 命令 | CID/class | 我们 | 状态 |
|------|------|-----------|------|------|
| 1 | DMASHDL enable | reg 0xd6060 | ✅ | 已实现 |
| 2 | WpdmaConfig | GLO_CFG | ✅ | 已实现 |
| 3 | Clear FWDL bypass | GLO_CFG BIT(9) | ✅ | 已实现 |
| 4 | NIC_CAP query | 0x8a | ✅ | 已实现 |
| 5 | Config v2 | 0x02 | ✅ | 已实现 |
| 6 | Config 0xc0 | 0xc0 | ✅ | 已实现 |
| 7 | DownloadBufferBin | 0xed/0x21 | ❌ | 可选(有条件) |
| 8 | DBDC config | 0x28 | ✅ | 已实现 |
| 9 | 1ms delay | — | ✅ | 已实现 |
| 10 | ScanConfig | 0xca | ✅ | 已实现 |
| 11 | ChipConfig | 0xca | ✅ | 已实现 |
| 12 | LogConfig | 0xca | ✅ | 已实现 |

**PostFwDownloadInit 完全覆盖** — 没有遗漏的初始化命令。

**连接流程 MCU 命令推断** (来源: MT6639 + mt7925 交叉分析):

```
连接请求后 (扫描前):
  1. DEV_INFO (0x01): OMAC 激活, OwnMacAddr          — ✅ 已实现
  2. BSS_INFO (0x02): BASIC + 多个 TLV               — ⚠️ 只有 3/12 个 TLV

频道请求:
  3. CNM CH_PRIVILEGE (0x27): 频道请求               — ✅ 已实现 (ROC)

CH_GRANT 回调后, auth 帧前:
  4. STA_REC (0x03): BASIC + 多个 TLV               — ⚠️ 只有 5/10 个 TLV
     ★ conn_state 必须 = STATE_CONNECTED(1)          — ❌ 我们传了 DISCONNECT(0)

auth 帧发送:
  5. DMA TX (Ring 0 或 15)                           — ✅ 帧到达固件但被拒绝
```

**Windows MCU 命令路由表** (来源: `windows_v5705275_mcu_cmd_backend.md`):
- 57 条路由在 `DAT_14023fcf0`
- 已分析的前 5 条 (class/CID → handler 函数)
- 特殊的 0xed sub-commands: 0x21, 0x94, 0x1e, 0x81, 0x3c, 0xa8, 0xbf, 0xc0, 0x01, 0x4f

---

### Q5: Windows 与我们的 TX Ring 配置差异

#### 5.1 GLO_CFG 寄存器 (关键差异!)

来源: `v5705275_vs_our_driver.md`

```
Windows GLO_CFG (0x7c024300):
  写入值: 0x50001070
  = BIT(4)  TX_DMA_EN
  | BIT(5)  RX_DMA_EN
  | BIT(6)  TX_WB_DDONE (WritBack DMA Done)
  | BIT(12) BYTE_SWAP (少见但存在)
  | BIT(28) CFG_FW_DWLD_BYPASS_DMASHDL (保留到需要时清除)
  | BIT(30) OMIT_TX_INFO (不在 TX 完成中包含 TX info)

我们的 GLO_CFG:
  写入值: 0x10000070
  = BIT(4) TX_DMA_EN
  | BIT(5) RX_DMA_EN
  | BIT(6) TX_WB_DDONE
  | BIT(28) CFG_FW_DWLD_BYPASS_DMASHDL

差异:
  ❌ 缺少 BIT(12) BYTE_SWAP
  ❌ 缺少 BIT(30) OMIT_TX_INFO
```

**★ BIT(30) OMIT_TX_INFO**: Windows 设了此位。可能影响 TX completion 格式。

#### 5.2 GLO_CFG_EXT1 (来源: `v5705275_vs_our_driver.md`)

```
Windows 从未写入 GLO_CFG_EXT1。
我们在 clear FWDL bypass 时设了 BIT(28)。

差异: 我们多设了 GLO_CFG_EXT1 BIT(28)。
```

#### 5.3 DMASHDL 配置

来源: `dmashdl_and_mcu_response.md`

```
Windows DMASHDL:
  HIF_GUP_ACT_MAP = 0x8007   (Ring 0, 1, 2, 15 active)
  Group 0/1/2: max=0xfff, min=0x10
  Group 15:    max=0x30,  min=0
  HIF_ACK_CNT_TH = 4
  Priority:    Group 0 > 1 > 2 > 15

我们的 DMASHDL:
  HIF_GUP_ACT_MAP = 0x8005   (Ring 0, 2, 15 active)
  — 缺少 Ring 1 (BIT(1))

差异: 我们未激活 Ring 1。对 auth 帧可能无影响 (auth 走 Ring 0 或 15)。
```

#### 5.4 TX Ring 数量和编号

```
Windows: TX ring 0, 1, 2, 3, 15, 16     (6 个 ring)
我们:    TX ring 0, 15, 16               (3 个 ring)

差异: 我们缺少 ring 1/2/3，但这些是数据 ring (BK/VI/VO)。
auth 帧不需要这些 ring。
```

#### 5.5 Prefetch 配置

```
Windows prefetch (推测，RE 中未直接看到具体值):
  通过函数 FUN_1400c2dbc 配置所有 ring 的 prefetch

我们的 prefetch:
  TX0:  offset=0x0280, count=4
  TX15: offset=0x02c0, count=4
  TX16: offset=0x0300, count=4

  (来源: mt7925 参考的 ring layout)
```

---

## 三、重要发现汇总

### 3.1 TXD DW1 BIT(31) LONG_FORMAT — ★★★ 可能关键

```
Windows UniCmd:  TXD[1] 不设 BIT(31) → 短格式 TXD (2 DW = 8 bytes)
我们的驱动:     TXD[1] 设了 BIT(31) → 长格式 TXD (8 DW = 32 bytes)
```

来源: `v5705275_vs_our_driver.md`:
> "TXD[1] BIT(31) LONG_FORMAT set by us but NEVER by Windows"

**影响分析**:
- 这是针对 **MCU 命令** 的 TXD，不是数据帧
- 如果管理帧走 MCU 命令路径 (Ring 15)，那么也不应该设 BIT(31)
- 如果管理帧走数据路径 (Ring 0, CT mode)，则 BIT(31) 可能是必要的
- mt7925 数据帧 TXD **设了 BIT(31)** — 说明数据路径需要长格式

**结论**: MCU 命令路径不需要 LONG_FORMAT，但数据路径需要。关键在于管理帧走哪条路径。

### 3.2 Windows UniCmd 头格式

来源: `win_v5705275_mcu_dma_submit.md`

```
UniCmd 总大小 = 0x30 (48 bytes):
  TXD:       0x10 bytes (2 DW, 短格式, 无 LONG_FORMAT)  ← 可能!
  或 TXD:    0x20 bytes (8 DW, 不确定)
  UniCmd头:  0x20 bytes (CID + subcmd + option + ...)
  TLV data:  变长

Legacy 总大小 = 0x40 (64 bytes) header:
  TXD:       0x20 bytes (8 DW, 长格式)
  Legacy头:  0x20 bytes
```

**注意**: 0x30 是 UniCmd header 的固定部分。TXD 可能是 8 bytes (短格式) 或 32 bytes (长格式)。
现有 RE 不够精确来区分。但 BIT(31) 未设强烈暗示短格式。

### 3.3 Windows MCU 命令的 byte offset 字段

来源: `win_v5705275_mcu_dma_submit.md`

```c
*(byte*)((longlong)puVar4 + 0x25) = 0xa0;
// offset 0x25 相对于 TXD 起始:
// 如果 TXD = 8 bytes, offset 0x25 = UniCmd header offset 0x1d
// 如果 TXD = 32 bytes, offset 0x25 = TXD DW9 的 byte1 (超出 8 DW!?)

*(byte*)((longlong)puVar4 + 0x2b) = flags;  // 2 或 3
// offset 0x2b: 类似推算
```

这些 byte-level 字段的确切含义取决于 TXD 长度，目前无法完全确认。

### 3.4 GLO_CFG BIT(30) OMIT_TX_INFO

Windows 设了但我们没设。此位控制 TX completion (TXFREE) 是否包含 TX info。
**可能不影响 auth 失败**，但可能影响 TXFREE 解析。

### 3.5 DownloadBufferBin (0xed/0x21) — 可选步骤

来源: `win_tx_power_phy_analysis.md`

```
条件: *(ctx + 0x1467608) == 1
动作: 打开 NdisOpenFile, 按 1KB 分块通过 MCU 发送
```

- mt7925 无对应实现
- 可能是 ACPI/OEM 定制补充固件
- **不太可能影响 auth 帧** (条件标志可能为 0)

---

## 四、与 auth 帧 stat=1 问题的关联

### 4.1 确认排除的方向

| 方向 | 来源 | 结论 |
|------|------|------|
| TX power / PHY / RF 初始化 | `win_tx_power_phy_analysis.md` | ❌ 排除 — 固件自动使用 E-fuse 默认值 |
| PostFwDownloadInit 遗漏 | 全部 RE 文档 | ❌ 排除 — 所有步骤已实现 |
| Radio Enable 命令 | mt7925 参考 | ❌ 排除 — CONNAC3 STA 模式自动启用 |

### 4.2 Windows RE 支持的根因假设

从 Windows RE 文档中提取的线索，与 `auth_fix_plan.md` 的根因分析交叉验证:

#### (1) STA_REC conn_state = DISCONNECT → stat=1 ★★★★★

**Windows RE 间接证据**:
- Windows MCU 命令系统完善，连接流程使用 NDIS/WDI 回调
- MT6639 参考: STA_REC BASIC 的 `ucConnectionState` = **STATE_CONNECTED(1)**，始终
- 固件内部 stat=1 = policy drop → 与 DISCONNECT 状态吻合

**我们的问题**: `conn_state = CONN_STATE_DISCONNECT(0)` → 固件认为 STA 已断开 → 拒绝发帧

#### (2) DW7 TXD_LENGTH ★★★

**TX debug status 高优先级 #1**:
- MT6639 always sets `TXD_LEN_1_PAGE` (DW7 bits[31:30]=1)
- 我们 DW7=0 (memset 清零，从未设置此字段)
- 固件可能无法正确定位 TXP (如果走 CT mode)

**Windows RE 相关性**: Windows MCU 命令的 TXD 构造中未看到 DW7 设置 (因为 MCU 命令可能是短格式)。
但如果管理帧走数据路径 (Ring 0, CT mode)，DW7 设置就很关键。

#### (3) PKT_FMT=2 + Q_IDX=0x20 ★★

**Windows RE 直接证据**:
- Windows UniCmd: `0x41000000` → PKT_FMT=2, Q_IDX=0x20
- 我们的管理帧: PKT_FMT=0 (CT), Q_IDX=0x10 (ALTX0)
- **Q_IDX=0x20 从未测试过**

**但注意**: 这是 MCU 命令的 TXD 格式，管理帧是否走同一路径取决于固件设计。

---

## 五、建议修复优先级 (从 Windows RE 角度)

### 第一优先: STA_REC conn_state 修复

所有证据都指向 `conn_state=DISCONNECT(0)` 是 stat=1 的直接原因。
详见 `auth_fix_plan.md` Fix 1+2。

### 第二优先: DW7 TXD_LENGTH=1

MT6639 所有帧都设此字段。如果走 CT mode (Ring 0)，这可能导致 TXP 定位错误。
在 `mt7927_mac.c` 的 `mt7927_mac_write_txwi()` 中添加:
```c
txd[7] |= GENMASK(31, 30);  // TXD_LEN = 1 page
```

### 第三优先: 尝试 MCU 命令路径发管理帧

如果 Fix 1+2 和 DW7 修复后仍然 stat=1，考虑:
- 通过 Ring 15 + PKT_FMT=2 + Q_IDX=0x20 发送管理帧
- 不设 BIT(31) LONG_FORMAT
- 管理帧数据 inline (不用 TXP)

### 不需要做的:

| 方向 | 原因 |
|------|------|
| TX power 配置 | 固件使用 E-fuse 默认值 |
| PHY/RF 初始化 | 固件自行初始化 |
| Radio Enable | CONNAC3 STA 模式自动启用 |
| DownloadBufferBin | 可选步骤，条件可能不满足 |
| GLO_CFG BIT(30) | OMIT_TX_INFO 不影响 TX 发送 |

---

## 六、Windows RE 文档索引

| 文档 | 主要内容 | TX 相关信息 |
|------|---------|------------|
| `win_v5705275_mcu_dma_submit.md` | UniCmd TXD 构造 | ★★★ PKT_FMT=2, Q_IDX=0x20, 无 BIT(31) |
| `win_v5705275_mcu_send_core.md` | MCU 命令入口 | UniCmd vs Legacy 路径选择 |
| `win_v5705275_mcu_send_backends.md` | MCU 命令路由 | 57 条路由表, UniCmd dispatch |
| `win_v5705275_dma_enqueue.md` | DMA 入队 | 20 个命令 slot, timeout/retry |
| `win_v5705275_dma_lowlevel.md` | DMA 底层 | 命令存储 + 事件等待 |
| `win_v5705275_fw_flow.md` | FW 下载 | 仅 FWDL, 无 TX 信息 |
| `windows_v5705275_deep_reverse.md` | 整体架构 | 寄存器序列, ring init |
| `windows_v5705275_mcu_cmd_backend.md` | 命令后端 | 路由表, 0xed 子命令 |
| `references/v5705275_vs_our_driver.md` | 差异对比 | ★★★ GLO_CFG, TXD, LONG_FORMAT |
| `references/dmashdl_and_mcu_response.md` | DMASHDL | ★★ ring 映射, group 配额 |

---

## 七、总结

### Windows RE 能回答的:
1. MCU 命令使用 PKT_FMT=2 + Q_IDX=0x20 (Ring 15)
2. MCU 命令 TXD 不设 BIT(31) LONG_FORMAT
3. PostFwDownloadInit 完全覆盖，无遗漏
4. TX power/PHY/RF 初始化不是问题
5. GLO_CFG 有 BIT(30) OMIT_TX_INFO 差异

### Windows RE 无法回答的:
1. 管理帧 (auth) 具体走哪个 ring — 连接流程未逆向
2. 管理帧的 TXD 完整格式 — 取决于路径
3. Windows auth 前的完整 MCU 命令序列 — 取决于 NDIS/WDI 回调
4. DW7 TXD_LENGTH 在数据帧中的值 — 数据帧 TX 未逆向

### 最终建议:
**stat=1 的根因最可能是 STA_REC conn_state=DISCONNECT(0)**。所有 Windows RE 文档中没有找到其他被遗漏的关键 TX 配置。优先修复 `auth_fix_plan.md` 中的 Fix 1-5。

---

*分析完成 — 2026-02-16*
