# MT7927 驱动开发进展 - 2026-02-14 会话 v2（WFDMA vendor 对齐）

## 当前状态简述

**TX DMA 完全正常，RX DMA 仍无事件响应。** 本轮已将 WFDMA 初始化序列与 vendor mt6639 驱动完全对齐（prefetch、GLO_CFG、DMA 使能顺序、RX pause threshold），但 MCU 仍不向任何 RX Ring 写入事件。

## 本轮修改（在上一轮基础上）

### 1. Prefetch EXT_CTRL 完全匹配 vendor mt6639WfdmaManualPrefetch ✓

旧值（任意分配）：
- TX15: PREFETCH(0x0500, 0x4)
- TX16: PREFETCH(0x0540, 0x4)
- RX6:  PREFETCH(0x0080, 0x4)

新值（vendor 顺序分配）：
| Ring | PREFETCH(base, depth) | 说明 |
|------|-----------------------|------|
| RX4  | (0x0000, 0x8)         | 数据接收 |
| RX5  | (0x0080, 0x8)         | 数据接收 |
| RX6  | (0x0100, 0x8)         | **事件环 - depth 从 4→8** |
| RX7  | (0x0180, 0x4)         | 状态 |
| TX16 | (0x01C0, 0x4)         | FWDL |
| TX0  | (0x0200, 0x10)        | 数据发送 |
| TX1  | (0x0300, 0x10)        | 数据发送 |
| TX2  | (0x0400, 0x4)         | 命令 |
| TX3  | (0x0440, 0x4)         | 命令 |
| TX15 | (0x0480, 0x4)         | MCU WM |

### 2. GLO_CFG 添加 BIT(20) csr_lbk_rx_q_sel_en ✓

vendor 无条件设置此位，控制 RX 队列选择。
- Phase1: 0x5430ba70（无 DMA_EN）
- Phase2: 0x5430ba75（加 DMA_EN）→ readback 0x5430ba7d（BIT3 RX_DMA_BUSY 硬件置位）

### 3. 两阶段 DMA 使能（vendor 序列）✓

旧：一次性写入 GLO_CFG（含 DMA_EN），然后配置中断。
新：
1. Phase 2a: GLO_CFG **不含** TX_DMA_EN/RX_DMA_EN
2. 配置 EXT1、中断、MSI、RX pause threshold
3. Phase 2b: 读改写 GLO_CFG 添加 TX_DMA_EN | RX_DMA_EN

### 4. RX Pause Threshold 写入 ✓

vendor `mt6639ConfigWfdmaRxRingThreshold()` 为所有 RX ring 对写入阈值：
- WFDMA0+0x260 ~ 0x274：全部写 0x00020002（threshold=2）

## 最新测试结果

```
glo_cfg_phase1_no_dma_en: val=0x5430ba70         ← 无 DMA_EN
rx6_ext: val=0x01000008 rb=0x01000008             ← 匹配 vendor
RX pause thresholds: TH10=0x00020002 TH76=0x00020002  ← 正确
glo_cfg_phase2_dma_en: val=0x5430ba75 rb=0x5430ba7d   ← DMA_EN 生效

q16 consumed (cidx=0x1 didx=0x1)                  ← TX FWDL 正常
q15 CIDX=0x1 DIDX=0x1                             ← TX MCU 正常
HOST_INT_STA=0x06000000                            ← TX done 中断正常

mcu-evt timeout: q6 tail=0 cidx=0x7f didx=0x0     ← 仍无 RX 事件
ALL RX rings 0-7: DIDX=0x00000000                  ← MCU 未向任何 Ring 写入
Patch download failed: -110
```

## 已排除的可能原因

| 假设 | 测试方式 | 结论 |
|------|---------|------|
| Prefetch depth/base 错误 | 完全匹配 vendor 值 | ✗ 不是原因 |
| GLO_CFG 缺 BIT(20) | 已添加 | ✗ 不是原因 |
| DMA 使能时序错误 | 两阶段使能 | ✗ 不是原因 |
| RX pause threshold 缺失 | 已写入 | ✗ 不是原因 |
| MCIF remap 地址错 | 已修正为 0xd1034 | ✗ 不是原因 |
| MCU ownership 未设置 | 已写 0x1f5034 | ✗ 不是原因 |
| cbinfra PCIe remap 缺失 | 已配置 | ✗ 不是原因 |
| DMASHDL 阻塞 | BIT(9) bypass | ✗ 不是原因 |
| WFDMA 逻辑残留 | 逻辑复位 | ✗ 不是原因 |
| RX ring 编号错（0 vs 6）| 两者都测试过 | ✗ 不是原因 |
| HOST2MCU 软中断缺失 | 每次 kick 后写 BIT(0) | ✗ 不是原因 |

## 剩余可能原因（按优先级排序）

### 1. 固件不兼容（优先级最高）
当前使用 MT7925 固件（`WIFI_MT7925_PATCH_MCU_1_1_hdr.bin`），但硬件是 MT6639。
- vendor 路径：`mediatek/mt6639/WIFI_MT6639_PATCH_MCU_*`
- MT7925 固件可能无法正确解析 CONNAC3X 的 DMA 描述符格式
- MCU 消费了 TX 描述符（WFDMA 硬件层面），但固件可能不理解命令内容

### 2. TXD DW1 BIT(31) 与固件的交互
- 我们设置 BIT(31)（LONG_FORMAT/FIXED_RATE），vendor 不设置
- 如果固件将此位解析为不同含义，可能导致命令被静默丢弃

### 3. PCIE2AP_REMAP_2 配置
- BAR0 0xFE068 的值未检查
- 可能影响 MCU 的 DMA 写回地址映射

### 4. MCU ROM bootloader 与 vendor 固件的差异
- ROM bootloader 可能需要特定的初始化握手
- vendor 在 MCU_IDLE 后可能有额外的寄存器配置步骤

## 下一步计划

1. **尝试 MT6639 固件**（最可能解决问题）
   ```bash
   insmod mt7927_init_dma.ko \
     fw_patch=mediatek/mt6639/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin \
     fw_ram=mediatek/mt6639/WIFI_RAM_CODE_MT6639_2_1.bin
   ```

2. **添加 DW1 BIT(31) 开关** — 测试不设置 BIT(31) 的行为

3. **读取 PCIE2AP_REMAP_2** (0xFE068) 诊断值

4. **Ghidra 逆向 Windows 驱动** — 找 MCU 命令处理和响应生成的完整路径

## 技术汇总表

| 项目 | 状态 | 值 |
|------|------|-----|
| RX Ring 6 Prefetch | ✓ 匹配 vendor | PREFETCH(0x0100, 0x8) |
| GLO_CFG BIT(20) | ✓ 已设置 | csr_lbk_rx_q_sel_en |
| 两阶段 DMA 使能 | ✓ 已实现 | Phase1→config→Phase2 |
| RX Pause Threshold | ✓ 已写入 | 0x00020002 (all pairs) |
| MCIF remap (0xd1034) | ✓ 正确 | 0x18051803 |
| MCU ownership (0x1f5034) | ✓ 已设置 | BIT(0) |
| cbinfra remap | ✓ 持久化 | 0x74037001 / 0x70007000 |
| GLO_CFG 最终值 | ✓ | write 0x5430ba75, rb 0x5430ba7d |
| TX Ring 15/16 | ✓ 工作正常 | CIDX=DIDX |
| **RX 事件** | **✗ 全部 DIDX=0** | **核心问题未解决** |
| 固件 | ✗ 使用 MT7925 | 需尝试 MT6639 |
