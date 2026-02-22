# Windows RE: Ring 2 管理帧 TX 完整操作流程分析

**日期**: 2026-02-21
**来源**: Ghidra RE of `mtkwecx.sys` v5705275 + v5603998
**目的**: 分析 Windows 驱动对 Ring 2 的完整操作流程，与我们的实现对比，找出差异

---

## Executive Summary

**Ring 2 不是固件崩溃的根因。** 根据已有的详尽 RE 分析，Windows 和我们的 Ring 2 初始化、DMA 描述符格式、TXD 构建都基本匹配。当前问题是 **Ring 0 CT mode 发送 auth 帧产生 MPDU_ERR (status=3, count=30)**，PHY 确实发射了 30 次但 AP 不回 ACK。

以下是 Ring 2 vs Ring 0 的完整对比和关键发现。

---

## 1. Ring 2 初始化流程

### 1.1 Windows 初始化时序

Windows 的 Ring 2 初始化分两个阶段：

**阶段 1 — Pre-FWDL (MT6639InitTxRxRing, FUN_1401e4580)**:

Windows 在固件下载**之前**就初始化所有 TX/RX rings。TX ring 循环初始化 4 个 ring：

| 序号 | HW Ring 偏移 | 计算方式 | BASE 寄存器 | 备注 |
|------|-------------|----------|-------------|------|
| 0 | 0x00 | 0 << 4 | 0x7c024300 (0xd4300) | 数据 TX |
| 1 | 0x10 | 1 << 4 | 0x7c024310 (0xd4310) | 数据 TX |
| 2 | config[0xd1] << 4 | 动态 | 动态 | **可能是 HW ring 2 或 ring 15/16** |
| 3 | config[0xd0] << 4 | 动态 | 动态 | **可能是 HW ring 15 或 16** |

> **关键**: TX ring "2" 和 "3" 的 HW ring 编号来自配置结构体的 byte 字段（偏移 0xd0, 0xd1），不是硬编码。其中一个映射到 HW ring 15 (MCU TX)，另一个映射到 HW ring 2 (mgmt TX)。
>
> **来源**: `docs/analysis_windows_full_init.md` Section 2a

每个 TX ring 写入 4 个寄存器：
```
WRITE(MT_WPDMA_TX_RING_BASE(n), ring_phys_addr)   // DMA 描述符物理地址
WRITE(MT_WPDMA_TX_RING_CNT(n), ring_ndesc)         // 描述符数量
WRITE(MT_WPDMA_TX_RING_CIDX(n), 0)                 // CPU index = 0
// DIDX 由硬件维护
```

**Windows 不写 per-ring EXT_CTRL 寄存器** (见 Section 4)。

**阶段 2 — Post-FWDL (PostFwDownloadInit)**:

PostFwDownloadInit 中**没有额外的 Ring 2 初始化操作**。唯一的 DMA 相关操作是 DMASHDL enable bits:
```c
readl(BAR0 + 0xd6060);   // MT_HIF_DMASHDL_QUEUE_MAP0
writel(val | 0x10101, BAR0 + 0xd6060);  // 设置 BIT(0)|BIT(8)|BIT(16)
```

> **来源**: `docs/win_re_ple_pse_init.md` Section 2, assembly at 0x1401d7738

### 1.2 我们的初始化

Ring 2 在 `mt7927_dma_init_data_rings()` 中初始化，该函数在 PostFwDownloadInit **之后**调用：

```c
// src/mt7927_dma.c lines 782-833
dma_wr(dev, MT_WPDMA_TX_RING_BASE(2), ring->desc_dma);  // 0xd4320
dma_wr(dev, MT_WPDMA_TX_RING_CNT(2), 256);              // 0xd4324
dma_wr(dev, MT_WPDMA_TX_RING_CIDX(2), 0);               // 0xd4328
dma_wr(dev, MT_WPDMA_TX_RING_DIDX(2), 0);               // 0xd432c (多余但无害)
```

### 1.3 初始化差异

| 项目 | Windows | 我们的驱动 | 差异 |
|------|---------|-----------|------|
| 初始化时机 | Pre-FWDL | Post-FWDL | ⚠️ Windows 先初始化 ring，再下载固件 |
| BASE 写入 | ✅ | ✅ | 匹配 |
| CNT 写入 | ✅ | ✅ 256 | 匹配 (Windows 大小由 WpdmaInitRing 预分配) |
| CIDX 写入 | 0 | 0 | ✅ 匹配 |
| DIDX 写入 | 不写 (HW 管理) | 写 0 | 多余但无害 |
| EXT_CTRL | **不写** | **写 PREFETCH(0x02c0, 0x4)** | ⚠️ 差异 (见 Section 4) |

**初始化时序差异可能重要**: Windows 在 FWDL 之前就配置好 Ring 2，固件启动后 Ring 2 已就绪。我们在固件启动后才初始化 Ring 2，但由于 CLR_OWN 会清零所有 HOST ring 的 BASE，且 WpdmaConfig 在 PostFwDownloadInit 中重新启用 DMA，所以 Post-FWDL 初始化应该也是正确的。

---

## 2. Ring 2 的 DMA Descriptor 格式

### 2.1 Windows DMA Descriptor (N6PciTxSendPkt, 0x14005d1a4)

**管理帧 (param_3 >= 2) DMA descriptor 格式**:

```c
// 来源: docs/win_re_dma_descriptor_format.md Section 3

// 步骤1: 清零 16 字节
memset(desc, 0, 16);

// 步骤2: DW0 = buf 物理地址低32位
desc[0] = *(uint32 *)(buf_node + 0x58);  // phys_addr_low

// 步骤3: DW3[15:0] = buf 物理地址高16位
*(uint16 *)(desc + 3) = *(uint16 *)(buf_node + 0x5c);  // phys_addr_high

// 步骤4: DW1 = 长度 + OWN bit (管理帧路径)
uint len = *(int *)(buf_node + 0x68);  // total length (TXD + frame)
desc[1] = ((len << 16) ^ desc[1]) & 0x3fff0000 ^ desc[1];  // bits[29:16] = total_len
desc[1] = (desc[1] & 0x7fffffff) | 0x40000000;               // bit30=1(OWN), bit31=0
```

**汇编确认** (0x14005d524-0x14005d570):
```asm
14005d524: CALL 0x14001022c         ; memset(desc, 0x10)
14005d529: MOV EAX,[RBX + 0x58]    ; buf_phys_low
14005d52c: MOV [R12],EAX           ; DW0 = phys_addr_low
14005d530: MOVZX EAX,word ptr [RBX + 0x5c]  ; buf_phys_high[15:0]
14005d534: MOV word ptr [R12 + 0xc],AX       ; DW3[15:0]
14005d53f: MOV EAX,[R12 + 0x4]     ; read DW1 (= 0)
14005d544: MOV ECX,[RBX + 0x68]    ; total_len
14005d547: SHL ECX,0x10            ; << 16
14005d54a: XOR ECX,EAX             ; XOR trick
14005d54c: AND ECX,0x3fff0000      ; keep bits[29:16]
14005d552: XOR ECX,EAX             ; = set bits[29:16] = total_len
14005d565: BTS ECX,0x1e            ; SET bit30 (OWN)
14005d56c: BTR ECX,0x1f            ; CLEAR bit31
14005d570: MOV [R12 + 0x4],ECX     ; write DW1
```

### 2.2 精确 DMA Descriptor 布局

```
DW0 (bytes 0-3):   [31:0]  = buf_phys_addr_low (物理地址低32位)
DW1 (bytes 4-7):   [31]    = 0 (显式清除)
                    [30]    = 1 (OWN bit, DMA 引擎所有权)
                    [29:16] = total_len (TXD + frame, 14位)
                    [15:0]  = 0 (未使用)
DW2 (bytes 8-11):  [31:0]  = 0 (清零)
DW3 (bytes 12-15): [15:0]  = buf_phys_addr_high (高16位, 32-bit DMA 时为0)
                    [31:16] = 0 (未使用)
```

### 2.3 与我们的对比

| 字段 | Windows | 我们的驱动 | 匹配? |
|------|---------|-----------|-------|
| DW0: phys_addr_low | `*(buf+0x58)` | `lower_32_bits(dma)` | ✅ |
| DW1: length bits[29:16] | `(len << 16) & 0x3fff0000` | `FIELD_PREP(MT_DMA_CTL_SD_LEN0, total_len)` | ✅ |
| DW1: OWN bit30 | 1 | `MT_DMA_CTL_LAST_SEC0 = BIT(30)` | ✅ |
| DW1: bit31 | 0 (显式清除) | 0 (未设置) | ✅ |
| DW2 | 0 | 0 (`buf1 = 0`) | ✅ |
| DW3: phys_addr_high | `*(buf+0x5c)` | 0 (32-bit DMA mask) | ✅ |

**结论: DMA 描述符格式完全匹配。**

> **来源**: `docs/win_re_dma_descriptor_format.md` Section 4-5

---

## 3. Ring 2 帧提交流程

### 3.1 Windows 管理帧提交 (N6PciTxSendPkt)

完整调用链:

```
MlmeAuthReqAction (0x14013f660)
  ├─ FUN_14013ff40() — 构建 auth 帧头
  ├─ FUN_14009a46c(param_1, 3, 1) — 序号生成器 (NOT ring selector!)
  └─ FUN_14000cf90(HIF_handle, frame_buf, frame_len, seq_num)
       └─ FUN_140053714 (NdisCommonHifPciMiniportMMRequest)
            ├─ FUN_1400aa324(adapter, 0x1600b71, &buf_ptr) — 分配 TX buffer
            └─ FUN_140053a0c (NdisCommonHifPciMlmeHardTransmit)
                 ├─ FUN_140035aec(adapter, 1) — DMA buffer 分配 (queue=1=mgmt)
                 ├─ memcpy(buf + 0x20, frame_data, frame_len) — 帧放在 TXD 后面
                 ├─ *(buf+0x68) = 0x20 + frame_len  — 总长度
                 ├─ XmitWriteTxDv1() — 写 TXD DW0-DW7 到 buf+0x00
                 └─ N6PciTxSendPkt(HIF, buf_node, **2**) — ring=2 HARDCODED!
                      ├─ FUN_14000cc44(adapter, 2, ?, 0xb, ?) — 检查 ≥11 空闲描述符
                      ├─ FUN_1400359cc() — mgmt 队列入队
                      ├─ 写 DMA descriptor (16 bytes)
                      └─ FUN_140009a18() — ring kick (写 CIDX)
```

### 3.2 关键确认

1. **Ring 2 是 SF mode**: TXD (32 bytes) + 802.11 帧连续放在单个 DMA buffer 中
   - `buf[0x00..0x1F]` = TXD (32 bytes)
   - `buf[0x20..end]` = 802.11 frame
   - **不使用 TXP scatter-gather**

2. **Ring index = 2 是硬编码的**: `FUN_14005d1a4(HIF_handle, buf_node, 2)` — param_3=2

3. **N6PciTxSendPkt 中 param_3 的含义**:
   - `param_3 < 2`: DATA 路径 → CT mode, TXP, Ring 0/1
   - `param_3 >= 2`: MGMT 路径 → SF mode, 直接 inline, Ring 2

4. **空闲描述符检查**: mgmt 需要 ≥11 个空闲 (`FUN_14000cc44` 第4参数=0xb)

5. **Ring kick**: 写 CIDX 寄存器 `MT_WPDMA_TX_RING_CIDX(2) = 0xd4328`

> **来源**: `docs/win_re_full_txd_dma_path.md` Section 2-7, `docs/win_re_dma_descriptor_format.md` Section 1-3

### 3.3 与我们的对比

| 步骤 | Windows | 我们的驱动 | 匹配? |
|------|---------|-----------|-------|
| Buffer 格式 | TXD+frame inline (SF) | TXD+frame inline (SF) | ✅ |
| TXD 偏移 | buf+0x00 (32 bytes) | buf+0x00 (32 bytes) | ✅ |
| Frame 偏移 | buf+0x20 | buf+0x20 (MT_TXD_SIZE) | ✅ |
| Ring index | 2 (hardcoded) | 2 (ring_tx2) | ✅ |
| DMA descriptor | 16 bytes | 16 bytes | ✅ |
| Ring kick | write CIDX | write CIDX | ✅ |
| 空闲描述符检查 | ≥11 | ≥1 | ⚠️ (无害) |

**结论: Ring 2 帧提交流程完全匹配。**

---

## 4. Packed Prefetch vs Per-ring EXT_CTRL

### 4.1 Windows Prefetch 配置

Windows 在 `MT6639WpdmaConfig` (FUN_1401e5be0) 中只使用 **packed prefetch**:

```c
// 来源: docs/win_re_wfdma_glo_cfg.md Section 4
WRITE(0xd70f0, 0x00660077);   // CFG0
WRITE(0xd70f4, 0x00001100);   // CFG1
WRITE(0xd70f8, 0x0030004f);   // CFG2
WRITE(0xd70fc, 0x00542200);   // CFG3
```

**Windows MT6639InitTxRxRing 不写 per-ring EXT_CTRL 寄存器。**
EXT_CTRL (0xd4600-0xd468c) 只出现在 STOP 路径 (FUN_1401e6430) 中用于清零。

> **来源**: `docs/win_re_wfdma_glo_cfg.md` Section 5

### 4.2 Packed Prefetch 值解析

```
CFG0: 0x00660077 — 编码 TX ring 0 和 TX ring 15 的 prefetch
CFG1: 0x00001100 — 编码 TX ring 16 和 RX ring 4
CFG2: 0x0030004f — 编码 RX ring 6 和 RX ring 7
CFG3: 0x00542200 — 编码其他 ring 或预留
```

**观察**: Packed prefetch 值中**没有明确包含 Ring 2 的配置**。但 Ring 2 的 DMA 仍然能工作（DIDX 前进），说明 packed prefetch 可能不是 Ring 2 功能的必要条件，或者 Ring 2 嵌入在 CFG3 中。

### 4.3 我们的配置

我们同时配置了两套:

```c
// Packed prefetch (与 Windows 相同)
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG0, 0x660077);
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG1, 0x1100);
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG2, 0x30004f);
mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG3, 0x542200);

// Per-ring EXT_CTRL (Windows 不写!)
mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(2), PREFETCH(0x02c0, 0x4));
```

### 4.4 差异分析

| 配置 | Windows | 我们 | 差异风险 |
|------|---------|------|---------|
| Packed prefetch | ✅ | ✅ 相同值 | 无 |
| Per-ring EXT_CTRL for Ring 2 | **不写** | **写 0x02c00004** | ⚠️ 可能冲突 |

**风险**: 如果 WFDMA 硬件同时处理 packed prefetch 和 per-ring EXT_CTRL，两者可能冲突。Windows 选择只用 packed prefetch，我们额外写了 per-ring EXT_CTRL。

**但**: 当前 Ring 0 CT mode 的 auth TX 也有 MPDU_ERR，Ring 0 同样有 per-ring EXT_CTRL。如果 per-ring EXT_CTRL 是问题所在，Ring 0 也应该受影响。因此 **EXT_CTRL 不太可能是 MPDU_ERR 的根因**。

---

## 5. Ring 2 的中断处理

### 5.1 中断位

- **BIT(6)** = `HOST_TX_DONE_INT_ENA2` — Ring 2 TX 完成中断

Windows 中断掩码 = `0x2600f000`，**不包含 BIT(6)**！
Windows 的标准中断掩码只包含:
- BIT(29) = MCU2HOST_SW_INT
- BIT(25) = 未确定
- BIT(15:12) = RX ring 4/5/6/7 完成

> **发现**: Windows 中断掩码没有 TX ring 完成中断！这意味着 Windows 不依赖 TX ring 中断来检测完成 — 可能使用轮询或其他机制。

### 5.2 我们的中断掩码

```c
#define MT_WFDMA_INT_MASK_WIN  0x2600f050  // 添加了 BIT(4) TX0 + BIT(6) TX2
```

我们添加了 BIT(4) 和 BIT(6)，这比 Windows 多。这不应导致问题 — 额外的中断只会让我们更快收到 TX 完成通知。

### 5.3 TXFREE / TX_DONE

Ring 2 的 TX 完成通过 TXFREE/TX_DONE 事件报告。当前（Ring 0 CT mode），我们能收到 TX_DONE 事件并看到 MPDU_ERR (status=3, count=30)。

**Ring 2 SF mode** 曾测试过: DMA 消费成功 (DIDX 前进) 但固件完全静默 — 无中断、无 TX_DONE、无 TXFREE。这是 **Session 22 确认的**：固件在消费第一帧后挂死，Ring 15 MCU 命令也停止响应。

---

## 6. Ring 0 vs Ring 2 对比

### 6.1 Windows 使用模式

| 属性 | Ring 0 | Ring 2 |
|------|--------|--------|
| **用途** | 数据帧 (AC_BE/AC_BK) | 管理帧 (auth/assoc/probe) |
| **Buffer 格式** | CT mode (TXD+TXP + separate payload) | SF mode (TXD+frame inline) |
| **DMA DW1 公式** | `(DW1 & 0xc040ffff \| 0x400000) & 0x7fffffff \| 0x40000000` | `(len << 16 & 0x3fff0000) \| 0x40000000` |
| **DMA DW1 值** | `0x40400000` (BIT(30)+BIT(22)) | `(len << 16) \| BIT(30)` |
| **空闲检查** | 1 描述符 | 11 描述符 |
| **TXP 使用** | 是 (scatter-gather) | 否 (inline) |
| **TXD 构建函数** | XmitWriteTxDv1 (相同) | XmitWriteTxDv1 (相同) |
| **Ring kick** | write CIDX | write CIDX (相同) |
| **N6PciTxSendPkt param_3** | < 2 | >= 2 |

> **来源**: `docs/win_re_dma_descriptor_format.md` Section 3, 6

### 6.2 TXD 差异 (Ring 0 data vs Ring 2 mgmt)

TXD 是由同一个函数 `XmitWriteTxDv1` 构建的，但 TxInfo 结构不同:

| TXD 字段 | Ring 0 (data) | Ring 2 (mgmt/auth) |
|----------|---------------|---------------------|
| DW0 PKT_FT | 依赖模式 | 0 (SF) |
| DW0 Q_IDX | 数据队列 | 1 |
| DW0 SN_EN | 1 | 1 |
| DW1 HDR_FORMAT | 802.3 (0b01) | 802.11 (0b11) |
| DW1 FIXED_RATE | 0 (auto rate) | 1 (fixed rate) |
| DW2 | 数据帧 flags | **0xA000000B** (fixed rate + auth) |
| DW5 | PID tracking (条件性) | **0x600 \| PID** (TX status request) |
| DW6 | 0 (auto rate) | **0x004B0000** (OFDM 6Mbps) |
| DW7 | DW7 有 frame type | **0** (frame type in DW2 for fixed-rate) |

### 6.3 我们当前的 TX 路径

**当前 Session 21 使用 Ring 0 CT mode** 发送 auth 帧:
- Ring 0 CT mode → TXFREE + TX_DONE(MPDU_ERR, status=3, count=30)
- PHY 确实发送 30 次重试
- AP 不回 ACK

**历史测试 Ring 2 SF mode**:
- DMA 消费成功 (DIDX 前进)
- 固件崩溃 (Session 22: Ring 15 MCU 命令停止响应)
- 无任何 TX_DONE/TXFREE/中断

---

## 7. Ring 2 相关的 MCU 命令

### 7.1 Windows PostFwDownloadInit 中无 Ring 2 特殊命令

PostFwDownloadInit (FUN_1401c9510) 的完整步骤:

1. 清除内部标志
2. **DMASHDL enable**: `readl(0xd6060) |= 0x10101`
3. NIC_CAP (CID=0x8a)
4. Config (CID=0x02)
5. Config2 (CID=0xc0)
6. DBDC (CID=0x28)
7. 1ms delay
8. ScanConfig (CID=0xca)
9. ChipConfig (CID=0xca)
10. LogConfig (CID=0xca)

**没有任何专门配置 Ring 2 的 MCU 命令。** Ring 2 在 Windows 中"天然"就能工作 — 只要 ring 的 DMA 硬件寄存器正确设置、DMASHDL enable bits 存在，固件就能处理 Ring 2 上的帧。

> **来源**: `docs/win_re_hif_ctrl_investigation.md` Section 1

### 7.2 HIF_CTRL

HIF_CTRL (CID=0x07) **不是 TX 使能命令** — 它是纯粹的 suspend/resume 电源管理。Windows 在 probe/init 期间从不发送 HIF_CTRL。

> **来源**: `docs/win_re_hif_ctrl_investigation.md` Section 2

### 7.3 DMASHDL QUEUE_MAP0

`0xd6060 |= 0x10101` (BIT(0)|BIT(8)|BIT(16)) 是 PostFwDownloadInit 中唯一与 DMA 相关的操作。我们已在 `mt7927_post_fw_init()` 中实现了这一步:

```c
// src/mt7927_pci.c line 1580-1589
u32 dmashdl_qm0 = mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0);
dmashdl_qm0 |= 0x10101;
mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP0, dmashdl_qm0);
```

### 7.4 连接流程中的 MCU 命令 (发送 auth 帧之前)

Windows 连接流程 (来源: `docs/win_re_connection_flow.md`):

```
WdiTaskConnect
  → [1] DEV_INFO (activate)
  → [2] nicUniCmdChReqPrivilege (信道请求)
  → [3] MtCmdSetBssInfo (BSS_INFO — 多 TLV)
  → [4] MtCmdSendStaRecUpdate (STA_REC — 多 TLV)
  → [5] MlmeAuthReqAction (auth 帧发送)
```

**我们缺失的关键步骤**:
- BSS_INFO 只发 3 个 TLV (BASIC+RLM+MLD), Windows 发 **14 个 TLV**
- STA_REC 只发 5 个 TLV, Windows 发 **13 个 TLV**
- Config 命令 (CID=0x0b) 被跳过

---

## 8. Ring 2 固件崩溃的可能原因

### 8.1 现象回顾

Session 22 测试 Ring 2 SF mode:
1. DMA 消费第一帧成功 (DIDX 从 0 变为 1)
2. 固件立即挂死 — Ring 15 MCU 命令停止响应
3. 无任何 TX_DONE、TXFREE、中断
4. 需要 PCI remove+rescan 恢复

### 8.2 可能的崩溃原因 (推测)

| 可能原因 | 证据 | 可能性 |
|----------|------|--------|
| **TXD DW0 格式导致固件解析错误** | Session 16 前的旧 TXD 格式可能与固件不兼容 | 🟡 中 |
| **缺少 BSS_INFO TLV** | 固件找不到 BSS 的速率集/保护模式 | 🟡 中 |
| **per-ring EXT_CTRL 与 packed prefetch 冲突** | Windows 不写 EXT_CTRL | 🟡 中 |
| **Ring 2 初始化时序** | Windows pre-FWDL 初始化, 我们 post-FWDL | 🟢 低 |
| **DMASHDL 配置覆盖** | full init 覆盖了固件的 DMASHDL 设置 | 🟢 低 (已修复) |

**注意**: 当前使用的是 Ring 0 CT mode (MPDU_ERR)，不是 Ring 2 SF mode。Ring 2 崩溃问题是历史问题，当前重点是解决 Ring 0 的 MPDU_ERR。

---

## 9. 完整差异总结

### 9.1 Ring 2 / DMA 层差异 (不太可能是 MPDU_ERR 根因)

| # | 差异 | 影响评估 |
|---|------|---------|
| 1 | Per-ring EXT_CTRL: 我们写, Windows 不写 | 可能导致 Ring 2 SF 崩溃, 但不影响 Ring 0 CT |
| 2 | Ring 2 初始化时序: 我们 post-FWDL, Windows pre-FWDL | 低风险 |
| 3 | 空闲描述符检查: 1 vs 11 | 无害 |
| 4 | 中断掩码: 我们多了 BIT(4)+BIT(6) | 无害 (额外中断) |

### 9.2 上层差异 (可能是 MPDU_ERR 根因)

| # | 差异 | 优先级 |
|---|------|--------|
| 1 | **BSS_INFO 缺 11 个 TLV** (RATE/PROTECT/IFS_TIME 等) | 🔴 高 |
| 2 | **Config 命令被跳过** (CID=0x0b, 12字节 payload) | 🟡 中 |
| 3 | **STA_REC 缺 8 个 TLV** (HT/VHT/HE 等) | 🟡 中 |
| 4 | **STA_REC option 0x07 vs Windows 0xed** | 🟡 中 |

---

## 10. 结论与建议

### Ring 2 DMA 层: 基本正确

Ring 2 的初始化、DMA 描述符格式、帧提交流程与 Windows 基本一致。DMA 层不是 MPDU_ERR 的根因。

### MPDU_ERR 的根因在更上层

当前 Ring 0 CT mode 发送 auth 帧:
- PHY 确实发射了 30 次 (B0_TX20=30 / B1_TX20=30)
- TX_DONE 确认速率 = OFDM 6Mbps (rate=0x004b)
- AP 完全不回 ACK

这说明:
1. TXD 格式正确 (否则固件不会发射)
2. DMA 路径正确 (帧到达 PHY)
3. **问题在 MAC 层帧内容或 BSS/STA 配置**

### 建议修复顺序

1. **BSS_INFO 补充 RATE TLV** — 固件可能因为不知道 BSS 的合法速率集而使用错误的 TX 参数
2. **BSS_INFO 补充 PROTECT + IFS_TIME TLV** — Windows `nicUniCmdSetBssRlm` 同时发 RLM+PROTECT+IFS_TIME
3. **Config 命令** (CID=0x0b) — PostFwDownloadInit step 3
4. **STA_REC 补充 TLV** — HT_INFO, VHT_INFO, HE_BASIC 等
5. **审计 STA_REC option 字段** — Windows 用 0xed, 我们用 0x07

---

## 附录 A: 关键 Ghidra 地址

| 函数 | 地址 | 用途 |
|------|------|------|
| XmitWriteTxDv1 | 0x1401a2ca4 | TXD DW0-DW7 构建 |
| FUN_1401a2c8c | 0x1401a2c8c | XmitWriteTxDv1 调用者 |
| N6PciTxSendPkt | 0x14005d1a4 | DMA 描述符 + ring kick |
| FUN_14000cc44 | 0x14000cc44 | 空闲描述符检查 |
| FUN_1400532e4 | 0x1400532e4 | NdisCommonHifPciFreeDescriptorRequest |
| FUN_1400359cc | 0x1400359cc | mgmt 队列入队 |
| FUN_140009a18 | 0x140009a18 | ring kick (MMIO write) |
| FUN_140057d48 | 0x140057d48 | NdisCommonHifPciWriteReg |
| MT6639InitTxRxRing | 0x1401e4580 | Ring 初始化 (v5705275) |
| MT6639WpdmaConfig | 0x1401e5be0 | 预取 + GLO_CFG |
| DMASHDL init | 0x1401d7738 | readl(0xd6060) |= 0x10101 |
| MlmeAuthReqAction | 0x14013f660 | Auth 帧发送入口 |
| NdisCommonHifPciMlmeHardTransmit | 0x140053a0c | TXD 构建 + DMA 提交 |

## 附录 B: 关键寄存器地址

| 寄存器 | BAR0 偏移 | Windows 地址 | 用途 |
|--------|----------|-------------|------|
| TX_RING2_BASE | 0xd4320 | 0x7c024320 | Ring 2 DMA 基地址 |
| TX_RING2_CNT | 0xd4324 | 0x7c024324 | Ring 2 描述符数量 |
| TX_RING2_CIDX | 0xd4328 | 0x7c024328 | Ring 2 CPU index |
| TX_RING2_DIDX | 0xd432c | 0x7c02432c | Ring 2 DMA index |
| TX_RING2_EXT_CTRL | 0xd4608 | 0x7c024608 | Ring 2 预取 (Windows 不写!) |
| GLO_CFG | 0xd4208 | 0x7c024208 | DMA 全局配置 |
| DMASHDL_QMAP0 | 0xd6060 | 0x7c026060 | DMASHDL 队列映射 |
| PREFETCH_CFG0 | 0xd70f0 | 0x7c0270f0 | Packed 预取配置 |
| HOST_INT_STA | 0xd4200 | 0x7c024200 | 中断状态 |
| HOST_INT_ENA | 0xd4204 | 0x7c024204 | 中断使能 |

## 附录 C: RE 文档交叉引用

| 文档 | 内容 | 关联 Section |
|------|------|-------------|
| `win_re_dma_descriptor_format.md` | DMA 描述符精确格式 | Section 2 |
| `win_re_full_txd_dma_path.md` | 完整 TX 路径 + TXD 对比 | Section 3, 6 |
| `win_re_tx_mgmt_path.md` | 管理帧 TX 调用链 | Section 3 |
| `win_re_wfdma_glo_cfg.md` | GLO_CFG + EXT_CTRL 分析 | Section 4 |
| `win_re_dw2_dw6_verified.md` | TXD DW2/DW5/DW6/DW7 汇编验证 | Section 6 |
| `win_re_txd_dw0_dw1_precise.md` | TXD DW0/DW1 精确分析 | Section 6 |
| `win_re_hif_ctrl_investigation.md` | HIF_CTRL + PostFwDownloadInit | Section 7 |
| `win_re_ple_pse_init.md` | DMASHDL init 汇编 | Section 1.1 |
| `win_re_connection_flow.md` | 连接命令序列 | Section 7.4 |
| `ring2_gap_analysis.md` | Ring 2 完整差异分析 | Section 9 |
| `analysis_windows_full_init.md` | Windows 完整初始化流程 | Section 1.1 |
