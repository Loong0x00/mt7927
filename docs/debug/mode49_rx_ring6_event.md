# Mode 49: HOST RX Ring 6 FWDL Phase Event 分析

**日期**: 2026-02-15
**分支**: `experiment/mode43-vendor-order`

## 背景

Mode 48 发现 HOST RX ring 6 有未消费的事件（CIDX=0x0b, DIDX=0x0c），Mode 49 实现读取该事件的内容。

## 实现细节

Mode 49 在 Phase 6a2（FWDL 完成后、发送 NIC_CAP 前）：
1. 检查 HOST RX ring 6 的 CIDX 和 DIDX
2. 读取未消费的 descriptor 和数据 buffer
3. Hex dump 前 128 字节数据
4. 更新 CIDX 消费该事件
5. 然后发送 NIC_CAP

## 关键发现

### RX Ring 6 状态
```
RX6 BASE=0x0b0ce000 CNT=0x80 CIDX=0xb DIDX=0xc (ring tail=12)
Found 1 pending event(s) in RX6 (DIDX=0xc > CIDX=0xb)
```

### 事件 Descriptor[11]
```
buf0=0x0b0da000 ctrl=0x08000000 buf1=0x00000000 info=0x00000000
DMA_DONE=0 pkt_len=2048
```

**异常点**: `DMA_DONE=0` — 但 DIDX 已前进，说明 FW 已写入数据

### 事件数据内容 (前 128 字节)
```
00000000: 2c 00 00 38 00 00 00 00 00 00 00 00 00 00 00 00
00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000020: 0c 00 07 00 01 0d 00 00 00 00 00 00 00 00 00 00
00000030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000040: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000060: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000070: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### 数据解析

#### 头部 (0x00-0x0f)
- `0x00: 2c 00 00 38` = 0x3800002c
  - **长度**: 0x002c (44 字节)
  - **标志**: 0x38
- `0x04-0x0f`: 全零

#### 有效数据 (0x20-0x2f)
- `0x20: 0c 00 07 00 01 0d 00 00`
  - `0x0c 0x00` = 12 (可能是序列号或长度)
  - `0x07 0x00` = 7 (可能是事件 ID)
  - `0x01 0x0d` = 0x0d01 = 3329 (可能是事件参数)

## 对比：正常 MCU 事件格式

从 Mode 40+ 的日志中，正常的 MCU 事件 (FWDL 阶段) 格式：
```
mcu-evt: q6 idx=10 ctrl=0xc02c0000
Event data:
  00000000: 2c 00 00 38 00 00 00 00 00 00 00 00 00 00 00 00
  ...
  00000020: 0c 00 07 00 01 0d 00 00 00 00 00 00 00 00 00 00
```

**一致性**: Mode 49 读取的事件数据与之前 Mode 40+ 观察到的 FWDL 事件完全一致！

## 问题分析

### 为什么 DMA_DONE=0？

可能的原因：
1. **FW 不设置 DMA_DONE 位** — MT6639 的 FW 可能只更新 DIDX，不修改 descriptor ctrl 字段
2. **Descriptor 格式不同** — CONNAC3 的 RX descriptor 格式可能与我们假设的不同
3. **需要 driver 清除** — DMA_DONE 可能是 driver 写入的确认位，不是 HW 自动设置的

### 这是什么事件？

根据数据模式：
- **事件 ID 0x07** 可能是 FW 启动相关事件
- **参数 0x0d01** 未知含义
- **长度 0x2c (44 字节)** 包含 header + payload

## 后续实验建议

### Mode 50: 解析事件内容
实现完整的 MCU 事件解析器：
- 识别事件类型 (0x07)
- 解析事件参数
- 查看是否有更多未消费的事件

### Mode 51: 先消费事件再发 NIC_CAP
测试消费这个事件后，NIC_CAP 的行为是否改变：
- 在 Phase 6a2 消费所有 RX6 事件
- 然后清除 INT_STA
- 再发送 NIC_CAP

### Mode 52: 对比 Windows 事件处理
使用 Ghidra 分析 Windows 驱动如何处理 FWDL 后的第一个 MCU 事件。

## 状态

- **MCU_RX0 BASE**: 仍然是 0x00000000 (BLOCKER)
- **NIC_CAP 结果**: -110 (ETIMEDOUT)
- **fw_sync**: 0x00000003 (FW 已启动)

## 结论

Mode 49 成功读取了 FWDL 阶段遗留的 HOST RX ring 6 事件。这个事件可能是：
- FW 启动完成通知
- FW 初始化状态报告
- 需要 driver ACK 的关键事件

**下一步**: 实现 Mode 50 来正确解析和处理这个事件，可能是解锁 MCU_RX0 配置的关键。
