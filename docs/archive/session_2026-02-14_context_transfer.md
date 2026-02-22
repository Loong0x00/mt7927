# MT7927 驱动开发 - 完整上下文转移文档
# 日期：2026-02-14（跨多个会话，本会话为第11+个）

## 一、项目概述

**目标**：为 MediaTek MT7927 WiFi 7 PCIe 卡开发 Linux 内核驱动。
**PCI ID**：`14c3:6639`（内部芯片名 MT6639，CONNAC3X 架构）
**单一驱动文件**：`/home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.c`（约2900行）
**构建命令**：`make tests`
**sudo密码**：`123456`
**YOLO模式**：已启用（不需要确认即可执行命令）

**参考代码**（只读）：
- `/home/user/mt7927/mt76/mt7925/` — 上游 mt7925 Linux 驱动（工作代码参考）
- `/home/user/mt7927/mt6639/` — Vendor MediaTek 移动端驱动源码
- `/home/user/mt7927/chips/` — 额外芯片参考代码
- `/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/` — Windows 驱动 Ghidra 逆向
- `/home/user/mt7927/DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/` — 另一版 Windows 驱动

## 二、核心问题 —— 当前卡点（SLP_STS=0x07770313）

### 2.1 精确故障描述

HOST TX DMA 完全正常（WFDMA消费描述符、MIB计数器确认），但数据卡在 WFDMA 内部的 txfifo1 中。R2A（Read-to-Arbitration）AXI 桥尝试向 MCU 地址 0x02aa0000 写入，但被 AXI 睡眠状态阻断。

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

### 2.2 关键诊断数据（最后一次测试输出）

```
# ===== HOST 侧 - 全部正常 =====
HOST TX15: CIDX=1 DIDX=1  (MCU CMD consumed) ✓
HOST TX16: CIDX=1 DIDX=1  (FWDL consumed) ✓
HOST_INT_STA = 0x06000000  (TX done for ring 15+16) ✓
MIB TX15: DMAD=1 PKT=1 ✓
MIB TX16: DMAD=1 PKT=1 ✓
MIB RX0-7: ALL DMAD=0 PKT=0  (WFDMA 从未写入任何 RX) ✗

# ===== R2A 桥接 - 故障点 =====
R2A STS=0x00000000
R2A FSM_CMD=0x03030101  (桥看到了活动但卡住)
R2A FSM_DAT=0x00000303  (数据路径也卡住)
R2A DMAWR=0x00000000     (零次 DMA 写入完成!)
R2A DMARD=0x00000400     (有读取活动)
R2A WR_DBG_OUT0=0x02aa0000  (目标地址=MCU SRAM)
R2A CTRL0=0xffff0c08  CTRL1=0x1ffe7ff8  CTRL2=0x86ffff1f

# ===== AXI 睡眠保护 - 所有路径均在睡眠中 =====
SLP_STS=0x07770313     (从未改变!)
SLPPROT_CTRL=0x00000004  (IDLE=1, EN=0)
HIF_BUSY=0x80000002  (txfifo1=1: 数据卡在 HOST→MCU FIFO)

# ===== CONN_INFRA 睡眠保护 - 已禁用 =====
WF_SLP_CTRL (0x18001620) = 0x00000000  (EN=0, BSY=0)
WFDMA_SLP_CTRL (0x18001624) = 0x00000000  (EN=0, BSY=0)

# ===== MCU 侧 =====
MCU_DMA0 GLO_CFG: 0x1070387d (TX_EN=1, RX_EN=1)
MCU_DMA0 RX0: BASE=0x00000000 CNT=0x200 CIDX=0x1ff DIDX=0
MCU_DMA0 RX2: BASE=0x0226ca00 CNT=0x028 CIDX=0x027 DIDX=0
MCU_DMA0 TX0-TX3: ALL BASE=0x00000000
fw_sync = 0x00000001 (ROM ready)
ROMCODE_INDEX = 0x00001d1e (MCU idle)
```

### 2.3 SLP_STS=0x07770313 解码

这是 R2A AXI 桥接的**内部**睡眠状态寄存器（BAR0 0xd7520），反映 WFDMA↔MCU 之间 AXI 总线的多条路径状态：
- 写路径 (高位 0x0777): 多个写通道处于睡眠/空闲
- 读路径 (低位 0x0313): 多个读通道处于睡眠/空闲
- 该值在整个生命周期中**从未改变**

**注意区分**：
- SLP_STS (0xd7520) = R2A 桥内部 AXI 睡眠状态（只读）← 这是问题所在
- SLPPROT_CTRL (0xd7050) = WFDMA 侧睡眠保护控制（可写）
- WF_SLP_CTRL (0x18001620) = CONN_INFRA 层 WF 睡眠保护（已禁用）
- WFDMA_SLP_CTRL (0x18001624) = CONN_INFRA 层 WFDMA 睡眠保护（已禁用）

## 三、本会话的关键发现

### 3.1 ★★★ 时序发现（最重要的发现）★★★

通过对比**首次加载**和**后续加载**的 dmesg 输出，发现了极其关键的时序问题：

**首次加载（clean state, timestamp 63896）：**
```
[63896.536243] bridge[post-wfsys-rst]: SLP_STS=0x00000000 CTRL0=00000000  ← WFSYS reset 后干净!
... 约2ms后 ...
[63896.538231] slpprot: WF_SLP_CTRL before=0x00000001 (EN=1 BSY=0)  ← 睡眠保护已重新启用!
[63896.538288] slpprot: SLP_STS=0x07770313  ← 又回到睡眠状态!
```

**重要发现**：
1. WFSYS reset **确实**将 SLP_STS 清零为 0x00000000
2. 但在约2ms内，SLP_STS 又回到 0x07770313
3. 原因：WFSYS reset 后，CONN_INFRA 睡眠保护默认恢复为 EN=1（启用）
4. MCU ROM 启动后发现 R2A 桥被睡眠保护阻断，放弃初始化，进入 idle (0x1D1E)
5. 我们之后虽然禁用了睡眠保护，但为时已晚——ROM 已经不再尝试

**后续加载（stale state, timestamp 66012）：**
```
[66012.042499] bridge[post-wfsys-rst]: SLP_STS=0x07770313 CTRL0=ffff0c08  ← WFSYS reset 没有清除!
```
WFSYS reset 甚至无法清除残留状态（来自上一次加载的 DMA 活动残留）。

### 3.2 Vendor 初始化序列对比

**Vendor soc3_0.c wf_pwr_on_consys_mcu() 完整序列：**
```
1.  Wakeup CONN_INFRA: 0x180601A4[0]=1
2.  Check AP2CONN slpprot: Poll 0x18060184[5]=0
3.  Check CONNSYS version: Poll 0x18001000 = 0x20010000
4.  ★ Assert CPU reset: 0x18000010[0]=0   ← 我们没做！关键！
5.  Bus clock ctrl (conninfra API)
6.  Turn on WFSYS power: 0x18000000 magic write (仅移动端)
7.  Poll WFSYS RGU status: 0x180602CC[2]=1
8.  Disable WF sleep protect: 0x18001620[0]=0, poll [3]=0
9.  Disable WFDMA sleep protect: 0x18001624[0]=0, poll [3]=0
10. ★ Enable VDNR: 0x1800E06C[0]=1   ← 我们没做！
11. Check WFSYS version
12. Setup EMI remap
13. ★ Reset WFSYS semaphore: 0x18000018[0]=0
14. ★ De-assert CPU reset: 0x18000010[0]=1   ← 我们没做！关键！
15. Poll ROMCODE_INDEX = 0x1D1E
16. conninfra_config_setup()
17. Disable bus clock
18. Disable CONN_INFRA wakeup
```

**Vendor 的关键设计**：先 Hold CPU (步骤4)，配置总线（步骤8-13），再 Release CPU (步骤14)。
这样 ROM 启动时总线已经就绪，R2A 桥不会被睡眠保护阻断。

**我们的问题**：WFSYS reset (0xf0140 toggle) 后 CPU **立即启动**，我们来不及配置总线。

### 3.3 Vendor 寄存器在 PCIe 上的可访问性

| 寄存器 | Vendor 地址 | BAR0 偏移 | 读取值 | 状态 |
|--------|------------|-----------|--------|------|
| AP2CONN_SLP_PROT_ACK | 0x18060184 | 0xe0184 | 0x00000000 (bit5=0) | ✓ 正常 |
| CONN_HW_VER | 0x18001000 | 0xf1000 | 0x80000000 | ✗ 不匹配 (期望 0x20010000) |
| CPU_SW_RST | 0x18000010 | 0xf0010 | 0x000001d2 | ✗ 不像重置控制器 |
| WFSYS_PWR_STS | 0x180602CC | 0xe02CC | 0x00000000 (bit2=0) | ✗ 不显示已上电 |
| VDNR | 0x1800E06C | 0xfE06C | 0x87654321 | ✗ 测试模式/死值 |
| WFSYS_SEMA | 0x18000018 | 0xf0018 | 0x00000000 | ? 可能正确 |

**结论**：Vendor 的 CONN_INFRA_RGU 寄存器 (0x18000010 CPU_SW_RST) 在 PCIe 上不工作。
VDNR (0xfE06C) 返回 0x87654321 明显是死区。
我们**无法**通过 Hold/Release CPU 的方式来序列化初始化。

### 3.4 CLR_OWN 不工作

**现象**：写 BIT(1) 到 LPCTL (BAR0 0xe0010) 后，BIT(2) OWN_SYNC 保持为 1，永远不清除。
**验证**：
- 地址正确（与 vendor + upstream 完全一致：0x7c060010）
- BIT(2) 是正确的同步位
- PCIe ASPM 已禁用
- 多次重试（10次，每次2ms）均失败
- WFSYS reset 硬件清除 LPCTL 到 0（但不是 CLR_OWN 机制）

**影响**：无法执行 upstream mt7925 的 post-DMA-init power cycle (SET_OWN → CLR_OWN)，该步骤用于触发 MCU ROM 处理 NEED_REINIT 并重新初始化 R2A 桥。

## 四、已排除的所有假设（26+个，跨所有会话）

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
| 17 | 需要 SET_OWN power cycle | SET_OWN→CLR_OWN 序列 | ✗ CLR_OWN失败 |
| 18 | TXD Q_IDX 路由到未配置的 MCU RX0 | 改 Q_IDX=2 路由到 RX2 | ✗ |
| 19 | MCU DMA RX0/RX1 CIDX=0 无空间 | 写 CIDX=0x1FF | ✗ DIDX仍=0 |
| 20 | CONN_INFRA WF/WFDMA 睡眠保护 | 尝试 clear bit[0] | ✗ 本来就=0 |
| 21 | R2A CTRL0 CLKGATE_BYP/BUFRDY_BYP | 设置 BIT(12)+BIT(13) | ✗ SLP_STS不变 |
| 22 | 早期 DMA init 竞争 MCU ROM | DMA init 11ms vs ROM 40us | ✗ |
| 23 | 早期 NEED_REINIT 写入 | ROM 启动时覆盖 | ✗ |
| 24 | WFSYS power domain via L1 remap | PCIe 变体不适用 | ✗ |
| 25 | Post-DMA-init power cycle | CLR_OWN 在 MT7927 上不工作 | ✗ |
| 26 | VDNR enable (0xfE06C) | 返回 0x87654321，不是真实寄存器 | ✗ |
| 27 | Vendor AP2CONN slpprot check | 0xe0184 bit5=0，不阻塞 | ✗ |

## 五、当前代码状态和正在进行的修改

### 5.1 当前 probe 流程

```
1. SET_OWN → CLR_OWN (CLR_OWN 总是失败, LPCTL 保持 0x4)
2. mt7927_mcu_init_mt6639():
   a. CONNINFRA wakeup (MT_WAKEPU_TOP=1)
   b. cbinfra PCIe remap
   c. EMI sleep protection
   d. [NEW] Vendor 诊断读取 (AP2CONN_ACK, CONN_HW_VER, CPU_RST, VDNR, SEMA)
   e. [NEW] VDNR enable attempt (无效, 0x87654321)
   f. [NEW] WFSYS semaphore reset
   g. [NEW] pre-wfsys-rst bridge dump
   h. WFSYS reset (0xf0140 toggle → INIT_DONE)
   i. [NEW] FAST sleep protection disable (紧接 INIT_DONE 后)
   j. post-wfsys-rst bridge dump
3. mt7927_disable_wfdma_slpprot() (L1 remap 方式)
4. ROMCODE polling (MCU ROM idle 0x1D1E)
5. MCU ownership + MCIF remap + clear CONNINFRA wakeup
6. mt7927_drv_own() (NO-OP: LPCTL already 0x0 from reset)
7. DMA init (logic reset enabled, NEED_REINIT set)
8. Post-DMA-init power cycle (SET_OWN→CLR_OWN FAILS)
9. mt7927_mcu_fw_download() (timeout -110)
```

### 5.2 本会话已做的代码修改

**1. 添加了 Vendor 寄存器定义（line 269-310）：**
```c
#define MT_AP2CONN_SLP_PROT_ACK          0xe0184   // BAR0
#define MT_AP2CONN_SLP_PROT_ACK_BIT      BIT(5)
#define MT_WFSYS_PWR_STS                 0xe02CC
#define MT_WFSYS_PWR_STS_RDY             BIT(2)
#define MT_CONN_HW_VER_DIRECT            0xf1000
#define MT_CONN_HW_VER_EXPECTED          0x20010000
#define MT_CPU_SW_RST                    0xf0010
#define MT_CPU_SW_RST_B_BIT              BIT(0)
#define MT_CONN_INFRA_VDNR               0xfE06C
#define MT_CONN_INFRA_VDNR_EN            BIT(0)
#define MT_WFSYS_SEMA_RST                0xf0018
```

**2. 在 mcu_init 中添加 vendor 诊断和初始化步骤（WFSYS reset 之前）：**
- Step 2c-1: 读取 AP2CONN_SLP_PROT_ACK → 0x00000000 (OK)
- Step 2c-2: 读取 CONN_HW_VER → 0x80000000 (不匹配)
- Step 2c-3: 读取 CPU_SW_RST, PWR_STS, SEMA, VDNR（诊断）
- Step 2c-4: 尝试 VDNR enable → 0x87654321（不可用）
- Step 2c-5: WFSYS semaphore reset

**3. WFSYS INIT_DONE 后立即禁用睡眠保护（正在测试）：**
```c
/* CRITICAL: Disable sleep protection IMMEDIATELY after INIT_DONE */
slp_val = mt7927_rr_l1(dev, CONN_INFRA_WF_SLP_CTRL_R);
if (slp_val & CONN_INFRA_SLP_CTRL_EN) {
    slp_val |= CONN_INFRA_SLP_CTRL_EN; /* W1C: write 1 to clear */
    mt7927_wr_l1(dev, CONN_INFRA_WF_SLP_CTRL_R, slp_val);
}
// ... same for WFDMA_SLP_CTRL ...
```

**4. 在 disable_wfdma_slpprot() 中添加 VDNR 检查/重启：**
- 读取 VDNR 状态，如果未启用则重新启用（无效因 0x87654321）
- 添加 post-vdnr bridge dump

**5. 在 remove 中添加 WFSYS reset（正在编辑，尚未测试）：**
```c
/* 在 remove 中先禁用 DMA，再 WFSYS reset，清除残留状态 */
val &= ~(TX_DMA_EN | RX_DMA_EN);
mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
// WFSYS reset toggle
```

**6. 计划添加但尚未编辑的：PCIe FLR 在 probe 开始时**

### 5.3 当前模块参数默认值

| 参数 | 默认值 | 说明 |
|------|--------|------|
| use_mt6639_init | true | 使用 MT6639 初始化序列 |
| use_wfsys_reset | true | 使用上游 WFSYS reset (0xf0140) |
| force_wf_reset | true | 即使 MCU 已 idle 也强制重置 |
| skip_logic_rst | false | 执行完整 WFDMA 逻辑重置 |
| skip_ext0 | true | 跳过 EXT0 写入 |
| evt_ring_qid | 6 | HOST RX ring 6 for MCU events |
| mcu_rx_qidx | 0 | TXD Q_IDX for CONNAC3 |
| use_emi_slpprot | true | EMI sleep protection |
| fix_mcu_rx_cidx | true | 修正 MCU DMA RX0/RX1 CIDX |
| use_upstream_txd | true | 使用上游 TXD 格式 |
| upstream_prefetch | false | 使用 vendor prefetch |
| disable_ext0_dmashdl | true | 清除 DMASHDL enable |
| r2a_clkgate_byp | false | R2A 时钟门控旁路 |

## 六、下一步方案（按优先级排序）

### 方案 A: 在 WFSYS INIT_DONE 后立即禁用睡眠保护 ★★★ 正在进行 ★★★

**原理**：
- 首次加载时 WFSYS reset 将 SLP_STS 清零为 0x00000000
- 在约2ms内（MCU ROM 启动前），如果我们能禁用 CONN_INFRA 睡眠保护
- ROM 启动时看到睡眠保护已禁用，应能正常初始化 R2A 桥
- 需要配合 **PCIe FLR** 或 **rmmod 时 WFSYS reset** 来确保每次加载都从干净状态开始

**代码已写好**（mcu_init 中 INIT_DONE 后立即调用 L1 remap 禁用睡眠保护），
但还需要：
1. 添加 PCIe FLR 在 probe 开始时（`pci_reset_function(pdev)`）确保干净状态
2. 确认 remove 中的 WFSYS reset 代码正确
3. 重新测试

**关键问题**：L1 remap 操作本身需要时间（读-修改-写 + L1 remap 寄存器设置），可能仍然不够快。如果 INIT_DONE 后 MCU CPU 立即开始运行，2ms 窗口可能太短。

### 方案 B: PCIe FLR (Function Level Reset)

**原理**：
- PCIe FLR 是硬件级复位，应重置设备所有内部状态
- 在 probe 开始时执行 `pci_reset_function(pdev)` 确保干净状态
- 然后配合方案 A 的快速睡眠保护禁用

**代码**：
```c
/* 在 pci_enable_device + pci_set_master 之后，pci_iomap 之前 */
ret = pci_reset_function(pdev);
if (ret)
    dev_warn(&pdev->dev, "PCIe FLR failed: %d\n", ret);
```

### 方案 C: 第二次 WFSYS reset（DMA init + NEED_REINIT 之后）

**原理**：
- WFSYS reset 后 WFDMA_DUMMY_CR 硬件默认值为 0xdead2217（NEED_REINIT=1）
- 如果在 WFSYS reset 之前确保睡眠保护已禁用
- ROM 启动时看到 NEED_REINIT=1 + 睡眠保护已禁用
- 应能正常初始化 WFDMA 和 R2A 桥

**序列**：
1. 第一次 WFSYS reset → 快速禁用睡眠保护 → ROM 启动
2. DMA init → 设置 host side rings
3. 再次禁用睡眠保护（确保恢复后的默认值被清除）
4. 第二次 WFSYS reset → ROM 启动，看到 host DMA 已配置 + 睡眠保护已禁用
5. ROM 初始化 R2A 桥

**问题**：第二次 WFSYS reset 会清除 host DMA 配置吗？需要测试。

### 方案 D: HOST2MCU 软中断触发

**原理**：
- 写 BIT(0) 到 MT_HOST2MCU_SW_INT_SET (BAR0 0xd4108)
- MCU ROM 可能在 idle 状态下响应软中断
- 如果 ROM 有 NEED_REINIT 处理逻辑，软中断可能触发它

### 方案 E: 研究 Windows 驱动的完整初始化路径

**原理**：
- Windows 驱动在同一硬件上工作
- Ghidra 逆向文件在 `/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/`
- 找出 Windows 驱动如何处理 R2A 桥初始化

## 七、技术架构关键信息

### 7.1 地址映射

**Bus2Chip 映射表（MT6639 PCIe）：**
```
Chip Address      BAR0 Offset    Size     用途
0x54000000       0x02000        0x1000   MCU WPDMA0 (DUMMY_CR等)
0x70000000       0x1e0000       0x9000   CONN_SEMAPHORE
0x70020000       0x1f0000       0x10000  CB_INFRA (RGU, MISC0, SLP_CTRL)
0x7c000000       0x0f0000       0x10000  CONN_INFRA (WFSYS_RST, HW_VER等)
0x7c020000       0x0d0000       0x10000  WF_HIF (DMASHDL, CONN_BUS_CR_VON)
0x7c060000       0x0e0000       0x10000  CONN_HOST_CSR_TOP (LPCTL, WAKEPU)
0x81020000       0x0c0000       0x10000  WF_TOP_CFG_ON (ROMCODE_INDEX)
0xd4000+         0xd4000+       -        HOST WFDMA0 (直接映射)
0xd7000+         0xd7000+       -        WFDMA EXT CSR (R2A bridge)
```

**L1 Remap（用于 0x18xxxxxx 地址）：**
- 控制寄存器：MT_HIF_REMAP_L1 = 0x155024
- 窗口基地址：MT_HIF_REMAP_BASE_L1 = 0x130000（64K 窗口）
- 用法：写 (target_addr >> 16) | enable_bit 到 0x155024，然后通过 0x130000+offset 访问

### 7.2 上游 mt7925 初始化流程

```
probe():
  1. SET_OWN → CLR_OWN (电源周期)
  2. WFSYS reset (0x7c000140 toggle)
  3. DMA init + NEED_REINIT

init_work() (deferred):
  4. SET_OWN → CLR_OWN (第二次电源周期，触发 MCU ROM 处理 NEED_REINIT)
  5. Firmware download
```

上游的关键：第二次 CLR_OWN 触发 MCU ROM 重新初始化 WFDMA（包括 R2A 桥）。
我们的问题：CLR_OWN 在 MT7927 上不工作。

### 7.3 寄存器详解

**LPCTL (BAR0 0xe0010 = chip 0x7c060010)：**
- BIT(0): SET_OWN (写1: FW 拥有, 睡眠)
- BIT(1): CLR_OWN (写1: Driver 拥有, 唤醒)
- BIT(2): OWN_SYNC (读: 1=FW拥有, 0=Driver拥有)
- 问题：写 BIT(1) 后 BIT(2) 不清除

**WFSYS_SW_RST (BAR0 0xf0140 = chip 0x7c000140)：**
- BIT(0): RST_B (0=reset, 1=release)
- BIT(4): INIT_DONE (只读, 1=初始化完成)
- 工作正常

**WFDMA_DUMMY_CR (BAR0 0x02120 = chip 0x54000120)：**
- BIT(1): NEED_REINIT
- 硬件默认(reset后)：0xdead2217 (NEED_REINIT=1)
- ROM 启动后清除为：0xffff0000
- DMA init 后设回：BIT(1)=1

**CONN_INFRA 睡眠保护：**
- WF_SLP_CTRL (0x18001620): BIT(0)=EN, BIT(3)=BSY
- WFDMA_SLP_CTRL (0x18001624): BIT(0)=EN, BIT(3)=BSY
- 在 PCIe 上 BIT(0) 可能是 W1C (Write-1-to-Clear)

## 八、给下一个上下文的操作指南

### 8.1 继续方案 A（INIT_DONE 后快速禁用睡眠保护）

1. 代码已经写好，但需要配合干净的设备状态
2. **首先**：在 probe 中添加 PCIe FLR：
   ```c
   /* 在 pci_enable_device() + pci_set_master() 之后添加 */
   ret = pci_reset_function(pdev);
   if (ret)
       dev_warn(&pdev->dev, "PCIe FLR failed: %d, continuing\n", ret);
   ```
3. **或者**：手动执行 PCIe reset 测试：
   ```bash
   echo 1 | sudo tee /sys/bus/pci/devices/0000:09:00.0/reset
   ```
4. 然后 `make tests && sudo insmod ...`
5. 检查 dmesg 中 `post-wfsys-rst: FAST slpprot disable` 行
6. 关键看 SLP_STS 是否仍为 0x07770313

### 8.2 如果方案 A 不够快

尝试**直接通过 BAR0 固定映射**访问睡眠保护寄存器（避免 L1 remap 开销）：
- WF_SLP_CTRL: chip 0x7c001620 → BAR0 0xf1620
- WFDMA_SLP_CTRL: chip 0x7c001624 → BAR0 0xf1624

```c
/* 比 L1 remap 更快的直接访问 */
mt7927_wr(dev, 0xf1620, BIT(0));  /* W1C 方式清除 */
mt7927_wr(dev, 0xf1624, BIT(0));
```

但需要先验证这些地址在 PCIe 上是否返回与 L1 remap 相同的值。

### 8.3 如果方案 A+B 都失败

**方案 C: 第二次 WFSYS reset**
1. 第一次 WFSYS reset → ROM 启动 → idle
2. DMA init → host rings 配置好
3. 禁用睡眠保护
4. 第二次 WFSYS reset（不需要重新配置 host DMA 因为 host DMA 在 BAR0 空间）
5. ROM 再次启动，此时睡眠保护已禁用

**方案 E: 深入 Windows 驱动逆向**
```bash
# 查看已有的逆向文档
ls /home/user/mt7927/docs/win_v5705275_*.md
ls /home/user/mt7927/docs/reverse/
```

### 8.4 构建和测试命令

```bash
# 构建
make tests

# 卸载旧模块
echo 123456 | sudo -S rmmod mt7927_init_dma 2>/dev/null

# 加载新模块
echo 123456 | sudo -S insmod /home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.ko

# 查看日志
echo 123456 | sudo -S dmesg | grep "mt7927_init_dma" | tail -80

# 查看特定关键字
echo 123456 | sudo -S dmesg | grep -E "SLP_STS|VDNR|slpprot|bridge\[|vendor-init|post-wfsys"

# PCIe FLR 手动测试
echo 123456 | sudo -S bash -c 'echo 1 > /sys/bus/pci/devices/0000:09:00.0/reset'
```

### 8.5 关键文件路径

```
驱动源码:    /home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.c
构建系统:    /home/user/mt7927/tests/Makefile
上游参考:    /home/user/mt7927/mt76/mt7925/{pci.c,regs.h,mcu.c}
上游 DMA:    /home/user/mt7927/mt76/mt792x_dma.c
上游 PM:     /home/user/mt7927/mt76/mt792x_core.c
Vendor 初始: /home/user/mt7927/mt6639/chips/soc3_0/soc3_0.c (wf_pwr_on_consys_mcu)
Vendor 寄存: /home/user/mt7927/mt6639/include/chips/soc3_0.h
Vendor 3X:   /home/user/mt7927/mt6639/chips/common/cmm_asic_connac3x.c
Vendor 头文: /home/user/mt7927/mt6639/include/nic/mt66xx_reg.h
进展文档:    /home/user/mt7927/docs/mt7927_dma_fw_status_2026-02-14_v4.md
Windows逆向: /home/user/mt7927/docs/win_v5705275_*.md
```

## 九、思考过程记录

### 9.1 本会话的核心思考链

1. **起点**：从上一会话继续，CLR_OWN 不工作，需要找替代方案唤醒 R2A 桥

2. **研究 vendor wf_pwr_on_consys_mcu()**：
   - 发现完整 18 步初始化序列
   - 识别出我们缺失的步骤：AP2CONN check, CPU reset hold/release, VDNR, semaphore
   - 关键洞察：vendor 先 hold CPU → 配置总线 → release CPU

3. **尝试 VDNR enable**：
   - VDNR (Virtual De-glitch Network Router) 控制内部总线路由和时钟
   - 在 0xfE06C 读到 0x87654321 → 明显是死区/不可用
   - 结论：PCIe 上该寄存器不存在或地址不同

4. **发现时序问题**（★最关键★）：
   - 对比首次加载 vs 后续加载的 dmesg
   - 首次：WFSYS reset 后 SLP_STS=0x00000000，2ms 后变回 0x07770313
   - 后续：WFSYS reset 后 SLP_STS 未清除（残留状态）
   - 原因：WFSYS reset 后 CONN_INFRA 睡眠保护默认恢复 EN=1
   - MCU ROM 在2ms内启动，发现 R2A 被阻断，放弃初始化

5. **方案设计**：
   - 在 WFSYS INIT_DONE 后立即（不做任何诊断dump）禁用睡眠保护
   - 配合 PCIe FLR / remove 时 reset 确保干净起始状态
   - 如果 L1 remap 太慢，改用直接 BAR0 固定映射地址

### 9.2 为什么 VDNR 等 vendor 寄存器在 PCIe 上不可用

0x7c000000 固定映射区域覆盖 0x7c000000-0x7c00FFFF (64K)。但这64K空间在不同芯片变体上映射到不同的物理寄存器：

- 0x7c000140 (WFSYS_SW_RST): ✓ 工作 — 这是所有变体通用的
- 0x7c000010 (CPU_SW_RST): 返回 0x1d2 — 在 PCIe 变体上可能是版本/ID 寄存器
- 0x7c001000 (CONN_HW_VER): 返回 0x80000000 — MT6639 PCIe 版本ID不同于 SoC3_0
- 0x7c00E06C (VDNR): 返回 0x87654321 — 死区，该寄存器在 PCIe 上不存在

这说明 PCIe 变体的 CONN_INFRA 寄存器空间与移动 SoC 变体有差异。上游 mt7925 只使用 0x7c000140，不使用其他 CONN_INFRA 寄存器。

### 9.3 关于 CONN_INFRA 睡眠保护的 W1C 机制

在 PCIe 上，WF_SLP_CTRL (0x18001620) 的 BIT(0) 行为：
- Vendor (SoC): `value &= ~BIT(0)` (CLEAR bit 0) → 禁用睡眠保护
- 我们 (PCIe): `value |= BIT(0)` (SET bit 0) → 禁用睡眠保护

两者效果相同，因为 PCIe 上该寄存器可能是 W1C (Write-1-to-Clear)。

**证据**：
- before: 0x00000001 (EN=1)
- 写入 `value |= BIT(0)` = 0x00000001
- after: 0x00000000 (EN=0, BSY=0)
- 写入1 → 读回0 → 典型 W1C 行为

### 9.4 PCIe ASPM 状态

PCIe ASPM 已确认**禁用**，不是导致 CLR_OWN 失败的原因。

## 十、用户的 Standing Instructions

1. 使用 `make tests` 构建
2. sudo 密码: `123456`
3. YOLO 模式已启用
4. 使用 Task agents 进行研究
5. 在里程碑后写文档
6. 用户之前做过 snapshot
7. 不需要额外询问用户，直接继续工作

## 十一、当前代码修改状态（半完成）

### 11.1 已写入代码但尚未测试

以下修改已写入 mt7927_init_dma.c 但尚未 build+test：

**a. remove 中添加的 WFSYS reset（line 2982-3020）：**
```c
static void mt7927_remove(struct pci_dev *pdev)
{
    /* ... */
    mt7927_dma_cleanup(dev);
    if (dev->bar0) {
        /* Disable HOST WFDMA DMA engines first */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN);
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
        msleep(5);
        /* WFSYS reset to clear internal state */
        val = mt7927_rr(dev, MT_WFSYS_SW_RST);
        val &= ~WFSYS_SW_RST_B;
        mt7927_wr(dev, MT_WFSYS_SW_RST, val);
        msleep(50);
        val |= WFSYS_SW_RST_B;
        mt7927_wr(dev, MT_WFSYS_SW_RST, val);
        msleep(10);
        /* ... */
    }
}
```

**b. WFSYS INIT_DONE 后立即禁用睡眠保护（line 1816-1852）：**
```c
/* CRITICAL: Disable sleep protection IMMEDIATELY after INIT_DONE */
{
    u32 slp_val;
    slp_val = mt7927_rr_l1(dev, CONN_INFRA_WF_SLP_CTRL_R);
    if (slp_val & CONN_INFRA_SLP_CTRL_EN) {
        slp_val |= CONN_INFRA_SLP_CTRL_EN; /* W1C: write 1 to clear */
        mt7927_wr_l1(dev, CONN_INFRA_WF_SLP_CTRL_R, slp_val);
    }
    slp_val = mt7927_rr_l1(dev, CONN_INFRA_WFDMA_SLP_CTRL_R);
    if (slp_val & CONN_INFRA_SLP_CTRL_EN) {
        slp_val |= CONN_INFRA_SLP_CTRL_EN;
        mt7927_wr_l1(dev, CONN_INFRA_WFDMA_SLP_CTRL_R, slp_val);
    }
    dev_info(&dev->pdev->dev,
             "post-wfsys-rst: FAST slpprot disable: WF=0x%08x WFDMA=0x%08x\n",
             mt7927_rr_l1(dev, CONN_INFRA_WF_SLP_CTRL_R),
             mt7927_rr_l1(dev, CONN_INFRA_WFDMA_SLP_CTRL_R));
}
```

### 11.2 计划写入但尚未编辑的

**c. PCIe FLR 在 probe 开始时（在 pci_set_master 之后，pci_iomap 之前）：**
```c
/* PCIe FLR to get clean device state */
ret = pci_reset_function(pdev);
if (ret)
    dev_warn(&pdev->dev, "PCIe FLR failed: %d, continuing\n", ret);
```
位置：约 line 2717 之后。

**d. 如果 L1 remap 太慢，改用直接 BAR0 地址：**
```c
/* 直接通过固定映射访问，比 L1 remap 快 */
mt7927_wr(dev, 0xf1620, BIT(0));  /* W1C: write 1 to WF_SLP_CTRL clear EN */
mt7927_wr(dev, 0xf1624, BIT(0));  /* W1C: write 1 to WFDMA_SLP_CTRL clear EN */
```
这些地址通过 bus2chip {0x7c000000, 0xf0000, 0x10000} 映射。
但需要先验证 0xf1620/0xf1624 是否返回与 L1 remap 相同的值。

### 11.3 下一个会话应该做的事情（按顺序）

1. **build+test 当前代码**：`make tests && rmmod && insmod` 看 remove 中的 WFSYS reset 是否帮助下次加载
2. **如果 SLP_STS 仍然 0x07770313**：添加 PCIe FLR
3. **验证 0xf1620/0xf1624 直接映射**：读取并对比 L1 remap 结果
4. **如果直接映射可用**：替换 L1 remap 为直接写入（更快）
5. **观察 SLP_STS 变化**：如果 SLP_STS 在 INIT_DONE 后能保持 0x00000000，说明时序修复有效
6. **然后继续固件下载流程**

## 十二、完整 dmesg 关键行参考（本会话最后一次运行）

```
# === 初始状态 ===
[66011.941324] Chip status: 0x00000000
[66011.941327] fw_pmctrl: LPCTL before SET_OWN=0x00000004  ← 残留自上次加载
[66011.970079] fw_pmctrl: CLR_OWN LPCTL=0x00000004 retries=10 FAILED

# === Vendor 诊断 ===
[66011.972126] vendor-init: AP2CONN_SLP_PROT_ACK=0x00000000 (bit5=0)  ← OK
[66011.972129] vendor-init: CONN_HW_VER=0x80000000 (expect 0x20010000) MISMATCH
[66011.972138] vendor-init: CPU_SW_RST=0x000001d2 PWR_STS=0x00000000 (wf_pwr=0)
[66011.972139] vendor-init: WFSYS_SEMA=0x00000000 VDNR=0x87654321 (VDNR_EN=1)  ← 死区
[66011.972144] vendor-init: VDNR after enable=0x87654321 (VDNR_EN=1)  ← 无效

# === WFSYS Reset ===
[66011.972163] bridge[pre-wfsys-rst]: SLP_STS=0x07770313 CTRL0=ffff0c08  ← 残留状态
[66011.972167] WFSYS_SW_RST before=0x00000011
[66012.022355] WFSYS_SW_RST after=0 ms, val=0x00000011 (INIT_DONE=1)
[66012.022360] post-wfsys-rst: VDNR=0x87654321 (EN=1) PWR_STS=0x00000000 (wf_pwr=0)
[66012.042499] bridge[post-wfsys-rst]: SLP_STS=0x07770313  ← 未被清除！(残留问题)

# === Sleep Protection ===
[66012.044516] slpprot: WF_SLP_CTRL before=0x00000000 (EN=0 BSY=0)  ← 因为残留
[66012.044525] slpprot: WFDMA_SLP_CTRL before=0x00000001 (EN=1 BSY=0)
[66012.044573] slpprot: SLP_STS=0x07770313  ← 仍然

# === DMA + Firmware ===
[66012.069673] dma-ready R2A: STS=0x00000000  ← DMA path probe 前桥状态
[66012.069679] dma-ready SLPPROT: SLP_STS=0x07770313
[66012.069758] dma-probe: q16 consumed (cidx=0x1 didx=0x1)  ← TX DMA 正常!
[66012.069771] bridge[pre-power-cycle]: SLP_STS=0x07770313 FSM=03030101/00000303
[66012.069780] power-cycle SET_OWN: LPCTL=0x00000004 after 0 polls
[66012.105391] power-cycle CLR_OWN: LPCTL=0x00000004 retries=10 FAILED

# === 超时 ===
[66012.326381] mcu-evt timeout: q6 tail=0 cidx=0x7f didx=0x0
[66012.326580] R2A FSM: CMD_ST=0x03030101 DAT_ST=0x00000303 SLP_STS=0x07770313
[66012.326592] HIF_BUSY=0x80000002 (txfifo1=1)  ← 数据卡在 HOST→MCU FIFO
[66012.326691] Patch download failed: -110
```

## 十三、首次加载 vs 后续加载的关键对比

| 指标 | 首次加载 (63896) | 后续加载 (66012) |
|------|-----------------|-----------------|
| LPCTL before SET_OWN | 0x00000000 | 0x00000004 |
| SLP_STS pre-WFSYS-rst | 0x07770313 | 0x07770313 |
| SLP_STS post-WFSYS-rst | **0x00000000** ✓ | **0x07770313** ✗ |
| CTRL0 post-WFSYS-rst | 0x00000000 | 0xffff0c08 |
| WF_SLP_CTRL before | 0x00000001 (EN=1) | 0x00000000 (EN=0) |
| SLP_STS post-slpprot | 0x07770313 | 0x07770313 |
| 最终结果 | timeout -110 | timeout -110 |

**关键差异**：首次加载时 WFSYS reset 能清除 SLP_STS，后续加载不能。
**原因**：rmmod 没有做 WFSYS reset，设备保留了 DMA 活动的残留状态。
**修复**：已在 remove 中添加 WFSYS reset + DMA disable（尚未测试）。
