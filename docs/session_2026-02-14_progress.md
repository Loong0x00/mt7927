# MT7927 驱动开发进展 - 2026-02-14 会话（更新版）

## 核心问题

TX DMA 完全正常（Ring 15/16 均被 MCU 消费），但 MCU 从不向任何 RX Ring 写入事件响应，导致固件下载超时 (-110)。

## 已完成的修改（本次会话全部）

### 1. 强制 WF 子系统复位（关键修复 ✓）

**问题**：PCI 总线复位后，即使 MCU 报告 IDLE (0x1D1E)，WFDMA 仍不工作。
**修复**：添加 `force_wf_reset=true` 参数，始终执行 WF 子系统复位。
**效果**：TX DMA 恢复工作，GLO_CFG 恢复正常值 0x5430b87d。

### 2. 恢复 TXD DW1 BIT(31)（关键修复 ✓）

**问题**：之前移除 BIT(31)（CONNAC3X 中为 FIXED_RATE），导致 Ring 15 命令不被消费。
**修复**：在 CONNAC3X TXD DW1 中恢复 `MT_TXD1_LONG_FORMAT`。
**注意**：vendor 驱动不设置此位，但 ROM bootloader 需要它。可能是 WFDMA 模式差异。

### 3. cbinfra PCIe 地址重映射（已实现 ✓ 已测试）

**寄存器**：
- `MT_CB_INFRA_MISC0_PCIE_REMAP_WF` (BAR0 0x1F6554) = 0x74037001
- `MT_CB_INFRA_MISC0_PCIE_REMAP_WF_BT` (BAR0 0x1F6558) = 0x70007000

**顺序**：CONNINFRA 唤醒 → cbinfra remap → WF 复位（顺序必须正确）
**验证**：remap 值在 WF 复位后保持（CB_INFRA 是独立域）。

### 4. 全 RX Ring 诊断 dump（已实现 ✓ 已测试）

MCU 命令超时后 dump Ring 0-7 的 DIDX/CIDX/BASE/CNT。
**结果**：所有 8 个 RX Ring 的 DIDX = 0，MCU 未向任何 Ring 写入。

### 5. PCIE2AP_REMAP_2 诊断读取（已添加，待测试）

在 MCU init 的 mcu_ready 标签后读取 BAR0 0xFE068 的值。

### 6. TX/RX prefetch EXT_CTRL 恢复（已实现 ✓）

WF 复位后所有 EXT_CTRL 值被清零。显式写入：
- TX15: PREFETCH(0x0500, 0x4)
- TX16: PREFETCH(0x0540, 0x4)
- TX17: PREFETCH(0x0580, 0x4)
- RX6:  PREFETCH(0x0080, 0x4)

### 7. 其他已完成项（来自上一轮会话的延续）

- RX 事件环 Ring 0 → Ring 6（`evt_ring_qid` 参数）
- CONNAC3X TXD DW1 HDR_FORMAT bits [15:14]
- MT6639 MCU init 序列（CONNINFRA 唤醒 → WF 复位 → MCU_IDLE 轮询）
- HOST2MCU 软中断（每次 kick 后写 BIT(0)）
- Patch 地址宏 MCU_PATCH_ADDRESS_MT6639 (0x900000)
- 固件路径可配置（fw_ram / fw_patch module_param）

## 最新测试结果

```
PCIE_REMAP_WF before=0x74037001 (persisted)
PCIE_REMAP_WF after=0x74037001 REMAP_WF_BT=0x70007000
q16 consumed (cidx=0x1 didx=0x1)     ← TX FWDL ring 正常
q15 CIDX=0x1 DIDX=0x1                ← TX MCU ring 正常
HOST_INT_STA=0x06000000              ← TX done 中断正常
mcu-evt timeout: q6 tail=0 cidx=0x7f didx=0x0  ← 无 RX 事件
ALL RX rings 0-7: DIDX=0x00000000   ← MCU 未向任何 Ring 写入
Patch download failed: -110
```

## 本次会话的代码分析发现（未转化为代码修改）

### 发现 1：CONNAC3X TXD 格式对比

对比了 vendor 驱动 `asicConnac3xFillInitCmdTxdInfo()` 与我们的 `mcu_send_cmd()`：

| 字段 | 我们的值 | Vendor 值 | 匹配？ |
|------|---------|----------|--------|
| DW0 TX_BYTES | total_len | total_len | ✓ |
| DW0 PKT_FMT | 2 (CMD) | 2 (CMD) | ✓ |
| DW0 Q_IDX | 0x20 | 0x20 | ✓ |
| DW1 HDR_FORMAT | bits[15:14]=1 | bits[14:15]=1 | ✓ |
| DW1 BIT(31) | 设置 (LONG_FORMAT) | 未设置 | ✗ 差异 |
| DW2-7 | 全 0 | 全 0 | ✓ |
| len/u2TxByteCount (偏移 32-33) | total-32 | total-32 | ✓ |
| pq_id (偏移 34-35) | 0x0000 (溢出) | 0x0000 (未设置) | ✓ 意外匹配 |
| cid (偏移 36) | CID | CID | ✓ |
| pkt_type (偏移 37) | 0xA0 | 0xA0 | ✓ |

**关键结论**：除了 DW1 BIT(31)，我们的 TXD 格式与 vendor 完全匹配。

### 发现 2：PQ_ID 溢出问题（非关键）

`MCU_PQ_ID(1, 0x20) = (1<<15) | (0x20<<10) = 0x10000` → u16 溢出为 0x0000。
但 vendor 也没显式设置 pq_id，所以双方都是 0x0000。非关键问题。

### 发现 3：mt7996（另一个 CONNAC3X 芯片）不设置 BIT(31)

upstream mt76 的 mt7996 代码在 DW1 中不设置 BIT(31) 且工作正常。
说明我们需要 BIT(31) 可能是因为 WFDMA 配置差异（如 LONG_FORMAT 解释模式）。

### 发现 4：MCIF 中断重映射——**最可能的缺失项**

vendor `mt6639_mcu_init()` 在 MCU_IDLE 后执行了一个关键步骤：

```c
/* setup 1a/1b remapping for mcif interrupt */
HAL_MCR_WR(ad,
    CONN_BUS_CR_VON_CONN_INFRA_PCIE2AP_REMAP_WF_1_BA_ADDR,
    0x18051803);
```

**我们的 TODO step 6 正是这个**。这配置了 MCU→Host 的 PCIe 地址重映射，使 MCU 的 DMA 写操作能正确路由到主机内存。

**问题**：`conn_bus_cr_von.h` 头文件不在仓库中，BAR0 偏移地址未知。

已找到的线索：
- `CONN_INFRA_BUS_CR_PCIE2AP_REMAP_WF_0_54_ADDR = 0x7c021008` → BAR0 0xd1008
- `CONN_BUS_CR_VON_CONN_INFRA_PCIE2AP_REMAP_WF_1_BA_ADDR` 来自缺失的 `conn_bus_cr_von.h`
- mt7996 的 `CONN_BUS_CR_VON_BASE = 0x155000`（不同芯片，仅供参考）
- 需要从 Windows 驱动逆向或其他途径获取地址

## 当前核心问题

**MCU 消费了 TX 命令但不返回 RX 事件。最可能的原因：**

1. **MCIF 中断重映射缺失**（优先级最高）
   - vendor 驱动在 MCU_IDLE 后写 `PCIE2AP_REMAP_WF_1_BA = 0x18051803`
   - 没有此步骤，MCU 的 DMA 写操作无法正确映射到主机 PCIe 地址空间
   - 这解释了为什么 TX（host→MCU）正常但 RX（MCU→host）失败

2. **MCU ownership 未设置**
   - vendor step 4: `CB_INFRA_SLP_CTRL_CB_INFRA_CRYPTO_TOP_MCU_OWN_SET = BIT(0)`
   - 地址同样来自缺失的头文件

3. **固件兼容性**
   - 可能需要 MT6639 专用固件而非 MT7925 固件

## 下一步计划

### 优先级 1：获取 MCIF remap 寄存器地址
- 方法 A：用 Ghidra 逆向 Windows 驱动 (DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998)
  - 搜索写入值 0x18051803 的代码路径
  - 从中提取 BAR0 偏移地址
- 方法 B：暴力探测 0xd0000-0xd1fff 区域的寄存器
  - CONN_INFRA_BUS_CR 区域在 0x7c020000 → BAR0 0xd0000
  - 已知 REMAP_WF_0_54 在 0xd1008
  - REMAP_WF_1_BA 可能在附近偏移

### 优先级 2：实现 MCU ownership 设置
- 找到 CB_INFRA_SLP_CTRL 寄存器的 BAR0 偏移
- 在 WF 复位后写 BIT(0)

### 优先级 3：尝试 MT6639 固件
```bash
insmod mt7927_init_dma.ko fw_patch=mediatek/mt6639/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin
```

### 优先级 4：添加 DW1 BIT(31) 切换参数
- 添加 module_param 允许运行时关闭 BIT(31)
- 测试不设置 BIT(31) 但设置 MCIF remap 后的行为

## 关键技术发现汇总

| 项目 | CONNAC2 (MT7925) | CONNAC3 (MT6639/MT7927) | 状态 |
|------|-----------------|----------------------|------|
| RX Event Ring | Ring 0 | Ring 6 (hw) | ✓ 配置正确，无事件 |
| TXD DW1 HDR_FORMAT | bits [17:16] | bits [15:14] | ✓ 已实现 |
| TXD DW1 BIT(31) | LONG_FORMAT | FIXED_RATE（但需要设置） | ✓ 已设置 |
| TX FWDL Ring | Ring 16 | Ring 16 | ✓ 工作正常 |
| TX CMD Ring | Ring 15 | Ring 15 | ✓ 工作正常 |
| Patch 地址 | 0x200000 | 0x900000 | ✓ 已实现 |
| PKT_TYPE | 0xa0 | 0xa0 | ✓ 匹配 |
| PQ_ID | 0x8000 | 0x0000（双方相同） | ✓ 匹配 |
| MCU 初始化 | drv_own only | WF reset + MCU_IDLE | ✓ 已实现 |
| force_wf_reset | 不需要 | 必须（PCI 复位后） | ✓ 已实现 |
| cbinfra remap | 不需要 | 0x74037001 / 0x70007000 | ✓ 已实现 |
| **MCIF remap** | 不需要 | 0x18051803 | **✗ 地址未知** |
| MCU ownership | 不需要 | BIT(0) | **✗ 地址未知** |
| 固件文件 | WIFI_*_MT7925_*.bin | WIFI_*_MT6639_*.bin | **✗ 待尝试** |
| GLO_CFG 工作值 | — | write 0x5430b875, read 0x5430b87d | ✓ |
| Prefetch EXT_CTRL | ROM 默认 | WF 复位后需重写 | ✓ 已实现 |
