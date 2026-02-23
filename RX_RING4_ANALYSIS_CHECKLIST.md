# RX Ring 4 问题分析清单（细化版）

适用问题：`docs/debug/rx_ring4_problem.md` 中的 **RX Ring 4 DIDX=0**（固件从不向数据 RX ring 投递帧）。

范围限定：本清单只用于分析与排查，不包含代码修改方案或补丁实现。

---

> ### 修订记录
>
> **Rev 2 — Session 37 (2026-02-23) by Claude Opus (主 agent, 37 sessions 项目经验)**
>
> 基于对原清单的审阅，增加以下修订。所有原文保留不动，修订内容用 `[REV2]` 标记。
>
> #### 修订摘要
>
> 1. **执行顺序调整**: B1（读所有 ring DIDX）提升到与 A1 并列第一 — 成本最低（10 分钟），
>    可能直接定位问题。上游 mt7925 用 Ring 0+Ring 2（非 Ring 4/6/7），从未验证过。
> 2. **新增 A5**: `RX_EXT_CTRL` 寄存器差异分析 — 当前 R4=0x00000008, R6=0x00800008,
>    R7=0x01000004, bits[23:20] 差异可能是 ring 路由标识，原清单未涉及。
> 3. **新增 A6**: Band→Ring 映射寄存器搜索 — CONNAC3 可能有专门的 "Band N RX → Ring M"
>    映射配置，不在通用 WFDMA 寄存器范围内。
> 4. **A3 补充**: `WFDMA_CFG` payload 并非完全未知 — `0x820cc800` 是 bus 地址
>    （WFDMA 寄存器区域 bus2chip 映射），`0x3c200` 是配置值，非两个黑箱参数。
> 5. **D3 优先级提升**: Ring 6 未处理事件可能包含 "data path ready" 通知，
>    当前代码只处理 TXFREE 和 scan 结果，若固件通过 event 通知 RX ready 则被忽略。
> 6. **新增第 9 节替代**: 修订后的执行顺序。
>
> **Rev 4 — Session 38 (2026-02-23) by Claude Opus (主 agent + 3 sub-agents 执行结果)**
>
> 本轮执行了 B1、A1、A5、A6、D3 五项任务，获得确定性结论。
>
> #### REV4 执行结果摘要
>
> 1. **B1 Ring 0-7 全套寄存器快照 — 确定性结论: 无 ring 映射错误**
>    ```
>    PRE-AUTH Ring 0: BASE=0x00000000 CNT=512 CIDX=0 DIDX=0 EXT=0x00000000  ← 未配置
>    PRE-AUTH Ring 1: BASE=0x00000000 CNT=512 CIDX=0 DIDX=0 EXT=0x00000000  ← 未配置
>    PRE-AUTH Ring 2: BASE=0x00000000 CNT=512 CIDX=0 DIDX=0 EXT=0x00000000  ← 未配置
>    PRE-AUTH Ring 3: BASE=0x00000000 CNT=512 CIDX=0 DIDX=0 EXT=0x00000000  ← 未配置
>    PRE-AUTH Ring 4: BASE=0x506d9000 CNT=256 CIDX=255 DIDX=0 EXT=0x00000008  ← 配置正确, DIDX=0
>    PRE-AUTH Ring 5: BASE=0x00000000 CNT=512 CIDX=0 DIDX=0 EXT=0x00000000  ← 未配置
>    PRE-AUTH Ring 6: BASE=0x4d8ce000 CNT=256 CIDX=112 DIDX=112 EXT=0x00800008  ← ✅ 工作中
>    PRE-AUTH Ring 7: BASE=0x4db61000 CNT=256 CIDX=255 DIDX=0 EXT=0x01000004  ← 配置正确, DIDX=0
>
>    AUTH-FAIL Ring 4: DIDX=0 (不变)  Ring 6: DIDX=115 (+3, TXFREE)  Ring 7: DIDX=0 (不变)
>    ```
>    **结论**: 固件从未向 Ring 0-7 中的任何 ring 投递 WiFi 数据帧。不是 ring 编号映射错误。
>    **B1 假设已排除**: 数据帧没有走 Ring 0 或 Ring 2（mt7925 的 ring layout），也没走任何其它 ring。
>
> 2. **A1 Windows WFDMA 寄存器审计 — 9 项匹配, 5 项差异**
>    - ✅ 匹配: GLO_CFG_EXT0/EXT1, packed prefetch CFG0-3, PREFETCH_CTRL, 0xd6060, INT mask
>    - ⚠️ 差异:
>      - **GLO_CFG BIT(20) CSR_LBK_RX_Q_SEL_EN**: 我们设了, Windows 不设 → 可操作实验
>      - **Per-ring EXT_CTRL**: 我们写了, Windows 不写 (auto-chain BIT(15)=1 时不需要)
>      - GLO_CFG BIT(26) ADDR_EXT_EN: 我们设了, 低风险
>    - **关键反证**: Ring 6 使用完全相同的 WFDMA 配置且正常 → WFDMA 配置不是根因
>
> 3. **A5 RX_EXT_CTRL 分析 — 格式正确但不必要**
>    - R4 SRAM_BASE=0x0000 DEPTH=8, R6=0x0080/8, R7=0x0100/4 — 区域不重叠，格式正确
>    - CODA 文档: "If BIT(15)=1 (auto-chain), firmware need to program EXT_CTRL instead" →
>      auto-chain 模式下 per-ring EXT_CTRL 被忽略，我们多写了但无害
>    - Ring 6 也有 EXT_CTRL 且正常 → **EXT_CTRL 不是根因**
>
> 4. **A6 Band→Ring 映射搜索 — 不存在**
>    搜索 CODA 头文件、全部 RE 文档、43 个 consensus 报告 — 无 RX_RING_MAP、BAND_MAP、
>    RX_DATA_RING 等寄存器。**Band→Ring 路由是固件内部逻辑**，通过 BSS/STA MCU 命令配置。
>
> 5. **D3 dev_dbg→dev_info — 无 rx-unknown 出现**
>    auth 窗口内未出现任何 `rx-unknown` 日志，确认没有未知 PKT_TYPE 帧到达任何 ring。
>
> #### REV4 更新后的根因排序
>
> | 排名 | 假设 | 概率 | 依据 |
> |------|------|------|------|
> | **#1** | **BSS/STA 配置不完整 — 固件内部 RX 路由未建立** | **60%** | STA_PAUSE0=0x03000001 始终不变; 无任何 ring 收到数据帧; Band→Ring 路由是固件逻辑 |
> | **#2** | **GLO_CFG BIT(20) CSR_LBK_RX_Q_SEL_EN 干扰** | **15%** | 我们设了 Windows 没设; 名字含 "RX_Q_SEL"; 可能重定向 RX 队列选择逻辑 |
> | **#3** | **WFDMA_CFG MCU 命令 payload 含 RX 路由参数** | **10%** | {0x820cc800, 0x3c200} 指向 MDP_DCR0; 可能配置固件内部 RX 投递规则 |
> | **#4** | **BSS_INFO BASIC 字段值错误 (phymode/packed_field)** | **10%** | 未与 Windows 二进制逐字节对比 |
> | **#5** | **PostFwDownloadInit 遗漏步骤** | **5%** | 已做过多轮审计但仍可能有盲点 |
>
> #### REV4 下一步实验计划 (按优先级)
>
> | 优先级 | 实验 | 预估耗时 | 说明 |
> |--------|------|----------|------|
> | **P0** | 移除 GLO_CFG BIT(20) | 5 min | 最低成本, 唯一确认的 Windows 差异且名字相关 |
> | **P0** | Auth 后读 MDP_DCR0/DCR1 (0x00e800/0x00e804) | 10 min | 看固件内部 RX 路由状态 |
> | **P1** | BSS_INFO BASIC 逐字节与 Windows 二进制对比 | 1 hr | Ghidra 追踪 Windows 构建的 BASIC TLV 原始字节 |
> | **P1** | WFDMA_CFG payload 位级分析 | 1-2 hr | 0x3c200 的位级含义 |
> | **P2** | 移除全部 per-ring EXT_CTRL 写入 | 15 min | Windows 不写, 虽然 Ring 6 证明无害但排除干扰 |
> | **P2** | PostFwDownloadInit 逐子步 Ghidra 深挖 | 2-4 hr | 收尾确认 |

> **Rev 3 — Session 37 (2026-02-23) by Codex / GPT-5（代码+RE 交叉审阅增补）**
>
> 基于对当前代码（`src/`）、`docs/re/` 与 `tmp/re_results/consensus/` 的交叉检查，
> 增加以下修订。所有原文保留不动，修订内容用 `[REV3]` 标记。
>
> #### 修订摘要
>
> 1. **新增文档冲突仲裁规则**：仓库内存在互相冲突且已过时的 RE 文档，若不先做“证据新鲜度”筛选会把排查顺序带偏。
> 2. **修正 D3 对代码现状的描述**：当前 `mt7927_mcu_rx_event()` 已对所有 MCU event 做 `eid + hex dump`；
>    真正盲点更可能在“未知 `PKT_TYPE` 仅 `dev_dbg` 可见”和“event 与 pkt_type 混淆”。
> 3. **增强 B1 观测项**：仅读 DIDX 容易误判，建议采集 `BASE/CNT/CIDX/DIDX/EXT_CTRL` 全套 ring 快照。
> 4. **增强 E1 中断旁证**：除 Ring4 `BIT(12)` 外，必须纳入 `MCU2HOST_SW_INT` 与 `MT_MCU_CMD_WAKE_RX_PCIE` 路径。
> 5. **增强 A1 输出物**：增加“寄存器所有权/写入来源（Host/FW）”和“证据新鲜度状态”字段，避免把 FW 动态值当成初始化差异。
> 6. **新增文档盲点与更优结构章节**：将“清单执行层”和“证据仲裁层”分开，降低后续 session 误读成本。

---

## 1. 目标与判定标准

### 目标
- 找出为什么 `Band0 WiFi data RX` 没有进入 host RX ring（当前预期为 Ring 4）。
- 将问题收敛到以下之一：
  - `WFDMA RX 路由/寄存器配置缺失`
  - `RX ring 编号映射错误`
  - `RX 描述符/内存格式不满足固件要求`
  - `MCU/BSS/STA 状态门控未满足`
  - `其它（需新增假设）`

### 成功判定（满足任一项）
- `Ring 4 DIDX` 在 auth 窗口内开始前进。
- 发现数据帧实际进入其它 ring（并有可重复证据）。
- 找到与 Windows 明确不一致且高度相关的寄存器/命令步骤（可解释 DIDX=0）。

### 失败判定（需要升级策略）
- 完成本清单 A/B/C/D 主线排查后，仍无法解释 `DIDX=0`，且没有新增高质量证据。

---

## 2. 已知事实（开始前复核）

开始任何新排查前，先确认以下事实未回归（避免重复掉坑）：

- TX 空口正常：Auth-1 已被抓到，AP 回 Auth-2。
- Ring 6（MCU events）正常，DIDX 会推进。
- `GLO_CFG`/Ring `BASE/CNT/CIDX`/prefetch/中断 mask 已对齐 Windows（至少当前认知）。
- 扫描走 Ring 6 内部路径，不经过 Ring 4。
- `RX_FILTER(0x0B)`、`BSS_RLM`、`STA_REC` 已尝试，`status=0` 但无效果。

参考：
- `CLAUDE.md`
- `docs/debug/rx_ring4_problem.md`

---

## 3. 工作原则（避免误导）

- 仅以 Windows RE 为权威来源：
  - `docs/re/`
  - `tmp/re_results/consensus/`
  - `tmp/ghidra_exports/`
- `docs/archive/low_trust/` 仅作寄存器名交叉参考，不用于行为结论。
- 每次实验必须记录“唯一变量”，禁止一次改多项后无法归因。

### [REV3] 证据新鲜度与文档冲突仲裁（新增）

在执行清单前，先对引用文档做一次“有效性判定”，否则会被旧结论误导。

必做检查（每次 session 开始时 2-3 分钟）：
- 文档日期 / Session 是否晚于 `CLAUDE.md` 当前状态描述
- 文档结论是否与 `docs/debug/rx_ring4_problem.md` 的“已排除项”冲突
- 文档中的“我们当前实现”描述是否仍符合 `src/` 代码现状

建议给每份引用文档标记状态：
- `ACTIVE`：与当前代码和 `CLAUDE.md` 一致，可直接用于决策
- `PARTIAL`：部分有效（仅某些寄存器/反汇编片段可用）
- `STALE`：结论已被后续 session 推翻，只能作历史背景

当前已观察到的冲突示例（用于提醒，不代表文档整体无价值）：
- `docs/re/win_re_rx_ring4_analysis.md` 中关于“缺 PM_DISABLE / BSS_INFO 仅 3 TLV / STA_REC 仅 5 TLV”的结论，与当前 `CLAUDE.md` 和代码状态不一致
- `docs/re/post_boot_mcu_rx_config.md` 中“RX0-3/upstream ring layout 主导”的旧假设，与当前已验证的 Ring6 MCU event 路径（Windows 4/6/7 布局）存在冲突

原则：
- **旧文档保留**，但在本清单执行时必须先标注“仍有效部分”和“已过时部分”。

建议记录模板字段（每轮实验一行）：
- 日期/Session
- 假设
- 变更点（仅 1 项）
- 观测窗口（probe / scan / auth）
- `R4/R6/R7 DIDX`
- `INT_STA`
- `MIB RX_OK/TX_OK`
- 结论（支持/反驳/不确定）

---

## 4. 主线 A：WFDMA RX 路由缺失（最高优先级）

这是当前最可能原因。目标是找出 Windows 写了但当前实现没写的 RX 路由相关配置。

### A1. 建立 Windows WFDMA 写寄存器清单（按阶段）

重点阶段：
- FW 下载完成后
- `PostFwDownloadInit`
- `WpdmaConfig`
- `WFDMA_CFG` UniCmd 前后
- 注册 `mac80211` 前

输出物（必须形成表格）：
- 地址（BAR0 offset）
- 写值 / 掩码写模式
- 所在函数（Ghidra 函数名）
- 所在阶段
- 当前驱动是否已写
- 备注（推测用途）

> **[REV3 补充]**: 表格建议额外增加 3 列（很关键）：
> - **写入来源/所有权**：`Host init` / `Host runtime` / `FW runtime` / `unknown`
> - **当前驱动写入点**：函数名 + 阶段（例如 pre-FWDL / post-FWDL / auth 前诊断）
> - **证据状态**：`ACTIVE` / `PARTIAL` / `STALE`
>
> 理由：同一寄存器可能被我们在多个阶段重复写（如 WPDMA/WFDMA 相关），
> 若不区分“最终生效写入”和“早期写入”，很容易误判差异根因。

重点地址范围（优先扫）：
- `0xd4000 ~ 0xd5000`（WFDMA）
- Ring 4/6/7 邻近寄存器块
- RX 扩展控制 / 路由 / pause / 中断相关块

### A2. 针对 Ring4 邻近地址做“引用反查”

在 Windows RE 中追踪以下地址及邻近偏移的写入来源：
- Ring4 base/cnt/cidx/didx 对应寄存器
- `0xd4540`（文档中提到的 Ring4 偏移线索）
- Ring4 附近连续寄存器（前后至少各看 `0x40~0x100`）

要回答的问题：
- 这些寄存器由哪个函数初始化？
- 是否只在某个条件下写入（band/DBDC/firmware ready）？
- 是否存在“同类寄存器对 Ring6 有写入、对 Ring4 没写入”的情况？

### A3. 解码 `WFDMA_CFG` payload（高价值）

当前线索：payload `{0x820cc800, 0x3c200}` 含义未知。

> **[REV2 补充]**: 并非完全未知 — `0x820cc800` 是 bus 地址（WFDMA 寄存器区域的
> bus2chip 映射，对应 BAR0 某个偏移），`0x3c200` 是配置值。分析时以此为起点，
> 不必从零猜测。可通过 CODA 头文件 `bus2chip` 表确认 `0x820cc800` 对应的物理偏移。

分析目标：
- Windows 驱动构造这两个参数时，输入依赖了哪些状态（chip id / band / ring count / feature flags）？
- 固件返回成功后，Windows 是否紧接着写 MMIO（说明 UniCmd 与寄存器写联动）？
- 参数位图是否包含 RX route / ring map / enable mask 语义？

最低输出物：
- 两个 `u32` 各自的位级注释草表（哪怕多数位未知，也先标注”变化条件”）。

### [REV2] A5. `RX_EXT_CTRL` 寄存器差异分析（高价值）

当前诊断显示三个 RX ring 的 EXT_CTRL 值不同：
```
R4 = 0x00000008    (bits[23:20] = 0x0)
R6 = 0x00800008    (bits[23:20] = 0x8)
R7 = 0x01000004    (bits[23:20] = 0x10, 且 bits[2:0] = 4 非 8)
```

分析目标：
- `bits[23:20]` 在 CONNAC3 中的语义（ring 优先级？路由标识？prefetch group？）
- Windows 对这三个 ring 的 EXT_CTRL 写入值是多少（Ghidra 追踪）
- Ring 4 的 `bits[23:20]=0` 是否意味着”未激活”或”路由未配置”
- `bits[2:0]` 的差异（R4/R6=8, R7=4）含义

注意：当前驱动写 EXT_CTRL 的代码路径需要核对是否与 Windows 一致，
这不是只读观测 — 可能是我们初始化写错了值。

### [REV2] A6. Band→Ring 映射寄存器搜索（中高价值）

CONNAC3 可能存在独立于 WFDMA 通用配置的 **Band→Ring 映射寄存器**，
告诉固件”Band 0 数据帧→Ring N, Band 1 数据帧→Ring M”。

搜索策略：
- 在 Windows .sys 中搜索对 Ring 4 编号 (常量 4) 的引用，追踪其用途
- 在 CODA 头文件中搜索 `RX_RING_MAP` / `BAND_MAP` / `RX_DATA_RING` 等关键词
- 查看 `WFDMA_CFG` 以外是否有其它 UniCmd 携带 ring 编号参数
- 查看 PostFwDownloadInit 中是否有写入包含 ring 编号的寄存器

这个配置可能不在 `0xd4000~0xd5000` 范围内（A1 的搜索范围），
可能在 PLE 控制寄存器区域 (0x08000~0x09000) 或 DMASHDL 区域。

### A4. 形成“差异优先级列表”

完成 A1-A3 后，把差异分成三类：
- P0：只影响 RX data path 的高相关寄存器（优先验证）
- P1：WFDMA 通用控制寄存器（可能相关）
- P2：看起来像性能/阈值/统计项（低优先级）

如果没有形成这张表，不要直接进入大范围试错。

---

## 5. 主线 B：RX ring 映射错误（高优先级）

> **[REV2 优先级提升]**: B1 提升到与 A1 并列第一执行。理由：
> - 成本极低（加几行 `mt7927_rr` 读寄存器，~10 分钟）
> - 上游 mt7925 用 **Ring 0 (MCU) + Ring 2 (Data)**，完全不同于我们假设的 Ring 4/6/7
> - 如果数据帧实际走 Ring 0 或 Ring 2，则 A/C/D 全部工作都不需要
> - 37 个 session 从未检查过 Ring 0-3/5 的 DIDX

目标：验证”数据帧是否实际上进入了非 Ring4 的 ring”。

### B1. 扩大观测范围（不只看 R4/R6/R7）

在同一 auth 窗口内观测更多 ring 的 `DIDX`（至少包含数据候选 ring）。

> **[REV2 具体化]**: 最低限度读取以下 ring 的 DIDX（只读，不配置）：
> - **Ring 0** — mt7925 的 MCU ring
> - **Ring 2** — mt7925 的 Data ring
> - **Ring 1, 3, 5** — 覆盖全部可能
> - 寄存器地址: `MT_WPDMA_RX_RING_DIDX(n)` = BAR0 + 0xd4508 + n*0x10
> - 即使这些 ring 未被我们初始化（BASE=0, CNT=0），固件若尝试写入也可能改 DIDX
>   或触发异常行为，读出非零值即为强证据

> **[REV3 补充]**: 不建议只读 DIDX。最低应读取 **每个 ring 的寄存器快照**：
> - `BASE / CNT / CIDX / DIDX / EXT_CTRL`
> - 如果成本可接受，再加 `PAUSE_RX_Q_TH`
>
> 理由：
> - 单看 DIDX 容易把“未初始化 ring 的噪声/残值”误判为有效线索
> - 若发现某 ring 的 `BASE/CNT` 被固件或其它路径改写，比 DIDX 更能直接指向路由/初始化问题
> - `EXT_CTRL` 与 Ring4/R6/R7 差异已出现强信号，应在 B1 阶段同步采样，不必等到 A5

建议最小集合（按可行性）：
- 所有已初始化 RX ring
- 若可低成本扩展，增加对未使用 ring 的只读 `DIDX` 采样（不改 ring 配置，仅观察）

判定逻辑：
- 若某 ring 在 auth 后随 AP 响应推进，则“ring 号预期错误”概率大增。
- 若所有数据候选 ring 都不动，继续主线 A/C/D。

### B2. 建立“ring 角色假设矩阵”

整理一张表（不必立刻改代码）：
- Ring 编号
- 当前配置角色（MCU/Data/辅助）
- Windows 证据来源
- Linux 当前观测（scan/auth 时 DIDX 变化）
- 可信度（高/中/低）

目的是防止后续混淆“Windows ring layout”与“当前芯片实际行为”。

---

## 6. 主线 C：RX 描述符/内存契约不满足（高优先级）

即便寄存器正确，若 ring memory 格式不满足固件要求，固件也可能完全不投递。

### C1. 核对 Ring4 描述符初始化内容（字节级）

检查点（逐项确认，不只看宏观参数）：
- 每个 desc 初始值是否与 Windows 一致（或至少关键字段一致）
- buffer 地址对齐要求
- buffer 长度字段是否足够承载管理帧/数据帧
- desc 保留位是否需要清零
- ring memory 是否在初始化后被其它流程覆盖

证据要求：
- 提供 Ring4 desc[0..N] 的 dump（至少几个样本）
- 与 Windows 参考样本逐字段比对（若有）

### C2. 核对 host buffer 分配策略

确认以下是否可能导致固件拒绝写入：
- DMA 地址方向/映射属性错误
- buffer 跨界或长度不符合硬件要求
- cache 同步时机不对（若架构相关）
- 实际提交给硬件的 `CIDX` 与 ring 内存内容不一致（tail 更新时序问题）

说明：
- `CIDX=255` 只能证明“逻辑上释放了 slot”，不能证明每个 slot 的 desc 合法。

### C3. 比较 Ring6 与 Ring4 的“初始化差异”

既然 Ring6 能工作，可做对照：
- ring 分配方式是否一致（只差 ring 编号）？
- desc 初始化模板是否一致 / 有无特殊字段差异？
- prefetch/EXT_CTRL 对 Ring6 的配置是否比 Ring4 多一步？

目标：
- 找出“Ring6 work / Ring4 dead”的最小差异集。

---

## 7. 主线 D：MCU/BSS/STA 状态门控未满足（中优先级）

命令 `status=0` 不代表数据面已开。目标是确认是否缺“状态条件”或“时序条件”。

### D1. 建立认证窗口时序表（命令 + 事件 + 寄存器）

按时间顺序记录：
- 发出的 UniCmd（命令名、关键参数）
- 收到的 MCU event（尤其 `ROC_GRANT`、TXFREE）
- auth TX 时刻
- `R4/R6/R7 DIDX`
- `INT_STA`
- `MIB RX_OK/TX_OK`

目的：
- 确认数据 RX 未开启是否发生在某个状态转换之前/之后。

### D2. 复核 BSS/STA 字段“语义一致性”（不是只看返回码）

重点字段（高风险）：
- `BSS_INFO BASIC` 中可能影响 RX 启用的模式字段（如 phymode / nonht_basic_phy）
- `STA_REC STATE`（wire 值）与 Windows 对应时机是否一致
- `BssActivateCtrl` / `BSS_INFO` / `STA_REC` 相对顺序是否与 Windows 完全一致

输出物：
- 一份“Windows vs 当前实现”的字段对照表（字段名 / 偏移 / 值 / 证据来源）

### D3. 检查是否存在未处理的”必须事件”

> **[REV2 优先级提升]**: 此项从 D 系列末尾提升到与 C 系列并列。理由：
> - 当前代码 Ring 6 事件处理只关注 TXFREE 和 scan 结果
> - 固件可能通过 Ring 6 发送 “data path ready” / “RX enable” 通知事件
> - 如果这类事件被 `mt7927_queue_rx_skb` 中的 default 分支静默丢弃，
>   就永远不会触发 data RX 路径的启用
> - 建议: 在 Ring 6 事件处理中增加 **全量 hex dump**（至少临时），
>   记录所有未识别的 event ID/类型，看是否有被忽略的重要事件

> **[REV3 校正]**: 需先区分两类“未处理”：
> - **MCU event（`PKT_TYPE_RX_EVENT`）**：当前 `src/mt7927_mac.c` 的 `mt7927_mcu_rx_event()`
>   已经会打印 `eid` 并 hex dump 前 64 字节（`dev_info` 级别）
> - **未知 PKT_TYPE / 未知 flag 组合**：`mt7927_queue_rx_skb()` 的 default 分支是 `dev_dbg`
>   级别，默认日志级别下可能看不到
>
> 因此本项的真正盲点是：
> - “事件没打印” vs “包类型没识别但被 `dev_dbg` 吃掉” 被混为一谈
> - `eid` 未处理与 `PKT_TYPE` 未识别是两条不同路径，排查时应分别统计

排查方向：
- 某个事件到了 Ring6，但当前代码未据此执行后续动作
- 某个事件被收到但过滤掉/未打印
- 某个 event status 非 0 被忽略，导致 data path 未被真正激活

---

## 8. 辅线 E：中断/统计/旁证（用于缩小范围）

这些通常不是根因，但有助于快速判断问题层级。

### E1. 中断旁证

已知：Ring4 `BIT(12)` 中断未触发。

要确认：
- 在 auth 窗口内 `INT_STA` 是否出现其它 RX 相关位（说明帧到了但没进 Ring4）
- 是否存在“DIDX 不变 + INT 有波动”的异常模式

> **[REV3 补充]**: 除 `INT_STA BIT(12)` 外，必须同时观察：
> - `MCU2HOST_SW_INT`（WFDMA 中断位）
> - `MT_MCU_CMD_REG` / `MT_MCU_CMD_WAKE_RX_PCIE`（固件通过软件中断唤醒 RX NAPI 的路径）
>
> 代码侧已存在这一路径：即使没有 Ring4 done 中断，tasklet 也可能因 `MCU2HOST_SW_INT`
> 调度 `napi_rx_data`。如果只盯 BIT(12)，会漏掉“固件试图唤醒 RX，但数据 ring 仍无包”的重要旁证。

### E2. MIB 旁证

已知：`TX_OK>0`, `RX_OK=0`。

继续观察：
- `RX_OK` 是否在不同阶段（scan/ROC/auth）始终为 0
- 若某次 `RX_OK` 突然变化但 Ring4 仍不动，说明帧可能被固件内部消费/走别的路径

### E3. PLE/DMASHDL 旁证

关注点：
- `STA_PAUSE0=0x03000001` 是否真为默认稳态（需 Windows 侧样本对照）
- `DIS_STA0=0xdeadbeef` 是否是读错地址/未映射，而不是有效状态

注意：
- 这些值可以提示方向，但不应替代 WFDMA/RX ring 直接证据。

---

## 9. 建议的执行顺序（按收益/成本比）

1. 完成主线 A（Windows WFDMA 写寄存器清单 + 差异表）
2. 同步完成主线 B（扩大 ring DIDX 观测，验证是否走错 ring）
3. 完成主线 C（Ring4 desc/内存字节级核对）
4. 再做主线 D（命令时序与状态门控）
5. 用辅线 E 做交叉验证与收敛

说明：
- A/B/C 任一条拿到强证据，都可能直接决定后续实现方向。
- D 更像”排除状态门控假设”的收尾项，但也可能产生关键线索。

> ### [REV2] 修订后的执行顺序
>
> 原顺序保留供参考。修订后的推荐顺序如下（按收益/成本比重新排列）：
>
> | 优先级 | 项目 | 预估耗时 | 理由 |
> |--------|------|----------|------|
> | **P0** | **B1** 读所有 ring DIDX (Ring 0-7) | 10 min | 成本最低，可能直接定位 ring 映射错误 |
> | **P0** | **A1** Windows WFDMA 寄存器审计 | 2-4 hr | 主力排查，覆盖面最广 |
> | **P1** | **A5** RX_EXT_CTRL 差异分析 | 30 min | 已有数据 (R4/R6/R7 值不同)，直接可分析 |
> | **P1** | **C3** Ring 4 vs Ring 6 初始化差异 | 1 hr | 利用 Ring 6 工作作对照组 |
> | **P1** | **D3** Ring 6 未处理事件全量 dump | 30 min | 低成本排除”被忽略的必须事件” |
> | **P2** | **A3** WFDMA_CFG payload 解码 | 1-2 hr | bus 地址已知，缩小分析范围 |
> | **P2** | **A6** Band→Ring 映射寄存器搜索 | 1-2 hr | 如果 B1 没发现走错 ring 再做 |
> | **P3** | **C1** Ring 4 描述符字节级核对 | 1 hr | 需要 dump + 对比 |
> | **P3** | **D1/D2** 时序表 + 字段对照 | 2 hr | 收尾验证项 |
> | **P3** | **A4** 差异优先级列表 | A1 完成后 | A1 的输出物，不独立执行 |
> | 辅助 | **E1/E2/E3** 中断/MIB/PLE 旁证 | 穿插 | 辅助交叉验证 |
>
> **关键原则**: B1 先行 — 如果数据帧走 Ring 0 或 Ring 2，则 A/C/D 全部跳过。

---

## 10. 每轮排查的交付物（建议固定格式）

每一轮（session）至少输出以下内容，避免知识丢失：

- 本轮假设（1 句话）
- 唯一变更/唯一分析对象（1 项）
- 关键观测（日志/寄存器/事件）
- 结论（支持/反驳）
- 下一轮最小动作（1 项）

建议存放位置（按现有目录结构）：
- 阶段性总结：`docs/debug/`
- RE 证据与函数笔记：`docs/re/` 或 `tmp/re_results/consensus/` 对应条目
- 临时导出/差异脚本产物：`tmp/` / `tools/`

---

## 11. 明确避免的低效动作

- 一次同时改多个寄存器/命令参数，导致无法归因
- 在没有 Windows 对照证据前大范围“猜测式”抄写低可信驱动逻辑
- 只盯 `status=0`，不看数据面实际证据（DIDX / MIB / ring dump）
- 只看 Ring4，不观察其它 ring 的 DIDX（容易错过映射问题）
- 只看寄存器宏观值，不做 desc/ring memory 字节级检查

---

## 12. 退出条件（何时切换策略）

若满足以下条件，建议切换到“更强证据获取”策略（例如更深入 Ghidra 或 MMIO trace）：

- 主线 A 完成后没有发现有效差异，但 Ring4 仍完全静止
- 主线 B/C/D 均无新证据，只得到“仍不工作”
- 所有观察都表明固件内部吞帧或重路由，但缺乏直接证据

可切换策略（分析层面）：
- 深挖 Windows 驱动 `PostFwDownloadInit` 全调用链
- 针对 WFDMA 初始化做地址级 trace 对照
- 对 `WFDMA_CFG` 参数做位级差分分析（不同场景采样）

---

## 13. [REV3] 本清单当前盲点（代码+RE 交叉审阅结论）

以下是本次对 `src/`、`docs/re/`、`tmp/re_results/consensus/` 交叉检查后发现的文档盲点：

- **盲点 1：缺少“文档有效性管理”层**
  - 当前清单默认所有 `docs/re/` 都可直接引用，但仓库内存在明显过时结论。
  - 若不先做状态标记，容易重复排查已被推翻的方向（例如早期 BSS/STA TLV 缺失类结论）。

- **盲点 2：D3 对当前代码日志覆盖描述不准确**
  - 清单假设“Ring6 事件处理只关心少数事件、需要新增全量 dump”。
  - 实际代码已对所有 MCU event 做 `eid + hex dump`；更大风险在未知 `PKT_TYPE` 的 `dev_dbg` 可见性。

- **盲点 3：B1 只看 DIDX 证据粒度不够**
  - DIDX 只能回答“动没动”，不能回答“这个 ring 是否被配置/改写/激活”。
  - 对 ring0-7 做整组寄存器快照能显著提高一次实验的信息密度。

- **盲点 4：E1 只盯硬件 RX done 中断位**
  - 当前代码存在 `MCU2HOST_SW_INT -> WAKE_RX_PCIE -> 调度 RX NAPI` 的替代路径。
  - 若不纳入该路径，可能误判“完全没有 RX 活动”。

- **盲点 5：缺少“寄存器所有权（Host/FW）”分类**
  - A1 仅要求做差异表，未要求区分寄存器在什么阶段由谁写入。
  - 对固件会动态更新的寄存器，直接做 Windows vs Linux 静态差异对比可能得出错误结论。

- **盲点 6：缺少“诊断寄存器地址有效性”通用规则**
  - 当前只在 `DIS_STA0=0xdeadbeef` 一处提示可能 remap 错。
  - 应推广为通用规则：任何非 WFDMA 核心寄存器结论，都要先验证 bus2chip/remap 路径。

---

## 14. [REV3] 更优文档形态（建议作为后续版本方向）

当前清单已经接近执行版，但为了降低跨 session 误读成本，建议升级为“两层文档”结构：

### 14.1 层 1：执行清单（本文件保留定位）

作用：
- 给当下 session 一个可执行的排查顺序（A/B/C/D/E）
- 记录单轮实验输出格式与退出条件

特点：
- 短周期更新（每次 session 都可能变）
- 可包含临时优先级调整（如 `REV2` 的 B1 前置）

### 14.2 层 2：证据仲裁索引（建议新增独立文档）

建议新增一个文档（示例名）：
- `docs/debug/rx_ring4_evidence_index.md`

建议内容（按表格维护）：
- 证据项名称（寄存器/命令/函数/日志）
- 来源文件（`docs/re/...` / `tmp/re_results/...` / `src/...`）
- 日期 / Session
- 状态（`ACTIVE/PARTIAL/STALE`）
- 与当前 `CLAUDE.md` 是否一致
- 备注（为何失效、哪些片段仍可复用）

价值：
- 把“执行动作”和“证据可信度管理”拆开
- 避免新 session 反复踩旧文档坑
- 让 RE 资料增多时仍能快速决策

### 14.3 本文件内可立即采用的轻量升级（无需重写）

- 在每个新增 `[REVx]` 条目前标注“证据依据文件”
- 对引用的 `docs/re/` 条目加状态标注（`ACTIVE/PARTIAL/STALE`）
- 在第 10 节交付物模板中增加 `参考文档状态变更` 一栏

---

## 15. [REV3] 作者与修补范围说明

- **本次增补作者**：Codex / GPT-5（基于当前仓库代码、`docs/re/`、`tmp/re_results/consensus/` 交叉审阅）
- **修补范围**：仅追加分析方法与文档治理相关内容，不删除或改写原有排查结论
- **目的**：减少过时 RE 结论对当前 `RX Ring4 DIDX=0` 排查的干扰，提高单轮实验信息密度
