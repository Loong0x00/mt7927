# Windows MT6639/MT7927 寄存器写入完整序列

**日期**: 2026-02-15
**来源**: Windows RE 逆向分析 (mtkwecx.sys v5603998 + v5705275)
**目标**: 作为 Linux 驱动从零重写的施工蓝图

---

## 核心发现总结

### Windows vs 我们的驱动的关键差异

| 项目 | Windows 驱动 | 我们的驱动 | 影响 |
|------|-------------|-----------|------|
| HOST RX rings | **Ring 4, 6, 7** (3个) | Ring 0(Mode53), 4, **5**, 7 | ⚠️⚠️⚠️ Ring 5 应该是 Ring 6 |
| 预取配置 | 4个寄存器配置 | **缺失** | ⚠️⚠️ 可能影响 DMA |
| GLO_CFG_EXT BIT(28) | 无条件设置 | **缺失** | ⚠️⚠️ 必需 |
| 中断掩码 | 0x2600f000 | 需检查 | ⚠️ 不匹配 |
| PostFwDL 寄存器写入 | **仅 0xd6060 \|= 0x10101** | 可能缺失 | ⚠️⚠️ MCU 命令前唯一写入 |

### 初始化时间线概览

```
1. MT6639PreFirmwareDownloadInit  — 芯片状态检查 + 轮询就绪
2. LoadRomPatch                   — 补丁下载
3. DMA Init:
   a. InitTxRxRing                — 配置 4 TX + 3 RX rings (4,6,7)
   b. WpdmaConfig                 — 预取 + GLO_CFG 启用
   c. ConfigIntMask               — 中断掩码
4. LoadFirmware                   — RAM 下载 + FW_START
5. Post-FW Intermediate           — (待分析)
6. HIF Init                       — PCIe 中断
7. PostFwDownloadInit:
   a. 0xd6060 |= 0x10101          ← DMASHDL 启用
   b. 9 条 MCU 命令               ← NIC_CAP 等
```

---

## 阶段 1: PreFirmwareDownloadInit

**函数**: `FUN_1401e5430` (MT6639PreFirmwareDownloadInit)

### 操作序列

| 顺序 | 操作类型 | 地址 | 值/操作 | 确定性 | 备注 |
|------|---------|------|---------|--------|------|
| 1 | 条件检查 | `ctx+0x14669e0` | `== 0` 判断首次初始化 | ✅ | 首次才记录日志 |
| 2 | 函数调用 | FUN_1401ce900 | 读取 `0x7c0600f0` (fw_sync) | ✅ | 检查芯片状态 |
| 3a | 成功分支 | `ctx+0x1464d29` | 写入 `0` | ✅ | 清除错误标志 |
| 3b | 失败分支 | `ctx+0x1464d29` | 写入 `1` | ✅ | 设置错误标志 |
| 4 | 函数调用 | FUN_14000d410 | 底层初始化（调用 FUN_140058eb8） | ✅ | 关键步骤 |
| 5 | 成功轮询 | 循环调用 FUN_1401ce900 | 轮询直到返回 `1` | ✅ | 最多 500 次，每次 1ms |
| 6 | 超时处理 | — | 返回 `0xc0000001` | ✅ | 500ms 超时 |

### 关键寄存器

| BAR0 偏移 | 总线地址 | 操作 | 值 | 确定性 | 来源 |
|-----------|---------|------|-----|--------|------|
| 未知 (需 L1 remap) | 0x7c0600f0 | 读取 | fw_sync 状态值 | ✅ | FUN_1401ce900 |

### 依赖关系
- **步骤 4 必须在步骤 5 之前** — 底层初始化后才能轮询
- **步骤 5 成功是 FWDL 前提** — 必须等到状态返回 `1`

---

## 阶段 2: InitTxRxRing

**函数**: `FUN_1401e4580` (MT6639InitTxRxRing)

### 2a. TX Ring 配置（4 个环）

| Ring | HW 偏移 | BASE 寄存器 | CNT 寄存器 | CIDX 寄存器 | DIDX 寄存器 | 确定性 |
|------|---------|-------------|-----------|-------------|-------------|--------|
| TX 0 | 0x00 | 0xd4300 (0x7c024300) | 0xd4304 | 0xd4308 | 0xd430c | ✅ |
| TX 1 | 0x10 | 0xd4310 (0x7c024310) | 0xd4314 | 0xd4318 | 0xd431c | ✅ |
| TX 2 | `config[0xd1] << 4` | 动态计算 | 动态 | 动态 | 动态 | ⚠️ 运行时确定 |
| TX 3 | `config[0xd0] << 4` | 动态计算 | 动态 | 动态 | 动态 | ⚠️ 运行时确定 |

**注意**: TX 2/3 的 HW ring 编号来自配置结构体，其中一个可能是 **ring 15**（MCU 命令 TX）。

### 2b. RX Ring 配置（3 个环）⚠️⚠️⚠️

| SW 索引 | HW Ring | HW 偏移 | BASE 寄存器 | CNT 寄存器 | CIDX 寄存器 | DIDX 寄存器 | 确定性 |
|---------|---------|---------|-------------|-----------|-------------|-------------|--------|
| RX 0 | **Ring 4** | **0x40** | **0xd4540** (0x7c024540) | 0xd4544 | 0xd4548 | 0xd454c | ✅ |
| RX 1 | **Ring 6** | **0x60** | **0xd4560** (0x7c024560) | 0xd4564 | 0xd4568 | 0xd456c | ✅ |
| RX 2 | **Ring 7** | **0x70** | **0xd4570** (0x7c024570) | 0xd4574 | 0xd4578 | 0xd457c | ✅ |

**⚠️⚠️⚠️ 关键发现**: Windows 使用 **Ring 4, 6, 7**，我们的驱动用的是 Ring 4, **5**, 7。
**Ring 5 寄存器地址完全不同于 Ring 6**:
- Ring 5: BASE=0xd4550, CIDX=0xd4558, DIDX=0xd455c
- Ring 6: BASE=0xd4560, CIDX=0xd4568, DIDX=0xd456c

### 寄存器计算公式

```c
// TX rings
base_reg = 0x7c024300 + (hw_ring_idx << 4);
cnt_reg  = base_reg + 0x04;
cidx_reg = base_reg + 0x08;
didx_reg = base_reg + 0x0c;

// RX rings
base_reg = 0x7c024500 + (hw_ring_idx << 4);
cnt_reg  = base_reg + 0x04;
cidx_reg = base_reg + 0x08;
didx_reg = base_reg + 0x0c;
```

### 写入顺序（每个 ring）

| 顺序 | 操作 | 值 | 确定性 |
|------|------|-----|--------|
| 1 | 写 BASE | DMA 物理地址（低 32 位） | ✅ |
| 2 | 写 CNT | ring 条目数 | ✅ |
| 3 | 写 CIDX | `ring_count - 1` (RX), `0` (TX) | ✅ |
| 4 | 写 DIDX | `0` | ✅ |
| 5 | RX: 初始化描述符 | 清 BIT(31), 设 buffer 大小 | ✅ |

### RX 描述符初始化细节

```c
for (each RX descriptor) {
    rxd->dw0 &= ~BIT(31);           // 清除 DMA_DONE
    rxd->dw0 &= ~0x3fff0000;        // 清除旧的 buffer 大小
    rxd->dw0 |= (buf_size << 16);   // 写入 buffer 大小到高 14 位
}
```

---

## 阶段 3: WpdmaConfig

**函数**: `FUN_1401e5be0` (MT6639WpdmaConfig)

### 寄存器写入序列

| 顺序 | BAR0 偏移 | 总线地址 | 操作 | 值 | 确定性 | 备注 |
|------|-----------|---------|------|-----|--------|------|
| 1 | — | — | 调用 FUN_1401d8724 | sub-setup | ✅ | 子设置函数 |
| 2 | 0xd4208 | 0x7c024208 | 读取 | GLO_CFG | ✅ | 保存原值 |
| 3a | 0xd7030 | 0x7c027030 | 读取 + 写入 | 读回值 | ✅ | 触发预取重置 |
| 3b | 0xd70f0 | 0x7c0270f0 | 写入 | **0x660077** | ✅ | 预取配置 0 |
| 3c | 0xd70f4 | 0x7c0270f4 | 写入 | **0x1100** | ✅ | 预取配置 1 |
| 3d | 0xd70f8 | 0x7c0270f8 | 写入 | **0x30004f** | ✅ | 预取配置 2 |
| 3e | 0xd70fc | 0x7c0270fc | 写入 | **0x542200** | ✅ | 预取配置 3 |
| 4 | 0xd4208 | 0x7c024208 | 写入 | `GLO_CFG \| 0x5` | ✅ | 启用 TX_DMA + RX_DMA |
| 5 | 0xd42b4 | 0x7c0242b4 | 读取 | GLO_CFG_EXT | ✅ | 读原值 |
| 6 | 0xd42b4 | 0x7c0242b4 | 写入 | `val \| 0x10000000` | ✅ | **设置 BIT(28)** |

### 条件执行

- **步骤 3a-3e**: 仅在 `*(ctx+0x1466a49) != 0` 时执行（预取启用标志）
- **步骤 4**: 仅在 `enable=1` 时执行
- **步骤 5-6**: **无条件执行**（即使 enable=0）

### GLO_CFG 位域

| 位 | 名称 | Windows 设置 | 备注 |
|----|------|-------------|------|
| 0 | TX_DMA_EN | 1 | 启用 TX DMA |
| 2 | RX_DMA_EN | 1 | 启用 RX DMA |
| 其他 | — | 保持原值 | OR 操作 |

### MT7925 vs MT6639 差异

MT7925 的 GLO_CFG 设置：`GLO_CFG | 0x4000005` (额外设置 BIT(26))
MT6639/MT7927: `GLO_CFG | 0x5`

---

## 阶段 4: ConfigIntMask

**函数**: `FUN_1401e43e0` (MT6639ConfigIntMask)

### 寄存器写入序列

| 顺序 | BAR0 偏移 | 总线地址 | 操作 | 值 | 确定性 | 备注 |
|------|-----------|---------|------|-----|--------|------|
| 1a | 0xd4228 | 0x7c024228 | 写入 (enable=1) | **0x2600f000** | ✅ | INT_ENA_SET |
| 1b | 0xd422c | 0x7c02422c | 写入 (enable=0) | **0x2600f000** | ✅ | INT_ENA_CLR |
| 2 | 0xd4204 | 0x7c024204 | 读取 | INT_STA_EXT | ✅ | 状态读取（调试用） |
| 3 | ❓ | 0x74030188 | 读改写 | `val \| BIT(16)` 或 `val & ~BIT(16)` | ⚠️ | MT6639/MT7927 特定 |

### 中断掩码 0x2600f000 分析

```
0x2600f000 = 0010_0110_0000_0000_1111_0000_0000_0000
  BIT(29) = MCU2HOST_SW_INT (MCU 命令完成中断)
  BIT(25) = 未确定（可能是 WDT）
  BIT(15:12) = 0xF = RX ring 完成中断（ring 4/5/6/7）
```

### 地址映射问题 ⚠️

**0x74030188** 在我们的 bus2chip 映射表中可能不存在，需要确认：
- 可能需要 L1 remap
- 或者是不同的 BAR 空间

---

## 阶段 5: 固件下载 (FWDL)

**函数**: `AsicConnac3xLoadFirmware` (FUN_1401c5020)

### 简化序列（已在驱动中工作）

| 阶段 | 操作 | 关键寄存器 | 备注 |
|------|------|-----------|------|
| 5.1 | SET_OWN | 0xe0010 (LPCTL) 写 BIT(0) | 必须先于 CLR_OWN |
| 5.2 | 等待 OWN_SYNC | 0xe0010 读取，等待 BIT(1)=1 | 约 10ms |
| 5.3 | CLR_OWN | 0xe0010 写 BIT(1) | 触发 ROM 初始化 |
| 5.4 | 等待清除完成 | 0xe0010 轮询，等待 BIT(1)=0 | 约 10ms |
| 5.5 | patch_sem | — | 补丁信号量 |
| 5.6 | init_dl | — | 初始化下载 |
| 5.7 | scatter (patch) | — | 补丁数据 |
| 5.8 | patch_finish | — | 完成信号 |
| 5.9 | sem_release | — | 释放信号量 |
| 5.10 | init_dl (RAM) | — | **必须带加密标志** |
| 5.11 | scatter (RAM regions) | — | DL_MODE_ENCRYPT\|DL_MODE_RESET_SEC_IV |
| 5.12 | FW_START_OVERRIDE | — | option=1 |
| 5.13 | 轮询 fw_sync | 读取 fw_sync 寄存器 | 等待 = 0x3 |
| 5.14 | mt7927_dma_reprogram_rings | — | **CLR_OWN 副作用** |

### ⚠️ CLR_OWN 副作用（关键！）

ROM 在 CLR_OWN 期间会：
1. **清零所有 HOST ring BASE 寄存器**
2. 禁用 GLO_CFG
3. 清除 INT_ENA
4. **仅配置 MCU_RX2/RX3**（FWDL ring）
5. **不配置 MCU_RX0/RX1**

**因此必须在步骤 5.14 重新编程所有 HOST ring！**

---

## 阶段 6: PostFwDownloadInit ⚠️⚠️⚠️

**函数**: `FUN_1401c9510` (AsicConnac3xPostFwDownloadInit)

### 关键发现：MCU 命令前**仅一个寄存器写入**

| 顺序 | BAR0 偏移 | 总线地址 | 操作 | 值 | 确定性 | 备注 |
|------|-----------|---------|------|-----|--------|------|
| 0 | — | `ctx+0x146e61c` | 写入 | `0` | ✅ | 清除标志（非 0x146e621） |
| 1 | **0xd6060** | **0x7c026060** | 读改写 | `val \| 0x10101` | ✅ | **DMASHDL 启用** |

**然后立即开始 MCU 命令序列** ↓

### MCU 命令序列（9 条命令）

| 顺序 | Class | Target | Subcmd | Payload | 用途 | 确定性 |
|------|-------|--------|--------|---------|------|--------|
| 2 | 0x8a | 0xed | 0 | NULL, len=0 | **NIC Capability** | ✅ |
| 3 | 0x02 | 0xed | 0 | {tag=1, pad=0, value=0x70000} | Config 命令 | ✅ |
| 4 | 0xc0 | 0xed | 0 | {0x820cc800, 0x3c200} | Config 命令 | ✅ |
| 5 | 0xed | 0xed | 0x21 | Buffer 数据（可选） | AsicConnac3xDownloadBufferBin | ⚠️ 条件执行 |
| 6 | 0x28 | 0xed | 0 | DBDC 参数（0x24 字节） | **仅 MT6639/MT7927** | ✅ |
| 7 | — | — | — | 1ms 延迟 | KeStallExecutionProcessor | ✅ |
| 8 | 0xca | 0xed | — | PassiveToActiveScan | SetPassiveToActiveScan | ✅ |
| 9 | 0xca | 0xed | — | FWChipConfig | SetFWChipConfig | ✅ |
| 10 | 0xca | 0xed | — | LogLevelConfig | SetLogLevelConfig | ✅ |

### TXD 格式（关键！）

**PostFwDownloadInit 使用哪条路径？**

取决于 `ctx+0x146e621` 标志：
- `0` → **Legacy 路径** (0x40 字节头部)
- `1` → CONNAC3 UniCmd 路径 (0x30 字节头部)

**WoWLAN 恢复流程**中，在 ToggleWfsysRst 之前会清除此标志为 `0`，因此 PostFwDownloadInit 可能使用 **Legacy 路径**。

#### Legacy TXD 格式 (flag=0)

```c
// 总长度 = payload_len + 0x40
TXD[0x00] = total_len | 0x41000000;  // Q_IDX=0x20, PKT_FMT=2
TXD[0x04] = flags | 0x4000;          // HDR_FORMAT_V3=1, 无 BIT(31)
TXD[0x20] = (uint16)(payload_len + 0x20);  // packet length
TXD[0x24] = class;                   // 0x8a, 0x02, 0xc0, etc.
TXD[0x25] = 0xa0;                    // type (固定)
TXD[0x27] = seq;                     // sequence number
// Payload 从 +0x40 开始
memcpy(TXD + 0x40, payload, payload_len);
```

#### CONNAC3 UniCmd TXD 格式 (flag=1)

```c
// 总长度 = payload_len + 0x30
TXD[0x00] = total_len | 0x41000000;
TXD[0x04] = flags | 0x4000;
TXD[0x20] = (uint16)(payload_len + 0x10);
TXD[0x24] = class;
TXD[0x25] = 0xa0;
TXD[0x27] = seq;
TXD[0x29] = subcmd;  // 用于 target=0xed
// Payload 从 +0x30 开始
memcpy(TXD + 0x30, payload, payload_len);
```

### Q_IDX 说明

**Windows 使用 Q_IDX=0x20** (MT_TX_MCU_PORT_RX_Q0)，对应：
```c
TXD[0] = ... | 0x41000000;
  BIT(30) = 1 (QUEUE_INDEX_EXT)
  BIT(24) = 1 (PKT_FMT=2, MCU 命令)
  Q_IDX = 0x20 (bits [31:27,25] = 0b100000)
```

---

## 阶段 7: ToggleWfsysRst（可选，仅用于复位恢复）

**函数**: `FUN_1401cb360` (AsicConnac3xToggleWfsysRst)

**警告**: 此序列仅用于 WoWLAN 恢复或错误恢复，**不在正常初始化中使用**。

### 完整 19 步序列

| 顺序 | BAR0 偏移 | 总线地址 | 操作 | 值 | 确定性 | 备注 |
|------|-----------|---------|------|-----|--------|------|
| 1 | ❓ (需 L1 remap) | 0x7c011100 | 读改写 | `val \| BIT(1)` | ✅ | 唤醒 CONN_INFRA |
| 2a | 0xf1600 | 0x7c001600 | 读改写 | `val & ~0xF` | ✅ | 清除睡眠保护 |
| 2b | 0xf1620 | 0x7c001620 | 条件写 | `val & 0x3` | ✅ | 清除 HIF 状态 1 |
| 2c | 0xf1630 | 0x7c001630 | 条件写 | `val & 0x3` | ✅ | 清除 HIF 状态 2 |
| 3 | — | — | Driver own check | — | ✅ | 检查驱动所有权 |
| 4 | 0xc3f00 | 0x81023f00 | 写入 | **0xc0000100** | ✅ | Pre-reset MCU reg |
| 5 | 0xc3008 | 0x81023008 | 写入 | **0** | ✅ | Pre-reset MCU reg |
| 6 | 0x1f8600 | 0x70028600 | 读取 | CB_INFRA_RGU 状态 | ✅ | 保存原值 |
| 7 | — | — | 检查停止标志 | — | ✅ | 如果设备停止则中止 |
| 8 | 0x1f8600 | 0x70028600 | 写入 | `val \| BIT(4)` | ✅ | **ASSERT WFSYS RESET** |
| 9 | — | — | 延迟 | 1000 µs | ✅ | 等待复位生效 |
| 10 | 0x1f8600 | 0x70028600 | 读取验证 | 检查 BIT(4)=1 | ✅ | 重试最多 5 次 |
| 11 | — | — | 延迟 | 20000 µs | ✅ | 等待复位完成 |
| 12 | 0x1f8600 | 0x70028600 | 写入 | `val & ~BIT(4)` | ✅ | **DEASSERT WFSYS RESET** |
| 13 | — | — | 延迟 | 200 µs | ✅ | 稳定时间 |
| 14 | 0xc1604 | 0x81021604 | 轮询 | 等待 = **0x1d1e** | ✅ | WFSYS_SW_INIT_DONE |
| 15 | 0x1f8600/0x1f8610 | 0x70028600/610 | 读取（失败时） | 调试信息 | ✅ | 失败时读取 |
| 16 | 0xe0010 | 0x7c060010 | 写入 | **BIT(0)** | ✅ | Assert CONN_INFRA reset |
| 17 | 0xe0010 | 0x7c060010 | 轮询 | 等待 BIT(2)=1 | ✅ | 最多 49 次 |
| 18 | 0xe0010 | 0x7c060010 | 写入 | **BIT(1)** | ✅ | Deassert CONN_INFRA reset |
| 19 | — | — | 额外状态检查 | — | ✅ | 最终验证 |

### ⚠️ 警告

**步骤 8 的 CB_INFRA_RGU 复位会使设备进入不可恢复状态（除非重启）**，已在我们的 Mode 40/52 中验证。**仅用于错误恢复，不用于正常初始化。**

---

## 附录 A: 完整寄存器速查表

按 BAR0 偏移排序，包含所有初始化中出现的寄存器。

| BAR0 偏移 | 总线地址 | 名称 | 读/写 | 值/操作 | 阶段 | 确定性 |
|-----------|---------|------|-------|---------|------|--------|
| 0xc1604 | 0x81021604 | **WFSYS_SW_INIT_DONE** | R | 轮询 = 0x1d1e | 1, 7 | ✅ |
| 0xc3008 | 0x81023008 | Pre-reset MCU reg | W | 0 | 7 | ✅ |
| 0xc3f00 | 0x81023f00 | Pre-reset MCU reg | W | 0xc0000100 | 7 | ✅ |
| 0xd4200 | 0x7c024200 | INT_STA | R | 中断状态 | — | ✅ |
| 0xd4204 | 0x7c024204 | INT_STA_EXT | R | 扩展中断状态 | 4 | ✅ |
| **0xd4208** | **0x7c024208** | **GLO_CFG** | RW | **\|= 0x5** | **3** | **✅** |
| 0xd4228 | 0x7c024228 | INT_ENA_SET | W | 0x2600f000 | 4 | ✅ |
| 0xd422c | 0x7c02422c | INT_ENA_CLR | W | 0x2600f000 | 4 | ✅ |
| 0xd4238 | 0x7c024238 | HOST_INT_STA_EXT | R | 诊断 | — | ✅ |
| **0xd42b4** | **0x7c0242b4** | **GLO_CFG_EXT** | **RW** | **\|= 0x10000000** | **3** | **✅** |
| 0xd4300 | 0x7c024300 | TX Ring 0 BASE | W | DMA 物理地址 | 2 | ✅ |
| 0xd4304 | 0x7c024304 | TX Ring 0 CNT | W | 条目数 | 2 | ✅ |
| 0xd4308 | 0x7c024308 | TX Ring 0 CIDX | W | 0 | 2 | ✅ |
| 0xd430c | 0x7c02430c | TX Ring 0 DIDX | W | 0 | 2 | ✅ |
| 0xd4310-31c | 0x7c024310-31c | TX Ring 1 (同上) | W | — | 2 | ✅ |
| 0xd4540 | 0x7c024540 | **RX Ring 4 BASE** | W | DMA 物理地址 | 2 | ✅ |
| 0xd4544 | 0x7c024544 | **RX Ring 4 CNT** | W | 条目数 | 2 | ✅ |
| 0xd4548 | 0x7c024548 | **RX Ring 4 CIDX** | W | ring_count - 1 | 2 | ✅ |
| 0xd454c | 0x7c02454c | **RX Ring 4 DIDX** | W | 0 | 2 | ✅ |
| **0xd4560** | **0x7c024560** | **RX Ring 6 BASE** ⚠️ | **W** | **DMA 物理地址** | **2** | **✅** |
| **0xd4564** | **0x7c024564** | **RX Ring 6 CNT** ⚠️ | **W** | **条目数** | **2** | **✅** |
| **0xd4568** | **0x7c024568** | **RX Ring 6 CIDX** ⚠️ | **W** | **ring_count - 1** | **2** | **✅** |
| **0xd456c** | **0x7c02456c** | **RX Ring 6 DIDX** ⚠️ | **W** | **0** | **2** | **✅** |
| 0xd4570 | 0x7c024570 | **RX Ring 7 BASE** | W | DMA 物理地址 | 2 | ✅ |
| 0xd4574 | 0x7c024574 | **RX Ring 7 CNT** | W | 条目数 | 2 | ✅ |
| 0xd4578 | 0x7c024578 | **RX Ring 7 CIDX** | W | ring_count - 1 | 2 | ✅ |
| 0xd457c | 0x7c02457c | **RX Ring 7 DIDX** | W | 0 | 2 | ✅ |
| **0xd6060** | **0x7c026060** | **DMASHDL enable** | **RW** | **\|= 0x10101** | **6** | **✅** |
| 0xd7030 | 0x7c027030 | Prefetch control | RW | 读回写 | 3 | ✅ |
| **0xd70f0** | **0x7c0270f0** | **Prefetch cfg 0** | **W** | **0x660077** | **3** | **✅** |
| **0xd70f4** | **0x7c0270f4** | **Prefetch cfg 1** | **W** | **0x1100** | **3** | **✅** |
| **0xd70f8** | **0x7c0270f8** | **Prefetch cfg 2** | **W** | **0x30004f** | **3** | **✅** |
| **0xd70fc** | **0x7c0270fc** | **Prefetch cfg 3** | **W** | **0x542200** | **3** | **✅** |
| **0xe0010** | **0x7c060010** | **LPCTL / CONN_INFRA_RST** | **RW** | **多用途** | **5, 7** | **✅** |
| 0xf1600 | 0x7c001600 | Sleep protection | RW | &= ~0xF | 7 | ✅ |
| 0xf1620 | 0x7c001620 | HIF status 1 | RW | 清低 2 位 | 7 | ✅ |
| 0xf1630 | 0x7c001630 | HIF status 2 | RW | 清低 2 位 | 7 | ✅ |
| 0x1f5018 | 0x70025018 | CB_INFRA PCIe sleep cfg | RW | 0xFFFFFFFF | — | ✅ |
| 0x1f8600 | 0x70028600 | **CB_INFRA_RGU WF_SUBSYS_RST** | RW | BIT(4) 控制 | 7 | ✅ |
| 0x1f8610 | 0x70028610 | CB_INFRA_RGU debug | R | 调试信息 | 7 | ✅ |
| ❓ | 0x7c011100 | CONN_INFRA_WAKEUP | RW | \|= BIT(1) | 7 | ⚠️ 需 L1 remap |
| ❓ | 0x74030188 | 芯片特定中断 | RW | BIT(16) 控制 | 4 | ⚠️ 需确认映射 |
| ❓ | 0x7c0600f0 | fw_sync (推测) | R | 芯片状态 | 1 | ⚠️ 需确认 |

---

## 附录 B: 我们驱动缺失的寄存器操作（按优先级）

### 🔴 严重缺失（可能导致 MCU 命令失败）

#### 1. HOST RX Ring 5 → 6 的修正 ⚠️⚠️⚠️

**影响**: 如果 Ring 6 是 MCU 事件接收环，用错 ring 编号将导致固件无法发送响应。

**修复**:
```c
// 当前（错误）
#define MT7927_RXQ_MCU_WM_EVENT  5  // HW offset 0x50

// 应改为
#define MT7927_RXQ_MCU_WM_EVENT  6  // HW offset 0x60

// 寄存器地址
#define MT7927_RX_RING_6_BASE    0xd4560  // 当前用的是 0xd4550
#define MT7927_RX_RING_6_CIDX    0xd4568  // 当前用的是 0xd4558
#define MT7927_RX_RING_6_DIDX    0xd456c  // 当前用的是 0xd455c
```

#### 2. 预取配置（4 个寄存器）⚠️⚠️

**影响**: 影响 WFDMA 如何从 DDR 读取 ring 描述符，可能导致 DMA 性能问题或异常。

**修复**:
```c
void mt7927_wpdma_prefetch_config(struct mt7927_dev *dev) {
    uint32_t val;

    // 触发预取重置
    val = readl(dev->bar0 + 0xd7030);
    writel(val, dev->bar0 + 0xd7030);

    // 写入预取配置值
    writel(0x660077, dev->bar0 + 0xd70f0);
    writel(0x1100,   dev->bar0 + 0xd70f4);
    writel(0x30004f, dev->bar0 + 0xd70f8);
    writel(0x542200, dev->bar0 + 0xd70fc);
}
```

#### 3. GLO_CFG_EXT BIT(28) ⚠️⚠️

**影响**: Windows 无条件设置此位，我们未设置。

**修复**:
```c
void mt7927_wpdma_config(struct mt7927_dev *dev, bool enable) {
    // ... 现有 GLO_CFG 操作 ...

    // 无条件设置 GLO_CFG_EXT BIT(28)
    uint32_t val = readl(dev->bar0 + 0xd42b4);
    writel(val | BIT(28), dev->bar0 + 0xd42b4);
}
```

#### 4. PostFwDL 的 DMASHDL 启用 ⚠️⚠️

**影响**: MCU 命令之前的唯一寄存器写入，可能启用 MCU 命令路由。

**修复**:
```c
void mt7927_post_fw_download_init(struct mt7927_dev *dev) {
    uint32_t val;

    // MCU 命令之前的唯一寄存器写入
    val = readl(dev->bar0 + 0xd6060);
    writel(val | 0x10101, dev->bar0 + 0xd6060);

    // 然后开始 MCU 命令序列...
}
```

### 🟡 重要缺失（可能影响稳定性）

#### 5. 中断掩码值不匹配 ⚠️

**修复**:
```c
#define MT7927_INT_MASK  0x2600f000
// BIT(29) = MCU2HOST_SW_INT
// BIT(25) = 未确定
// BIT(15:12) = RX ring 完成中断
```

#### 6. 0x74030188 芯片特定中断寄存器 ⚠️

**问题**: 地址需要确认 BAR0 映射。

#### 7. RX Ring CIDX 初始值 ⚠️

**修复**:
```c
// Windows 设置 RX CIDX = ring_count - 1（预设消费者索引）
writel(ring->count - 1, dev->bar0 + ring->cidx_reg);
```

#### 8. PreFirmwareDownloadInit 轮询 ⚠️

**修复**: 添加芯片状态轮询，最多 500ms。

### 🟢 次要缺失

#### 9. CB_INFRA PCIe 睡眠配置

```c
void mt7927_disable_pcie_sleep(struct mt7927_dev *dev) {
    uint32_t val = readl(dev->bar0 + 0x1f5018);
    if (val != 0xFFFFFFFF) {
        writel(0xFFFFFFFF, dev->bar0 + 0x1f5018);
    }
}
```

#### 10. RX 描述符初始化验证

确保清除 BIT(31) 并正确设置 buffer 大小到高 14 位。

---

## 附录 C: 立即行动建议（Mode 54）

### 最小改动测试（优先级排序）

#### Test 1: Ring 5 → 6 + DMASHDL 启用

```c
// 1. 将 ring_rx5 改为 ring_rx6
ring_rx6.hw_idx = 6;  // 从 5 改为 6
ring_rx6.base_reg = 0xd4560;  // 从 0xd4550
ring_rx6.cidx_reg = 0xd4568;  // 从 0xd4558
ring_rx6.didx_reg = 0xd456c;  // 从 0xd455c

// 2. Post-FW 添加 DMASHDL
val = readl(bar0 + 0xd6060);
writel(val | 0x10101, bar0 + 0xd6060);
```

#### Test 2: 添加预取配置

```c
// 在 WpdmaConfig 中
val = readl(bar0 + 0xd7030);
writel(val, bar0 + 0xd7030);
writel(0x660077, bar0 + 0xd70f0);
writel(0x1100,   bar0 + 0xd70f4);
writel(0x30004f, bar0 + 0xd70f8);
writel(0x542200, bar0 + 0xd70fc);
```

#### Test 3: GLO_CFG_EXT BIT(28)

```c
val = readl(bar0 + 0xd42b4);
writel(val | BIT(28), bar0 + 0xd42b4);
```

#### Test 4: 中断掩码修正

```c
writel(0x2600f000, bar0 + 0xd4228);  // INT_ENA_SET
```

---

## 附录 D: 关键确定性数据汇总

### 100% 确定的值（可直接使用）

| 寄存器 | 值 | 操作 |
|--------|-----|------|
| 0xd70f0 | 0x660077 | 写入 |
| 0xd70f4 | 0x1100 | 写入 |
| 0xd70f8 | 0x30004f | 写入 |
| 0xd70fc | 0x542200 | 写入 |
| 0xd4208 | \|= 0x5 | OR 操作 |
| 0xd42b4 | \|= 0x10000000 | OR 操作 |
| 0xd6060 | \|= 0x10101 | OR 操作 |
| 0xd4228/0xd422c | 0x2600f000 | 写入 |
| 0xc1604 | 0x1d1e | 轮询等待 |

### 运行时确定的值

| 项目 | 来源 |
|------|------|
| TX Ring 2/3 HW 编号 | `config[0xd0]`, `config[0xd1]` |
| Ring 物理地址 | DMA 分配结果 |
| Ring 条目数 | 驱动配置 |

### 需要确认的映射

| 总线地址 | 用途 | 问题 |
|---------|------|------|
| 0x7c011100 | CONN_INFRA_WAKEUP | 需 L1 remap |
| 0x74030188 | 芯片特定中断 | BAR0 映射未知 |
| 0x7c0600f0 | fw_sync (推测) | 需验证 |

---

## 附录 E: 与 mt7925 差异对照

| 项目 | MT6639/MT7927 | MT7925 |
|------|---------------|--------|
| GLO_CFG 启用值 | 0x5 | 0x4000005 (多 BIT(26)) |
| CB_INFRA_RGU 复位位 | BIT(4) | BIT(0) |
| DBDC 命令 (class=0x28) | **需要** | 不需要 |
| HOST RX rings | 4, 6, 7 | 0, 4, 5, 7 (4 个) |
| 芯片架构 | CONNAC3 | CONNAC2 |

---

*文档结束 — 2026-02-15*

**下一步**: 基于此 playbook 实现 Mode 54，按优先级修复缺失的寄存器操作。
