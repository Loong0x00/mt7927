# MT7927 驱动开发进展 - 2026-02-14 v4（AXI 睡眠保护深度调查）

## 当前状态简述

**根因已精确定位：WFDMA 内部 R2A AXI 桥接路径处于睡眠状态，阻断了 HOST TX → MCU DMA RX 的数据投递。**

HOST TX DMA 完全正常（WFDMA 消费描述符、MIB 计数器确认），但数据卡在 WFDMA 内部的 txfifo1 中，R2A 桥尝试向 MCU 地址 0x02aa0000 写入但被 AXI 睡眠保护阻断。

## 关键诊断数据（最新 - 含 R2A/MIB/SLPPROT 完整输出）

```
# ===== HOST 侧 - 全部正常 =====
HOST TX15: CIDX=1 DIDX=1  (MCU CMD consumed) ✓
HOST TX16: CIDX=1 DIDX=1  (FWDL consumed) ✓
HOST_INT_STA = 0x06000000  (TX done for ring 15+16) ✓
MIB TX15: DMAD=1 PKT=1 ✓
MIB TX16: DMAD=1 PKT=1 ✓
MIB RX0-7: ALL DMAD=0 PKT=0  (WFDMA 从未写入任何 RX) ✗

# ===== R2A 桥接 - 故障点 =====
# dma-ready 基线（TX 前）：
R2A STS=0x00000000  FSM_CMD=0x00000202  FSM_DAT=0x00000000
# timeout 时（TX 后）：
R2A STS=0x00000000
R2A FSM_CMD=0x03030101  (changed! 桥看到了活动但卡住)
R2A FSM_DAT=0x00000303  (changed! 数据路径也卡住)
R2A DMAWR=0x00000000     (零次 DMA 写入完成!)
R2A DMARD=0x00000400     (有读取活动)
R2A WR_DBG_OUT0=0x02aa0000  (目标地址=MCU SRAM 0x02aa0000)
R2A WR_DBG_OUT1=0x00000000
R2A RD_DBG_OUT0=0x00220000  RD_DBG_OUT1=0x00000000
R2A CTRL0=0xffff0c08  CTRL1=0x1ffe7ff8  CTRL2=0x86ffff1f

# ===== AXI 睡眠保护 - 所有路径均在睡眠中 =====
SLP_STS=0x07770313     (dma-ready 和 timeout 完全一致，从未改变!)
SLPPROT_CTRL=0x00000004  (IDLE=1, EN=0)
SLPPROT_VIO_ADDR=0x00000000  (无违规地址记录)

# ===== CONN_INFRA 睡眠保护寄存器 - 已是禁用状态 =====
WF_SLP_CTRL (0x18001620) = 0x00000000  (EN=0, BSY=0) ← 本来就没启用!
WFDMA_SLP_CTRL (0x18001624) = 0x00000000  (EN=0, BSY=0) ← 本来就没启用!
→ 我们的 disable_wfdma_slpprot() 是空操作

# ===== HIF 状态 =====
HIF_BUSY=0x80000002  (conn_hif_busy=1 rxfifo=0 txfifo1=1 txfifo0=0)
→ txfifo1=1: 数据卡在 HOST→MCU 方向的 FIFO 中

# ===== WFDMA Debug Probe =====
WPDMA_DBG[0]=0x00550004
WPDMA_DBG[1]=0x00445420
WPDMA_DBG[2]=0x00550057
WPDMA_DBG[3]=0x00004880
CONN_HIF_DBG_PROBE=0x00004880
DMASHDL_DBG=0x00000000

# ===== MCU 侧 - 未变化 =====
MCU_DMA0 GLO_CFG: dma-ready=0x10703875 → timeout=0x1070387d (RX_BUSY set)
MCU_DMA0 INT_ENA = 0x00000000
MCU_DMA0 RX0: BASE=0x00000000 CNT=0x200 CIDX=0x1ff DIDX=0 (已修CIDX，有空间，但DIDX仍=0)
MCU_DMA0 RX1: BASE=0x00000000 CNT=0x200 CIDX=0x1ff DIDX=0
MCU_DMA0 RX2: BASE=0x0226ca00 CNT=0x028 CIDX=0x027 DIDX=0
MCU_DMA0 RX3: BASE=0x0226cc80 CNT=0x028 CIDX=0x027 DIDX=0
MCU_DMA0 TX0-TX3: ALL BASE=0x00000000
MCU_WRAP GLO = 0x00000000 (disabled, writes don't stick)
fw_sync = 0x00000001 (ROM ready)
ROMCODE_INDEX = 0x00001d1e (MCU idle)
```

## 核心问题分析

### 精确故障定位

```
HOST CPU                    WFDMA                         MCU
  │                           │                            │
  ├─→ HOST TX Ring 15/16 ──→ WFDMA fetches (MIB TX=1) ──→ ┐
  │                           │                            │
  │                      ┌────┤ txfifo1 (DATA STUCK HERE)  │
  │                      │    │                            │
  │                      │    ├─ R2A bridge ──→ AXI bus ──→│ MCU DMA RX
  │                      │    │   FSM stuck     SLP_STS    │ (DIDX=0)
  │                      │    │   DMAWR=0      =0x07770313 │
  │                      │    │   target:0x02aa0000 BLOCKED│
  │                      │    │                            │
  ←── HOST RX Ring 6 ←───┘    │   (never reaches here)     │
      (DIDX=0, timeout)       │                            │
```

### SLP_STS=0x07770313 解码

这是 R2A AXI 桥接的睡眠状态寄存器，反映 WFDMA↔MCU 之间 AXI 总线的多条路径状态：
- **写路径 (高位 0x0777)**: 多个写通道处于睡眠/空闲
- **读路径 (低位 0x0313)**: 多个读通道处于睡眠/空闲
- 该值在整个生命周期中**从未改变** (dma-ready → timeout 一致)
- CONN_INFRA 睡眠保护 (0x18001620/0x18001624) 已禁用，不控制此状态

### 问题本质

SLP_STS 不受 CONN_INFRA 层睡眠保护控制。可能的控制源：
1. **WFSYS 电源域** - MCU 子系统可能未完全上电
2. **MCU 侧 AXI 睡眠保护** - MCU 自身的 AXI slave 端口可能有独立的睡眠保护
3. **GALS (Generic AXI Low-Speed) 桥** - WFDMA↔CONN 的时钟域交叉桥未释放
4. **vendor wf_pwr_on_consys_mcu() 中的其他步骤** - 我们只实现了一小部分

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
| 14 | WFDMA_DUMMY_CR NEED_REINIT 缺失 | 已设 BIT(1) | ✗ |
| 15 | EMI sleep protection 缺失 | 已通过 L1 remap 设 BIT(1) | ✗ |
| 16 | 需要 WFSYS reset 而非 CB_INFRA_RGU | 测试 0xf0140 WFSYS reset | ✗ |
| 17 | 需要 SET_OWN power cycle | SET_OWN→CLR_OWN 序列 | ✗ |
| 18 | TXD Q_IDX 路由到未配置的 MCU RX0 | 改 Q_IDX=2 路由到 RX2 | ✗ |
| 19 | MCU DMA RX0/RX1 CIDX=0 无空间 | 写 CIDX=0x1FF | ✗ DIDX仍=0 |
| 20 | CONN_INFRA WF/WFDMA 睡眠保护 (0x18001620/0x18001624) | 尝试 clear bit[0] | ✗ 本来就=0 |

## 下一步方向

### 方向 A：研究 vendor wf_pwr_on_consys_mcu() 完整序列（优先级最高）
soc3_0.c 中的 `wf_pwr_on_consys_mcu()` 包含完整的 WFSYS 上电序列：
- WFSYS 电源域使能 (CONN_INFRA_WF_PWRON)
- 电压域使能 (CONN_INFRA_WF_VDD)
- MCU 复位序列
- 多层睡眠保护解除
- 总线时钟使能

我们目前只实现了其中的 CONN_INFRA 睡眠保护部分（且发现已禁用）。SLP_STS=0x07770313 可能需要更深层的电源域操作才能改变。

### 方向 B：研究 SLP_STS 各 bit 的含义和控制寄存器
SLP_STS 是只读状态寄存器。需要找到控制这些 AXI 路径从睡眠唤醒的具体寄存器。

### 方向 C：研究 Windows 驱动的完整初始化路径
Windows 驱动在 fw_sync=0 时执行 FUN_140058eb8（完整初始化），fw_sync=1 时跳过。我们的 fw_sync=1 说明 ROM 已就绪，但可能遗漏了首次上电时的某些步骤。

## 模块参数当前值

| 参数 | 默认值 | 说明 |
|------|--------|------|
| evt_ring_qid | 6 | HOST RX ring for MCU events |
| mcu_rx_qidx | 0 | TXD Q_IDX for CONNAC3 |
| skip_ext0 | 1 | Skip EXT0 write |
| skip_logic_rst | 0 | Skip WFDMA logic reset |
| use_wfsys_reset | 0 | Use upstream WFSYS reset |
| use_emi_slpprot | 1 | Enable EMI sleep protection |
| fix_mcu_rx_cidx | 1 | Fix MCU DMA RX0/RX1 CIDX |
| clean_glo_cfg | 0 | Clean GLO_CFG before write |
| no_long_format | 0 | Disable long format TXD |
| use_upstream_txd | 1 | Use upstream TXD format |
| upstream_prefetch | 0 | Use upstream prefetch values |
| disable_ext0_dmashdl | 1 | Clear DMASHDL bit in EXT0 |
