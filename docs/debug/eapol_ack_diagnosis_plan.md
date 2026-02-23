# EAPOL ACK 问题补充分析与细化排查路径

**创建**: Session 41 (2026-02-23)  
**作者**: Codex / GPT-5（基于 `CLAUDE.md` + `docs/debug/eapol_ack_problem.md` + 当前代码交叉分析）  
**适用问题**: Auth + Assoc 成功后，AP 不发 EAPOL Key 1/4；空口观察到 AP 管理帧重传，怀疑双向 ACK 异常

---

## 1. 结论先行（当前最可能根因）

当前问题大概率不是 WPA/EAPOL 协议层逻辑错误，而是 **RMAC 本机地址识别 / 硬件自动 ACK 路径未打通**。

最可能链路：

```
DEV_INFO/BSS 激活后 MUAR 未正确编程（或 RMAC ACK 模式未生效）
    ↓
AP 发来的单播管理帧可被 DROP_OTHER_UC=0 绕过并上送 Host
    ↓
但硬件因 MUAR/RMAC 不匹配不自动发 ACK
    ↓
AP 重传 Auth-2 / Assoc Resp，不认为会话稳定完成
    ↓
AP 不启动 EAPOL 1/4
```

---

## 2. 当前证据（来自文档 + 代码）

### 2.1 已确认事实（来自 `CLAUDE.md` / `eapol_ack_problem.md`)

- RX Ring 4 已恢复，Auth-2 和 Assoc Response 能到达 mac80211
- Auth / Assoc 均成功（至少主机栈视角成功）
- AP 不发 EAPOL Key 1/4
- AP 会重复发送 Auth-2 / Assoc Resp（强烈指向我们未 ACK）
- `MIB RX_OK=0` 长期为 0
- 清除 `RFCR DROP_OTHER_UC` 后帧能到 Host（说明 Host RX 可见性不等于 RMAC 正常匹配）

### 2.2 当前代码里已经具备的强诊断能力（可直接复用）

- **WTBL dump 可解析 MUAR index**
  - `src/mt7927_pci.c` 中 `mt7927_dump_wtbl()` 会打印 `LWTBL[x] PARSED: ... MUAR=...`
  - 这是当前验证 “固件是否给 WTBL 条目分配 MUAR” 的最佳现成工具

- **TXS 解析可判断 ACK timeout / RTS timeout / Queue timeout**
  - `src/mt7927_mac.c` 中 `mt7927_mac_dump_txs()` 已解码 `ack_to/rts_to/q_to`
  - 可用来确认 “我们的管理帧是否收到 AP ACK”

- **ROC_GRANT 后会清 `DROP_OTHER_UC`（诊断绕过路径）**
  - `src/mt7927_mac.c` 与 `src/mt7927_pci.c` 中已有多处 RFCR 清位
  - 这解释了“Host 能收到 AP 帧，但硬件仍可能不 ACK”的矛盾现象

- **管理帧 TX 会请求 TX status（Host + MCU）**
  - `src/mt7927_mac.c` 中 `mt7927_mac_write_txwi()` 对 mgmt 设置 `TX_STATUS_HOST | TX_STATUS_MCU`
  - 理论上应能收到 TXS / TX_DONE 辅助判断 ACK 结果

### 2.3 当前代码的诊断盲点（会影响判断效率）

- **未实现 `ieee80211_tx_status*` 回报**
  - 已设置 `REPORTS_TX_ACK_STATUS`，但未见 `ieee80211_tx_status_irqsafe()` 调用
  - 不影响 AP 是否发 EAPOL，但会让 mac80211 层 ACK 诊断能力不足

- **管理帧 CT 路径 PID 固定为 0（TXS 难精确关联）**
  - `mt7927_tx_prepare_skb()` 走 `mt7927_mac_write_txwi(..., pid=0, ...)`
  - TXS 能来，但多帧并发时很难把 TXS 精准对应到某一帧（Auth/Assoc/EAPOL）

- **MUAR 硬件寄存器（WMUDR/WMUMR）尚未定义/读取**
  - 文档给了候选 BAR0 地址，但代码中未见对应寄存器宏和读取逻辑

---

## 3. 假设优先级（建议按此顺序验证）

### H1. MUAR 未编程或编程错误（概率最高）

表现吻合：
- 清掉 `DROP_OTHER_UC` 后 Host 能看到帧
- 但硬件不自动 ACK（AP 重传）
- `MIB RX_OK=0`（可能并未走正常 RMAC 匹配计数路径）

### H2. RMAC ACK / MUAR 模式寄存器未正确初始化（高）

即使 MUAR 内容正确，也可能因为 RMAC 模式位未开导致不 ACK。

### H3. MUAR 曾正确，但被后续步骤覆盖/重置（中）

例如：
- `AUTH→ASSOC` 重新发 BSS_INFO + STA_REC(state=2)
- 固件重配 BSS 时恢复某些 RMAC/RFCR 配置

### H4. 非 ACK 根因（较低）

例如：
- AP 其实 ACK 了我们的帧，只是抓包看不到 ACK 控制帧
- 但这与 AP 大量重传 Auth-2 / Assoc Resp 不一致，因此优先级低

---

## 4. 细化检测路径（按一次连接流程时间线）

目标：用最少试验轮次把问题收敛到 `MUAR`、`RMAC ACK 模式` 或“其它”。

### T0: 连接前基线（关联前）

记录一次基线状态（避免把默认值误判为异常）：
- Band0 `RFCR/RFCR1`
- Band0 `MIB RX_OK/TX_OK`
- `WTBL[0]` / `WTBL[1]`（如果已有内容）

用途：
- 后续对比“谁在何时改了什么”

### T1: `DEV_INFO` 后立即检测（关键）

当前代码注释已明确假设：
- `DEV_INFO` 负责 “registers MAC in MUAR”

因此在 `mgd_prepare_tx` 发送 `DEV_INFO` 之后，应该优先验证：

1. `WTBL[0]`（vif/self entry）中的 MUAR 字段是否变为有效值  
2. `WTBL[0]` 的 MAC 是否等于 `vif->addr`  
3. 若可读取 MUAR 硬件表（WMUDR/WMUMR），slot0/相关 slot 是否出现本机 MAC  

判定：
- `WTBL[0].MUAR = 0xF (UNMATCHED)` 或无有效变化：强支持 H1
- `WTBL[0].MUAR` 有效但 MUAR 表为空：说明固件分配逻辑/寄存器解释可能有偏差（H1/H2）

### T2: `BssActivateCtrl + PM_DISABLE + full BSS_INFO + ROC_GRANT + first STA_REC(state=0)` 后

在当前 `mgd_prepare_tx` 流程里，这是 auth 发送前最后一个关键阶段。

建议固定采样：
- `WTBL[0]`, `WTBL[1]`
- Band0 `RFCR/RFCR1`
- `MIB RX_OK/TX_OK`
- （新增）MUAR 硬件寄存器表项

重点观察：
- `WTBL[1]`（AP STA 条目）是否已创建且 MUAR 是否合理
- `WTBL[0]` 的 MUAR 是否在这一阶段被覆盖
- `RFCR DROP_OTHER_UC` 是否被清除（现有代码会清）

### T3: Auth/Assoc 空口交互期间（用 TXS 做闭环）

此阶段不只看空口抓包，必须结合芯片内部 TXS：

检查项：
- 管理帧发送后是否收到 `PKT_TYPE_TXS`
- TXS 中 `ack_to / rts_to / q_to`

判定逻辑：
- `ack_to=1` 且 `q_to=0`：帧上空口了，但没收到 AP ACK（支持“入站 ACK 接收异常”）
- `q_to=1`：帧根本没成功离开芯片（当前问题链不匹配）
- `ack_to=0` 且 AP 仍重传很多：需重新审视空口观察与 TXS 对应关系

说明：
- 监控模式抓包不可靠地看不到 ACK 控制帧是常见现象
- 但 **AP 重传单播管理帧** + **TXS ack_to=1** 组合是强证据

### T4: `AUTH→ASSOC` / `ASSOC→AUTHORIZED` 状态转换后（当前最容易漏）

当前代码在 `AUTH→ASSOC` 会再次发送：
- `BSS_INFO(full)`
- `STA_REC(state=2)`
- 并重新清 `DROP_OTHER_UC`

这里要验证是否发生“后续覆盖”：
- `WTBL[0].MUAR` 是否改变
- `WTBL[1].MUAR` 是否改变
- `RFCR` 是否被固件恢复为不利值
- MUAR 硬件表项是否被重置

如果 T1/T2 正常、T4 异常，优先考虑 H3（后续步骤覆盖）

---

## 5. 建议的决策树（检测结果 → 下一步）

### 分支 A：`WTBL[0]/WTBL[1]` 的 MUAR 明显无效（如 0xF）

结论倾向：
- 固件没有正确建立 MUAR 映射（H1）

下一步：
1. 复核 `DEV_INFO` payload 字段（尤其 `band_idx/ownmac_idx/phy_idx/mac_addr`）
2. 复核 `BssActivateCtrl` 与 `DEV_INFO` 的时序（当前已接近 Windows，但仍需确认无重入）
3. 做一次 **手动 MUAR 编程实验**（见第 7 节）

### 分支 B：WTBL 显示 MUAR 有效，但 MUAR 硬件表项为空/不匹配

结论倾向：
- “WTBL 中 MUAR index”和“实际 MUAR 表”之间存在失配
- 可能是寄存器读取方法不对，或固件未真正写表（H1/H2）

下一步：
1. 先确认 `WMUDR/WMUMR` 寄存器访问方法和 slot 索引机制
2. 对照 Windows RE 查 MUAR 表写入序列
3. 验证是否需要额外 commit/trigger 寄存器

### 分支 C：WTBL + MUAR 表都正确，但 `TXS ack_to=1` 持续出现

结论倾向：
- MUAR 内容存在，但 RMAC ACK 模式/匹配路径未生效（H2）

下一步：
1. 排查 RMAC 相关模式寄存器（`RMAC_MORE/MUAR_MODE` 等）
2. 对比 Windows 在 `RMAC` 区域（Band0 `BAR0 0x021400` 附近）的初始化写入
3. 检查是否有固件在 BSS 重配后覆盖 RMAC 模式位

### 分支 D：`TXS ack_to=0`（收到 ACK）但仍无 EAPOL 1/4

结论倾向：
- ACK 不是主要根因，转向关联后状态/加密前置条件

下一步：
1. 验证 AP 是否实际将 STA 标记为 associated（空口+驱动日志双证据）
2. 检查关联后 BSS/STA 状态是否仍与 Windows 有差异
3. 检查 RSN/AKM/能力字段在 assoc req 中是否有问题（这属于更后续方向）

---

## 6. 建议优先添加/强化的检测点（代码插桩路径）

以下是“性价比最高”的插桩点，按优先级排序。

### P0. MUAR 相关硬件寄存器读取（新增）

目的：
- 直接验证 MUAR 表里是否有本机 MAC，而不是只看 WTBL 的 MUAR index

建议位置：
- `mgd_prepare_tx` 中 `DEV_INFO` 后
- `ROC_GRANT` 后首次 `STA_REC(state=0)` 后
- `sta_state AUTH→ASSOC` 后（`STA_REC(state=2)` 后）

预期输出：
- slot 编号
- MAC 值
- mask/valid 状态

### P0. WTBL dump 在关键时点固定化（已有能力，建议制度化）

已有函数：
- `mt7927_dump_wtbl(dev, idx)`

建议固定 dump：
- `wlan_idx=0`（vif/self）
- `wlan_idx=1`（AP STA，当前常见场景）

关键时点：
- T1、T2、T4（见第 4 节）

### P0. TXS 与管理帧时序关联增强

现状：
- TXS 已能看 `ack_to`
- 但 CT mgmt 路径 PID=0，难精确关联

建议（检测优化，不一定是最终修复）：
- 给管理帧（至少 auth/assoc）分配非零 PID
- 打印 “frame type/subtype + PID”
- 在 TXS 日志中输出 PID 后按 PID 对应

这样能快速确认：
- Assoc Req 是否真的 `ack_to=1`
- 是否只有某类帧 ACK 异常

### P1. `MCU2HOST` / `TX_DONE` / `TXS` 全链路对时

目的：
- 证明 “管理帧确实发出且没 ACK”，而不是仅凭空口/重传推测

建议统一记录：
- `TXFREE header stat/count`
- `TX_DONE(UNI/legacy)` 状态
- `TXS ack_to/rts_to/q_to`

注意：
- `TXFREE` 与 `TXS` 含义不同，不要互相替代

### P1. `MIB RX_OK` 与 Host `rx_ok_count` 同时记录

目的：
- 验证 “Host 收到帧但 MIB 不涨” 是否稳定复现

如果长期稳定成立：
- 强烈支持“非正常 RMAC 匹配路径”或 MIB路径未计数

---

## 7. 更细化的解决路径（建议按阶段推进）

### 阶段 1：把问题坐实为 MUAR / ACK 路径（不改行为，只加检测）

完成以下 4 项即可显著收敛：
1. 关键时点 `WTBL[0]/[1]` MUAR dump
2. MUAR 硬件表读取（WMUDR/WMUMR）
3. 管理帧 TXS `ack_to` 证据
4. `MIB RX_OK` vs Host `rx_ok_count` 对照

阶段 1 的目标不是修复，而是明确落入哪条分支（A/B/C/D）。

### 阶段 2：手动 MUAR 编程实验（诊断型 workaround，价值很高）

适用条件：
- 分支 A 或 B（MUAR 明显异常）

实验目标：
- 不改上层 MCU 命令逻辑，仅手动写 MUAR slot，让硬件识别本机 MAC
- 观察 AP 是否开始停止重传并发送 EAPOL 1/4

若实验成功：
- 根因几乎可确定为 “MUAR 编程缺失/错误”
- 后续再追求 Windows 一致做法（通过 DEV_INFO/BSS 激活触发，而非长期手写）

若实验失败：
- 转向 RMAC ACK 模式（阶段 3）

### 阶段 3：RMAC ACK/MUAR 模式寄存器对比（Windows RE 主导）

适用条件：
- MUAR 内容看起来正确，但仍不 ACK（分支 C）

重点区域：
- Band0 `RMAC` 区域（`BAR0 0x021400` 附近）
- `RFCR/RFCR1` 邻近寄存器
- `RMAC_MORE/MUAR_MODE` 类寄存器（文档候选）

建议方法：
1. 在 Windows RE 中定位 `mac_init_band` 同类函数的 RMAC 写入
2. 对比当前驱动 `mt7927_mac_init_band()` 的写入覆盖范围
3. 找出“Windows 写了、当前没写”的 ACK/MUAR 模式寄存器

### 阶段 4：回归为“固件触发 MUAR 编程”的最终修复

目标：
- 不依赖诊断型手写 MUAR，恢复 Windows 风格行为

重点复核对象：
- `DEV_INFO` payload 字段（尤其 `omac/band/phy/mac_addr`）
- `BssActivateCtrl` 的 `active/band_idx/link_idx` 字段
- 相关命令时序是否在某个状态迁移中被重入或覆盖

成功标准：
- 无需 `DROP_OTHER_UC` 绕过也能完成 Auth/Assoc/EAPOL
- `MIB RX_OK` 开始有意义增长
- AP 不再重传 Assoc Resp

---

## 8. 当前代码下的几个具体判断点（便于快速读日志）

### 8.1 看到了 `RX-EAPOL` 日志才算问题进入下一阶段

当前 `src/mt7927_mac.c` 已对 `ethertype=0x888E` 做专门日志。

如果完全没有 `RX-EAPOL`：
- 不要先查 `set_key()`
- 因为 AP 根本没发 Key 1/4，密钥安装路径尚未进入

### 8.2 `REPORTS_TX_ACK_STATUS` 已开，不等于 ACK 路径已打通

这只是告诉 mac80211 “驱动会报告 ACK 状态”，不影响硬件是否真正收/发 ACK。

### 8.3 `DROP_OTHER_UC` 清零是诊断绕过，不是最终修复

它能帮助 Host 收到“本不匹配 MUAR 的帧”，但不能替代硬件自动 ACK。

---

## 9. 建议的最小执行清单（下一轮 session）

按收益/成本比排序：

1. 在连接流程 3 个时点固定 dump `WTBL[0]/[1]`，记录 MUAR 变化
2. 增加 MUAR 硬件表读取（WMUDR/WMUMR）并与 WTBL MUAR index 对照
3. 用 TXS 明确记录 Assoc Req 的 `ack_to/rts_to/q_to`
4. 对照 `MIB RX_OK` 与 Host `rx_ok_count`
5. 若 MUAR 异常明显，做一次手动 MUAR 编程实验
6. 若 MUAR 正常但仍 `ack_to=1`，转 RMAC ACK 模式寄存器对比

---

## 10. 记录模板（建议用于每次实验）

每轮实验只改 1 个变量，并记录：

- 实验目标（1句话）
- 修改点（唯一变量）
- 时点采样（T1/T2/T3/T4）
- `WTBL[0]/[1] MUAR`
- MUAR 硬件表内容（slot → MAC）
- TXS (`ack_to/rts_to/q_to`)
- `MIB RX_OK/TX_OK`
- AP 重传现象（Auth-2/Assoc Resp/EAPOL）
- 结论（支持 H1/H2/H3/H4）

---

## 11. 附：本分析对原问题文档的补充定位

`docs/debug/eapol_ack_problem.md` 已很好地给出问题定义与初步假设。  
本文件的作用是：

- 把“MUAR/ACK”假设转化为 **可操作的检测路径**
- 明确哪些证据是现有代码已具备的（WTBL dump/TXS）
- 给出 “检测结果 → 下一步” 的决策树，减少试错轮数

