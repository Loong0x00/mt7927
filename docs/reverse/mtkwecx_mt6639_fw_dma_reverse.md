# MTK Windows MT6639/MT7927 固件下载与 DMA 初始化逆向笔记

## 1. 样本与方法
- 样本: `DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/mtkwecx.sys`
- SHA256: `7c6d86c80aecbea5cf34b14828e96b9be31cb94f2bb6a4a629c4cc11bb2e32ea`
- 工具:
  - `ghidra_12.0.3_PUBLIC/support/analyzeHeadless`
  - `objdump -d -M intel`
  - `strings`
- 自动提取结果: `docs/reverse/mtkwecx_fw_flow_auto.md`

## 2. 关键函数定位（MT6639）
- `MT6639PreFirmwareDownloadInit` -> `FUN_1401d7ae0`
- `MT6639ConfigIntMask` -> `FUN_1401d6b90`
- `MT6639InitTxRxRing` -> `FUN_1401d6d30`
- `MT6639WpdmaConfig` -> `FUN_1401d8290`
- `HalDownloadPatchFirmware` -> `FUN_1401f0be4`
- `AsicConnac3xLoadRomPatch` -> `FUN_1401c6210`
- `AsicConnac3xLoadFirmware` -> `FUN_1401c5020`
- `AsicConnac3xWfdmaWaitIdle` -> `FUN_1401ccdf0`
- `AsicConnac3xWpdmaInitRing` -> `FUN_1401ccfe0`

额外证据（字符串）:
- `P0T15:AP CMD`
- `P0T16:FWDL`
- `P0R0:FWDL`

## 3. 逆向得到的寄存器写序（重点）

### 3.1 `MT6639WpdmaConfig` (`0x1401d8290`)
观察到的关键序列:
- 先调用 `0x1401ccb54`（形态上是 wait-idle/停 DMA 前置流程）。
- 读 `0x7c024208`。
- 条件分支下配置:
  - 读改写 `0x7c027030`
  - 写 `0x7c0270f0 = 0x00660077`
  - 写 `0x7c0270f4 = 0x00001100`
  - 写 `0x7c0270f8 = 0x0030004f`
  - 写 `0x7c0270fc = 0x00542200`
- 将 `0x7c024208` OR `0x5` 后写回（使能位）。
- 读改写 `0x7c0242b4`，置位 bit 28。

### 3.2 `MT6639ConfigIntMask` (`0x1401d6b90`)
观察到:
- 通过 `0x140008ff8/0x140008f8c`（写/读寄存器包装）操作:
  - `0x7c02422c`
  - `0x7c024204`
- 芯片分支（含 `0x6639/0x7927/0x7925`）下操作:
  - `0x74030188` 的 bit16（bts/btr）

### 3.3 `MT6639InitTxRxRing` (`0x1401d6d30`)
观察到成组 ring 寄存器编程（按 port/queue 偏移计算）:
- 一组 base 在 `0x7c0243xx`:
  - `0x7c024300 / 0x304 / 0x308 / 0x30c`
- 另一组 base 在 `0x7c0245xx`:
  - `0x7c024500 / 0x504 / 0x508 / 0x50c`
- 每组都先读 ring 状态，再写 base/cnt/cidx 等字段。

### 3.4 `MT6639PreFirmwareDownloadInit` (`0x1401d7ae0`)
前置初始化中出现的关键点:
- 调用 `0x1401c3840` 获取状态，失败则走错误路径。
- 调用 `0x14000ca20` + 轮询（`0x3e8` 延迟循环，最多约 `0x1f4` 次）。
- 观察到对这些地址的读写:
  - `0x7c021000`（写值见 `0x70011840`）
  - `0x00010200 / 0x00010204 / 0x00010208`
  - `0x00010020`

## 4. 固件下载主流程（`HalDownloadPatchFirmware`）
`FUN_1401f0be4` 内部大量状态机分支，但可确定:
- 该函数是下载总入口（多个调用点直接 `call 0x1401f0be4`）。
- 先检查多个回调/函数指针（结构体偏移 `0x14666xx` 区域），按返回值决定 patch/ram 阶段。
- 在流程中调用 `0x1401d2ab8`（出现两次，形态上像“下载阶段执行/切换”）。
- 结合字符串可确认包含 patch 及 RAM FW 两阶段逻辑。

## 5. 与当前 Linux 原型问题的直接关系
你当前日志的核心症状是:
- `ring15 not consumed: cpu_idx=1 dma_idx=0`
- `HOST_INT_STA=0, MCU_CMD=0`
- 出现 `AMD-Vi: IO_PAGE_FAULT`（DMA 地址访问异常）

这和 Windows 流程对比后，最可疑缺口是:
- 缺失/不完整的 DMA 前置寄存器序列（尤其 `0x7c0270f0~0x7c0270fc`、`0x7c0242b4 bit28`、`0x7c024208` 使能顺序）。
- IOMMU 映射或 ring 地址生命周期问题（设备访问了未映射/陈旧地址）。
- `PreFirmwareDownloadInit` 的前置握手与轮询未完整对齐。

## 6. 结论（当前可确认）
- Windows 驱动不是“只写 ring base 就发 MCU 包”，而是有一段较重的 WPDMA/WFDMA 前置配置。
- `P0T15/P0T16/P0R0` 映射方向与你当前判断一致（CMD/FWDL/EVENT）。
- 你现在卡在“DMA 引擎尚未真正消费 ring”的阶段，问题在固件协议之前。

## 7. 置信度说明
- 高置信: 函数定位、调用点、关键寄存器地址与立即数（有反汇编直接证据）。
- 中置信: 某些子函数语义命名（如 `0x1401d2ab8` 的具体职责，需继续深挖调用上下文/结构体定义）。
