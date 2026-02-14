# Windows v5.7.0.5275 深度逆向结论（MT7925/MT7927）

## 样本
- `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys`
- `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.inf`
- INF 版本: `01/05/2026,5.7.0.5275`

## 这版驱动是否同时支持 7925 + 7927
- 是。`inf` 同时包含 `DEV_7925` 与 `DEV_7927/6639`（含多个子系统 ID）。
- 对我们现在的问题（7927 DMA + FW 下载）可以直接作为主参考样本。

## 本次重点函数（Ghidra）
- `FUN_1401e5430` -> `MT6639PreFirmwareDownloadInit`
- `FUN_1401e5be0` -> `MT6639WpdmaConfig`
- `FUN_1401e4580` -> `MT6639InitTxRxRing`
- `FUN_1401e43e0` -> `MT6639ConfigIntMask`
- `FUN_1401d01d0` -> `AsicConnac3xLoadFirmware`
- `FUN_1401d13e0` -> `AsicConnac3xLoadRomPatch`
- `FUN_1401ce900` -> `AsicConnac3xGetFwSyncValue`（读取 `0x7c0600f0`）
- `FUN_1401d8724` -> WFDMA 使能/停机包装（操作 `0x7c024208`）

## 关键寄存器序列（可直接映射 Linux）

### 1) DMA GLO/门控（`FUN_1401e5be0` + `FUN_1401d8724`）
- 先经 `FUN_1401d8724(param, 0, enable)` 做前置。
- 读写 `0x7c024208`（GLO），并在停机路径调用 wait-idle。
- 停机路径额外清:
  - `0x7c02420c = 0xffffffff`
  - `0x7c024280 = 0xffffffff`
- 若条件满足，写：
  - `0x7c027030`（read-modify-write）
  - `0x7c0270f0 = 0x00660077`
  - `0x7c0270f4 = 0x00001100`
  - `0x7c0270f8 = 0x0030004f`
  - `0x7c0270fc = 0x00542200`
- `0x7c024208 |= 0x5`
- `0x7c0242b4` 置 bit28 (`|= 0x10000000`)

### 2) 中断掩码（`FUN_1401e43e0`）
- 写 `0x7c02422c` 或 `0x7c024230`（由参数选择，代码中是 `+0xfffffffc` 偏移逻辑）
- 值为 `0x2600f000`
- 读 `0x7c024204`
- 对 `0x74030188` 的 bit16 做 set/clear（含 6639/7927/7925/0738/0717 芯片分支）

### 3) Ring 初始化（`FUN_1401e4580`）
- TX/CMD 一组寄存器基址模式:
  - `0x7c024300 / 0x304 / 0x308 / 0x30c`
- FWDL/事件另一组:
  - `0x7c024500 / 0x504 / 0x508 / 0x50c`
- 初始化后会遍历 descriptor，修正 owner/idx 位（可见 `0x3fff0000` 掩码路径）。

## 固件下载协议（从 `FUN_1401d01d0` 抽象）
- 解析 FW section header 后，逐 section 处理。
- 对 section 调 `FUN_1401cb88c(...)` 做 section 命令/状态准备。
- 数据按 `0x800` 分块，调用 `FUN_1401cde70(...)` 下发。
- section 结束后再进入阶段提交：
  - `FUN_1401ce7c0(param, local_1dc != 0, local_1dc, 0)`
- 之后轮询 `FUN_1401ce900()`（即读 `0x7c0600f0`）等待状态变为 `3`（ready）。
- 超时路径使用 1ms sleep，最多约 500 次。

### 相关 MCU 命令包装（`FUN_1400cdc4c` 调用形态）
- `FUN_1401cb88c`:
  - 组 12-byte payload（section addr/len/flag）。
  - 通过 `FUN_1400cdc4c(..., cmd=0x0d, payload_len=0x0c, ...)` 发送。
- `FUN_1401cde70`:
  - 发送 FW 数据块（外层按 0x800 分块）。
  - 通过 `FUN_1400cdc4c(..., cmd=0x10, payload_len=chunk_len, token=0xee, ...)` 发送。
- `FUN_1401ce7c0`:
  - 组 8-byte 提交 payload（flags + param3）。
  - flags 低位可见 `|0x2`、条件 `|0x1`、条件 `|0x4`。
  - 通过 `FUN_1400cdc4c(..., cmd=0x17, payload_len=0x08, ...)` 发送。

## 对当前 Linux 卡点的直接意义

### 已确认的重要点
- Windows 的“FW 状态同步值”核心来自 `0x7c0600f0`，不是 `HOST_INT_STA`。
- Windows 下载协议是“section 命令 + 0x800 scatter 分块 + 提交 + 同步值轮询”，不是只看 ring index。

### 对应你当前症状
- 你现在 `ring16 not consumed` / `ring15 not consumed` 与 `HOST_INT_STA=0`，不能证明 MCU 没响应；更可能是：
  - 发送格式（descriptor/token）未达 Windows 期望；
  - 或发送后缺少 `FUN_1401ce7c0` 等“阶段提交”语义；
  - 或状态判断寄存器选错（应优先追 `0x7c0600f0` 的状态机变化）。

## 新增深入结论（发送后端与 `FW_SCATTER` 特例）
- `FUN_1400cdc4c` 是发送总入口，会在 `FUN_14014e644` / `FUN_1400cd2a8` 间分流。
- `FUN_1400cd2a8`（`MtCmdSendSetQueryCmdAdv`）可清晰看到 FW 下载命令白名单：
  - `0x01/0x02/0x03/0x05/0x07/0x10/0x11/0xee/0xef`
- `FW_SCATTER (0xee)` 在该路径有显式特殊编码，不是裸 DMA chunk：
  - `*(u16 *)(hdr + 0x24) = 0xa000`
  - `*(u8  *)(hdr + 0x27) = 0`
- 命令最终经 `FUN_1400c8340 -> FUN_1400d316c -> FUN_1400c9810` 入队与等待（槽位+事件模型），不是单靠 DIDX 前进判定成功。

## 产物
- 流程抽取: `docs/win_v5705275_fw_flow.md`
- 核心函数反编译: `docs/win_v5705275_core_funcs.md`
- 辅助函数反编译: `docs/win_v5705275_helpers.md`
- 发送后端专项: `docs/win_v5705275_mcu_send_core.md`
- 发送后端反编译: `docs/win_v5705275_mcu_send_backends.md`
- DMA 提交路径: `docs/win_v5705275_mcu_dma_submit.md`
- 入队与等待路径: `docs/win_v5705275_dma_enqueue.md`, `docs/win_v5705275_dma_lowlevel.md`
- 归纳文档: `docs/windows_v5705275_mcu_cmd_backend.md`
