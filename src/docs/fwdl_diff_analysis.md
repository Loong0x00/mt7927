# FWDL 差异分析: 新驱动 vs 旧驱动

**分析日期**: 2026-02-15
**新驱动**: `src/mt7927_pci.c` + `src/mt7927_pci.h`
**旧驱动**: `tests/04_risky_ops/mt7927_init_dma.c`
**问题**: 新驱动 FWDL 在 `patch_sem` 命令处超时 (-110)，旧驱动在 clean boot 上 FWDL 成功 (fw_sync=0x3)

---

## 差异 #1: WFDMA0_RST 寄存器偏移不同 (严重)

### 差异描述
新旧驱动对 WFDMA 逻辑复位寄存器使用了**不同的偏移地址**。

### 旧驱动代码
```c
// 行 622
#define MT_WFDMA0_RST              MT_WFDMA0(0x100)   // = 0xd4000 + 0x100 = 0xd4100
```

### 新驱动代码
```c
// mt7927_pci.h 行 190
#define MT_WFDMA0_RST               MT_WFDMA0(0x0234)  // = 0xd4000 + 0x234 = 0xd4234
```

### 可能影响: **高**
新驱动的 `mt7927_dma_disable()` 在行 344-348 对 `0xd4234` 做逻辑复位脉冲。如果正确地址是 `0xd4100`（旧驱动使用），那么新驱动的逻辑复位**写到了错误的寄存器**，可能没有效果，或者写到一个不相关的寄存器导致副作用。

**但注意**: 旧驱动的 `0x100` 注释说来自 `mt7996/regs.h`，而新驱动的 `0x234` 没有明确来源。两个值都需要验证。

### 修复建议
确认 MT6639/CONNAC3 的 WFDMA 逻辑复位寄存器的正确偏移。参考 `wf_wfdma_host_dma0.h` coda header 确认。如果旧驱动值正确，新驱动应改为 `MT_WFDMA0(0x100)`。

---

## 差异 #2: 初始化顺序 — CLR_OWN 在不同阶段 (严重)

### 差异描述
旧驱动和新驱动的初始化时序差异巨大。

### 旧驱动流程
```
1. SET_OWN (行 4063)
2. mt7927_mcu_init_mt6639() — WF 复位 + MCU 空闲轮询 (行 4071-4076)
3. mt7927_drv_own() — 裸 CLR_OWN (行 4078)
   ⚠️ 注意: drv_own 是裸 CLR_OWN，没有先 SET_OWN！
   但前面已经做过 SET_OWN 了，所以 OWN_SYNC 可能已经是 1
4. mt7927_dma_init() — 分配 rings + 预取 + GLO_CFG + INT_ENA + DMA enable (行 4084)
5. mt7927_dma_path_probe() — 可选 DMA 探测 (行 4088)
6. mt7927_mcu_fw_download() — FWDL (行 4460)
   内部没有额外的 SET_OWN/CLR_OWN！
```

### 新驱动流程
```
1. mt7927_mcu_init_mt6639() — WF 复位 + MCU 空闲轮询 (行 1396)
2. PCIe 睡眠禁用 (行 1402-1410)
3. mt7927_dma_disable() — 禁用 DMA + 逻辑复位 (行 1413)
4. mt7927_init_tx_rx_ring() — 分配 rings (行 1420)
5. mt7927_wpdma_config(true) — 预取 + GLO_CFG enable (行 1427)
6. mt7927_config_int_mask(true) — 中断掩码 (行 1430)
7. mt7927_fw_download() — FWDL (行 1436)
   7a. mt7927_set_drv_own() — SET_OWN + CLR_OWN (行 1130)
   7b. 复位 DMA 指针 + reprogram rings (行 1136-1147)
   7c. 重新启用 DMA + bypass DMASHDL + NEED_REINIT (行 1150-1167)
   7d. mt7927_load_patch() — 补丁下载 (行 1170)
```

### 可能影响: **极高**
关键区别:
- **旧驱动**在 FWDL 之前就做了 CLR_OWN（通过 `mt7927_drv_own()`），ROM 初始化了 WFDMA，然后 `mt7927_dma_init()` 在 ROM 已经配置好的状态上进行配置。FWDL 时不再做 SET_OWN/CLR_OWN。
- **新驱动**在 probe 中**没有**初始 SET_OWN/CLR_OWN。它先配置好 rings 和 DMA，然后在 `mt7927_fw_download()` 内部做 SET_OWN/CLR_OWN。CLR_OWN 会**清零所有 HOST ring BASE**，所以必须重新编程。

**但问题是**: 新驱动在 `mt7927_fw_download()` 内做 SET_OWN/CLR_OWN + reprogram，这个逻辑应该是正确的。真正的差异在于旧驱动的 CLR_OWN 由 ROM 触发 WFDMA 初始化后，旧驱动的 `dma_init()` 在这个已初始化的状态上配置了**大量额外的寄存器**（中断优先级、RX 暂停阈值、GLO_CFG 各种位等），而新驱动在 CLR_OWN 后的 reprogram 比较简洁。

### 修复建议
确认新驱动是否需要在 probe 阶段（`mt7927_mcu_init_mt6639()` 之后、rings 分配之前）先做一次 SET_OWN + CLR_OWN（让 ROM 初始化 WFDMA），然后再分配 rings。

---

## 差异 #3: GLO_CFG 值差异 (中等)

### 差异描述
两个驱动在 DMA 启用时设置的 GLO_CFG 位不同。

### 旧驱动代码 (行 1888-1895, 2001-2003)
```c
// Phase 2a: 先配置各种位（不含 DMA_EN）
val |= MT_WPDMA_GLO_CFG_MT76_SET;  // BIT(4,5,6,11,12,13,15,21,28,30)
val |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;  // BIT(20)
val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL; // BIT(9)
val |= BIT(26);  // ADDR_EXT_EN

// Phase 2b: 再加上 DMA_EN
val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
```
旧驱动设置的位总结: **BIT(0,2,4,5,6,9,11,12,13,15,20,21,26,28,30)**

### 新驱动代码 (行 931-936)
```c
val |= MT_WFDMA_GLO_CFG_TX_DMA_EN |     // BIT(0)
       MT_WFDMA_GLO_CFG_RX_DMA_EN;      // BIT(2)
val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL; // BIT(9) — FWDL 阶段
```
新驱动设置的位总结（仅 OR 上去的）: **BIT(0,2,9)**
其余位来自 ROM CLR_OWN 后硬件默认值。

### 可能影响: **中等**
缺少的关键位:
- **BIT(6) TX_WB_DDONE**: TX writeback done — 可能影响 TX 完成通知机制
- **BIT(15) CSR_DISP_BASE_PTR_CHAIN_EN**: 预取链模式 — **关键**！没有这个位，per-ring 预取配置可能不生效
- **BIT(20) CSR_LBK_RX_Q_SEL_EN**: RX 队列选择 — vendor 必须设置
- **BIT(21) OMIT_RX_INFO_PFET2**: 省略 RX 信息 — 可能影响 RX 描述符格式
- **BIT(26)**: 地址扩展使能
- **BIT(28) OMIT_TX_INFO**: 省略 TX 信息
- **BIT(30) CLK_GATE_DIS**: 时钟门控禁用

**BIT(15) CHAIN_EN 是最可疑的** — 如果没有设置，WFDMA 可能无法正确分发数据到各个 ring。

### 修复建议
在 `mt7927_wpdma_config(true)` 中添加与旧驱动一致的 GLO_CFG 位，特别是 BIT(6,15,20,21,26,28,30)。

---

## 差异 #4: INT_ENA 寄存器写入方式 (中等)

### 差异描述
旧驱动直接写 `0xd4204` (INT_ENA)，新驱动写 `0xd4228` (INT_ENA_SET)。

### 旧驱动代码 (行 1933-1937)
```c
mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_ENA,   // 0xd4204 — 直接覆写
                 HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                 HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                 HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                 HOST_TX_DONE_INT_ENA17, "host_int_ena");
// 写入值: BIT(0) | BIT(1) | BIT(6) | BIT(25) | BIT(26) | BIT(27) = 0x0E000043
```

### 新驱动代码 (行 963-964)
```c
mt7927_wr(dev, MT_WFDMA_INT_ENA_SET,    // 0xd4228 — SET 寄存器 (原子 OR)
          MT_WFDMA_INT_MASK_WIN);        // 0x2600f000
```
写入值: `0x2600f000` = BIT(29) | BIT(25) | GENMASK(15,12)

### 可能影响: **低-中**
- 写 `0xd4204` 是**直接覆写**整个 INT_ENA 寄存器
- 写 `0xd4228` 是**原子 SET** (只设置指定位，不清除其他位)

Windows 驱动使用 SET 寄存器 (`0xd4228`)，所以新驱动的方式更正确。

但**中断掩码值完全不同**:
- 旧驱动: `0x0E000043` — 包含 BIT(0,1,6) 对应 RX ring 0,1,6 完成中断
- 新驱动: `0x2600f000` — 包含 BIT(12-15) 对应 RX ring 4,5,6,7 完成中断 + BIT(29) MCU2HOST

对 FWDL 的影响可能不大，因为 FWDL 使用轮询而非中断。但如果 ROM 需要看到特定中断位才能开始处理，则可能有关。

### 修复建议
不太可能是 FWDL 失败的原因，但建议保持与 Windows 一致的 SET 寄存器方式和掩码值。

---

## 差异 #5: 预取配置值差异 (中等)

### 差异描述
旧驱动和新驱动的 per-ring 预取配置布局不同。

### 旧驱动代码 (行 1849-1868)
```c
RX4: PREFETCH(0x0000, 0x8)   // base=0x0000, depth=8
RX5: PREFETCH(0x0080, 0x8)   // base=0x0080, depth=8
RX6: PREFETCH(0x0100, 0x8)   // base=0x0100, depth=8
RX7: PREFETCH(0x0180, 0x4)   // base=0x0180, depth=4
TX16: PREFETCH(0x01C0, 0x4)  // base=0x01C0, depth=4
TX0:  PREFETCH(0x0200, 0x10) // base=0x0200, depth=16
TX1:  PREFETCH(0x0300, 0x10) // base=0x0300, depth=16
TX2:  PREFETCH(0x0400, 0x4)  // base=0x0400, depth=4
TX3:  PREFETCH(0x0440, 0x4)  // base=0x0440, depth=4
TX15: PREFETCH(0x0480, 0x4)  // base=0x0480, depth=4
```
特别注意: **旧驱动包含 RX5 和 TX0-3 的预取配置**。

### 新驱动代码 (行 913-917)
```c
RX4:  PREFETCH(0x0000, 0x8)   // base=0x0000, depth=8
RX6:  PREFETCH(0x0080, 0x8)   // base=0x0080, depth=8
RX7:  PREFETCH(0x0100, 0x4)   // base=0x0100, depth=4
TX16: PREFETCH(0x0140, 0x4)   // base=0x0140, depth=4
TX15: PREFETCH(0x0180, 0x10)  // base=0x0180, depth=16
```

### 可能影响: **中等**
关键差异:
1. **新驱动没有 RX5 的预取** — 因为新驱动不分配 RX5 ring
2. **base 偏移不同** — RX6 在旧驱动是 `0x0100`，新驱动是 `0x0080`
3. **TX15 depth 不同** — 旧驱动 depth=4，新驱动 depth=16

如果没有设置 BIT(15) CHAIN_EN（差异 #3），per-ring 预取可能根本不工作，此差异影响减小。

### 修复建议
与差异 #3 一起修复。确保 CHAIN_EN 位设置，且预取配置与分配的 rings 匹配。

---

## 差异 #6: patch 下载 max_len 不同 (中等)

### 差异描述
补丁固件分块大小不同。

### 旧驱动代码 (行 1150)
```c
u32 max_len = 0x800;  // 2048 字节
```

### 新驱动代码 (行 999)
```c
u32 max_len = MT_FWDL_MAX_LEN;  // 4096 字节
```

### 可能影响: **低-中**
如果 ROM 的 FWDL 环缓冲区小于 4096，可能导致溢出或错误。旧驱动使用 2048，RAM 下载使用 4096，这与上游 mt7925 一致。新驱动在 patch 和 RAM 下载都使用 4096。

### 修复建议
将新驱动的 `mt7927_load_patch()` 中 `max_len` 改为 `0x800` (2048)，与旧驱动和上游一致。

---

## 差异 #7: TXD[1] 中 no_long_format 逻辑 (低)

### 差异描述
旧驱动有 `no_long_format` 参数控制 BIT(31)，新驱动永远不设置。

### 旧驱动代码 (行 954-956)
```c
val = (no_long_format ? 0 : MT_TXD1_LONG_FORMAT) |
      FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
```
默认 `no_long_format=false`，所以**默认会设置 BIT(31)**。
但 Mode 40 测试记录中使用的 NIC_CAP 命令 (`mt7927_mode40_send_nic_cap`) 硬编码不设置 BIT(31)。

### 新驱动代码 (行 498-499)
```c
val = FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
// 永不设置 BIT(31)
```

### 可能影响: **低**
新驱动的行为与 Windows 一致（永不设置 BIT(31)）。旧驱动在 Mode 40 成功测试中也是不设置 BIT(31)。FWDL 命令（patch_sem 等）使用 `mt7927_mcu_send_cmd` 发送，在旧驱动中默认设置 BIT(31) 但仍然成功。这说明 BIT(31) **不是 FWDL 失败的原因**。

不过要注意: 旧驱动的 FWDL 成功是在 `no_long_format=false` 默认设置下（**有** BIT(31)）。

### 修复建议
不需要修改。

---

## 差异 #8: MCU2HOST_SW_INT_ENA 从未设置 (低-中)

### 差异描述
旧驱动显式设置 MCU2HOST 软件中断使能，新驱动没有。

### 旧驱动代码 (行 1932)
```c
mt7927_wr_verify(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE, "sw_int_ena");
// MT_MCU2HOST_SW_INT_ENA = 0xd41f4
// MT_MCU_CMD_WAKE_RX_PCIE = BIT(0)
```

### 新驱动代码
无对应写入。

### 可能影响: **低-中**
这个寄存器控制 MCU 到 HOST 的软件中断使能。如果 ROM 需要检查这个位来决定是否通过 DMA 发送事件（vs 通过寄存器信号），可能影响事件接收。

但对 FWDL 阶段的 patch_sem 命令来说，ROM 应该不需要这个设置。

### 修复建议
在 DMA 初始化阶段添加 `mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, BIT(0))`。

---

## 差异 #9: RX 暂停阈值从未配置 (低)

### 差异描述
旧驱动配置了 RX 暂停阈值，新驱动没有。

### 旧驱动代码 (行 1947-1950)
```c
for (th = 0; th < 6; th++)
    mt7927_wr(dev, MT_WPDMA_PAUSE_RX_Q_TH(th), MT_WPDMA_PAUSE_RX_Q_TH_VAL);
// 值: 0x00020002 — 每个 ring 对的暂停阈值=2
```

### 新驱动代码
无对应配置。

### 可能影响: **低**
RX 暂停阈值影响流控。FWDL 阶段数据量小，不太可能因流控问题导致超时。

### 修复建议
可选。在初始化中添加。

---

## 差异 #10: INT_RX_PRI / INT_TX_PRI 从未配置 (低)

### 差异描述
旧驱动设置了中断优先级，新驱动没有。

### 旧驱动代码 (行 1939-1942)
```c
mt7927_wr_verify(dev, MT_WFDMA_INT_RX_PRI,
                 mt7927_rr(dev, MT_WFDMA_INT_RX_PRI) | 0x0F00, "int_rx_pri");
mt7927_wr_verify(dev, MT_WFDMA_INT_TX_PRI,
                 mt7927_rr(dev, MT_WFDMA_INT_TX_PRI) | 0x7F00, "int_tx_pri");
```

### 新驱动代码
无对应配置。

### 可能影响: **低**
中断优先级不应影响轮询模式的 FWDL 流程。

### 修复建议
可选。

---

## 差异 #11: MSI 中断配置寄存器写入差异 (中等)

### 差异描述
旧驱动显式写入 MSI 配置寄存器（Windows 预取配置值），新驱动也写了相同的值但到不同命名的寄存器。

### 旧驱动代码 (行 1927-1930)
```c
mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG0, MT_WFDMA_MSI_CFG0_WIN, "msi_cfg0"); // 0xd70f0 = 0x660077
mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG1, MT_WFDMA_MSI_CFG1_WIN, "msi_cfg1"); // 0xd70f4 = 0x1100
mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG2, MT_WFDMA_MSI_CFG2_WIN, "msi_cfg2"); // 0xd70f8 = 0x30004f
mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG3, MT_WFDMA_MSI_CFG3_WIN, "msi_cfg3"); // 0xd70fc = 0x542200
```

### 新驱动代码 (行 900-907)
```c
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG0, MT_WFDMA_PREFETCH_VAL0); // 0xd70f0 = 0x660077
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG1, MT_WFDMA_PREFETCH_VAL1); // 0xd70f4 = 0x1100
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG2, MT_WFDMA_PREFETCH_VAL2); // 0xd70f8 = 0x30004f
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG3, MT_WFDMA_PREFETCH_VAL3); // 0xd70fc = 0x542200
```

### 可能影响: **无差异**
两个驱动写相同的值到相同的地址。名字不同但地址完全相同:
- `MT_WFDMA_MSI_INT_CFG0` = `MT_WFDMA_PREFETCH_CFG0` = `0xd7000 + 0xf0` = `0xd70f0`

**关键区别**: 旧驱动在初始 DMA 初始化中写了这些值（在 CLR_OWN 后），新驱动在 `mt7927_wpdma_config()` 中写（在 CLR_OWN 前后都写了）。新驱动在 CLR_OWN 后的 reprogram 中**没有**重写这些 packed prefetch 寄存器（只重写了 per-ring EXT_CTRL）。

但实际上 CLR_OWN 后新驱动会重新调用 `mt7927_wpdma_config(true)`（在 `mt7927_fw_download()` 行 1149-1157 中间接设置了 GLO_CFG，但没有调用 `mt7927_wpdma_config`）。

**等等** — 仔细看新驱动 `mt7927_fw_download()`:
- 行 1147: `mt7927_reprogram_prefetch(dev)` — 只写 per-ring EXT_CTRL，**没有**写 packed prefetch 0xd70f0-0xd70fc
- 行 1150-1157: 直接 RMW GLO_CFG 加上 DMA_EN 位

**这意味着 CLR_OWN 后，新驱动没有重写 packed prefetch 配置**。如果 CLR_OWN 清零了这些寄存器，WFDMA 可能无法正确预取。

### 修复建议
在 `mt7927_fw_download()` 的 CLR_OWN 后 reprogram 阶段，除了 per-ring EXT_CTRL，也要重写 packed prefetch 寄存器 (0xd70f0-0xd70fc)。

---

## 差异 #12: GLO_CFG_EXT1 写入时机 (低)

### 差异描述
两个驱动都写 GLO_CFG_EXT1 BIT(28)，但新驱动在 CLR_OWN 后的 reprogram 阶段可能没有重写。

### 旧驱动代码 (行 1915-1917)
```c
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
val |= MT_WPDMA_GLO_CFG_EXT1_WIN;  // BIT(28)
mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG_EXT1, val, "glo_cfg_ext1");
```

### 新驱动代码 (行 942-944)
```c
val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
val |= MT_WPDMA_GLO_CFG_EXT1_WIN;  // BIT(28)
mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, val);
```

新驱动在 CLR_OWN 后没有重写 EXT1。如果 CLR_OWN 清零了 EXT1，BIT(28) 会丢失。

### 可能影响: **低-中**
Mode 40 测试日志显示 CLR_OWN 后 EXT1 仍保留了某些值（`0x8c800404` → `0x9c800404`），说明 CLR_OWN 可能不完全清零 EXT1。

### 修复建议
在 reprogram 阶段也重写 EXT1。

---

## 差异 #13: HOST_CONFIG (0xd7030) 写入 (低)

### 差异描述
旧驱动读写 `MT_WFDMA_HOST_CONFIG` (0xd7030)，新驱动没有。

### 旧驱动代码 (行 1922-1926)
```c
val = mt7927_rr(dev, MT_WFDMA_HOST_CONFIG);
// 记录 pcie_int_en 和 wfdma_int_mode 位
mt7927_wr_verify(dev, MT_WFDMA_HOST_CONFIG, val, "host_cfg");
```
注意: 旧驱动只是**读回写**（不修改值），所以实际没有改变什么。

### 可能影响: **无**
没有修改值，不影响功能。

---

## 差异 #14: PREFETCH_CTRL 触发方式 (低)

### 差异描述
新驱动在 `mt7927_wpdma_config(true)` 中触发预取重置。

### 新驱动代码 (行 896-897)
```c
val = mt7927_rr(dev, MT_WFDMA_PREFETCH_CTRL);  // 0xd7030
mt7927_wr(dev, MT_WFDMA_PREFETCH_CTRL, val);    // 读回写
```

### 旧驱动
旧驱动在手动预取配置中通过清除 CHAIN_EN 来准备预取。

### 可能影响: **低**
两种方式都会触发预取引擎重新加载配置。

---

## 差异 #15: PRI_DLY_INT_CFG0 从未清零 (低)

### 差异描述
旧驱动清零中断延迟配置，新驱动没有。

### 旧驱动代码 (行 1920)
```c
mt7927_wr_verify(dev, MT_WFDMA_PRI_DLY_INT_CFG0, 0x0, "pri_dly0");
```

### 新驱动代码
无对应写入。

### 可能影响: **低**
中断延迟配置不影响 FWDL 的轮询模式。

---

## 差异 #16: HOST_INT_STA 清除时机 (低-中)

### 差异描述
旧驱动在 DMA 启用前清除所有中断状态，新驱动没有。

### 旧驱动代码 (行 1886)
```c
mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_STA, 0xffffffff, "host_int_sta_clr");
```

### 新驱动代码
无对应写入。

### 可能影响: **低-中**
如果有残留的中断状态位，可能干扰后续中断处理。但 FWDL 使用轮询不使用中断。

### 修复建议
在 DMA 启用前添加 `mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, 0xffffffff)` 清除中断状态。

---

## 差异 #17: CONN_MISC_CFG (0xd1000) 写入 (低)

### 差异描述
旧驱动的 `mt7927_apply_predl_cfg()` 写入 `0x70011840` 到 `0xd1000`，但此函数默认不启用 (`enable_predl_regs=false`)。

### 可能影响: **无**
默认不启用，旧驱动 FWDL 成功时也没有启用此功能。

---

## 差异 #18: RX Ring 5 分配差异 (低-中)

### 差异描述
旧驱动分配了 RX ring 5（小的 dummy ring），新驱动没有。

### 旧驱动代码 (行 1771-1772)
```c
ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx5, 5, 4, 512);
```

### 新驱动代码
不分配 RX ring 5。只有 RX 4, 6, 7。

### 可能影响: **低-中**
Windows RE 确认不使用 ring 5。但旧驱动分配 ring 5 作为 "dummy"，防止预取链在遇到 BASE=0 时阻塞。新驱动的预取配置中跳过了 ring 5，如果 WFDMA 硬件的预取链需要连续的 ring 配置，缺少 ring 5 可能导致预取引擎在 ring 4 → ring 6 之间阻塞。

### 修复建议
在预取配置中，如果不分配 ring 5，确保预取 base_ptr 不会落在 ring 5 的空洞上。或者像旧驱动一样分配一个小的 dummy ring 5。

---

## 差异 #19: 旧驱动的 drv_own 是裸 CLR_OWN vs 新驱动的 SET_OWN+CLR_OWN (关键理解)

### 差异描述
旧驱动 probe 中用 `mt7927_drv_own()` 做裸 CLR_OWN，前面 SET_OWN 在更早处。新驱动在 `mt7927_set_drv_own()` 中做 SET_OWN+CLR_OWN。

### 旧驱动流程 (行 4060-4078)
```c
// 行 4063: SET_OWN (probe 开头)
mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_SET_OWN);
usleep_range(2000, 3000);

// 行 4071-4076: MCU init (WF reset + MCU idle 轮询)

// 行 4078: mt7927_drv_own() — 裸 CLR_OWN
for (i = 0; i < 10; i++) {
    mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN);
    usleep_range(1000, 2000);
    lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
    if (!(lpctl & PCIE_LPCR_HOST_OWN_SYNC))
        break;
}
```

### 新驱动流程 (行 762-795)
```c
// SET_OWN 和 CLR_OWN 在同一个函数中
mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_SET_OWN);
// wait OWN_SYNC=1
mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN);
// wait CLR_OWN clears
```

### 可能影响: **中等**
旧驱动的 SET_OWN 在 MCU init **之前**做（确保 MCU 在 WF 复位前处于已知电源状态）。CLR_OWN 在 MCU init **之后**做。

新驱动的 MCU init 也在 SET_OWN/CLR_OWN 之前做。但新驱动的 SET_OWN/CLR_OWN 发生在 FWDL 函数内部（probe 的中间），而**不是**在 DMA 初始化之前。

更重要的是：旧驱动在 CLR_OWN（`drv_own`）之后紧接着做 `dma_init()`（包括所有 rings + 预取 + GLO_CFG），然后才做 FWDL。新驱动在 probe 阶段先配置好 rings + 预取 + GLO_CFG，然后在 FWDL 内部做 CLR_OWN + reprogram。

**关键差异**: 旧驱动 CLR_OWN → ROM 初始化 WFDMA → 配置所有东西（在 ROM 已初始化的状态上）→ FWDL。
新驱动: 配置所有东西 → CLR_OWN → ROM 清零 HOST rings → reprogram（可能遗漏一些 ROM 也清零的东西）→ FWDL。

### 修复建议
考虑改为旧驱动的顺序: CLR_OWN 放在 ring 分配之前，让 ROM 先初始化 WFDMA，然后再配置 rings。

---

## 按可能性排序的修复优先级列表

### 1. (最高) WFDMA0_RST 寄存器偏移错误
- **差异 #1**: `0x100` vs `0x234`
- 如果 `0x234` 是错误的，`mt7927_dma_disable()` 的逻辑复位无效，可能导致 DMA 引擎处于不可预测状态
- **修复**: 验证正确偏移并更正

### 2. (高) GLO_CFG 缺少关键位（特别是 BIT(15) CHAIN_EN）
- **差异 #3**: 新驱动缺少 BIT(6,15,20,21,26,28,30)
- BIT(15) CHAIN_EN 可能使预取配置无法生效
- **修复**: 在 `mt7927_wpdma_config` 中添加旧驱动的 GLO_CFG 位

### 3. (高) CLR_OWN 后未重写 packed prefetch 配置
- **差异 #11 的分析**: CLR_OWN 后 `mt7927_reprogram_prefetch()` 只写 per-ring EXT_CTRL，不重写 0xd70f0-0xd70fc
- **修复**: 在 reprogram 阶段也写 packed prefetch 寄存器

### 4. (中高) 初始化顺序差异
- **差异 #2**: 新驱动 probe 阶段没有初始 CLR_OWN
- ROM 可能需要 CLR_OWN 触发 WFDMA 基本初始化后才能正确处理后续配置
- **修复**: 考虑在 probe 阶段（ring 分配前）先做 SET_OWN + CLR_OWN

### 5. (中) Patch 分块大小不匹配
- **差异 #6**: 补丁用 4096 vs 旧驱动 2048
- **修复**: 改为 `0x800`

### 6. (中) 缺少 MCU2HOST_SW_INT_ENA 设置
- **差异 #8**: 新驱动没有写 0xd41f4
- **修复**: 添加写入

### 7. (中低) 缺少 RX ring 5 dummy 分配
- **差异 #18**: 可能导致预取链阻塞
- **修复**: 分配小的 dummy ring 5

### 8. (低) 未清除 HOST_INT_STA
- **差异 #16**: 残留中断状态
- **修复**: 添加清除

### 9. (低) 缺少 INT_RX_PRI / INT_TX_PRI 配置
- **差异 #10**: 中断优先级
- **修复**: 添加配置

### 10. (低) 缺少 RX 暂停阈值配置
- **差异 #9**
- **修复**: 添加配置

---

## 总结

新驱动 FWDL 在 `patch_sem` 命令处超时 (-110)，最可能的原因排序:

1. **WFDMA0_RST 偏移错误** (`0x234` vs `0x100`) — 导致逻辑复位写到错误寄存器
2. **GLO_CFG 缺少 CHAIN_EN 等关键位** — 导致 WFDMA 预取/分发不工作
3. **CLR_OWN 后 packed prefetch 未重写** — 导致 WFDMA 无法正确路由数据
4. **初始化顺序差异** — ROM 可能期望先 CLR_OWN 后配置

建议按优先级 1→2→3→4 逐项修复测试。
