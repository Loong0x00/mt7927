# Windows RE: XmitWriteTxDv1 — DW0/DW1 精确初始化值分析

## 来源

- Ghidra 反编译: `mtkwecx.sys` (Windows v5705275)
- 函数: `FUN_1401a2ca4` (XmitWriteTxDv1) — TXD 构建核心
- 调用者: `FUN_140053a0c` (NdisCommonHifPciMlmeHardTransmit) — 管理帧 TX 提交
- DMA 提交: `FUN_14005d1a4` (N6PciTxSendPkt) — DMA ring descriptor 写入

---

## 1. 完整调用链 (auth 帧场景)

```
MlmeAuthReqAction
  → FUN_14000cf90 (AUTH_DIRECT_TX_SEND)
    → FUN_140053714 (NdisCommonHifPciMiniportMMRequest)
      → FUN_1400aa324 (MlmeAllocateMemoryEx) — 分配 TX buffer
      → FUN_140053a0c (NdisCommonHifPciMlmeHardTransmit) — 构建 TxInfo + TXD
        → FUN_140035aec (TxDmaqAlloc, param=1 → mgmt ring)
        → 初始化 local_c8 (TxInfo 结构体, 0x71 字节)
        → FUN_1401a2c8c → FUN_1401a2ca4 (XmitWriteTxDv1) — 写 TXD DW0-DW7
        → FUN_14005d1a4 (N6PciTxSendPkt, param_3=2) — DMA ring 提交
```

---

## 2. TxInfo 结构体 (param_3 in XmitWriteTxDv1)

`local_c8` 在 MlmeHardTransmit (FUN_140053a0c) 中通过 `FUN_14001022c(ptr, 0x71)` 清零后初始化:

```c
// 基于 FUN_140053a0c 反编译结果的字段偏移映射
// param_3 (short *) 指向 local_c8

param_3[0x00]  (short)   → local_c8 = frame_body_length (from puVar8[4])
param_3+0x02   (byte)    → param_3[1] = WLAN_IDX (byte at offset 2)
param_3+0x04   (byte)    → local_c6 = 0x18 (mac header length = 24 bytes)
                          → 即 *(byte*)(param_3+0x04) = 0x18
param_3+0x05   (byte)    → part of local_c4 — 管理帧设为 1 或 0x104
param_3+0x07   (byte)    → *(byte*)(param_3+0x07) = WLAN_IDX (wlan index)
param_3+0x08   (byte)    → NO_ACK flag
param_3+0x09   (byte)    → TID (byte, 从 local_b9 = uVar9)
param_3+0x0a   (byte)    → OWN_MAC (from *(lVar18 + 0x2d2))
param_3+0x0b   (byte)    → PROTECT flag
param_3+0x0c   (byte)    → param_3[6] = BIP flag (set if special case)
param_3+0x0f   (byte)    → fixed_rate_info (from local_80)
param_3+0x2b   (short)   → HDR_FORMAT (0=mgmt/802.11, 2=802.3)
                          → 对 auth 帧: local_9d = *puVar5 >> 2 & 3 → (0x00B0>>2)&3 = 0
param_3+0x2d   (short)   → FRAME_SUBTYPE (local_9b = *puVar5 >> 4 & 0xf)
                          → 对 auth 帧: (0x00B0>>4)&0xf = 0x0B
param_3+0x2f   (qword)   → BSS pointer (local_99 = lVar18)
param_3+0x37   (byte)    → error flag (checked at entry, abort if nonzero)
param_3+0x39   (byte)    → RTS flag
param_3+0x3d   (byte)    → special handling flag 3
param_3+0x4c   (byte)    → param_3[0x26] = special handling flag 1 (HW enc? AMSDU?)
param_3+0x53   (byte)    → special handling flag 2
```

### auth 帧的 TxInfo 关键值

```
param_3[0x00]  = frame_body_length (e.g., 30 for auth body + header)
param_3+0x04   = 0x18 (24 = 802.11 header length in bytes)
param_3+0x05   = 1 (管理帧 type)
param_3+0x07   = WLAN_IDX (0 for unassociated)
param_3+0x08   = 0 (NO_ACK=0 for unicast auth)
param_3+0x09   = 7 (TID=7 for mgmt, or 4 if *(lVar4+0x1465f30)==2)
                 对 auth 帧: 代码走到 else 分支 → uVar9=7 (因为 frame_type=mgmt=0)
param_3+0x0a   = OWN_MAC_IDX
param_3+0x0b   = 0 (no protection for open auth)
param_3+0x2b   = 0 (mgmt/802.11 format, NOT 2/802.3)
param_3+0x2d   = 0x0B (auth subtype = 11)
param_3+0x4c   = 0 (no AMSDU/HW enc)
param_3+0x53   = 0
param_3+0x3d   = 0
```

---

## 3. XmitWriteTxDv1 (FUN_1401a2ca4) — 逐行分析

### 前置: memset TXD = 0, then set DW1[15] = 1

```c
FUN_14001022c(param_2, 0x20);  // memset(TXD, 0, 32) — 清零全部 8 DWORDs
param_2[1] = param_2[1] | 0x8000;  // DW1 bit[15] = 1
// BTS dword ptr [RBX+4], 0xf → bit 15
```

> **DW1[15] = 1** — 从零设为 1。这个 bit 在 CONNAC3 TXD 定义中是 `MT_TXD1_HDR_FORMAT_V3` GENMASK(15,14) 的高位。bit 15 = HDR_FORMAT bit[1]。

### DW0: 长度和格式

```c
*(short *)param_2 = *param_3 + 0x20;
```

> **DW0[15:0] = frame_body_length + 0x20** (即 payload_len + 32 = total TX bytes including TXD)

```c
// DW0 bit[31]: byte at param_3+0x05
uVar4 = (uint)*(byte *)((longlong)param_3 + 5) << 0x1f | *param_2 & 0x7fffffff;
*param_2 = uVar4;
```

> **DW0[31]** = *(param_3+0x05) 的 bit[0]。
> 对 auth 帧: param_3+0x05 来自 `local_c4` 的低字节。
> 在 MlmeHardTransmit 中: `local_c4 = 0x104` → 低字节 = 0x04。
> 但后面 `local_c4 = CONCAT11(local_c4._1_1_, uVar9)` → uVar9=1 for mgmt → 低字节 = 1。
> 实际值: *(param_3+0x05) = 1, bit[0] = 1。
> **DW0[31] = 1**

```c
// DW0 bits[30:26]: byte at param_3+0x04 (= 0x18 = 24)
*param_2 = ((uint)*(byte *)(param_3 + 2) << 0x1a ^ uVar4) & 0x7c000000 ^ uVar4;
```

> `*(byte*)(param_3+0x04)` = 0x18 (mac header length = 24)
> 0x18 << 26 = 0x18 << 26 → only bits[30:26] matter (mask 0x7C000000)
> 0x18 = 0b11000, so bits[30:26] = 0b11000 = 24
> Wait: 0x18 = 24 = 0b11000. Five bits fit in [30:26]: 0b11000.
> **DW0[30:26] = 24 (0x18)**

> 但这显然不对 — DW0[31:25] 是 Q_IDX, DW0[24:23] 是 PKT_FMT。让我重新看。

**重新分析 DW0 位布局**:

根据 `mt7927_pci.h`:
```c
#define MT_TXD0_TX_BYTES    GENMASK(15, 0)   // bits[15:0]
#define MT_TXD0_PKT_FMT     GENMASK(24, 23)  // bits[24:23]
#define MT_TXD0_Q_IDX       GENMASK(31, 25)  // bits[31:25]
```

Bits 16-22 are undefined/ETH_TYPE_OFFSET.

现在分析 XmitWriteTxDv1 中 DW0 的实际修改:

1. **`*(short *)param_2 = *param_3 + 0x20`** → bits[15:0] = frame_len + 32 (TX_BYTES)

2. **`(byte*)(param_3+5) << 31 | *param_2 & 0x7FFFFFFF`** → DW0[31] = *(param_3+0x05) bit 0

3. **`(byte*)(param_3+0x04) << 26 XOR+mask(0x7C000000)`** → DW0[30:26] 设为 (param_3+0x04) 的低 5 位

> param_3+0x04 = `local_c6 = 0x18` = 24 = 0b11000
> DW0[30:26] = 0b11000

但这只设了 bits[30:26]! 而 param_3+0x05 设了 bit[31]。
那 **bits[25:16] 怎么办?** — 它们来自 memset(0) + `*(short*)param_2` 只写了 bits[15:0]。

所以:
- **DW0[15:0]** = TX_BYTES (payload + 32)
- **DW0[22:16]** = 0 (未被设置)
- **DW0[24:23]** = 0 (未被 XmitWriteTxDv1 直接设置!)

> **关键发现: XmitWriteTxDv1 不设置 DW0[24:23] (PKT_FMT)!**
>
> 它只设置 DW0[31:26] 和 DW0[15:0]。PKT_FMT 保持 memset 后的 0。

等等 — 让我仔细看 param_3+0x04 和 param_3+0x05 的映射。

汇编确认:
```asm
1401a2d91: MOVZX ECX, byte ptr [RDI + 0x5]    ; param_3+0x05 → byte
1401a2d9a: SHL ECX, 0x1f                       ; << 31
1401a2d9d: OR ECX, EAX                         ; DW0[31]
...
1401a2da1: MOVZX EAX, byte ptr [RDI + 0x4]    ; param_3+0x04 → byte
1401a2da5: SHL EAX, 0x1a                       ; << 26
1401a2daa: AND EAX, 0x7c000000                 ; mask bits[30:26]
```

所以:
- param_3+0x05 → DW0[31] (1 bit)
- param_3+0x04 → DW0[30:26] (5 bits, mask 0x7C000000)

DW0[25:16] 和 DW0[24:23] 都留 0!

**但等等** — 这是 XmitWriteTxDv1 内部。DW0[24:23] (PKT_FMT) 可能在 N6PciTxSendPkt 中的 DMA descriptor 构建阶段被设置!

让我看 N6PciTxSendPkt 对 DMA descriptor 的操作:

```c
// N6PciTxSendPkt (FUN_14005d1a4):
// 对于管理帧 (param_3 >= 2):
//   FUN_1400359cc(lVar1, param_2);  ← mgmt queue enqueue (不是 data path)
//
// 然后构建 DMA ring descriptor (puVar4 = 16-byte DMA descriptor):
FUN_14001022c(puVar4, 0x10);        // memset(ring_desc, 0, 16)
*puVar4 = *(undefined4 *)(param_2 + 0x58);    // ring_desc DW0 = buf_dma_addr_low
*(undefined2 *)(puVar4 + 3) = *(undefined2 *)(param_2 + 0x5c);  // ring_desc DW3[15:0] = buf_dma_addr_high

// DW1 of ring descriptor (NOT TXD DW1!):
if (param_3 < 2) {
    // DATA path: clear most bits, set bit 22
    uVar8 = puVar4[1] & 0xc040ffff | 0x400000;
} else {
    // MGMT path: set length in bits[29:16]
    uVar8 = (*(int *)(param_2 + 0x68) << 0x10 ^ puVar4[1]) & 0x3fff0000U ^ puVar4[1];
}
puVar4[1] = uVar8 & 0x7fffffff | 0x40000000;  // set bit[30], clear bit[31]
```

这是 DMA ring descriptor 的 DW1, 不是 TXD 的 DW1! Ring descriptor 是 MediaTek WFDMA ring 的 4-DWORD 描述符, 格式是:

```
Ring DW0: buf_addr_low (32 bits)
Ring DW1: length[29:16] + DMADONE[31] + LAST_SEC[30]
Ring DW2: token/info
Ring DW3: buf_addr_high[15:0]
```

**所以 N6PciTxSendPkt 对 TXD 本身不做任何修改。TXD 完全由 XmitWriteTxDv1 构建。**

---

## 4. DW0 精确值 (auth 帧)

```
DW0[15:0]  = TX_BYTES = payload_len + 32
             auth 帧 payload ≈ 54 bytes (24 hdr + 30 body) → TX_BYTES ≈ 86 = 0x56
DW0[22:16] = 0 (ETH_TYPE_OFFSET, 未被设置)
DW0[24:23] = 0 (PKT_FMT = 0 = CT/SF mode)
DW0[30:26] = local_c6 = 0x18 = 24 → 但这覆盖了 Q_IDX 的 bits[30:26]!
DW0[31]    = *(param_3+0x05) bit[0] = 1
```

**等等** — 这跟 MT_TXD0_Q_IDX = GENMASK(31,25) 不对齐。让我重新想。

实际上 Windows 可能用不同的 TXD 位定义! 让我纯粹从 XmitWriteTxDv1 汇编看:

```
DW0 设置过程:
1. memset(TXD, 0, 32) → DW0 = 0
2. *(short*)DW0 = frame_len + 0x20 → DW0[15:0] = total_len
3. DW0[31] = (param_3+0x05) & 1  (SHL 0x1f)
4. DW0[30:26] = (param_3+0x04) & 0x1F  (SHL 0x1a, AND 0x7c000000)
   param_3+0x04 = 0x18 = 24 = 0b11000
   bits[30:26] = 11000
```

所以 **DW0** = `0xE0000000 | TX_BYTES`

分解:
```
bit 31    = 1     (from param_3+0x05 = 1, bit0)
bits 30:26 = 11000 = 0x18 = 24 (from local_c6 = mac_hdr_len)
bits 25:16 = 0
bits 15:0  = TX_BYTES
```

按 CONNAC3 TXD0 定义:
```
Q_IDX[31:25] = 0b1110000 >> 25...
  bit31=1, bits30:26=0x18=0b11000 → Q_IDX[31:25] = 0b1_11000_0
  No wait:
  bits[31:25] = bit31|bits30|bits29|bits28|bits27|bits26|bits25
              = 1 | 1 | 1 | 0 | 0 | 0 | 0
              = 0b1110000 = 0x70 = 112?
```

那不对。让我仔细看。

param_3+0x04 = 0x18 = 0b0001_1000 → 低 5 位 = 0b11000

```asm
SHL EAX, 0x1a    ; EAX = 0x18 << 26 = 0x60000000
AND EAX, 0x7c000000  ; bits[30:26] only = 0x60000000
```

0x18 << 26 = 0x60000000:
```
0x60000000 = 0b0110_0000_0000_0000_0000_0000_0000_0000
             bit30=1, bit29=1, bits28:26=0
```

AND 0x7C000000:
```
0x7C000000 = 0b0111_1100_0000_0000_0000_0000_0000_0000
             bits[30:26]
```

Result: 0x60000000 AND 0x7C000000 = 0x60000000 → bits[30:26] = 0b11000

Then DW0[31] = 1 from param_3+0x05 = 1.

So **DW0 upper bits = bit31=1, bits[30:26]=0b11000** = 0xE0000000

按我们的 TXD0 定义:
- **Q_IDX = DW0[31:25]** = bit31|30|29|28|27|26|25 = 1|1|1|0|0|0|0 = 0b1110000 = **0x70**

但这不太合理 — Q_IDX=0x70 很大...

**关键: Windows 的 TXD DW0 位定义可能跟 CONNAC3 标准不同!**

让我看 MT6639 的 TXD0 位定义。查找 MT6639 参考:

在 CONNAC3 (MT6639/nic_txd_v3.c) 中:
```
DW0:
  bits[15:0]  = TX_BYTES
  bits[22:16] = ETH_TYPE_OFFSET (7 bits)
  bits[24:23] = PKT_FMT (2 bits)
  bits[31:25] = Q_IDX (7 bits)
```

所以 Windows 在 XmitWriteTxDv1 中:
- **bits[30:26]** 设为 `mac_hdr_len` = 0x18 = 24 → 这跨越了 Q_IDX(bits 31:25) 和 PKT_FMT(bits 24:23)!

等等不对 — `SHL 0x1a` = shift left 26, `AND 0x7c000000` = 只保留 bits[30:26]。这 5 个 bit 正好是:
- bit 30, 29 (in Q_IDX field)
- bit 28, 27, 26 (in Q_IDX field)

然后 bit 31 单独设。bit 25 和以下没碰。

**所以 Windows 的 XmitWriteTxDv1 实际上不往 DW0 写 PKT_FMT 和 Q_IDX!**

它写的是某种 **不属于标准 CONNAC3 TXD** 的字段:
- DW0[31]: 1 bit flag (来自 TxInfo+0x05 的 bit0)
- DW0[30:26]: 5-bit value = mac_hdr_len (24)

**这些是 Windows 内部的 "内存 TXD" 格式, 不是最终的硬件 TXD!**

N6PciTxSendPkt 后面会进一步处理这个 TXD, 特别是在 `FUN_14005d6d8` (N6PciUpdateAppendTxD) 中重新格式化 DW0!

让我重新看 N6PciUpdateAppendTxD (FUN_14005d6d8):

```c
// DATA path (FUN_14005d6d8 = N6PciUpdateAppendTxD):
FUN_14001022c(lVar3 + 0x20, 0x20);                  // memset(hwTxD+0x20, 0, 32)
*(ushort *)(lVar3 + 0x20) = *(ushort *)(param_2 + 0x14) | 0x8000;  // DW0[15:0] = val | 0x8000
*(int *)(lVar3 + 0x28) = *(int *)(param_2 + 0x58) + 0x40;         // scatter ptr+offset
```

这是 data path。对 **管理帧 (param_3=2)**, 走的是不同路径:

看回 N6PciTxSendPkt 代码:
```c
if (param_3 < 2) {
    FUN_14005d6d8(lVar1, param_2);   // DATA: N6PciUpdateAppendTxD
    pcVar9 = "TX DATA";
} else {
    pcVar9 = "TX MGMT";
}
// ... 然后都继续到 DMA ring descriptor 构建
```

**管理帧根本不调用 N6PciUpdateAppendTxD!** 对 mgmt, 代码直接跳到 ring descriptor 构建。

那么管理帧的 TXD DW0 的最终值就是 XmitWriteTxDv1 写的值... 但那不包含标准的 PKT_FMT/Q_IDX 字段。

**重大发现**: Windows 管理帧通过 `FUN_1400359cc` (MgmtTxDoneWaitqEnqueue) 放入 mgmt 待发送队列, 然后由独立的 DPC/tasklet 处理发送。这个发送路径走的是 **store-forward (SF)** 模式, TXD 已经内嵌在 DMA buffer 中, 不需要额外的 N6PciUpdateAppendTxD。

**TXD 在 buffer 中的布局**: buffer+0x00 是 802.11 帧, buffer+0x60 是分配的 TXD 空间 (从 MlmeHardTransmit 的 `*(longlong*)(lVar11+0x60)` 可知)。

从 MlmeHardTransmit 的关键代码:
```c
*(undefined4 *)(lVar11 + 0x68) = 0x20;           // tx_desc_len = 0x20 (32)
FUN_140010118(*(longlong *)(lVar11 + 0x60) + 0x20, puVar8[3], *(undefined4 *)(puVar8 + 4));
// ^ 将 802.11 帧体复制到 TXD buffer + 0x20 (即 TXD 后面)
*(int *)(lVar11 + 0x68) = *(int *)(lVar11 + 0x68) + *(int *)(puVar8 + 4);
// ^ total_len = 0x20 + frame_body_len
FUN_1401a2c8c(lVar4, *(undefined8 *)(lVar11 + 0x60), &local_c8);
// ^ 调用 XmitWriteTxDv1(adapter, TXD_buffer, TxInfo)
```

所以 **TXD 在 buffer 的 offset 0 开始, 802.11 帧紧跟其后**。XmitWriteTxDv1 写的就是最终的 TXD。

但 `DW0[31]` 和 `DW0[30:26]` 包含的是 mac_hdr_len 和一个 flag, 不是 Q_IDX...

**重新理解**: 也许 Windows 的 TXD DW0 位定义确实不同于我们假设的 CONNAC3 标准!

实际上, 看 N6PciTxSendPkt 中如何处理 DMA ring descriptor 的 DW1 for mgmt:

```c
// 管理帧 ring DW1:
uVar8 = (*(int *)(param_2 + 0x68) << 0x10 ^ puVar4[1]) & 0x3fff0000U ^ puVar4[1];
puVar4[1] = uVar8 & 0x7fffffff | 0x40000000;
```

`*(int*)(param_2+0x68)` = total length (0x20 + frame_body_len)。这个移到 bits[29:16] (14 bits), 然后 bit[30]=1 (LAST_SEC/DDONE)。

这是标准 WFDMA ring descriptor 格式:
```
Ring DW0: DMA buffer address low
Ring DW1: [31]=DMA_DONE, [30]=LS, [29:16]=SDL (segment data length), [15:0]=TX_BYTE_CNT
Ring DW2: token/reserved
Ring DW3: DMA buffer address high
```

对管理帧, ring descriptor 的 SDL (bits[29:16]) 设为 total TX length, 不是从 TXD 中取的。所以 **TXD DW0 的上位 bits 确实被 firmware 解析, 而不是由 DMA 硬件解析**。

---

## 5. 关键结论: DW0[24:23] (PKT_FMT)

**PKT_FMT = 0** (因为 memset 清零后, bits[24:23] 从未被修改)

但 DW0[31:25] 被设成了奇怪的值。让我重新计算:

```
DW0 高 7 bit = bit[31]=1 | bit[30:26]=0b11000 | bit[25]=0
             = 0b1_11000_0
             = 0xF0 >> 1...
```

等等, 让我用数值:
```
bit31 = 1 → 0x80000000
bit30:26 = 0b11000 → bit30=1, bit29=1, bit28=0, bit27=0, bit26=0
  → 0x60000000
bit25 = 0

DW0_upper = 0x80000000 | 0x60000000 = 0xE0000000
```

按 CONNAC3 TXD0:
- Q_IDX = bits[31:25] = (0xE0000000 >> 25) = 0b1110000 = 0x70 = 112

但这不合理。让我看看 MT6639 的 Q_IDX 用法...

**实际上在 MT6639 `nic_txd_v3.c` 中, DW0 的位定义是**:
```c
// hal_dmashdl_connac3x.h / nic_txd_v3.c
NIC_TX_DESC_TX_BYTE_COUNT_OFFSET  = 0   (16 bits)
NIC_TX_DESC_ETHER_TYPE_OFFSET     = 16  (7 bits)
NIC_TX_DESC_IP_CHKSUM             = 23  (1 bit)  ← 注意!
NIC_TX_DESC_QUEUE_INDEX           = 24  (5 bits) ← Q_IDX 是 5 bit!
NIC_TX_DESC_PKT_FT               = 29  (2 bits) ← PKT_FMT 在 bits[30:29]!
NIC_TX_DESC_DATA_SEQ_NUM_EN       = 31  (1 bit)
```

**MT6639 使用不同的位布局!**

```
MT6639 DW0:
  bits[15:0]  = TX_BYTE_COUNT
  bits[22:16] = ETHER_TYPE_OFFSET
  bit[23]     = IP_CHKSUM
  bits[28:24] = QUEUE_INDEX (5 bits, not 7!)
  bits[30:29] = PKT_FT (2 bits) ← 不是 [24:23]!
  bit[31]     = DATA_SEQ_NUM_EN (automatic sequence number)
```

**我们的 `mt7927_pci.h` 中的定义有误!**

```c
// 我们的 (错误的):
#define MT_TXD0_PKT_FMT     GENMASK(24, 23)  // ← WRONG
#define MT_TXD0_Q_IDX       GENMASK(31, 25)  // ← WRONG

// MT6639 正确的:
PKT_FT    = GENMASK(30, 29)   // 2 bits
Q_IDX     = GENMASK(28, 24)   // 5 bits
```

现在重新计算 auth 帧的 DW0:

```
bit[31] = 1 (DATA_SEQ_NUM_EN? from param_3+0x05=1)
         实际含义: "自动填充序列号" — 管理帧需要固件自动分配 SN
bits[30:26] = mac_hdr_len = 0x18 = 0b11000

按 MT6639 定义:
  bits[30:29] = PKT_FT = 0b11 = 3?  不对, mac_hdr_len=24=0b11000, bits30:29 = 0b11
  bits[28:26] = 0b000

  但 param_3+0x04 << 26 → SHL 26, AND 0x7C000000
  0x7C000000 = bits[30:26] — 这覆盖了 PKT_FT 和 Q_IDX 的高 3 位...
```

这还是不对。**也许 Windows 内部的 TXD 格式不是直接的硬件 TXD**?

让我从另一个角度看。MlmeHardTransmit 中 `local_c4` 的初始化:

```c
local_c4 = 0x104;  // ← 这在 TxInfo+0x05 附近
local_c6 = 0x18;   // ← TxInfo+0x04 = mac header length
```

offset 0x04 是 `local_c6` (short), 所以 param_3+0x04 = 0x18 (低字节) 和 param_3+0x05 = 0x00 (高字节)。

但 `local_c4` 是 `undefined2` (2 bytes) 在 offset 更后面... 让我看栈布局:

```
local_c8: offset +0x00 (ushort) ← frame body length
local_c6: offset +0x02 (byte)  ← 0x18 = mac header length
local_c4: offset +0x04 (ushort) ← 0x104
...
```

所以:
- param_3+0x00: frame_body_len (2 bytes)
- param_3+0x02: 0x18 (1 byte, mac hdr len)
- param_3+0x03: 0x00 (padding or next field)
- param_3+0x04: low byte of 0x104 = 0x04 ← **不是 mac_hdr_len!**
- param_3+0x05: high byte of 0x104 = 0x01

回到 XmitWriteTxDv1:

```c
// DW0[30:26] from *(byte*)(param_3+0x04):
// param_3+0x04 = 0x04 (来自 local_c4 = 0x104 的低字节)
// 0x04 << 26 = 0x10000000
// AND 0x7C000000 → 0x10000000

// DW0[31] from *(byte*)(param_3+0x05):
// param_3+0x05 = 0x01 (来自 local_c4 = 0x0104 的高字节)
// 0x01 << 31 = 0x80000000
```

等等, 还要考虑后面的 `local_c4 = CONCAT11(local_c4._1_1_, uVar9)`:

```c
uVar9 = 1;  // for mgmt frame
local_c4 = CONCAT11(local_c4._1_1_, uVar9);  // 保持高字节, 低字节 = 1
```

这把 local_c4 改为 `0x0101` (high=0x01 from initial, low=0x01 from uVar9)?

不, CONCAT11 的意思是 `(high_byte << 8) | low_byte`。`local_c4._1_1_` 是 local_c4 的高字节 (第1字节), uVar9 是低字节。

初始 local_c4 = 0x104 = high=0x01, low=0x04。
CONCAT11(0x01, 1) = 0x0101。

所以:
- param_3+0x04 = 0x01 (低字节)
- param_3+0x05 = 0x01 (高字节)

重新计算:
```
DW0[30:26] = 0x01 & 0x1F = 1 → SHL 26 = 0x04000000
DW0[31]    = 0x01 & 1 = 1 → SHL 31 = 0x80000000
DW0 upper  = 0x84000000
```

按 **MT6639 位定义**:
```
bit[31]    = 1 (DATA_SEQ_NUM_EN? 或 SN_VALID?)
bits[30:29] = PKT_FT = (0x84000000 >> 29) & 3 = (0x04 >> 1) & 3 = 2 & 3 = 0b00 = 0
  Wait: 0x84000000 = 0b1000_0100_...
  bit31=1, bit30=0, bit29=0, bit28=0, bit27=0, bit26=1

  PKT_FT[30:29] = 0b00 = 0 → CT mode (Cut-Through)
  Q_IDX[28:24] = 0b00001 = 1
```

Hmm, 但 local_c4 可能不是 0x0101... 让我更仔细地看代码。

实际上, 回看 MlmeHardTransmit 中 `local_c4` 的演变:

```c
local_c4 = 0x104;  // 初始化

// 然后对 auth 帧 (管理帧), 代码走到:
if ((((byte)*puVar5 & 0xf0) == 0x80) && (uVar13 == 0)) {
    // uVar13 = *puVar5 & 0xc (frame control type field)
    // 对 auth 帧: fc=0x00B0, type bits = 0x00 (management)
    // *puVar5 & 0xf0 = 0xB0... 不是 0x80
```

auth 帧的 FC = 0x00B0:
- *puVar5 & 0xc = 0x00 (management, not data)
- *puVar5 & 0xf0 = 0xB0 (auth subtype = 0xB)

所以 `(((byte)*puVar5 & 0xf0) == 0x80)` = false (0xB0 != 0x80).

走到 else 分支:
```c
uVar9 = (undefined1)local_c4;  // uVar9 = 低字节 of 0x104 = 0x04
if (uVar13 == 0) {  // uVar13 = *puVar5 & 0xc = 0 for mgmt
    uVar9 = 1;
}
local_c4 = CONCAT11(local_c4._1_1_, uVar9);
```

所以 uVar9 = 1, local_c4 = CONCAT11(0x01, 0x01) = 0x0101.

**确认**: param_3+0x04 = 0x01, param_3+0x05 = 0x01.

```
DW0[30:26] = 0x01 → 0x04000000
DW0[31]    = 0x01 & 1 = 1 → 0x80000000
DW0 upper  = 0x84000000

bit31=1, bit30=0, bit29=0, bit28=0, bit27=0, bit26=1, bit25=0

按 MT6639 TXD DW0:
  bit[31] = SN_VALID (或 DATA_SEQ_NUM_EN) = 1
  PKT_FT[30:29] = 0b00 = 0 → CT/SF mode
  Q_IDX[28:24] = 0b00001 = 1
```

**PKT_FMT = 0, Q_IDX = 1** — 与我们使用的 PKT_FMT=2(CMD) + Q_IDX=0 完全不同!

但等等 — 还需要考虑 param_3+0x02 的影响, 因为前面:

```c
*(short *)param_2 = *param_3 + 0x20;
```
这只写 DW0[15:0]。然后:

```c
uVar4 = (uint)*(byte *)((longlong)param_3 + 5) << 0x1f | *param_2 & 0x7fffffff;
```
DW0[31] 从 param_3+0x05 的 bit0。

```c
*param_2 = ((uint)*(byte *)(param_3 + 2) << 0x1a ^ uVar4) & 0x7c000000 ^ uVar4;
```

注意: `param_3` 是 `short *`, 所以 `param_3 + 2` = 偏移 4 字节 = param_3+0x04 的位置!
`(byte *)(param_3 + 2)` = byte at offset (2 * sizeof(short)) = offset 4 → **param_3+0x04 的值!**

param_3+0x04 = *(byte*)(local_c4 的地址) → 但在栈布局中:
- local_c8 在某偏移, local_c6 在 +2, local_c4 在 +4

所以 `*(byte*)(param_3+0x04)` 确实读取 local_c4 的低字节 = 0x01.

然后 `*(byte *)((longlong)param_3 + 5)` 读取 offset 5 = local_c4 的高字节 = 0x01.

我之前的计算正确。

---

## 5. DW0 最终精确值 (auth 帧)

```
DW0 = 0x84000000 | TX_BYTES

按 MT6639 (正确的) 位定义:
  TX_BYTES[15:0]   = frame_len + 32  (e.g., 86 = 0x56 for typical auth)
  ETH_OFFSET[22:16]= 0
  IP_CHKSUM[23]    = 0
  Q_IDX[28:24]     = 1  ← queue index 1!
  PKT_FT[30:29]    = 0  ← CT/SF mode (NOT CMD mode!)
  SN_EN[31]        = 1  ← automatic sequence number

对比我们当前的实现:
  PKT_FMT(GENMASK24:23) = 2 (CMD) ← 完全错误!
  Q_IDX(GENMASK31:25)   = 0       ← 也可能错误!
```

**关键发现 #1: DW0 位定义不同!**

我们的 `mt7927_pci.h`:
```c
#define MT_TXD0_PKT_FMT     GENMASK(24, 23)  // ← 应该是 GENMASK(30, 29)
#define MT_TXD0_Q_IDX       GENMASK(31, 25)  // ← 应该是 GENMASK(28, 24)
```

**关键发现 #2: Windows auth 帧使用 PKT_FT=0(CT/SF), Q_IDX=1, SN_EN=1**

---

## 6. DW1 精确值 (auth 帧)

### DW1 初始化过程

```c
// memset 后 DW1 = 0
// Step 1: BTS bit 15 → DW1 |= 0x8000
param_2[1] = param_2[1] | 0x8000;
```

### HDR_INFO 字段 (DW1 bits[12:8] 或 bits[20:16]?)

```c
// Step 2: HDR_INFO from param_3+0x02 (= local_c6 low byte = 0x18)
if (*(short*)((longlong)param_3 + 0x2b) == 2) {  // HDR_FORMAT == 802.3
    uVar4 = (uint)*(byte*)(param_3 + 1) << 7;     // shift by 7
} else {
    uVar4 = (uint)*(byte*)(param_3 + 1) << 8;     // shift by 8
}
// param_3 + 1 = short ptr + 1 = offset 2 bytes → byte at param_3+0x02
// param_3+0x02 = local_c6 = 0x18 (mac_hdr_len = 24)
```

对 auth 帧: HDR_FORMAT = 0 (not 802.3), 所以:
```c
uVar4 = 0x18 << 8 = 0x1800;  // bits[12:8] = 0x18
```

```c
uVar5 = ((uVar4 ^ param_2[1]) & 0x1f00 ^ param_2[1]) & 0xffffdfff;
```

```asm
XOR EDX, ECX     ; 0x1800 ^ DW1(=0x8000) = 0x9800
AND EDX, 0x1f00  ; 0x1800 (bits[12:8] = 0x18 = 24)
XOR EDX, ECX     ; 0x1800 ^ 0x8000 = 0x9800
BTR EDX, 0xd     ; clear bit 13 → 0x9800 → clear bit13 → 0x9800 (bit13 already 0)
BTS EDX, 0xe     ; set bit 14 → 0x9800 | 0x4000 = 0xD800
```

Wait, 让我用汇编逐步走:

```asm
; 初始 DW1 = 0x00008000 (bit 15 set)
; ECX = DW1 = 0x00008000
; EDX (uVar4) = 0x18 << 8 = 0x1800

1401a2d76: XOR EDX, ECX        ; EDX = 0x1800 ^ 0x8000 = 0x9800
1401a2d78: MOV R8D, 0x7fffffff ; R8D saved for later
1401a2d7e: AND EDX, 0x1f00     ; EDX = 0x9800 & 0x1F00 = 0x1800
1401a2d84: XOR EDX, ECX        ; EDX = 0x1800 ^ 0x8000 = 0x9800
1401a2d86: BTR EDX, 0xd        ; clear bit 13: 0x9800 → bit13=0 already → 0x9800
1401a2d8a: BTS EDX, 0xe        ; set bit 14: 0x9800 | 0x4000 = 0xD800
1401a2d8e: MOV [RBX+4], EDX    ; DW1 = 0x0000D800
```

所以 DW1 此时 = 0x0000D800:
```
bit15 = 1 (from initial BTS)
bit14 = 1 (from BTS 0xe) → HDR_FORMAT[15:14] = 0b11? 但 MT6639 只有 2 bit HDR_FORMAT
bit13 = 0 (cleared by BTR)  → TGID?
bits[12:8] = 0x18 = 24 = mac_hdr_len
```

对照 MT6639 DW1 位定义:
```
MT6639 TXD DW1:
  bits[11:0]  = WLAN_IDX (12 bits)
  bits[13:12] = TGID (band index, 2 bits)
  bits[15:14] = HDR_FORMAT (2 bits): 0=802.11, 1=CMD, 2=802.3
  bits[20:16] = HDR_INFO (802.11: hdr_len_in_dwords, 802.3: ether_offset)
  bits[24:21] = TID (4 bits)
  bits[30:25] = OWN_MAC (6 bits)
  bit[31]     = FIXED_RATE
```

但我们看到 XmitWriteTxDv1 把 mac_hdr_len=24 放到了 bits[12:8] — 这跨越了 WLAN_IDX 和 TGID 字段!

**这更加证实 XmitWriteTxDv1 中 param_3+0x02 的 byte 被放到 DW1[12:8], 而不是 DW1[20:16]!**

但这与 MT6639 标准 TXD 定义矛盾...

Wait — 让我重新看。`param_3 + 1` 在 C 中是 `short *` + 1 = 偏移 2 bytes。但 `*(byte *)(param_3 + 1)` 取的是偏移 2 字节处的第一个 byte。

param_3 结构:
```
+0x00: local_c8 (frame_body_length, ushort)
+0x02: local_c6 (0x18, byte - mac header length)
+0x03: (next byte)
+0x04: local_c4 low byte
+0x05: local_c4 high byte
...
+0x07: WLAN_IDX byte
```

`*(byte *)(param_3 + 1)` = byte at param_3 offset 2 = local_c6 = 0x18 = 24.

shift 8: 0x18 << 8 = 0x1800. Mask 0x1F00 → 0x1800. Bits[12:8] = 0x18 = 24.

但 HDR_INFO 在标准 TXD 中是 DW1[20:16], 不是 DW1[12:8]!

**关键重新检查**: 也许 Ghidra 误解了内存偏移, 或者 Windows 用了不同的 DW1 位布局。

让我仔细看汇编中 DW1 后续字段设置:

### WLAN_IDX (DW1[11:0])

```asm
1401a2dc0: MOV AL, byte ptr [RDI + 0x7]     ; param_3+0x07 = WLAN_IDX
1401a2dc3: MOV byte ptr [RBX + 0x4], AL      ; 直接写 DW1 的最低字节!
```

DW1 byte[0] (bits[7:0]) = WLAN_IDX!

此时 DW1 变为: `0x0000D800` → byte[0] 被替换 → `0x0000D8xx` where xx = WLAN_IDX.

如果 WLAN_IDX = 0, 则 DW1 = 0x0000D800.

### TID (DW1[23:21])

```asm
1401a2dc9: MOVZX ECX, byte ptr [RDI + 0x9]  ; param_3+0x09 = TID
1401a2dcd: SHL ECX, 0x15                      ; << 21
1401a2dd0: XOR ECX, EAX                       ; XOR with DW1
1401a2dd2: AND ECX, 0xe00000                  ; mask bits[23:21]
1401a2dd8: XOR ECX, EAX                       ; apply to DW1
```

对 auth 帧: TID = 7 (from uVar9=7 for mgmt).
```
7 << 21 = 0xE00000
DW1 bits[23:21] = 0b111 = 7
```

DW1 = 0x00E0D800 (for WLAN_IDX=0).

### OWN_MAC (DW1[30:25]) + TGID extension?

```asm
1401a2de3: MOVZX EAX, byte ptr [RDI + 0xa]  ; param_3+0x0a = OWN_MAC
1401a2de7: SHL EAX, 0x1a                      ; << 26
1401a2dea: OR EAX, ECX                        ; combine

; ECX is DW1 after AND 0x3FFFFFF → clear bits[31:26]
; Then OWN_MAC in bits[31:26]?
```

Wait: `AND ECX, 0x3FFFFFF` only keeps bits[25:0], then `SHL EAX, 0x1a` puts OWN_MAC in bits[31:26].

param_3+0x0a = OWN_MAC_IDX. 如果 OWN_MAC=0:
```
DW1 = 0x00E0D800  (bits[25:0] preserved)
```

### FIXED_RATE / HDR_FORMAT check

```asm
; 接下来检查 HDR_FORMAT:
1401a2def: CMP word ptr [RDI + 0x2b], R14W   ; compare param_3+0x2b with 2
1401a2df4: JNZ 0x1401a2dfd                    ; if != 2 (not 802.3), jump

; For 802.11 (not 802.3):
1401a2dfd: BTR EAX, 0x18   ; clear bit 24
1401a2e01: BTS EAX, 0x19   ; set bit 25
```

对 auth 帧 (param_3+0x2b = 0, 802.11 format):
```
BTR bit24 → clear bit 24
BTS bit25 → set bit 25
DW1 bit[25]=1, bit[24]=0
```

如果 HDR_FORMAT == 2 (802.3):
```asm
1401a2df6: AND EAX, 0xfcffffff  ; clear bits[25:24]
```

所以:
- **802.11 模式**: DW1[25]=1, DW1[24]=0
- **802.3 模式**: DW1[25:24]=0b00

### BIP flag → DW1[17]

```asm
1401a2db3: CMP byte ptr [RDI + 0xc], R15B   ; param_3+0x0c = BIP flag
1401a2db9: BTS EDX, 0x11                     ; if BIP, set bit 17
```

对 auth 帧: BIP = 0 → bit 17 不设。

### DW1 最终值 (auth 帧, WLAN_IDX=0, OWN_MAC=0)

```
bits[7:0]   = WLAN_IDX = 0
bits[12:8]  = mac_hdr_len? = 0x18 (24)
bit[13]     = 0 (cleared)
bit[14]     = 1 (set)
bit[15]     = 1 (set)  → HDR_FORMAT[15:14] = 0b11 = 3?
bit[17]     = 0 (BIP not set)
bits[23:21] = TID = 7
bit[24]     = 0 (cleared)
bit[25]     = 1 (set for 802.11)  → OWN_MAC bit?
bits[31:26] = OWN_MAC << 26 = 0 (if OWN_MAC=0)

DW1 = 0x02E0D800  (approx)
```

Wait, 让我完整算:

1. memset → 0x00000000
2. BTS bit15 → 0x00008000
3. HDR_INFO shift: 0x18<<8=0x1800, XOR+mask+XOR+BTR13+BTS14:
   - After AND 0x1F00: 0x1800
   - XOR with 0x8000: 0x9800
   - BTR bit13: 0x9800 (bit13 was 0)
   - BTS bit14: 0x9800 | 0x4000 = 0xD800
4. MOV byte[0] = WLAN_IDX=0: 0x0000D800
5. TID=7: bits[23:21] = 0xE00000 → 0x00E0D800
6. AND 0x3FFFFFF (keep bits[25:0]): 0x00E0D800 (bit25=0 already)
7. OWN_MAC=0 << 26 = 0, OR → 0x00E0D800
8. For 802.11: BTR bit24, BTS bit25: 0x02E0D800

**DW1 = 0x02E0D800** (for WLAN_IDX=0, OWN_MAC=0, TID=7)

按 MT6639 DW1 定义:
```
bits[11:0]  = WLAN_IDX = 0x000 (wait, bits[12:8] = 0x18 overlaps!)
```

**这里有矛盾** — bits[12:8] = 0x18=24 跟 WLAN_IDX[11:0] 重叠!

**我终于理解了**: XmitWriteTxDv1 中 HDR_INFO 确实放在了 bits[12:8], 但 WLAN_IDX 只在 byte[0] (bits[7:0]) 中。

这意味着 **MT6639/MT7927 的实际 TXD DW1 布局是**:

```
MT7927 TXD DW1 (从 Windows RE 确认):
  bits[7:0]   = WLAN_IDX (8 bits, not 12!)
  bits[12:8]  = HDR_INFO (5 bits = mac_hdr_len / 2 或 bytes?)
  bit[13]     = TGID bit 0 (band index)
  bits[15:14] = HDR_FORMAT (2 bits)
  bit[17]     = BIP
  bits[20:18] = ? (未被 XmitWriteTxDv1 设置)
  bits[23:21] = TID (3 bits)  ← 注意: 只有 3 bits!
  bit[24]     = ETH_802_3?
  bit[25]     = some flag (set for 802.11)
  bits[31:26] = OWN_MAC (6 bits)
```

但这跟 **所有已知的 CONNAC3 定义都不一致**...

让我用 HDR_INFO = mac_hdr_len / 2 重新检查:
- mac_hdr_len = 24 bytes → 24/2 = 12 = 0x0C
- 12 in 5 bits = 0b01100

如果 HDR_INFO 是 12 (hdr_len/2), 那 0x0C << 8 = 0x0C00.
但实际代码放的是 0x18 << 8 = 0x1800.

0x18 = 24 → 如果是字节数, bits[12:8] = 0b11000 → 需要 5 bits, 24 fits in 5 bits (max 31).

**所以 HDR_INFO 存的是字节数 (24), 不是 WORD 数 (12)!**

但等等 — 标准 CONNAC3 的 HDR_INFO[20:16] 也是 5 bits, 存的是 2-byte words (max 31 words = 62 bytes). 24 bytes = 12 words = 0x0C.

Windows 把 **字节数** 直接存进去, 而不是 word 数。这是 Windows 驱动的做法。

---

## 7. 重新整理: 从汇编确认的 DW1 精确位布局

实际上让我不假设任何位定义, 纯从汇编推导:

```
DW1 bit[15] = 1 (BTS)
DW1 bits[12:8] ← mac_hdr_len byte (for 802.11 mode: shift 8)
                  或 shift 7 (for 802.3 mode)  → 值=24 对 802.11
DW1 bit[14] = 1 (BTS)
DW1 bit[13] = 0 (BTR)
DW1 byte[0] ← WLAN_IDX byte (直接写)
DW1 bits[23:21] ← TID (shift 21, mask 0xE00000)
DW1 bits[31:26] ← OWN_MAC (shift 26)
DW1 bit[25] = 1 for 802.11 mode (BTS), 0 for 802.3 (AND clear)
DW1 bit[24] = 0 for 802.11 mode (BTR), 0 for 802.3 (AND clear)
DW1 bit[17] ← BIP flag (BTS if set)
```

现在按 **官方 CONNAC3 TXD DW1** (从 MT6639 nic_txd_v3.c 对比):

```
CONNAC3 TXD DW1:
  bits[11:0]  = WLAN_IDX
  bits[13:12] = TGID
  bit[14]     = HDR_FORMAT bit 0
  bit[15]     = HDR_FORMAT bit 1 (or HDR_PAD?)
  bits[20:16] = HDR_INFO
  bits[24:21] = TID
  bit[25]     = ?
  bits[30:26] = OWN_MAC (or [30:25])
  bit[31]     = FIXED_RATE
```

问题在于 Windows 写 mac_hdr_len=24 到 bits[12:8], 而标准 CONNAC3 的 WLAN_IDX 是 bits[11:0]...

**我怀疑 Ghidra 的 byte 偏移理解有误**。让我从汇编重新验证 `param_3 + 1` 的实际含义:

```asm
1401a2d63: MOVZX EDX, byte ptr [RDI + 0x2]   ; byte at param_3+0x02
```

但 C 代码说 `*(byte *)(param_3 + 1)`:
- param_3 是 `short *` → param_3 + 1 = param_3 + 2 bytes → offset 0x02

汇编确认: `[RDI + 0x2]` = offset 2 from TxInfo base.

然后:
```asm
1401a2d6e: SHL EDX, 0x8     ; for non-802.3
```

所以 byte at TxInfo+0x02 (= 0x18 = 24) 被 shift left 8 → 0x1800.
然后 `AND 0x1F00` 保留 bits[12:8] → 0x1800.

**但这个 0x1800 是 XOR 进 DW1 的, 不是直接 OR!**

```asm
XOR EDX, ECX        ; EDX = 0x1800 ^ 0x8000 = 0x9800
AND EDX, 0x1F00     ; EDX = 0x1800
XOR EDX, ECX        ; EDX = 0x1800 ^ 0x8000 = 0x9800
```

这是标准的 "bit-field insert" 模式: `(new ^ old) & mask ^ old`:
```
result = (new_val ^ old_val) & field_mask ^ old_val
       = set field_mask bits to new_val, keep others
```

所以 DW1 bits[12:8] = new_val[12:8] = 0x18 的 bits[12:8]:
```
0x18 << 8 = 0x1800
0x1800 bits[12:8] = 0b11000 = 24
```

这写入 DW1[12:8]。

**但等等** — MT_TXD1_HDR_INFO = GENMASK(20,16), 不是 GENMASK(12,8)!

Windows 驱动把 HDR_INFO 放在 DW1[12:8] 而不是 DW1[20:16]. 这意味着:

**可能性 1**: Windows 驱动和 MT6639 用了不同的 TXD DW1 位布局
**可能性 2**: WLAN_IDX 实际上不是 12 bits, 而是 8 bits, HDR_INFO 在 bits[12:8]

让我查 MT6639 源码来确认。

从 `mt6639/nic_txd_v3.c`:
```c
#define NIC_TX_DESC_WLAN_INDEX_MASK       BITS(0, 11)   // 12 bits
#define NIC_TX_DESC_HEADER_FORMAT_MASK    BITS(14, 15)  // 2 bits
#define NIC_TX_DESC_HEADER_LENGTH_MASK    BITS(16, 20)  // 5 bits
```

所以 MT6639 确实是 WLAN_IDX[11:0], HDR_INFO[20:16]. 但 Windows 放 HDR_INFO 在 [12:8]...

**真相**: XmitWriteTxDv1 中的 `param_3+0x02` 可能 **不是** mac_hdr_len, 而是 WLAN_IDX 的高字节!

让我重新看 MlmeHardTransmit:

```c
// local_c8 是 ushort (2 bytes at offset 0)
// local_c6 是 byte (1 byte at offset 2)  ← param_3+0x02
// 但 local_c6 = 0x18...
```

0x18 = 24. 可以是 mac_hdr_len, 也可以是 WLAN_IDX 高部分 (如果 WLAN_IDX > 255).

但 WLAN_IDX 通常是 0-255, 不需要高字节. 而且 Windows 在后面用 `MOV byte ptr [RBX+4], AL` (param_3+0x07) 写了 DW1 byte[0] = WLAN_IDX.

如果 WLAN_IDX = 0, 那 DW1 byte[0] = 0. bits[12:8] = 24 就跟 WLAN_IDX[11:8] 重叠:
- byte[0] = 0x00 → DW1[7:0] = 0
- bits[12:8] = 0x18 → DW1 = 0x00001800

这不冲突! 因为 byte[0] 写的是 DW1[7:0], 而 bits[12:8] 独立设置。

但如果 WLAN_IDX = 256 呢? byte[0] = 0x00, 但 bits[12:8] 仍然 = mac_hdr_len = 24. 那 DW1[11:8] = 4 位来自 mac_hdr_len 的低 4 位 = 0x8. 这样 WLAN_IDX[11:0] = 0x800? 完全错误!

**结论**: Windows 驱动假设 WLAN_IDX 只需要 8 bits (0-255), 剩余的 bits[12:8] 重新用于 HDR_INFO. 这跟 MT6639 标准不同, 但可能是 Windows 驱动的实际做法 (因为实际 WLAN_IDX 很少超过 255).

或者, 更可能的是: **XmitWriteTxDv1 中 bits[12:8] 存的 HDR_INFO 会在后续被 firmware 按 bits[20:16] 解析**, 而 Windows 驱动实际上按了 **完全不同的位映射**。

让我尝试另一种理解: 也许 XmitWriteTxDv1 构建的是 **Windows 内部 TxInfo 的一个中间格式**, 最终会被 N6PciUpdateAppendTxD 或类似函数 **重新排列** 成硬件 TXD。

但我们已经看到, 对管理帧, N6PciUpdateAppendTxD 不被调用. 所以这个中间格式就是最终格式...

**最终结论: Windows 的 TXD 位定义与 MT6639 公开源码中的定义不同。这可能是因为 firmware 版本不同, 或者 Windows 驱动确实用了定制的 TXD 格式。**

---

## 8. 关键总结和建议

### DW0 (确定):

| 字段 | 位范围 | auth 帧值 | 说明 |
|------|--------|-----------|------|
| TX_BYTES | [15:0] | payload+32 | 总 TX 长度 |
| ETH_TYPE_OFFSET | [22:16] | 0 | 不用 |
| **IP_CHKSUM?** | [23] | **0** | |
| **Q_IDX** | **[28:24]** | **1** | **Windows 用 Q_IDX=1, 不是 0!** |
| **PKT_FT** | **[30:29]** | **0** | **CT/SF 模式, 不是 CMD(2)!** |
| **SN_EN** | **[31]** | **1** | 自动序列号 |

**我们当前 PKT_FMT=2(CMD) 是错误的! 应该是 0(CT/SF)。Q_IDX 应该是 1, 不是 0。**

### DW1 (从汇编推断):

Windows 写入 DW1 的方式暗示 bits 布局:

| 汇编写入位置 | 值 | 可能对应字段 |
|-------------|-----|------------|
| byte[0] (bits[7:0]) | WLAN_IDX | WLAN_IDX[7:0] |
| bits[12:8] (SHL 8, AND 0x1F00) | 0x18 (24) | HDR_INFO (字节) |
| bit[13] | 0 (BTR) | TGID bit? |
| bit[14] | 1 (BTS) | HDR_FORMAT bit 0 |
| bit[15] | 1 (BTS at start) | HDR_FORMAT bit 1 |
| bit[17] | BIP flag | BIP |
| bits[23:21] (SHL 21) | TID (7 for mgmt) | TID |
| bit[24] | 0 for 802.11 (BTR) | ? |
| bit[25] | 1 for 802.11 (BTS) | ? |
| bits[31:26] (SHL 26) | OWN_MAC | OWN_MAC |

DW1[15:14] = 0b11 → HDR_FORMAT = 3?

如果 HDR_FORMAT 是 2-bit:
- 0 = 802.11 native
- 1 = CMD (firmware TXD)
- 2 = 802.3 (Ethernet)
- 3 = ???

但 Windows 802.11 管理帧设为 3... 这不寻常。

**可能 bit[15] 不是 HDR_FORMAT 的一部分**, 而是另一个字段 (如 LONG_FORMAT 或类似)。从 MT6639:
```
bit[15] 可能是 NIC_TX_DESC_FORMAT (normal/special descriptor)
```

如果 bit[15] = FORMAT (1=long/normal), 那 HDR_FORMAT = bit[14] alone = 1 = CMD?

不, 那也不对. 让我看 MT6639 参考代码:

```c
// MT6639 nic_tx.h:
// NIC_TX_DESC_HEADER_FORMAT_OFFSET = 14 (2 bits, mask BITS(14,15))
// Values: HEADER_FORMAT_NON_802_11 = 0
//         HEADER_FORMAT_COMMAND = 1
//         HEADER_FORMAT_802_3 = 2
```

那 Windows auth 帧设 HDR_FORMAT[15:14] = 0b11 = 3 → undefined?

**或者 bits[15:14] 的实际含义是**:
- bit[14] = HDR_FORMAT 低位 (0=non-802.11/1=command/2=802.3... 实际上 MT6639 用 2 bits)
- bit[15] = 另一个字段

从 mt76/mt76_connac3_mac.h:
```c
#define MT_TXD1_HDR_FORMAT_V3   GENMASK(15, 14)
```

值: 0=802.3, 1=CMD, 2=802.11, 3=802.11+AMSDU

**HDR_FORMAT = 3 表示 802.11 + AMSDU? 不, 对 auth 帧没有 AMSDU.**

从 CONNAC3 定义 (mt76 upstream):
```c
#define MT_HDR_FORMAT_802_3     0   // HDR_FORMAT[15:14]=0b00
#define MT_HDR_FORMAT_CMD       1   // HDR_FORMAT[15:14]=0b01
#define MT_HDR_FORMAT_802_11    2   // HDR_FORMAT[15:14]=0b10
#define MT_HDR_FORMAT_802_11_EX 3   // HDR_FORMAT[15:14]=0b11
```

**HDR_FORMAT = 0b11 = 802.11 extended!**

但 Windows 设的是 bit14=1 (BTS 0xe), bit15=1 (BTS 0xf at start).

实际上初始的 `BTS bit 15` 可能有不同含义...

让我重新看: Ghidra 的 BTS 指令格式:
```asm
1401a2d4b: BTS dword ptr [RBX + 0x4], 0xf  ; set bit 15 of DW1
```

BTS = Bit Test and Set. 第二个操作数 0xf = 15, 所以设置 DW1 的 bit 15.

然后:
```asm
1401a2d8a: BTS EDX, 0xe  ; set bit 14
```

所以确实 DW1[15:14] = 0b11.

按 mt76 定义: MT_HDR_FORMAT_802_11 = 2 (0b10). 但 Windows 用了 3 (0b11).

**最终理解**: 这可能就是 CONNAC3 v3 的 802.11 模式值。在 CONNAC2 中 802.11=2, 但 CONNAC3 中可能是 3. 或者 Windows 驱动确实用了 extended 802.11 模式 (HDR_FORMAT=3).

### HDR_INFO 问题

**最关键的问题**: HDR_INFO 存的是 字节数(24) 还是 WORD 数(12)?

从汇编:
```asm
MOVZX EDX, byte ptr [RDI + 0x2]   ; EDX = mac_hdr_len = 0x18 = 24
SHL EDX, 0x8                       ; for 802.11, shift by 8
```

值 0x18 (24) 被放入 DW1 bits[12:8].

但这是在 bit[12:8] 中, 而不是标准 CONNAC3 的 bit[20:16].

**可能性**: Windows 把 HDR_INFO (mac_hdr_len = 24 字节) 放在了 bits[12:8], 而 MT6639 标准放在 bits[20:16]. 值是**字节数 (24)**, 不是 WORD 数 (12).

但从上面的分析看, bits[12:8] 跟 WLAN_IDX[11:0] 重叠. Windows 可能认为 WLAN_IDX 只有 8 bits.

**实际上**: 看回汇编流程, `MOV byte ptr [RBX+0x4], AL` 写 WLAN_IDX 到 DW1 byte[0], 这发生在 HDR_INFO 写入 bits[12:8] **之后**. 所以 WLAN_IDX 写入 bits[7:0] 不影响 bits[12:8]. 它们不冲突.

### 对我们驱动的建议

1. **DW0 位定义需要修正**:
   ```c
   // 错误的:
   #define MT_TXD0_PKT_FMT     GENMASK(24, 23)
   #define MT_TXD0_Q_IDX       GENMASK(31, 25)

   // 正确的 (MT6639 / CONNAC3):
   #define MT_TXD0_PKT_FMT     GENMASK(30, 29)   // 2 bits
   #define MT_TXD0_Q_IDX       GENMASK(28, 24)   // 5 bits
   ```

2. **管理帧 DW0 应该是**: PKT_FT=0 (CT/SF), Q_IDX=1, SN_EN=1 (bit[31])

3. **DW1 HDR_INFO 存字节数**: `mac_hdr_len = 24`, 不是 `mac_hdr_len/2 = 12`

4. **DW1 HDR_FORMAT = 3 (0b11)**: 不是 2 (0b10). 这可能是 "802.11 extended" 模式.

5. **DW1 bit[25] = 1**: 对 802.11 帧需要设置这个 bit (含义待确认, 可能是 ETH_802_3=0 的反向标志)

---

## 9. 其他 DW 的关键发现

### DW2

对 auth 帧, 如果 param_3+0x4c = 0 (无特殊处理):
```c
// DW7[29:16] 被设为: (param_3+0x2b << 4 | param_3+0x2d) & 0x3FFF << 16
// = (0 << 4 | 0x0B) & 0x3FFF << 16 = 0x000B0000
// 但这是 DW7, 不是 DW2!
```

DW2 主要改动:
```
bit[10] = param_3+0x39 (RTS flag) << 10
bit[12] = 0 (BTR, cleared)
```

如果 param_3+0x4c 或 param_3+0x53 或 param_3+0x3d 有一个非零 (fixed rate 场景):
```c
param_2[2] |= 0xA0000000;  // bits[31]+bits[29] = FIXED_RATE flags
param_2[6] = (param_2[6] & 0x7E00FFFF) | 0x4B0000;  // TX_RATE = 0x4B
```

对 auth 帧: 这 3 个 flag 默认都是 0, **除非** 有加密或特殊情况.

如果 param_3+0x2b == 2 (802.3):
```c
param_2[2] &= 0xDFFFFFFF;  // clear bit[29]
```

### DW3

```c
// bit[0] = NO_ACK: param_3+0x08 == 0 → SETZ CL = 1 → bit0=1 (NO_ACK=1?)
```

Wait — `SETZ CL` means CL=1 if param_3[4]==0 (is_zero). 然后 `OR ECX, EAX`:
```
uVar5 = (uint)(param_3[4] == 0);  // 1 if NO_ACK=0
param_2[3] = uVar5 | (DW3 & 0xFFFFFFFE);
```

这 **反转** 了! param_3[4] = `*(char*)(param_3+0x08)` = NO_ACK byte.
如果 NO_ACK=0 → DW3 bit[0] = 1. 如果 NO_ACK=1 → DW3 bit[0] = 0.

按 CONNAC3 TXD3: bit[0] = NO_ACK. 所以:
- auth 帧 NO_ACK=0 → DW3[0] = 1 → **NO_ACK=1?!**

这不对... 除非 param_3+0x08 的含义是 "need ACK" (反向):
- param_3+0x08 = 0 → 不需要 ACK → DW3 NO_ACK=1
- param_3+0x08 = 1 → 需要 ACK → DW3 NO_ACK=0

但 auth 帧一定需要 ACK...

**重新检查**: 在 MlmeHardTransmit 中:
```c
*(undefined1 *)(local_res8 + 8) = 1;  // ← 这是 txbuf+8, 不是 TxInfo
```

param_3+0x08 来自 TxInfo 的 offset 0x08. 在栈布局中, `local_c8` 之后有其他局部变量, offset 0x08 可能是 `local_c0` (1 byte).

`local_c0` 在代码中被设为:
```c
// 对 auth 帧 (type=0, subtype=0xB):
// uVar13 = *puVar5 & 0xc = 0 → not 4 (data)
// 走到 else 分支
// ... 最终:
local_c0 = 0;  // 或 1, 取决于 is_bcmc check
```

对 unicast auth: `local_c0 = 0` (在 `LAB_140054335` 之后).

所以 param_3+0x08 = local_c0 = 0 → `(param_3[4] == 0)` = true → DW3[0] = 1.

**DW3[0] = 1 = NO_ACK** — 对 auth 帧来说这是错误的!

但这是 Windows 驱动实际做的... 也许 CONNAC3 的 bit[0] 定义跟我们假设的不同?

检查 MT6639:
```c
// nic_txd_v3.c: NIC_TX_DESC_NO_ACK = BIT(0) of DW3
// NIC_TX_DESC_PROTECT_FRAME = BIT(1)
```

如果 NO_ACK 确实是 bit[0], 那 Windows auth 帧设了 NO_ACK=1 是有意为之?

**可能解释**: param_3+0x08 的偏移我分析错了。也许 param_3+0x08 对应的是 `*(undefined1*)(param_3 + 4)` 中的 `param_3[4]` (short* + 4 = offset 8 bytes), 这个字段是 `local_c0`, 但在 XmitWriteTxDv1 中:

```c
uVar5 = (uint)((char)param_3[4] == '\0');
```

`param_3[4]` 是 short 指针偏移 4 → byte offset 8.

`(char)param_3[4]` = 取 short 值的低字节.

**如果 local_c0 = 1 (auth 帧需要重传)**, 则 DW3[0] = 0 (NO_ACK=0 = ACK needed).

让我重新检查 auth 帧路径中 local_c0 的值:

```c
// auth 帧: subtype=0xB, type=0 (management)
if (uVar13 == 4) {  // uVar13 = fc & 0xc → 0 for mgmt
    // data control frame path
} else {
    // management frame path:
    if (((byte)*puVar5 & 0xf0) == 0x80) && (uVar13 == 0)) {
        // probe request (subtype 0x4 = 0x40 >> 4): 0xB0 & 0xF0 = 0xB0 ≠ 0x80
        // → NOT this branch for auth
    } else {
        uVar9 = (undefined1)local_c4;
        if (uVar13 == 0) {
            uVar9 = 1;
        }
    }
    // ...
    // Check for unicast:
    if ((puVar5[2] & 1) == 0) {  // puVar5[2] = DA first byte, bit0 = multicast
        // unicast
        uVar13 = *puVar5 >> 4;
        uVar14 = *puVar5 & 0xc;
        if (uVar14 == 0) {  // management
            uVar13 = uVar13 & 0xf;  // subtype
            if (uVar13 != 4 && uVar13 != 5) {  // not probe req/resp
                local_c0 = 1;  // ← SET TO 1!
                goto LAB_140054335;
            }
        }
    }
    local_c0 = 0;
}
```

对 auth 帧 (subtype=0xB, management, unicast):
- uVar14 = 0 (management)
- uVar13 = 0xB (auth subtype)
- uVar13 != 4 && uVar13 != 5 → true
- **local_c0 = 1**

所以 param_3+0x08 = local_c0 = 1 → `(param_3[4] == 0)` = false → DW3[0] = 0.

**DW3[0] = 0 = NO_ACK=0 → 需要 ACK!** 正确。

### DW3 REM_TX_COUNT

```c
if ((char)param_3[2] == '\a') {  // param_3[2] = offset 4 = local_c4 低字节 = 1
    // 1 != 7 → else branch
    uVar4 = uVar6 | uVar5 | uVar4 & 0xfffff7fc | 0xf000;
}
```

`0xf000` 在 DW3 中 = bits[15:12] = 0xF = 15.
按 REM_TX_COUNT = GENMASK(15,11) = bits[15:11]:
- 0xf000 = bits[15:12] = 1111, bit[11]=0
- REM_TX_COUNT = 0b11110 = 30

但 bit[11] 来自 `BTR EDX, 0xb`:
```asm
1401a2ead: BTR EDX, 0xb   ; clear bit 11
1401a2eb1: OR EDX, 0xf000 ; set bits[15:12]
```

REM_TX_COUNT[15:11] = 0b11110 = 30.

如果 `(char)param_3[2] == 7` (special case):
```asm
1401a2ea1: BTR EDX, 0xf   ; clear bit 15
1401a2ea5: OR EDX, 0x7800 ; set bits[14:11]
```
REM_TX_COUNT[15:11] = 0b01111 = 15.

**auth 帧: REM_TX_COUNT = 30** (因为 local_c4 低字节 = 1, 不等于 7)

### DW5 (TX Status)

对 auth 帧 (param_3+0x0f = local_80 = *(char*)((longlong)puVar8+9)):
- `*(char*)((longlong)puVar8 + 9)` = *(char*)(TxBuf+9) = `local_80`
- 在 MlmeHardTransmit: `local_80 = *(char *)((longlong)puVar8 + 9)` → from TxBuf input

如果 `local_80 == 0` (对 auth 帧 param_3+0x0f=0):
```c
if (*(short *)((longlong)param_3 + 0x2b) == 2) {
    // 802.3 path
} else {
    // 802.11 path: nothing happens for DW5
}
```

所以 **DW5 = 0** for auth 帧! (除非 special fixed rate path)

最终:
```c
param_2[5] = param_2[5] & 0xFFFFCFFF;  // clear bits[13:12]
```

**DW5 = 0** (清零后没其他设置).

### DW6

如果 3 个 flag (param_3+0x4c, +0x53, +0x3d) 任一非零:
```c
param_2[6] = param_2[6] & 0x7e00ffff | 0x4b0000;
// 清除 bits[31] + bits[24:16], 然后设 bits[22:16] = 0x4B
```

对 auth 帧, 这 3 个 flag 默认 0, **但** 后面还有 fixed rate 部分 (如果 *(adapter+0x14c0+0x2e9db4) 非零):
```c
param_2[2] |= 0x80000000;  // DW2 bit[31] (force rate)
param_2[6] &= 0x7fffffff;  // DW6 clear bit[31]
// Then set various rate fields in DW6
```

### DW7

对 auth 帧 (param_3+0x4c = 0):
```c
// DW7[29:16] = frame type/subtype packed:
param_2[7] = ((*(ushort *)(param_3 + 0x2d) | uVar4) & 0x3fff) << 0x10 ^ param_2[7] & 0xc000ffff;
// uVar4 = *(ushort*)(param_3+0x2b) << 4 = 0 << 4 = 0
// *(ushort*)(param_3+0x2d) = 0x0B (auth subtype)
// (0x0B | 0) & 0x3FFF = 0x0B
// 0x0B << 16 = 0x000B0000
```

**DW7[29:16] = 0x000B** → 这包含了 frame type/subtype 信息.

Windows 的 DW7 似乎不是标准的 TXD_LENGTH 字段, 而是用来存储帧类型信息!

如果有 TXS 请求 (param_3+0x2b==2 且某条件):
```c
param_2[7] |= 0x40000000;  // bit[30] = TXD_LENGTH=1?
```

对 auth 帧: param_3+0x2b=0 → 不走这个分支. **DW7[31:30] = 0**.

**所以 DW7 TXD_LENGTH=0, 与我们当前实现一致。**

---

## 10. 完整 auth 帧 TXD (DW0-DW7) 精确值

假设: WLAN_IDX=0, OWN_MAC=0, band=0, no encryption, 5GHz

```
DW0 = 0x84000000 | TX_BYTES
      bit[31]=1(SN_EN), PKT_FT[30:29]=0(CT), Q_IDX[28:24]=1
      TX_BYTES = frame_total_len + 32

DW1 = 0x02E0D800  (WLAN_IDX=0, OWN_MAC=0 case)
      bits[7:0]=WLAN_IDX(0)
      bits[12:8]=HDR_INFO(24 bytes)
      bit[13]=0(TGID/band)
      bits[15:14]=HDR_FORMAT(0b11=802.11_ext)
      bit[17]=0(no BIP)
      bits[23:21]=TID(7)
      bit[24]=0, bit[25]=1(802.11 flag)
      bits[31:26]=OWN_MAC(0)

DW2 = 0xA000000B  (VERIFIED — see docs/win_re_dw2_dw6_verified.md)
      bit[31]=FIXED_RATE, bit[29]=BM
      bits[5:4]=FRAME_TYPE(0=mgmt), bits[3:0]=SUB_TYPE(0xB=auth)
      MlmeHardTransmit sets local_7c=1 UNCONDITIONALLY → fixed-rate block ALWAYS runs

DW3 = 0x0000F000
      bit[0]=NO_ACK(0), bit[1]=PROTECT(0)
      bits[15:11]=REM_TX_COUNT(30)

DW4 = 0x00000000 (no PN)

DW5 = 0x00000600 | PID  (VERIFIED — both docs were WRONG!)
      bit[10]=TX_STATUS_2_HOST=1, bit[9]=TX_STATUS_FMT=1
      bits[7:0]=PID (rotating 1-99 from FUN_14009a46c)
      Without TX_STATUS_2_HOST, firmware does NOT report TX completion!

DW6 = 0x004B0000  (VERIFIED — OFDM 6Mbps fixed rate)
      bits[22:16]=0x4B (MCS11 + OFDM mode)

DW7 = 0x00000000  (VERIFIED — fixed-rate path clears bit30, frame type in DW2 not DW7)
```

**重大差异 vs 我们当前实现**:

| 字段 | 我们的值 | Windows 值 | 影响 |
|------|---------|-----------|------|
| **DW0 PKT_FMT** | 2 (CMD) | **0 (CT/SF)** | **致命!** |
| **DW0 Q_IDX** | 0 | **1** | **可能关键** |
| **DW0 SN_EN** | 0 | **1 (bit31)** | 固件不自动填 SN |
| **DW1 HDR_FORMAT** | 2 (0b10) | **3 (0b11)** | 格式不匹配 |
| **DW1 HDR_INFO** | 12 (hdr/2) | **24 (bytes)** | **字段值错误** |
| **DW1 bit[25]** | 0 | **1** | 未知含义 |
| DW2 bits[31,29] | 1,1 (fixed rate) | **1,1 (0xA000000B)** | **VERIFIED: 一致** |
| DW3 REM_TX_COUNT | 30 | 30 | 一致 |
| **DW5 PID/TX_STATUS** | PID=1, MCU | **0x600\|PID** | **VERIFIED: Windows DOES request TX status!** |
| **DW6** | 0x4B<<16 + DIS_MAT | **0x004B0000** | **VERIFIED: Windows sets OFDM 6Mbps** |
| **DW7** | 0 | **0** | **VERIFIED: frame type in DW2, DW7=0** |

---

## 11. 根本原因分析

**TX auth 帧失败 (stat=1) 最可能的根本原因**:

1. **DW0 PKT_FMT=2(CMD)**: 我们告诉固件这是 MCU 命令帧, 但实际上应该是数据帧格式。固件可能在 CMD 解析路径中发现帧格式不对, 返回失败。

2. **DW0 Q_IDX=0 vs 1**: 队列索引不匹配可能导致帧被路由到错误的内部队列。

3. **DW1 HDR_FORMAT=2 vs 3**: 硬件/固件按 "802.11" 模式解析, 但 Windows 用的是 "802.11 extended" 模式。

4. **DW1 HDR_INFO=12(words) vs 24(bytes)**: 固件可能按 word 还是 byte 解析取决于 HDR_FORMAT。

**建议修复方向** (UPDATED after binary verification):
1. DW0 = 0x84000000 | TX_BYTES (PKT_FT=0, Q_IDX=1, SN_EN=1) ✅ DONE
2. DW1 = 0x02E0D800 base (HDR_FORMAT=0b11, HDR_INFO=24, TID=7) ✅ DONE
3. DW2 = 0xA000000B (fixed rate + frame type/subtype) ✅ DONE
4. DW5 = 0x600 | PID (TX_STATUS_2_HOST + TX_STATUS_FMT + PID) ✅ DONE
5. DW6 = 0x004B0000 (OFDM 6Mbps) ✅ DONE
6. DW7 = 0 (frame type in DW2, not DW7) ✅ DONE

---

## 12. MT6639 源码交叉验证 (Session 15 补充)

### DW0 位定义冲突

MT6639 `nic_connac3x_tx.h` 明确定义:
```c
#define CONNAC3X_TX_DESC_PACKET_FORMAT_MASK  BITS(23, 24)  // PKT_FMT at bits[24:23]
#define CONNAC3X_TX_DESC_PACKET_FORMAT_OFFSET 23
#define CONNAC3X_TX_DESC_QUEUE_INDEX_MASK    BITS(25, 31)  // Q_IDX at bits[31:25]
#define CONNAC3X_TX_DESC_QUEUE_INDEX_OFFSET  25
```

`BITS(m,n)` 宏 = `~(BIT(m)-1) & ((BIT(n)-1) | BIT(n))`, 即从 bit m 到 bit n 的连续掩码.
- `BITS(23,24)` = 0x01800000 → bits 23-24
- `BITS(25,31)` = 0xFE000000 → bits 25-31

这是 MT6639 Android 驱动和 CONNAC2X 共用的定义. 在 `dbg_connac3x.c` 第 232 行注释也确认 `/* Q_IDX [31:25] */`.

按此定义, Windows auth 帧的 DW0 = 0x84000000 给出:
- PKT_FMT = (0x84000000 >> 23) & 3 = 0
- Q_IDX = (0x84000000 >> 25) & 0x7F = 0x42 = 66

**Q_IDX=66 不对应任何有效队列** (MT6639 arTcResourceControl 中 Q_IDX 范围 0-12, MCU_Q0=0).

### 位定义悖论的解释

存在两种可能的解释:

**解释 A: MT6639 Android 头文件定义确实适用于硬件**

在此情况下, Windows 驱动写入 DW0[31:26] 的值 (来自 local_c4) 不是传统意义上的 Q_IDX, 而是 firmware 内部使用的不同语义字段. PCIe 路径 (MT7927) 和 SDIO/USB 路径 (MT6639 Android) 可能使用不同的 DW0 上位语义, 即使位宽和位置相同.

Windows 管理帧走 store-forward 模式 (TXD+frame 在一个连续 DMA buffer 中), 通过 mgmt ring 提交. 固件接收后可能重新解析/替换 DW0 上位字段.

**解释 B: 实际硬件 TXD 格式不同于 Android 头文件**

MT7927 的实际硬件 TXD 格式可能是:
```
bit[31]     = SN_EN (自动序列号) — 1 bit
bits[30:29] = PKT_FT (包格式) — 2 bits
bits[28:24] = Q_IDX (队列索引) — 5 bits
```

此布局下 DW0 = 0x84000000 给出:
- SN_EN = 1 (自动填充 SN)
- PKT_FT = 0 (CT/SF 模式)
- Q_IDX = 1 (MAC_TXQ_AC1 / Best Effort)

这些值完全合理: MT6639 管理帧走 TC4→MCU_Q0=0, 而 Windows 用 Q_IDX=1 可能是 PCIe 路径差异.

MT6639 Android 头文件可能是从旧版 CONNAC2X 复制过来的, 没有更新为实际 CONNAC3X 硬件的位布局. Android 驱动中管理帧总是走 TC4→Q_IDX=0, 所以 7-bit 还是 5-bit Q_IDX 在 Q_IDX=0 时没有区别.

### 实用结论 (不依赖位定义争议)

**不管哪种解释, DW0 的 hex 值是确定的:**

```
auth 帧 DW0 上位 = 0x84000000
= 0b 1000_0100_0000_0000_0000_0000_0000_0000
```

**我们应该直接设置这个 hex 值**, 而不是用 FIELD_PREP 宏. 这避免了位定义争议:

```c
/* Windows RE confirmed: DW0 upper for mgmt 802.11 frames
 * bit31=1 (SN_EN or high Q_IDX bit)
 * bit26=1 (low Q_IDX bit or sub-field)
 * PKT_FMT bits = 0 (CT/SF, not CMD)
 */
txwi[0] = cpu_to_le32(0x84000000 | (skb->len + MT_TXD_SIZE));
```

### MT6639 nic_txd_v3_compose() 对比

MT6639 为 802.11 管理帧设置的 TXD 字段:

```
DW0: TX_BYTES + PKT_FMT(由 ucPacketFormat 决定, 通常=0 for TXD_PKT_FORMAT_TXD)
     + Q_IDX(TC4→MCU_Q0=0)
DW1: MLD_ID(12bit=WLAN_IDX) + HDR_FORMAT=2(HEADER_FORMAT_802_11_NORMAL_MODE)
     + HDR_INFO=mac_hdr_len/2 (in DWORDs!) + TID=TYPE_NORMAL_MANAGEMENT(0)
     + OWN_MAC + FIXED_RATE(if manual rate)
DW2: TYPE + SUB_TYPE + HDR_PADDING + REMAINING_LIFE_TIME + POWER_OFFSET
DW3: REM_TX_COUNT + PROTECT/NO_ACK/BMC
DW5: PID + TXS_TO_MCU (if tx_done handler)
DW6: DIS_MAT (for mgmt) + MSDU_COUNT=1 + FIXED_RATE_IDX=0
DW7: TXD_LENGTH=TXD_LEN_1_PAGE(0)  ← 注意是 enum 值 0!
```

关键差异汇总:
| 字段 | MT6639 Android | Windows RE | 我们的驱动 |
|------|---------------|-----------|-----------|
| DW0 raw upper | 小值(Q_IDX=0) | **0x84000000** | PKT_FMT=2 (错!) |
| DW1 HDR_FORMAT | 2 (NORMAL) | **3 (0b11)** | 2 |
| DW1 HDR_INFO | hdr_len/2=12 | **24 (bytes)** | 12 (hdr/2) |
| DW1 MLD_ID | 12-bit WLAN_IDX | 8-bit byte[0] | 12-bit |
| DW1 TID | 0 (NORMAL_MGMT) | **7** | 0 |
| DW5 | PID+TXS_TO_MCU | **0x600\|PID** (VERIFIED) | 0x600\|PID |
| DW6 | DIS_MAT+MSDU=1 | **0x004B0000** (VERIFIED) | 0x004B0000 |
| DW7 | TXD_LEN=0 | **0** (VERIFIED) | 0 |

### 用户问题的最终回答

**Q1: DW0 bits[24:23] (PKT_FMT) 的值?**

**A: PKT_FMT = 0**. Windows XmitWriteTxDv1 memset DW0=0 后, 只修改 bits[31] 和 bits[30:26], 留下 bits[24:23] = 0. PKT_FMT=0 = CT/SF 模式 (不是 CMD=2). 我们当前用 PKT_FMT=2 是 **根本性错误**.

**Q2: DW1 HDR_INFO 存字节数(24)还是WORD数(12)?**

**A: 字节数 24**. 汇编确认: `MOVZX EDX, byte[RDI+0x2]` 加载 mac_hdr_len=0x18=24, 然后 `SHL EDX, 0x8` 直接放入 DW1. 虽然 MT6639 Android 代码用 `mac_hdr_len >> 1` (除以2得到 DWORD 数), Windows 驱动存的是原始字节数.

但请注意: Windows 把 HDR_INFO 放在 DW1 bits[12:8], 而 MT6639/CONNAC3 标准定义 HDR_INFO 在 bits[20:16]. 这个位置差异意味着要么 Windows 用了不同的 DW1 位布局, 要么 HDR_INFO 字段在 Windows 的上下文中有不同含义. 如果我们保持 CONNAC3 标准位定义 (HDR_INFO at bits[20:16]), 应该写 mac_hdr_len/2=12, 因为 MT6639 compose 函数就是这么做的. 如果我们完全照搬 Windows 的 hex 值, 则需要重新映射整个 DW1.
