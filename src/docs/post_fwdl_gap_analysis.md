# Post-FWDL Gap 分析：fw_sync=0x3 到 NIC_CAPABILITY 命令之间的缺失步骤

**日期**: 2026-02-15
**目标**: 找出 FWDL 完成（fw_sync=0x3）到第一条 MCU 命令成功之间所有缺失的步骤
**当前问题**: NIC_CAP UniCmd 被 WFDMA 消费（TX15 DIDX 前进），但无 DMA 响应（RX6 DIDX 不前进），MCU_RX0 BASE=0，MCU_CMD=0x8000

---

## 一、核心发现总结 🎯

### 1. **Windows 驱动 Post-FWDL 序列（来自 Ghidra RE）**

**唯一的寄存器写入**（在任何 MCU 命令之前）：
```c
// BAR0+0xd6060 |= 0x10101  (DMASHDL enable)
READ(ctx, 0x7c026060, &val);
val |= 0x10101;  // BIT(0)|BIT(8)|BIT(16)
WRITE(ctx, 0x7c026060, val);
```

**9 条 MCU 命令**（全部使用 target=0xed）：
1. class=0x8a (NIC capability) ← 第一条命令
2. class=0x02 (config {1, 0, 0x70000})
3. class=0xc0 ({0x820cc800, 0x3c200})
4. class=0xed (buffer download, optional)
5. class=0x28 (DBDC, **仅 MT6639/MT7927**)
6. class=0xca (SetPassiveToActiveScan)
7. class=0xca (SetFWChipConfig)
8. class=0xca (SetLogLevelConfig)
9. 其他可选命令

**关键**: Windows 在 FWDL 后**立即**发送 MCU 命令，**没有**中间的握手或等待步骤。

---

### 2. **Upstream mt7925 Post-FWDL 序列**

**源文件**: `mt76/mt7925/mcu.c:1045-1064` — `mt7925_run_firmware()`

```c
int mt7925_run_firmware(struct mt792x_dev *dev)
{
    // 1️⃣ FWDL（patch + RAM）
    err = mt792x_load_firmware(dev);
    if (err)
        return err;

    // 2️⃣ 【第一个 post-boot MCU 命令】
    err = mt7925_mcu_get_nic_capability(dev);  // ← 关键！
    if (err)
        return err;

    // 3️⃣ 加载 CLC（Country/Region 配置）
    err = mt7925_load_clc(dev, mt792x_ram_name(dev));

    // 4️⃣ 启用 FW log
    return mt7925_mcu_fw_log_2_host(dev, 1);
}
```

**第一个 MCU 命令**: `MCU_UNI_CMD(CHIP_CONFIG)` with tag `UNI_CHIP_CONFIG_NIC_CAPA`

**关键**: mt7925 也是 FWDL 后**立即**发送 MCU 命令，**没有**额外的寄存器写入或握手。

---

### 3. **Vendor mt6639 Post-FWDL 序列**

**源文件**: `mt6639/common/wlan_lib.c` — `wlanAdapterStart()`

```
Phase 3: Post-FW-Ready
  9. wlanCheckWifiFunc(TRUE):
     - 轮询 0x7C0600F0 for bits 0x3
     - 等效于我们的 fw_sync=0x3

  10. wlanQueryNicResourceInformation()
      → 纯本地操作（nicTxResetResource），不发送 MCU 命令

  11. wlanQueryNicCapabilityV2()  ← 第一条实际 MCU 命令
      → CMD_ID_GET_NIC_CAPABILITY_V2 via HOST TX ring 15
```

**关键**: Vendor 也是 fw_sync=0x3 后**立即**发送 NIC_CAP 命令，**没有**中间步骤。

**注意**: Vendor 为 CONNAC3X **不**发送 dummy command（代码中明确跳过）。

---

## 二、我们遗漏了什么？

### 对比分析：Windows vs Upstream vs Vendor vs 我们

| 步骤 | Windows | mt7925 | Vendor mt6639 | 我们的驱动 |
|------|---------|--------|--------------|-----------|
| **DMASHDL 启用** | ✅ 0xd6060 \|= 0x10101 | ❌ 无此写入 | ✅ 在 DMA init | ❌ **缺失** |
| **HOST RX ring 0** | ✅ ring 4,6,7 | ✅ ring 0,2 | ✅ ring 4,5,6,7 | ❌ ring 4,5,6,7（**无 ring 0**）|
| **预取配置** | ✅ 0xd70f0-fc | ❓ 未确认 | ✅ manual prefetch | ❌ **缺失** |
| **中断掩码** | ✅ 0x2600f000 | ✅ AUTO | ✅ AUTO | ❓ 需检查 |
| **PCIe 睡眠配置** | ✅ 0x1f5018=0xFF..FF | ❓ 未确认 | ✅ 在 init | ❌ **缺失** |
| **芯片特定中断** | ✅ 0x74030188 BIT(16) | ❓ 未确认 | ❓ 未确认 | ❌ **缺失** |
| **GLO_CFG_EXT BIT(28)** | ✅ 0xd42b4 \|= BIT(28) | ✅ 已设置 | ✅ 已设置 | ❓ 需检查 |

---

## 三、关键寄存器配置差异

### 1. DMASHDL 启用（BAR0+0xd6060）⚠️⚠️⚠️

**Windows 驱动**:
```c
// AsicConnac3xPostFwDownloadInit 第一步
READ(ctx, 0x7c026060, &val);   // BAR0+0xd6060
val |= 0x10101;                 // BIT(0)|BIT(8)|BIT(16)
WRITE(ctx, 0x7c026060, val);
```

**我们的驱动**: ❌ **从未写入此寄存器**

**可能影响**: DMASHDL（DMA Scheduler）未启用可能导致 MCU 命令路由失败。

---

### 2. 预取配置寄存器（BAR0+0xd70f0-0xd70fc）⚠️⚠️

**Windows 驱动**（来自 MT6639WpdmaConfig）:
```c
if (*(ctx + 0x1466a49) != 0) {  // prefetch flag
    READ(ctx, 0x7c027030, &val);
    WRITE(ctx, 0x7c027030, val);     // 触发预取重置
    WRITE(ctx, 0x7c0270f0, 0x660077);
    WRITE(ctx, 0x7c0270f4, 0x1100);
    WRITE(ctx, 0x7c0270f8, 0x30004f);
    WRITE(ctx, 0x7c0270fc, 0x542200);
}
```

**我们的驱动**: ❌ **从未配置预取**

**可能影响**: WFDMA 预取配置错误可能导致 ring 描述符读取失败。

---

### 3. HOST RX Ring 映射差异 ⚠️⚠️⚠️

| 驱动 | RX Ring 配置 | MCU Event Ring |
|------|-------------|---------------|
| **Windows** | Ring 4, 6, 7 | **Ring 6** |
| **mt7925** | Ring 0, 2 | **Ring 0** |
| **Vendor mt6639** | Ring 4, 5, 6, 7 | **Ring 6** |
| **我们** | Ring 4, **5**, 6, 7 | **Ring 5** ❌ |

**关键问题**:
- Windows 使用 **Ring 6** 作为 MCU 事件接收 ring
- 我们使用 **Ring 5**（错误！）
- Ring 5 vs Ring 6 寄存器地址完全不同：
  - Ring 5: BASE=0x7c024550, CIDX=0x7c024558, DIDX=0x7c02455c
  - Ring 6: BASE=0x7c024560, CIDX=0x7c024568, DIDX=0x7c02456c

---

### 4. 中断掩码配置（BAR0+0xd4204）⚠️

**Windows 驱动**（MT6639ConfigIntMask）:
```c
// 主中断掩码
WRITE(ctx, 0x7c024228, 0x2600f000);  // INT_ENA_SET

// 芯片特定中断（仅 MT6639/MT7927/MT7925）
READ(ctx, 0x74030188, &val);
val |= BIT(16);
WRITE(ctx, 0x74030188, val);
```

**中断掩码 0x2600f000 解析**:
```
BIT(29) = MCU2HOST_SW_INT（MCU 命令完成）
BIT(26) = ?
BIT(25) = ?
BIT(15:12) = 0xF = RX ring 4-7 完成中断
```

**我们的驱动**: ❓ 需检查当前中断配置

---

### 5. PCIe 睡眠配置（BAR0+0x1f5018）⚠️

**Windows 驱动**（AsicConnac3xSetCbInfraPcieSlpCfg）:
```c
READ(ctx, 0x70025018, &val);   // Bus addr
if (val != 0xFFFFFFFF) {
    WRITE(ctx, 0x70025018, 0xFFFFFFFF);  // 禁用所有睡眠
}
```

**BAR0 偏移**: 0x70025018 → BAR0+0x1f5018

**我们的驱动**: ❌ **从未配置**

---

### 6. GLO_CFG_EXT BIT(28)（BAR0+0xd42b4）

**Windows/mt7925/Vendor 都设置此位**:
```c
READ(ctx, 0x7c0242b4, &val);
val |= 0x10000000;  // BIT(28)
WRITE(ctx, 0x7c0242b4, val);
```

**我们的驱动**: ❓ 需检查

---

## 四、target=0xed 的含义

### Windows MCU 命令路径分析

**来源**: Ghidra RE of AsicConnac3xPostFwDownloadInit

**所有 post-FWDL MCU 命令都使用 target=0xed**:
- class=0x8a, target=0xed (NIC_CAP)
- class=0x02, target=0xed (Config)
- class=0xc0, target=0xed (Config)
- class=0x28, target=0xed (DBDC)
- class=0xca, target=0xed (各种配置)

**target 参数含义**:
- **0xee** = FW_SCATTER（FWDL 专用命令）
- **0xed** = **普通 MCU 命令**（post-boot 使用）

**MCU 命令白名单**（当 DAT_14024b439==0 时）:
```c
// 仅允许这些 target ID:
[0x01, 0x02, 0x03, 0x05, 0x07, 0x10, 0x11, 0xee, 0xef]
```

**注意**: **0xed 不在白名单中**，但白名单仅在 `DAT_14024b439 == 0` 时检查。
在正常运行时，`DAT_14024b439 = 1`，白名单被跳过。

**结论**: target=0xed 是普通 MCU 命令的标准路由，不是特殊握手。

---

## 五、SWDEF_MODE / DRIVER_READY 握手？

### 搜索结果：❌ **不存在此类握手**

**已搜索的关键字**:
- SWDEF, SWDEF_MODE
- DRIVER_READY, INIT_DONE
- sw_ready, fw_ready

**结论**:
1. **Vendor mt6639**: fw_ready 检查后**立即**发送 NIC_CAP 命令
2. **mt7925**: `MT_TOP_MISC2_FW_N9_RDY` 后**立即**发送 NIC_CAP 命令
3. **Windows**: fw_sync=0x3 后**立即**发送 MCU 命令（仅先写 DMASHDL 寄存器）

**没有**任何 SWDEF_MODE 或 DRIVER_READY 握手寄存器。

---

## 六、NEED_REINIT 在 FWDL 后的作用

### Vendor mt6639 NEED_REINIT 机制

**源文件**: `mt6639/chips/common/cmm_asic_connac3x.c`

```c
// DUMMY_CR = 0x54000120 (bus: 0x02120)
// NEED_REINIT_BIT = BIT(1)

void asicConnac3xWfdmaReInit(struct ADAPTER *prAdapter)
{
    // 检查 NEED_REINIT bit 是否被 ROM 清除
    asicConnac3xWfdmaDummyCrRead(prAdapter, &fgResult);

    if (fgResult) {  // NEED_REINIT 已被消费
        // 仅重置 TX ring 软件索引，不重新编程 ring BASE
        for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
            TxRing[u4Idx].TxSwUsedIdx = 0;
            TxRing[u4Idx].u4UsedCnt = 0;
            TxRing[u4Idx].TxCpuIdx = 0;
        }
    }
}
```

**关键发现**:
1. NEED_REINIT 是**深度睡眠恢复**机制，不是 FWDL 后的必需步骤
2. Vendor 在 `halHifSwInfoInit` 时设置 NEED_REINIT=1
3. 下一次 CLR_OWN 时，ROM 处理 NEED_REINIT 并重新配置 MCU_RX2/RX3（FWDL rings）
4. **ROM 不配置 MCU_RX0/RX1**（这些由运行中的 FW 配置）

**结论**: NEED_REINIT 与我们当前的 FWDL 后 MCU 命令失败**无关**。

---

## 七、MCU_RX0 配置机制

### 三个驱动的一致结论

**Windows 驱动**:
- InitTxRxRing 仅配置 HOST TX/RX rings
- 从不写入 MCU_RX0 BASE 寄存器
- PostFwDownloadInit 仅写 DMASHDL，不操作 MCU DMA 寄存器

**Vendor mt6639**:
- halWpdmaInitRing 仅配置 HOST rings
- 从不直接操作 MCU DMA 寄存器空间（0x54000000）
- MCU events 通过 HOST RX ring 6 接收，不是 MCU_RX rings

**mt7925**:
- DMA init 仅分配 HOST rings (RX 0, 2)
- 从不写入 MCU_RX0 BASE
- MCU events 通过 HOST RX ring 0 接收

**统一结论**: **MCU_RX0 BASE 由固件在启动时自动配置，驱动不直接操作**

**MCU_RX0 BASE=0 的含义**:
- 这可能是**正常状态**（Windows 也从不读取或写入它）
- MCU events 走 HOST RX rings（不是 MCU_RX rings）
- MCU_RX rings 是 WFDMA 内部路由使用，对 HOST 不可见

---

## 八、DMA 响应路径分析

### 当前问题症状

1. ✅ **TX15 DIDX 前进** → WFDMA 已消费 TXD
2. ❌ **RX6 DIDX 不前进** → 没有 DMA 响应写入
3. ✅ **MCU_CMD=0x8000** → FW 通过寄存器信号（MCU2HOST_SW_INT BIT(15)）
4. ❌ **MCU_RX0 BASE=0** → FW 未配置（或不需要配置）

### 可能的根本原因（按优先级）

#### 1. ⚠️⚠️⚠️ **HOST RX Ring 6 vs Ring 5 错误**

**问题**: 我们使用 Ring 5，Windows 使用 Ring 6
- Ring 5 BASE: 0x7c024550
- Ring 6 BASE: 0x7c024560

**如果 FW 将 MCU 响应路由到 Ring 6，但我们监听 Ring 5 →** DMA 响应永远不会被发现

**修复**: 将 `ring_rx5` 改为 `ring_rx6`（HW offset 0x50 → 0x60）

---

#### 2. ⚠️⚠️ **DMASHDL 未启用**

**问题**: BAR0+0xd6060 从未写入
- Windows 在所有 MCU 命令之前写入 0x10101
- 此寄存器控制 DMASHDL（DMA Scheduler）

**可能影响**: MCU 命令可能被调度到错误的 ring 或根本不被路由

**修复**: 在 NIC_CAP 命令之前添加：
```c
uint32_t val = readl(dev->bar0 + 0xd6060);
writel(val | 0x10101, dev->bar0 + 0xd6060);
```

---

#### 3. ⚠️⚠️ **预取配置缺失**

**问题**: BAR0+0xd70f0-0xd70fc 从未配置
- Windows 写入固定值
- 控制 WFDMA 如何预取 ring 描述符

**可能影响**: RX ring 描述符预取失败 → FW 写入 RXD 但 HOST 永远看不到

**修复**: 添加预取配置（在 WFDMA 启用时）

---

#### 4. ⚠️ **中断配置不匹配**

**问题**: 中断掩码可能未正确设置
- Windows: 0x2600f000
- 需要检查我们的当前值

**可能影响**: RX 中断未触发 → NAPI poll 未运行 → RXD 不被处理

**修复**: 设置正确的中断掩码

---

#### 5. ⚠️ **RX 描述符初始化不完整**

**问题**: Windows 在 InitTxRxRing 中对每个 RXD 做了：
- 清除 BIT(31)（DMA_DONE 标志）
- 设置 buffer 大小到高 14 位

**我们的驱动**: 需检查 RXD 初始化代码

---

## 九、立即行动计划 🔧

### Mode 55 候选（按优先级）

#### 1. **修复 HOST RX Ring 6** ⚠️⚠️⚠️

```c
// 将 ring_rx5 改为 ring_rx6
// HW ring offset: 0x50 → 0x60

// BASE 寄存器: 0x7c024550 → 0x7c024560
// CNT 寄存器:  0x7c024554 → 0x7c024564
// CIDX 寄存器: 0x7c024558 → 0x7c024568
// DIDX 寄存器: 0x7c02455c → 0x7c02456c
```

#### 2. **添加 DMASHDL 启用** ⚠️⚠️

```c
// 在 NIC_CAP 命令之前
uint32_t val = readl(dev->bar0 + 0xd6060);
writel(val | 0x10101, dev->bar0 + 0xd6060);
dev_info(&pdev->dev, "DMASHDL enabled: 0x%08x\n",
         readl(dev->bar0 + 0xd6060));
```

#### 3. **添加预取配置** ⚠️⚠️

```c
// 在 WFDMA 启用时（GLO_CFG 之前）
uint32_t val = readl(dev->bar0 + 0xd7030);
writel(val, dev->bar0 + 0xd7030);  // 触发预取重置

writel(0x660077, dev->bar0 + 0xd70f0);
writel(0x1100,   dev->bar0 + 0xd70f4);
writel(0x30004f, dev->bar0 + 0xd70f8);
writel(0x542200, dev->bar0 + 0xd70fc);
```

#### 4. **添加 PCIe 睡眠配置**

```c
uint32_t val = readl(dev->bar0 + 0x1f5018);
if (val != 0xFFFFFFFF) {
    writel(0xFFFFFFFF, dev->bar0 + 0x1f5018);
}
```

#### 5. **检查并设置正确的中断掩码**

```c
// 主中断掩码
writel(0x2600f000, dev->bar0 + 0xd4204);  // INT_ENA

// 芯片特定中断（需要确认 0x74030188 的 BAR0 映射）
// 如果此地址可访问：
uint32_t val = readl(dev->bar0 + ???);  // 需要找到正确的偏移
writel(val | BIT(16), dev->bar0 + ???);
```

---

## 十、验证步骤

### 1. 编译并加载 Mode 55

```bash
cd /home/user/mt7927
make clean && make tests
sudo rmmod mt7927_init_dma 2>/dev/null
sudo insmod tests/04_risky_ops/mt7927_init_dma.ko reinit_mode=55
```

### 2. 检查关键寄存器

```bash
# DMASHDL
sudo cat /sys/kernel/debug/mt7927/registers | grep "0xd6060"

# Prefetch
sudo cat /sys/kernel/debug/mt7927/registers | grep "0xd70f[0-9a-f]"

# HOST RX Ring 6
sudo cat /sys/kernel/debug/mt7927/rings/rx6

# GLO_CFG_EXT
sudo cat /sys/kernel/debug/mt7927/registers | grep "0xd42b4"
```

### 3. 发送 NIC_CAP 命令并检查响应

```bash
# 查看 dmesg
sudo dmesg | tail -50

# 预期输出（如果成功）:
# mt7927: NIC_CAP command sent
# mt7927: RX6 DIDX advanced: 0 → 1
# mt7927: NIC_CAP response received
# mt7927: Capability flags: 0xXXXXXXXX
```

---

## 十一、参考文档索引

### Windows 驱动逆向工程
1. `/home/user/mt7927/docs/references/ghidra_post_fw_init.md` — **最关键**
   - AsicConnac3xPostFwDownloadInit 完整分析
   - MCU 命令序列
   - 寄存器写入顺序

2. `/home/user/mt7927/docs/analysis_windows_full_init.md` — **完整初始化流程**
   - PreFWDL 和 PostFWDL DMA 配置差异
   - HOST RX ring 4, 6, 7 映射
   - 预取配置、中断掩码

3. `/home/user/mt7927/docs/win_v5705275_core_funcs.md`
   - MT6639InitTxRxRing 详细分析
   - MT6639WpdmaConfig 预取配置
   - MT6639ConfigIntMask 中断配置

### Upstream mt7925 分析
4. `/home/user/mt7927/docs/analysis_mt7925_post_boot.md` — **完整 post-FWDL 序列**
   - mt7925_run_firmware() 流程
   - HOST RX ring 0 分配（512 entries）
   - MCU_UNI_CMD(CHIP_CONFIG) 格式

5. `/home/user/mt7927/docs/analysis_mt7925_dma_init.md`
   - mt7925_dma_init() 完整分析
   - RX ring 0, 2 分配时机（FWDL **之前**）
   - WFDMA 启用配置

### Vendor mt6639 分析
6. `/home/user/mt7927/docs/references/vendor_post_boot_analysis.md` — **Vendor 完整序列**
   - wlanAdapterStart() 流程
   - WFSYS reset 序列
   - fw_ready 检查后立即发送 NIC_CAP
   - NEED_REINIT 机制详解

7. `/home/user/mt7927/docs/references/post_boot_mcu_rx_config.md`
   - 预取配置分析（manual vs auto mode）
   - CLR_OWN 后 HOST ring BASEs 的处理
   - MCU_RX0 配置由 FW 完成的证据

---

## 十二、关键结论

### ✅ **确定性发现**

1. **Windows 在 MCU 命令之前仅写一个寄存器**: BAR0+0xd6060 |= 0x10101（DMASHDL）
2. **所有驱动在 fw_sync=0x3 后立即发送 MCU 命令**: 没有中间握手
3. **MCU_RX0 由 FW 配置，驱动不直接操作**: 所有三个驱动都不写 MCU_RX BASE
4. **Windows 使用 HOST RX ring 4, 6, 7**: 我们使用 ring 5 是**错误的**
5. **CONNAC3X 不需要 dummy command**: Vendor 代码明确跳过

### ❌ **已排除的假设**

1. ❌ SWDEF_MODE / DRIVER_READY 握手不存在
2. ❌ NEED_REINIT 不影响 post-FWDL MCU 命令（仅用于深度睡眠恢复）
3. ❌ MCU_RX0 BASE=0 不是异常（HOST 不应读写它）
4. ❌ target=0xed 不是特殊握手（是普通 MCU 命令路由）

### ⚠️ **最可能的根本原因**

1. **HOST RX Ring 6 vs Ring 5 错误** — 最高优先级修复
2. **DMASHDL 未启用** — Windows 必需的唯一寄存器写入
3. **预取配置缺失** — 影响 RX 描述符读取
4. **中断配置不匹配** — 可能导致 RX 中断未触发

---

## 十三、下一步行动

### 立即实施（Mode 55）

1. ✅ 将 ring_rx5 改为 ring_rx6（HW offset 0x60）
2. ✅ 添加 DMASHDL 启用（0xd6060 |= 0x10101）
3. ✅ 添加预取配置（0xd70f0-0xd70fc）
4. ✅ 检查并修复中断掩码
5. ✅ 添加 PCIe 睡眠配置（0x1f5018）

### 验证

1. ✅ 编译并加载驱动
2. ✅ 检查寄存器值
3. ✅ 发送 NIC_CAP 命令
4. ✅ 检查 RX6 DIDX 是否前进
5. ✅ 检查是否收到 MCU 响应

### 如果仍然失败

6. ⏭️ 检查 RXD 初始化（BIT(31)、buffer size）
7. ⏭️ 检查 0x74030188 芯片特定中断寄存器
8. ⏭️ 对比 Windows 和我们的 WFDMA GLO_CFG 完整配置
9. ⏭️ 分析 WFDMA 路由表（DISP_CTRL）

---

**分析完成时间**: 2026-02-15
**下一个 Mode**: 55（修复 ring 6 + DMASHDL + 预取）
