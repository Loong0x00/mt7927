# HIF_CTRL / TX 使能命令调查报告

**日期**: 2026-02-17
**目的**: 调查 Windows 驱动中 PostFwDownloadInit 后是否存在 HIF_CTRL 或类似的 TX 使能命令
**结论**: **HIF_CTRL 不是 TX 使能命令**，它是电源管理 (suspend/resume) 命令。但调查发现了其他可能的根因。

---

## 1. Windows PostFwDownloadInit 完整步骤对比

### 1.1 Windows 步骤 (来源: ghidra_post_fw_init.md, AsicConnac3xPostFwDownloadInit @ 0x1401c9510)

| # | Windows 步骤 | 我们实现? | 备注 |
|---|-------------|----------|------|
| 1 | 清除内部标志 *(ctx+0x146e61c) = 0 | N/A | 软件标志，非硬件操作 |
| 2 | WRITE 0xd6060 \|= 0x10101 (WFDMA enable) | ❓ **未确认** | **PostFwDownloadInit 最先做的寄存器写入** |
| 3 | MCU cmd class=0x8a (NIC_CAP) | ✅ | 我们的 Step 2 |
| 4 | MCU cmd class=0x02 (Config) | ⚠️ 跳过 | 诊断发现它破坏 MCU 通道，已跳过 |
| 5 | MCU cmd class=0xc0 (Config) | ✅ | 我们的 Step 4 |
| 6 | **AsicConnac3xDownloadBufferBin** | ❌ **缺失** | 额外二进制下载，仅当 flag==1 时 |
| 7 | MtCmdUpdateDBDCSetting class=0x28 | ✅ | 我们的 Step 5 |
| 8 | 1ms 延迟 | ✅ | 我们的 Step 6 |
| 9 | SetPassiveToActiveScan class=0xca | ✅ | 我们的 Step 7 |
| 10 | SetFWChipConfig class=0xca | ✅ | 我们的 Step 8 |
| 11 | SetLogLevelConfig class=0xca | ✅ | 我们的 Step 9 |
| 12 | **"Additional MCU commands via function pointers"** | ❌ **未知** | vtable 可能有额外步骤 |

### 1.2 差异分析

**可能缺失的步骤**:

1. **0xd6060 |= 0x10101** — WFDMA enable 寄存器写入。这是 PostFwDownloadInit 的**第一个操作**，在所有 MCU 命令之前。BAR0 偏移 = 0xd6060。含义:
   - BIT(0): 未知 enable bit
   - BIT(8): 未知 enable bit
   - BIT(16): 未知 enable bit

   **我们的代码中未搜索到对 0xd6060 的写入。这可能是关键缺失。**

2. **AsicConnac3xDownloadBufferBin** — 通过 NdisOpenFile 打开一个额外的二进制文件，以 1KB chunks 通过 MCU cmd (class=0xed, subcmd=0x21) 下载。但仅当 `*(ctx + 0x1467608) == 1` 时执行，**可能是可选的**。

3. **Step 12 的 "function pointers"** — vtable 偏移 +0x50 处有 FUN_1401c3240 (post-init)，在 PostFwDownloadInit 之后执行。这个函数的内容**未被反编译**。

4. **MCU cmd class=0x02** — 我们跳过了它（因为之前发现它破坏 MCU 通道），但 Windows 发送它。Payload: {1, 0, 0x70000}。这可能是 BSS_INFO_UPDATE (CID=2) 的某种初始化配置，我们的 payload 格式可能与 Windows 不一致。

---

## 2. HIF_CTRL 命令详情

### 2.1 mt7925 中的 HIF_CTRL

**CID**: `MCU_UNI_CMD_HIF_CTRL = 0x07`
**Event EID**: `MCU_UNI_EVENT_HIF_CTRL = 0x03`

**用途**: 纯粹的电源管理 — suspend/resume

```c
// mt76_connac_mcu.c: mt76_connac_mcu_set_hif_suspend()
struct {
    struct { u8 hif_type; u8 pad[3]; } hdr;  // hif_type: 0=SDIO, 1=USB, 2=PCIe
    struct { __le16 tag; __le16 len; u8 suspend; } hif_suspend;
} req;
req.hif_suspend.tag = 0;  // UNI_HIF_CTRL_BASIC
req.hif_suspend.suspend = suspend;  // 1=suspend, 0=resume
mt76_mcu_send_msg(dev, MCU_UNI_CMD(HIF_CTRL), &req, sizeof(req), wait_resp);
```

**发送时机** (仅 suspend/resume):
- `mt7925_pci_suspend()` → `set_hif_suspend(dev, true, false)` → 进入休眠
- `mt7925_pci_resume()` → `set_hif_suspend(dev, false, false)` → 从休眠恢复
- `mt7925u_suspend()` / `mt7925u_resume()` — USB 同理

**固件响应**: EVENT_HIF_CTRL 包含:
- `hifsuspend`: 是否成功进入休眠
- `hif_tx_traffic_status` / `hif_rx_traffic_status`: 流量状态 (IDLE=0x2)

**关键: mt7925 在正常 probe/init 期间从不发送 HIF_CTRL！** 它只在 suspend/resume 时使用。

### 2.2 mt6639 中的 HIF_CTRL

```c
// wsys_cmd_handler_fw.h
CMD_ID_HIF_CTRL = 0xF6,  // (Set) USB suspend/resume
EVENT_ID_HIF_CTRL = 0xF6,

struct CMD_HIF_CTRL {
    uint8_t ucHifType;       // HIF 类型
    uint8_t ucHifDirection;  // 方向
    uint8_t ucHifStop;       // 停止标志
    uint8_t ucHifSuspend;    // 休眠标志
    uint32_t u4WakeupHifType;
};
```

**同样是电源管理命令**，与 TX 数据使能无关。

### 2.3 Windows 驱动中的 HIF_CTRL

在所有 Ghidra 反编译输出中 (`/tmp/ghidra_*.txt`)，**未发现任何 HIF_CTRL 相关字符串或调用模式**。

- 搜索 "HifCtrl", "hif_ctrl", "HIF_CTRL": 无结果
- 搜索 "suspend", "resume": 无结果 (在 TX 相关代码中)
- PostFwDownloadInit 中没有 CID=0x07 的命令

**结论: HIF_CTRL 在初始化阶段不存在，不是我们缺失的命令。**

---

## 3. Data Ring Init 后的命令

### 3.1 Windows 初始化完整时序 (vtable)

```
vtable offset | 函数 | 用途
+0x08 | pre-FW setup | FW 下载前准备
+0x10 | DMA init | 初始 DMA 配置
+0x20 | LoadFirmware | FW 下载
+0x28 | post-FW intermediate | FW 启动后中间步骤
+0x40 | HIF init | HIF 初始化 (thunk_FUN_1400506ac)
+0x48 | PostFwDownloadInit | 我们分析的主函数
+0x50 | post-init (FUN_1401c3240) | **PostFwDownloadInit 之后!!**
+0x78 | InitTxRxRing | TX/RX ring 初始化
+0x80 | WpdmaConfig | WPDMA 配置
```

### 3.2 关键发现: post-init (FUN_1401c3240)

vtable +0x50 处的 `FUN_1401c3240` 在 PostFwDownloadInit **之后**执行。这个函数：
- **未被我们反编译过**
- 可能包含额外的 MCU 命令或配置
- 这是一个需要进一步 RE 的目标

### 3.3 Ring 初始化后无额外命令

从 Ghidra 分析来看，N6PciTxSendPkt (TX 提交函数) 直接写 DMA 描述符并 kick ring，**没有在第一次 TX 前发送任何 "enable" 命令**。这意味着 Windows 驱动在 init 阶段就已经完成了所有必要的使能操作。

---

## 4. mt7925 HIF_CTRL 总结

### 4.1 不适用于我们的问题

| 特征 | HIF_CTRL | 我们的问题 |
|------|----------|-----------|
| 用途 | 电源管理 (suspend/resume) | 初始化后 TX 不工作 |
| 调用时机 | 系统进入/退出休眠时 | 第一次 TX 时 |
| 在 probe 中调用? | **否** | - |
| CID | 0x07 | - |
| 发送方式 | fire-and-forget (no ACK) | - |

### 4.2 不建议直接借用

mt7925 的 HIF_CTRL 是挂起/恢复机制，在正常 probe 流程中从不发送。强行在 init 后发送 `suspend=0` (resume) 可能：
- 被固件忽略 (因为从未 suspend 过)
- 导致固件状态混乱
- 不解决实际问题

---

## 5. 真正的根因候选 (按可能性排序)

### 🔴 最高可能性

#### 5.1 缺少 0xd6060 |= 0x10101 寄存器写入

这是 PostFwDownloadInit 的**第一步**，在所有 MCU 命令之前：

```c
val = readl(bar0 + 0xd6060);
writel(val | 0x10101, bar0 + 0xd6060);
```

Ghidra 注释: `ghidra_post_fw_init.md` 明确标注这是 "enable something"。
- BIT(0) + BIT(8) + BIT(16) = 3 个 enable 位
- 可能控制 HOST ↔ FW 的数据通道使能
- **如果这些位未设置，DMA 硬件层可以工作 (descriptor 消费) 但固件软件层不处理数据帧**

**这完美解释了 "DMA 消费成功但固件静默" 的现象。**

**验证方法**: 读取当前 0xd6060 的值，看这 3 个 bit 是否已设置。

#### 5.2 BSS_INFO 缺失关键 TLV

Windows BSS_INFO dispatch table 有 **14 个 TLV**，我们只发 3 个 (BASIC + RLM + MLD)。

最关键的缺失:
| Index | Tag | 名称 | 重要性 |
|-------|-----|------|--------|
| 1 | TBD | **RATE** | **固件不知道 BSS 合法速率集 → 无法选择 TX 速率** |
| 2 | TBD | SEC | 安全配置 |
| 3 | TBD | QBSS | QoS 配置 |

注意: RLM/PROTECT/IFS_TIME (tag 2/3/0x17) 通过独立的 `nicUniCmdSetBssRlm` 发送，不在 dispatch table 中。我们**确实发送了 RLM**，但 **PROTECT 和 IFS_TIME 没有**。

#### 5.3 STA_REC 缺失关键 TLV

Windows STA_REC dispatch table 有 **13 个 TLV**，我们只发 5 个。缺失:
- HT_INFO (index 1)
- VHT_INFO (index 2)
- HE_BASIC (index 3, tag=0x19)
- HE_6G_CAP (index 4, tag=0x17)
- BA_OFFLOAD (index 8)
- UAPSD (index 9)
- EHT_INFO (index 10, tag=0x22)
- EHT_MLD (index 11, tag=0x21)

### 🟡 中等可能性

#### 5.4 MCU cmd class=0x02 被跳过

我们跳过了 PostFwDownloadInit 的 Step 4 (class=0x02, payload={1, 0, 0x70000})，因为之前发现它 "破坏 MCU 通道"。但 Windows 总是发送它。

可能的问题: 我们的 payload 格式与 Windows 不一致。Windows 用的是 legacy MtCmd 格式 (0x40 header) 而非 UniCmd 格式。如果我们用 UniCmd 格式发送 class=0x02，固件可能无法正确解析 → 破坏 MCU 通道。

**需要确认**: 这个命令在 Windows 中走 legacy 还是 UniCmd 路径。

#### 5.5 DownloadBufferBin 缺失

PostFwDownloadInit Step 6 下载额外二进制数据 (class=0xed, subcmd=0x21)。如果固件需要这些数据进行 PHY/RF 校准，缺少它可能导致射频不工作。

但: 此步骤有 flag 保护 (`*(ctx+0x1467608)==1`)，可能是可选的。

### 🟢 低可能性

#### 5.6 post-init 函数 (vtable +0x50) 有未知操作

FUN_1401c3240 可能包含额外的 MCU 命令或寄存器写入，但未被分析。

---

## 6. 固件内部队列状态诊断

### 6.1 已有的 PLE/PSE 读取能力

我们的驱动在 `mt7927_mac.c:676-721` 已经有 PLE/PSE 诊断代码，但**仅在收到 TXFREE (TX completion) 事件时触发**。由于固件完全静默，这些诊断代码永远不会执行。

### 6.2 关键 PLE/PSE 寄存器 (MT6639)

| 寄存器 | 地址 | 用途 |
|--------|------|------|
| WF_PLE_TOP_BASE | 0x820c0000 | PLE 基地址 |
| PLE_QUEUE_EMPTY | 0x820c0360 | 各队列空状态位图 |
| PSE_QUEUE_EMPTY | 0x820c80b0 | PSE 队列空状态 |
| PLE_STA0 | 0x820c0024 | Station 0 TX 排队包数 |
| PLE_STA1 | 0x820c0028 | Station 1 TX 排队包数 |
| PLE_PG_HIF_GROUP | 0x820c000c | HIF group 页面使用 |
| PLE_FREEPG | 0x820c0008 | 空闲页面数 |

### 6.3 建议: 主动诊断

在 TX 提交后添加延迟读取 PLE/PSE 状态:

```c
// 在 mt7927_dma_enqueue_ring2_sf() 之后:
msleep(100);  // 等待固件处理
u32 ple_empty = mt7927_rr_l1(dev, 0x820c0360);
u32 pse_empty = mt7927_rr_l1(dev, 0x820c80b0);
u32 ple_hif = mt7927_rr_l1(dev, 0x820c000c);
u32 ple_freepg = mt7927_rr_l1(dev, 0x820c0008);
dev_info(&dev->pdev->dev,
    "POST-TX: PLE_EMPTY=0x%08x PSE_EMPTY=0x%08x HIF_GRP=0x%08x FREE=0x%08x\n",
    ple_empty, pse_empty, ple_hif, ple_freepg);
```

这可以告诉我们:
- 帧是否进入了固件内部队列 (PLE_EMPTY 的对应 bit 是否清零)
- 帧是否卡在 HIF → PLE 的转换阶段
- 空闲页面是否足够

同时读取 **0xd6060** 的当前值，确认 WFDMA enable bits 是否已设置。

---

## 7. 建议修复计划

### 第一步: 验证 0xd6060 (最快, 最可能)

```c
// 在 mt7927_post_fw_init() 开头添加:
u32 wfdma_en = mt7927_rr(dev, 0xd6060);
dev_info(&dev->pdev->dev, "WFDMA enable: 0xd6060 = 0x%08x\n", wfdma_en);
wfdma_en |= 0x10101;  // BIT(0) | BIT(8) | BIT(16)
mt7927_wr(dev, 0xd6060, wfdma_en);
dev_info(&dev->pdev->dev, "WFDMA enable: 0xd6060 → 0x%08x\n",
         mt7927_rr(dev, 0xd6060));
```

### 第二步: PLE/PSE 主动诊断

TX 提交后读取固件内部状态，确认帧是否进入固件队列。

### 第三步: 修复 class=0x02 命令

研究 Windows 发送 class=0x02 时使用的**确切的 header 格式** (legacy vs UniCmd)，以正确的格式重新发送。

### 第四步: 添加 BSS_INFO RATE TLV

从 Windows dispatch table index 1 的函数 (地址待确认) 反编译 RATE TLV 格式，添加到我们的 BSS_INFO 命令中。

### 第五步: 添加 BSS_INFO PROTECT + IFS_TIME TLV

Windows 的 `nicUniCmdSetBssRlm` 同时发送 RLM + PROTECT + IFS_TIME 三个 TLV，我们只发了 RLM。

### 第六步: 反编译 FUN_1401c3240 (post-init)

这个 vtable +0x50 处的函数在 PostFwDownloadInit 之后执行，可能包含关键操作。

---

## 附录 A: mt7925 HIF_CTRL 完整调用链

```
mt7925_pci_suspend()
  → mt76_connac_mcu_set_hif_suspend(mdev, true, false)
    → MCU_UNI_CMD(HIF_CTRL)
      → CID=0x07, tag=0, suspend=1
    → wait for EVENT_HIF_CTRL (hif_idle=true)
  → 禁用 NAPI, DMA
  → pci_save_state + pci_set_power_state(D3hot)

mt7925_pci_resume()
  → pci_set_power_state(D0) + pci_restore_state
  → 检查是否需要 DMA 重新初始化
  → 启用 NAPI, DMA
  → mt76_connac_mcu_set_hif_suspend(mdev, false, false)
    → MCU_UNI_CMD(HIF_CTRL)
      → CID=0x07, tag=0, suspend=0
    → wait for EVENT_HIF_CTRL (hif_resumed=true)
```

**关键: 在正常 probe 中，mt7925 不发送 HIF_CTRL。它只在 suspend/resume 电源管理中使用。**

## 附录 B: 0xd6060 可能的含义

| Bit | BAR0 偏移 | 可能含义 |
|-----|----------|---------|
| BIT(0) | 0xd6060 | HOST DMA0 TX enable? |
| BIT(8) | 0xd6060 | HOST DMA0 RX enable? |
| BIT(16) | 0xd6060 | MCU ↔ HOST event enable? |

这个寄存器位于 WFDMA 区域 (0xd0000-0xdFFFF)，地址 +0x6060 相对于 WFDMA HOST DMA0 基址 (0xd4000) 的偏移为 0x2060，这不在标准的 WFDMA 寄存器映射中。但它可能是 CONNAC3 特有的使能寄存器。

## 附录 C: 关键 Ghidra 地址参考

| 地址 | 函数 | 用途 |
|------|------|------|
| 0x1401c9510 | AsicConnac3xPostFwDownloadInit | PostFwDownloadInit 主函数 |
| 0x1401c3240 | (post-init, 未分析) | PostFwDownloadInit 后的额外步骤 |
| 0x1401d6d30 | MT6639InitTxRxRing | Ring 初始化 |
| 0x1401d8290 | MT6639WpdmaConfig | WPDMA 配置 |
| 0x14005d1a4 | N6PciTxSendPkt | TX DMA 提交 |
| 0x1401a2ca4 | XmitWriteTxDv1 | TXD 构建 |
| 0x1402505b0 | (data) | BSS_INFO dispatch table (14 entries) |
| 0x140250710 | (data) | STA_REC dispatch table (13 entries) |
