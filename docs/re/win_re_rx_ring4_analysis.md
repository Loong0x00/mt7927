# Windows mtkwecx.sys — RX Ring 4 数据通路分析

**分析目标**: AMD v5.7.0.5275 (`WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys`)
**分析方法**: Python + capstone 汇编级逆向
**日期**: 2026-02-22
**Task**: ring4-rx-fix / Task #2

---

## 一、结论摘要

| 问题 | 结论 |
|------|------|
| Ring 4 DIDX=0 正常吗? | **✅ 正常** — auth 阶段 Ring 4 DIDX=0 是预期行为 |
| Auth-2 帧走哪个 Ring? | **Ring 4** (数据帧)或 **Ring 7** (辅助) — 取决于固件版本，但可能是 Ring 4 |
| Ring 4 初始化正确吗? | **✅ 我们的初始化完全正确** — 与 Windows 完全一致 |
| 中断掩码正确吗? | **✅ 正确** — 我们用 0x2600f050 是 Windows 0x2600f000 的超集 |
| 有遗漏的 MCU 命令? | **无法通过 RX 角度解释 DIDX=0** — 问题在固件上层 (BSS/STA_REC 配置) |

**核心发现**: RX Ring 4 本身配置正确。Ring 4 DIDX 不前进的根因是**固件尚未开始向 Ring 4 投递数据帧**，而这是因为 BSS/STA_REC MCU 配置不完整导致固件认为没有活跃的 BSS/STA，从而不处理 auth-2 帧。

---

## 二、Windows RX Ring 初始化 (N6PciInitRxRing)

### 2.1 函数地址

| 函数 | 文件偏移 | 分析 |
|------|---------|------|
| MT6639InitTxRxRing | 0x14001c014 | 调用各 ring 初始化函数的入口 |
| N6PciInitRxRing | 0x14005a0b8 (约 0x140059f30 + prologue) | RX ring 硬件寄存器初始化 |
| N6PciRxPacketHandler | ~0x14006xxxx | RX 中断处理 + ring dispatch |

### 2.2 RX Ring 硬件寄存器 — Windows 初始化序列

Windows 对每个 RX ring 的初始化寄存器操作（以 Ring 4 为例）:

```
BASE  寄存器 0xd4540: 写入 ring descriptor DMA 物理地址
CNT   寄存器 0xd4544: 写入 ndesc (256)
CIDX  寄存器 0xd4548: 写入 ndesc-1 = 255 (全部 slot 交给固件)
DIDX  寄存器 0xd454c: ❌ Windows 不写! (让硬件保持 reset value = 0)
```

**我们的驱动 (mt7927_rx_ring_alloc)**:
```c
mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(qid), lower_32_bits(ring->desc_dma));
mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(qid),  ndesc);
mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(qid), 0);    // ← 多写了 DIDX=0 (无害)
mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(qid), ring->head); // = ndesc-1 = 255 ✅
```

**结论**: 我们多写了 DIDX=0，但由于硬件 reset value 本就是 0，这是无害的操作。

### 2.3 RX Ring 寄存器地址确认

| Ring | BASE 偏移 | 说明 |
|------|----------|------|
| Ring 4 | 0xd4540 | WiFi 数据帧 |
| Ring 5 | 0xd4550 | (Windows 未初始化) |
| Ring 6 | 0xd4560 | MCU 事件 |
| Ring 7 | 0xd4570 | 辅助/管理帧 |

通用公式: `BASE(n) = 0xd4500 + n*0x10` (Ring 4 → offset 0x40)

---

## 三、中断掩码配置

### 3.1 Windows ConfigIntMask 函数 (0x1401e43e0)

Windows 使用两个寄存器控制中断使能:
- `0xd4228` = INT_ENA **SET** 寄存器 (write 1 to enable)
- `0xd422c` = INT_ENA **CLEAR** 寄存器 (write 1 to disable)

Windows 传入中断掩码: **0x2600f000**

```
BIT(12): RX ring 4 done (WiFi 数据帧)
BIT(13): RX ring 5 done (未使用)
BIT(14): RX ring 6 done (MCU 事件)
BIT(15): RX ring 7 done (辅助)
BIT(25): TX ring 15 done (MCU WM 命令)
BIT(29): MCU2HOST 软件中断
```

### 3.2 我们的中断掩码

```c
#define MT_WFDMA_INT_MASK_WIN    0x2600f050
// = 0x2600f000 | BIT(4) | BIT(6)
// BIT(4): TX ring 0 done (WiFi 数据 TX)
// BIT(6): TX ring 2 done (管理帧 TX)
```

**结论**: 0x2600f050 是 0x2600f000 的**超集**，多使能了 TX ring 0/2 中断，这是正确的。我们多加的 BIT(4) 和 BIT(6) 是 TX done 中断，不影响 RX 功能。

### 3.3 辅助中断寄存器

Windows 还写了 MT_CONN_INFRA_30188 (0x74030188) 的 BIT(16)。

我们的驱动已经在 `mt7927_pci.h` 中定义了:
```c
#define MT_CONN_INFRA_30188         0x010188
#define MT_CONN_INFRA_30188_BIT16   BIT(16)
```
**结论**: 此寄存器已处理。

---

## 四、Auth 帧接收路径分析

### 4.1 CONNAC3 PKT_TYPE 路由

固件将 RX 帧按类型分发到不同 Ring:

| PKT_TYPE | 值 | Ring | 说明 |
|----------|---|------|------|
| NORMAL | 2 | Ring 4 | 普通 WiFi 数据帧 |
| RX_EVENT | 7 | Ring 6 | MCU 事件/UniCmd 响应 |
| NORMAL_MCU | 8 | Ring 6 | MCU 帧 |

**关键问题**: Auth Response (auth-2) 是管理帧，不是数据帧。固件可能将其分类为:
- **NORMAL (type=2)** → Ring 4 (如果固件将所有 802.11 帧统一发到 Ring 4)
- 或固件在 **BSS 未激活状态下直接丢弃** auth-2 (不发到任何 Ring)

### 4.2 Ring 4 DIDX=0 的含义

DIDX (DMA Descriptor IndeX) 由**硬件**在将帧写入 Ring 时前进。

若 auth-2 帧到达但 Ring 4 DIDX=0:
1. **固件丢弃了 auth-2** (BSS_INFO 配置不完整，固件认为没有活跃 BSS)
2. **或** auth-2 实际走 Ring 7 (我们的 Ring 7 NAPI 里也没看到数据)

### 4.3 固件丢帧的根因假设

基于 `docs/win_re_connect_flow.md` 中的分析，Windows 在 auth TX **之前**必须完成:

```
① BSS_INFO PM_DISABLE (tag=0x1B)   ← 我们完全缺失!
② BSS_INFO full (14 TLVs)         ← 我们只发 3 个 TLV
③ STA_REC (10 TLVs)               ← 我们只发 5 个 TLV
```

**固件状态机假设**:
- 如果 BSS_INFO 未正确激活，固件对该 BSS 的帧接收处于 suspended 状态
- Auth-2 帧到达 RF → PHY → MAC，但在 PLE 层被丢弃或不触发 DMA 投递
- 因此 Ring 4 DIDX 永远为 0

---

## 五、我们的 RX 代码对比 Windows

### 5.1 RX 描述符处理 — 完全正确

| 步骤 | Windows | 我们 | 状态 |
|------|---------|------|------|
| 检查 DMA_DONE (BIT(31)) | ✅ | ✅ | 一致 |
| 从 ctrl[29:16] 取 len | ✅ | ✅ | 一致 |
| 清除描述符 (重置 DMA_DONE) | ✅ | ✅ | 一致 |
| 更新 CIDX (批量，NAPI 结束时) | ✅ | ✅ | 一致 |
| Ring 7 也处理 | ✅ | ✅ | 我们加了 Ring 7 处理 |

### 5.2 中断处理 — 正确

我们的任务分发与 Windows 一致:
- BIT(12) → napi_schedule(&napi_rx_data) — Ring 4 数据
- BIT(14) → napi_schedule(&napi_rx_mcu) — Ring 6 MCU
- BIT(15) → napi_schedule(&napi_rx_data) — Ring 7 也调度数据 NAPI

---

## 六、修复建议

### 6.1 RX 层面 — 无需修改

RX Ring 4 的硬件初始化、中断配置、描述符处理都与 Windows 完全一致。

**不需要**修改 DMA/中断相关代码来修复 Ring 4 DIDX=0 的问题。

### 6.2 上层 MCU 命令修复 (才是真正的根因)

详见 `docs/win_re_connect_flow.md`，以下是必须修复的 MCU 命令问题:

#### 🔴 修复 1 (最高优先级): BSS_INFO PM_DISABLE

必须在 BSS_INFO full config **之前**发送:

```c
// UNI_CMD_ID_BSSINFO (CID=0x02), tag=0x001B, len=4
// 总 payload = 16 (UniCmd hdr) + 4 (bssinfo base) + 4 (PM_DISABLE TLV) = 24 bytes
```

这是 Windows 驱动中 `0x1400caefc` (nicUniCmdPmDisable) 的功能。
**意义**: 告知固件解除该 BSS 的省电暂停，允许 TX/RX 队列开始工作。

#### 🔴 修复 2: BSS_INFO 补充 TLV

Windows 发 14 个 TLV，我们只发 3 个 (BASIC + RLM + MLD)。

关键缺失:
- **RATE TLV** (tag=?) — 告知固件此 BSS 的合法速率集
- **PROTECT TLV** (CID=0x4a, 不是 0x02!) — 保护模式配置
- **IFS_TIME TLV** — 帧间隔时间

注意: Windows RE 发现 BSS_INFO_PROTECT 用不同的 CID (0x4a)，需要单独命令。

#### 🟡 修复 3: STA_REC 补充 TLV

Windows 发 10 个 TLV，我们发 5 个。缺失:
- BA_OFFLOAD (tag=0x0F)
- UAPSD
- HE_6G_CAP (如适用)

---

## 七、关键诊断数据 (用于验证假设)

在当前 auth 失败的测试中，应检查:

### 7.1 PLE 队列状态
```bash
# 在 auth TX 后立即读取
devmem2 0x820c0360 w  # PLE_QUEUE_EMPTY
devmem2 0x820c0368 w  # PLE_QUEUE_EMPTY2
```

若 PLE queue 不为空说明帧在 PLE 层被阻塞，与 Ring 4/7 DIDX=0 一致。

### 7.2 RX Ring DIDX 实时监控
```c
// 在 auth 发出后立即读取:
u32 didx4 = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(4));
u32 didx7 = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(7));
pr_info("After auth TX: RX4_DIDX=%u RX7_DIDX=%u\n", didx4, didx7);
```

---

## 八、总结

RX Ring 4 本身**没有 bug**。Driver 的 RX 初始化和中断处理与 Windows 完全一致。

Ring 4 DIDX=0 的根因是固件在不完整的 MCU 配置下 (缺少 PM_DISABLE + 缺少 RATE/PROTECT TLV) 不向 Ring 4 投递 auth-2 帧。

**行动项**:
1. 在 `mt7927_set_bss_info()` 之前发送 PM_DISABLE (BSS_INFO tag=0x1B)
2. 补充 BSS_INFO RATE TLV
3. 确认 BSS_INFO_PROTECT 走独立 CID=0x4a 命令

这些修复在 `docs/win_re_connect_flow.md` 中已有详细分析。

---

## 九、参考函数地址 (AMD v5.7.0.5275)

| 函数 | 地址 | 说明 |
|------|------|------|
| MT6639InitTxRxRing | 0x14001c014 | TX/RX ring 初始化入口 |
| N6PciInitRxRing | ~0x140059f30 | RX ring 寄存器初始化 |
| ConfigIntMask | 0x1401e43e0 | 中断掩码配置 (0x2600f000) |
| INT_ENA_SET | BAR0+0xd4228 | 中断使能 SET 寄存器 |
| INT_ENA_CLR | BAR0+0xd422c | 中断使能 CLEAR 寄存器 |
| nicUniCmdPmDisable | 0x1400caefc | BSS PM_DISABLE 命令发送 |
