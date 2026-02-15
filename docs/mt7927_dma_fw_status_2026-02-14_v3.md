# MT7927 驱动开发进展 - 2026-02-14 v3（MCU DMA RX 路由调查）

## 当前状态简述

**TX DMA 完全正常，MCU 从不写任何 RX 事件。** 经过多轮调试，已确认问题不在 HOST 侧 DMA 配置，而在 WFDMA 无法将 HOST TX 数据投递到 MCU DMA RX ring。

## 关键诊断数据（最新）

```
# HOST 侧 - 工作正常
HOST TX15: CIDX=1 DIDX=1  (MCU CMD consumed)
HOST TX16: CIDX=1 DIDX=1  (FWDL consumed)
HOST_INT_STA = 0x06000000  (TX done for ring 15+16)

# MCU 侧 - 关键线索
MCU_DMA0 GLO_CFG = 0x10703875 (TX_EN=1, RX_EN=1)
MCU_DMA0 INT_ENA = 0x00000000

MCU_DMA0 RX0: BASE=0x00000000 CNT=0x200 CIDX=0x000 DIDX=0 ← 未配置, CIDX=0(无空间!)
MCU_DMA0 RX1: BASE=0x00000000 CNT=0x200 CIDX=0x000 DIDX=0 ← 未配置, CIDX=0(无空间!)
MCU_DMA0 RX2: BASE=0x0226ca00 CNT=0x028 CIDX=0x027 DIDX=0 ← ROM配置, 有空间
MCU_DMA0 RX3: BASE=0x0226cc80 CNT=0x028 CIDX=0x027 DIDX=0 ← ROM配置, 有空间
MCU_DMA0 TX0-TX3: ALL BASE=0x00000000  ← MCU无法发送事件回HOST

# MCU 侧 Prefetch
MCU_DMA0 RX_EXT_CTRL: RX0=0x00000000 RX1=0x00000000 RX2=0x00200002 RX3=0x00400002
MCU_DMA0 TX_EXT_CTRL: TX0=0x00000000 TX1=0x00000000 TX2=0x00000000 TX3=0x00000000

# WFDMA 其他状态
MCU_WRAP GLO = 0x00000000 (disabled)
WFSYS_SW_RST = 0x00000011 (RST_B=1, INIT_DONE=1)
HOST_WFDMA BUSY_ENA = 0x0000001f
HIF_MISC = 0x001c2000
HIF_BUSY = 0x80000002
WFDMA_DUMMY_CR = 0xffff0002 (NEED_REINIT=1)
EMI_CTL = 0x00000002 (slpprot=1)
fw_sync = 0x00000001 (ROM ready)
ROMCODE_INDEX = 0x00001d1e (MCU idle)
MCU_INT_STA = 0x00000001 (HOST2MCU SW int delivered)
```

## 核心问题分析

### 问题链
1. HOST TX Ring 15/16 描述符被 WFDMA 消费 ✓
2. WFDMA 应将数据投递到 MCU DMA RX ring → **失败，所有 MCU RX DIDX=0**
3. MCU 无数据可处理 → 不生成事件
4. MCU DMA TX ALL BASE=0 → MCU 即使要响应也无法写回
5. HOST RX Ring 事件 DIDX=0 → 超时 -110

### 新发现：MCU DMA RX0/RX1 CIDX=0 问题
在 DMA ring 语义中：
- CIDX = 消费者已处理到的位置
- 可用空间 = (CIDX - DIDX) mod CNT
- CIDX=0, DIDX=0 → 可用空间=0 → **WFDMA 无法写入！**
- CIDX=CNT-1, DIDX=0 → 可用空间=CNT-1 → 大量空间

ROM bootloader：
- RX2/RX3: CIDX=0x27=39(CNT-1), 有 39 个空闲槽位
- RX0/RX1: CIDX=0, **没有空闲槽位！**

如果 WFDMA 固定路由 HOST TX15 → MCU RX0（或 HOST TX16 → MCU RX1），
则即使 CIDX=0 也无法写入 → 数据丢失。

## 已排除的可能原因（全部会话累计）

| # | 假设 | 测试方式 | 结论 |
|---|------|---------|------|
| 1 | Prefetch depth/base 错误 | 完全匹配 vendor 值 | ✗ |
| 2 | GLO_CFG 缺 BIT(20) | 已添加 csr_lbk_rx_q_sel_en | ✗ |
| 3 | DMA 使能时序错误 | 两阶段使能 | ✗ |
| 4 | RX pause threshold 缺失 | 已写入 0x00020002 | ✗ |
| 5 | MCIF remap 地址错 | 修正为 0xd1034=0x18051803 | ✗ |
| 6 | MCU ownership 未设 | 已写 0x1f5034 BIT(0) | ✗ |
| 7 | cbinfra remap 缺失 | 已配置 0x74037001 | ✗ |
| 8 | DMASHDL 阻塞 | BIT(28) bypass ON | ✗ |
| 9 | WFDMA 逻辑残留 | 逻辑复位 GLO_CFG toggle | ✗ |
| 10 | RX ring 编号(0 vs 6) | 都试过 evt_ring_qid=0/6 | ✗ |
| 11 | HOST2MCU 软中断缺失 | 每次 kick 后写 BIT(0) | ✗ |
| 12 | EXT0 寄存器值错 | 试过写入/跳过 | ✗ |
| 13 | HOST_CONFIG PDMA_BAND BIT(0) | 只用于 multi-HIF | ✗ |
| 14 | WFDMA_DUMMY_CR NEED_REINIT 缺失 | 已设 BIT(1) | ✗ (仅用于固件非 ROM) |
| 15 | EMI sleep protection 缺失 | 已通过 L1 remap 设 BIT(1) | ✗ |
| 16 | 需要 WFSYS reset 而非 CB_INFRA_RGU | 测试 0xf0140 WFSYS reset | ✗ |
| 17 | 需要 SET_OWN power cycle | SET_OWN→CLR_OWN 序列 | ✗ |
| 18 | TXD Q_IDX=0 路由到未配置的 MCU RX0 | 改 Q_IDX=2 路由到 RX2 | ✗ MCU RX2 DIDX 仍=0 |

## 当前待测假设

### 假设 A：MCU DMA RX0/RX1 CIDX=0 导致无空间（优先级最高）
- Q_IDX 可能与 MCU RX ring 无直接映射关系
- WFDMA 可能按固定规则路由到 RX0/RX1
- RX0/RX1 CIDX=0 = 无可用空间 → WFDMA 无法写入
- **测试方案**：从 HOST 侧写 MCU DMA RX0 CIDX=0x1FF, RX1 CIDX=0x1FF

### 假设 B：MCU DMA TX ring 未配置导致 MCU 无法回写事件
- MCU DMA TX0-TX3 全部 BASE=0
- 即使 MCU 收到命令，也无法通过 DMA TX 写回事件到 HOST RX ring
- ROM bootloader 可能在收到第一个命令后才配置 TX ring？
- **观察**：需先解决假设 A，看 MCU 是否开始响应

### 假设 C：MCU_WRAP GLO=0 (disabled) 阻断了数据流
- MCU WFDMA wrap 未启用
- 可能需要启用才能连通 HOST↔MCU 的数据通道

## 模块参数当前值

| 参数 | 默认值 | 说明 |
|------|--------|------|
| evt_ring_qid | 6 | HOST RX ring for MCU events (CONNAC3 vendor=6, upstream=0) |
| mcu_rx_qidx | 2 | TXD Q_IDX for CONNAC3 (vendor=0, testing=2) |
| skip_ext0 | 1 | Skip EXT0 write (vendor only writes with HOST_OFFLOAD) |
| skip_logic_rst | 0 | Skip WFDMA logic reset |
| use_wfsys_reset | 0 | Use upstream WFSYS reset vs CB_INFRA_RGU |
| use_emi_slpprot | 1 | Enable EMI sleep protection |

## 技术架构理解

```
HOST CPU                      WFDMA                        MCU
  │                             │                           │
  ├─→ HOST TX Ring 15 ──→ WFDMA fetches ──→ MCU DMA RX? ──→ MCU processes
  ├─→ HOST TX Ring 16 ──→ WFDMA fetches ──→ MCU DMA RX? ──→ MCU processes
  │                             │                           │
  ←── HOST RX Ring 6 ←── WFDMA writes? ←── MCU DMA TX? ←── MCU responds
```

**已确认工作**：HOST TX → WFDMA fetches (HOST_INT_STA TX done)
**未确认**：WFDMA → MCU DMA RX 投递（所有 MCU RX DIDX=0）
**未确认**：MCU DMA TX → WFDMA → HOST RX（MCU TX BASE=0）
