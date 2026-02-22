# MT7927 当前问题分析与 Windows 逆向结论（阶段总结）

## 1. 目标与现状
- 目标：让 `tests/04_risky_ops/mt7927_init_dma.ko` 完成 MT7927/6639 的 MCU 固件下载流程。
- 当前卡点：**DMA TX 描述符没有被硬件消费**，导致后续 MCU 命令与固件下载都超时。

---

## 2. 现场现象（从多轮日志反复出现）

### 2.1 核心重复现象
- `kick-after` 后，主机侧索引会变化（通常 `CIDX` 从 `0 -> 1`）。
- 设备侧索引不变化（`DIDX` 持续 `0`）。
- 随后报错：
  - `ring16 not consumed: cpu_idx=0x1 dma_idx=0x0`（DMA probe）
  - 或 `ring15 not consumed: cpu_idx=0x1 dma_idx=0x0`（MCU 命令）
- 同时：
  - `HOST_INT_STA=0x00000000`
  - `MCU_CMD=0x00000000`
- 结果：`Patch download failed: -110`（超时）。

### 2.2 哪些变量已被排除（已做过 A/B）
- WM ring 选择：
  - `wm_ring_qid=15` 失败
  - `wm_ring_qid=17` 失败
- scatter 发送路径：
  - `scatter_via_wm=0`（raw FWDL ring）失败
  - `scatter_via_wm=1`（WM 封装）也失败
- kick 方向：
  - 写 `DIDX` 作为 doorbell：在你的机器上更差（索引几乎不动）
  - 写 `CIDX` 作为 doorbell：至少能看到主机索引推进，但硬件仍不消费
- Windows 预下载寄存器序列：
  - `enable_predl_regs=1` 无改善（仍不消费）
- 降配初始化：
  - `minimal_dma_cfg=1`（去掉大量附加配置）无改善（仍不消费）

结论：问题不在 FW 协议细节（0xee/chunk）本身，而在更底层的 **TX 队列激活/消费前置条件未满足**。

---

## 3. 在 Windows 驱动逆向里已经确认的事实

样本：
- `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys`
- `DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/`（旧版对照）

关键结论（已落地到 `docs/windows_v5705275_*` 系列文档）：

1. **发送总入口与后端分流**
- `FUN_1400cdc4c` 是发送总入口。
- 会在至少两条后端间分流（含 `MtCmdSendSetQueryCmdAdv` 路径）。

2. **FW 下载命令不是“裸 DMA 数据”**
- `0x01/0x02/0x05/0x07/0x10/0xee/0xef` 走统一命令封装与队列模型。
- `FW_SCATTER (0xee)` 有专门分支，不是直接把 chunk 挂 ring16 就结束。

3. **`0xee` 的已知特例**
- 逆向中可见 `*(u16 *)(hdr + 0x24) = 0xa000`、token/字段特殊处理分支。
- 这解释了“即使 DMA 能动，协议头不对也会卡”。

4. **Windows 不是只看 DIDX**
- 命令对象 + 入队 + 槽位状态 + 事件等待（`FUN_1400c8340 -> ... -> FUN_1400c9810`）。
- 即使某次队列消费，也不等于协议成功。

5. **Ring 初始化关键寄存器簇**
- TX：`0x7c024300/304/308/30c`
- FWDL/事件相关：`0x7c024500/504/508/50c`
- 停机/清理：`0x7c02420c`、`0x7c024280`
- Windows 初始化中会读取 idx 并同步软件状态，这一点已在 Linux PoC 补过。

---

## 4. 为什么现在看起来“越改越乱”
- 我们同时在推进两条线：
  - 线A：协议层（0xee、chunk、事件握手）
  - 线B：DMA 基础层（ring 能否被消费）
- 但你的最新多轮结果显示：线B都没打通时，线A改再多也不会生效。
- 所以应当先把问题收敛成单点：**让任意一个 TX ring 的最小包稳定被硬件消费**。

---

## 5. 当前最可靠结论（可作为后续工作的“基线”）
- 你的设备上目前状态是：
  - DMA 配置寄存器“写得进去、读得回”
  - 但 TX ring 描述符不被消费
  - MCU 完全没有进入有效处理（`HOST_INT_STA/MCU_CMD` 无变化）
- 这意味着最可能缺少的是：
  - 某个“队列激活/doorbell/ownership”额外步骤
  - 或 ring 所在地址/属性虽合法但对该引擎实例无效（Windows 中可能有额外路由选择）

---

## 6. 建议的单线程推进策略（避免继续发散）
1. 固定参数基线（不再多变量混测）：
   - `wm_ring_qid=15 scatter_via_wm=0 wait_mcu_event=1 tx_kick_use_didx=0 minimal_dma_cfg=1`
2. 只做一件事：
   - 从 Windows `MT6639InitTxRxRing` 与 enqueue 路径再抽取“首包前必须写的额外寄存器”，逐条最小增量验证。
3. 一次只改 1~2 个寄存器步骤，记录：
   - `kick-before/after`、`CIDX/DIDX`、`HOST_INT_STA/MCU_CMD`
4. 只要出现“ring16 或 ring15 首次被稳定消费”，再切回协议层处理 0xee。

---

## 7. 结论
- 现在不是“找不到方向”，而是方向需要收束：
  - **先修 DMA 消费，再谈 FW 协议。**
- Windows 逆向已提供了足够证据证明这一点，下一步应围绕“队列激活前置寄存器”做精确补齐，而不是继续大范围试错。

