# Windows v5.7.0.5275: MCU 命令后端与 FW 下载关键结构

## 1) 发送后端选择（`FUN_1400cdc4c`）

`FUN_1400cdc4c` 会根据设备 ID（包含 `0x6639/0x7927/0x7925`）和设备私有标志位（`+0x146cde9`）在两条发送后端间切换：

- `FUN_14014e644`
- `FUN_1400cd2a8`（日志名：`MtCmdSendSetQueryCmdAdv`）

当前逆向里，`FUN_1400cd2a8` 的反编译更完整，且直接覆盖 `0x01/0x02/0x03/0x05/0x07/0x10/0x11/0xee/0xef` 这组 FW 下载相关命令。

## 2) FW 下载相关命令在 Windows 中的编码方式

在 `FUN_1400cd2a8` 内，FW 下载不是“裸 DMA 数据包”，而是统一走 `MtCmdSendSetQueryCmdAdv` 封装后再入队：

- `param_3` 是 MCU CID（例如 `0x01/0x02/0x05/0x07/0xee/0xef`）
- `param_8` 是 payload length
- `param_7` 指向 payload

关键字段写入（以 `*(pcVar8 + 0x68)` 指向的 TX 缓冲头为中心）：

- `word +0x20 = param_8 + 0x20`
- `byte +0x24 = param_2`（set/query 维度）
- `byte +0x25 = 0xa0`
- `byte +0x27 = token/seq`（`FUN_14009a46c(...)`）
- `word +0x22` / `word +0x24` 在分支中会被覆盖
- payload 最终拷入 `+0x40` 起始区

`FW_SCATTER (CID=0xee)` 有专门分支：

- `*(u16 *)(hdr + 0x24) = 0xa000`
- `*(u8  *)(hdr + 0x27) = 0`

这说明 Windows 对 scatter 包头有特殊编码，不是简单“把固件 chunk 直接挂到 TX ring16”。

## 2.1) 调度表（`DAT_1402507e0`）解析结果

按 `0x0d` 字节步长解析 `DAT_1402507e0`（共 `0x3a` 项）可得到结构：

- `u16 cmd_key`
- `u16 mark`（`0xa5` 表示占位/跳过）
- `u8 sub_key`
- `u64 handler`

其中与 `cmd_key=0xed` 相关的有效项至少包括：

- `sub=0x21 -> 0x140146d60`
- `sub=0x94 -> 0x140144f80`
- `sub=0x1e -> 0x1401450d0`
- `sub=0x81 -> 0x140145230`
- `sub=0x3c -> 0x140145460`
- `sub=0xa8 -> 0x140146ec0`
- `sub=0xbf -> 0x140147010`
- `sub=0xc0 -> 0x140147160`
- `sub=0x01 -> 0x140147410`
- `sub=0x4f -> 0x140147550`

说明 `0xed` 路径在 Windows 内部并非单一处理函数，而是按 `sub_key` 路由。

## 3) 入队/等待模型（不是单次 doorbell）

`FUN_1400cd2a8` 组包后调用 `FUN_1400c8340`，后者再调用 `FUN_1400d316c` 存储命令上下文并驱动发送：

- 命令槽位数组步长 `0x60`，槽位数 `0x14`（20）
- 每槽位跟踪：CID、token、done/event、错误码、回包序号等
- 事件等待通过 `FUN_1400c9810`（超时路径和重试路径都在这里）

含义：Windows 是“命令对象 + 队列槽位 + 事件等待”的完整协议栈，不是“只要 DIDX 前进就算成功”。

## 4) 与当前 Linux PoC 行为的直接对应

结合你当前日志：

- `ring15` 在前几个 MCU 命令上可被消费（`cpu_idx`/`dma_idx` 前进）
- `ring16` 在 FW scatter 阶段常不消费
- `HOST_INT_STA` 常为 `0x00000000`，`MCU_CMD` 也无有效反馈

这与 Windows 反汇编结论一致：

- scatter 阶段缺少 Windows 要求的特殊头/状态组织，设备会静默不接收或不回事件；
- 即使某次 DMA probe 可消费，也不能代表 FW 下载协议已成立。

## 5) 对下一步改造的约束（代码层）

1. `mt7927_mcu_send_scatter()` 不能继续“裸 chunk + ring16”。
2. scatter 必须走与普通 MCU 命令一致的封装通道，并补齐 `CID=0xee` 特殊头字段（至少包含 `0xa000` 分支行为）。
3. `0x0d/0x10/0x17` 必须绑定到统一 token/事件模型；仅看 DIDX 不足以判定成功。
4. 在未完成 2/3 前，`FW_STATUS` 长期停在错误态是预期现象。

---

## 附：本次新增逆向产物

- `docs/win_v5705275_mcu_send_core.md`
- `docs/win_v5705275_mcu_send_backends.md`
- `docs/win_v5705275_mcu_dma_submit.md`
- `docs/win_v5705275_dma_enqueue.md`
- `docs/win_v5705275_dma_lowlevel.md`

这些文件对应发送入口、后端、命令封装、队列入队、低层等待五层调用链。
