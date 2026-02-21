# Windows mtkwecx.sys — DMA Descriptor Format Analysis

**RE Date**: 2026-02-21
**Tool**: Ghidra 12.0.3 headless decompilation
**Binary**: mtkwecx.sys v5705275
**Functions**: FUN_14000cc44, FUN_14005d1a4 (N6PciTxSendPkt), FUN_1400532e4

---

## 结论摘要

**DMA 描述符格式已从 Ghidra 二进制验证 — 我们当前代码是正确的。**
DMA 描述符不是固件静默的根因。

---

## 1. FUN_14000cc44 完整分析

### 反编译输出

```c
void FUN_14000cc44(longlong adapter, byte ring_idx, undefined8 buf_flag /*IGNORED*/,
                   uint required_descs, byte lock_flag)
{
    // param_3 (buf_flag) 被静默丢弃 — 完全没有用！
    FUN_1400532e4(*(undefined8 *)(adapter + 0x1f80),
                  ring_idx,
                  required_descs,   // 0xb for mgmt, 1 for data
                  lock_flag);
    return;
}
```

### 参数映射

N6PciTxSendPkt 中的调用：
```c
// mgmt (param_3 >= 2):
FUN_14000cc44(adapter, ring_idx_byte, *(buf_node+0x2b), 0xb, stack_param);
              //                      ^^ 这个参数被丢弃！

// data (param_3 < 2):
FUN_14000cc44(adapter, ring_idx_byte, *(buf_node+0x2b), 0x1, stack_param);
```

**关键发现**: `buf_flag` (param_3 = `*(buf_node+0x2b)`) 完全被忽略。
FUN_14000cc44 只是转发 `ring_idx` 和 `required_descs` 给 FUN_1400532e4。

---

## 2. FUN_1400532e4 — 真正的描述符计数检查函数

名称: `NdisCommonHifPciFreeDescriptorRequest` (从日志字符串推断)

```c
undefined8 FUN_1400532e4(adapter_sub, int ring_idx, uint required_count, byte lock_flag)
{
    if (ring_idx >= 4)       return 0xc0000001;  // INVALID_PARAMETER
    if (required_count == 0) return 0xc0000001;

    if (lock_flag) KeAcquireSpinLock(ring_spinlock[ring_idx]);

    // 计算 ring 空闲描述符数
    uint DIDX = *(adapter_sub + ring_stride*ring_idx + 0xe0230);  // DMA 消费指针
    uint CIDX = *(adapter_sub + ring_stride*ring_idx + 0xe0228);  // CPU 生产指针
    uint free;

    if (CIDX < DIDX) {
        free = DIDX - CIDX - 1;
    } else {
        uint ring_size = *(adapter_sub + ring_stride*ring_idx + 0xe0234);
        free = ring_size - CIDX + DIDX - 1;
    }

    if (lock_flag) KeReleaseSpinLock(ring_spinlock[ring_idx]);

    if (free >= required_count) return 0;          // SUCCESS

    // 日志: "NdisCommonHifPciFreeDescriptorRequest" line 0x494
    return 0xc000009a;  // INSUFFICIENT_RESOURCES
}
```

### 为什么 mgmt 需要 11 个空闲描述符？

`0xb = 11` 是一个**最小空闲槽保证**，不是"每帧用11个描述符"。
管理帧是高优先级，Windows 要求 ring 至少有 11 个空闲槽才提交新帧，防止 ring 即将满时丢管理帧。

**对我们的影响**: Ring 2 有 256 个描述符，初始全部空闲，`free = 255 >= 11`，检查必然通过。这不是问题。

---

## 3. N6PciTxSendPkt DMA 描述符写入 — 精确分析

### 核心描述符写入代码 (从反编译)

```c
// puVar4 = 指向当前 head 对应的 16 字节 DMA 描述符

// 步骤1: 清零 16 字节
FUN_14001022c(puVar4, 0x10);  // memset(desc, 0, 16)

// 步骤2: DW0 = buf 物理地址低32位
*puVar4 = *(uint32 *)(buf_node + 0x58);

// 步骤3: DW3[15:0] = buf 物理地址高16位
*(uint16 *)(puVar4 + 3) = *(uint16 *)(buf_node + 0x5c);

// 步骤4: DW1 计算 (mgmt vs data 不同)
if (param_3 < 2) {
    // DATA: BIT(22) 特殊 (CT mode 相关)
    uVar8 = puVar4[1] & 0xc040ffff | 0x400000;
} else {
    // MGMT: 长度写入 bits[29:16]
    uVar8 = (*(int *)(buf_node + 0x68) << 0x10 ^ puVar4[1]) & 0x3fff0000U ^ puVar4[1];
}
// 设置 OWN bit (bit30), 清除 bit31
puVar4[1] = uVar8 & 0x7fffffff | 0x40000000;
```

### 汇编级原始验证 (来自 Ghidra 反汇编)

```asm
14005d524: CALL 0x14001022c         ; memset(desc, 0x10)
14005d529: MOV EAX,dword ptr [RBX + 0x58]    ; buf_phys_low
14005d52c: MOV dword ptr [R12],EAX  ; DW0 = phys_addr_low
14005d530: MOVZX EAX,word ptr [RBX + 0x5c]  ; buf_phys_high[15:0]
14005d534: MOV word ptr [R12 + 0xc],AX       ; DW3[15:0] = phys_addr_high
;                         ^^^^ 0xc = 12 = DW3 起始字节偏移

14005d53f: MOV EAX,dword ptr [R12 + 0x4]    ; read DW1 (= 0 after memset)
14005d544: MOV ECX,dword ptr [RBX + 0x68]   ; total_len (TXD + frame)
14005d547: SHL ECX,0x10                     ; total_len << 16
14005d54a: XOR ECX,EAX                      ; XOR trick: ^DW1
14005d54c: AND ECX,0x3fff0000               ; keep bits[29:16]
14005d552: XOR ECX,EAX                      ; = set bits[29:16] = total_len
; (以上3行等价: DW1 = (DW1 & ~0x3fff0000) | (total_len << 16))

14005d565: BTS ECX,0x1e                     ; SET bit30 (OWN)
14005d56c: BTR ECX,0x1f                     ; CLEAR bit31
14005d570: MOV dword ptr [R12 + 0x4],ECX   ; write DW1
```

---

## 4. 管理帧 DMA 描述符精确格式 (Ghidra 汇编级验证)

```
DW0 (bytes 0-3):
    [31:0]  = buf_phys_addr_low  (物理地址低32位)

DW1 (bytes 4-7):
    [31]    = 0  (BTR bit31 — 显式清除)
    [30]    = 1  (BTS bit30 — OWN bit, DMA引擎所有权)
    [29:16] = total_len  (TXD + frame 总长度, 14位)
    [15:0]  = 0  (未使用)

DW2 (bytes 8-11):
    [31:0]  = 0  (memset清零，从不写入)

DW3 (bytes 12-15):
    [15:0]  = buf_phys_addr_high  (物理地址高16位, 32位DMA mask时为0)
    [31:16] = 0  (未使用)
```

### 简化公式

```c
// mgmt DMA 描述符写入:
desc[0] = phys_addr & 0xffffffff;           // DW0
desc[1] = (total_len << 16 & 0x3fff0000)    // DW1
          | 0x40000000;                      // OWN bit
desc[2] = 0;                                 // DW2
desc[3] = (phys_addr >> 32) & 0xffff;       // DW3 (高16位)
```

---

## 5. 与我们代码的比较

### 当前 Linux 驱动代码 (src/mt7927_dma.c)

```c
desc->buf0 = cpu_to_le32(lower_32_bits(dma));  // DW0: phys_addr_low ✓
desc->buf1 = cpu_to_le32(0);                    // DW2: 0 ✓
desc->info = cpu_to_le32(0);                    // DW3: 0 ✓ (DMA mask=32bit, high=0)

ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, total_len)  // bits[29:16] = total_len ✓
     | MT_DMA_CTL_LAST_SEC0;                       // BIT(30) = OWN ✓
desc->ctrl = cpu_to_le32(ctrl);                    // DW1 ✓
```

### 对比表

| 字段 | Windows | 我们的驱动 | 匹配? |
|------|---------|-----------|-------|
| DW0 phys_addr_low | `*(buf+0x58)` | `lower_32_bits(dma)` | ✅ |
| DW1 length | bits[29:16] = total_len | `FIELD_PREP(GENMASK(29,16), total_len)` | ✅ |
| DW1 OWN | BIT(30) = 1 | `MT_DMA_CTL_LAST_SEC0 = BIT(30)` | ✅ |
| DW1 bit31 | 0 (清除) | 0 (未设置) | ✅ |
| DW2 | 0 | `buf1 = 0` | ✅ |
| DW3 phys_addr_high | `*(buf+0x5c)` | 0 (32-bit DMA mask) | ✅ |

**结论: DMA 描述符格式完全匹配 Windows。**

---

## 6. 数据帧 DMA 描述符 (参考)

数据帧使用不同的 DW1 计算:

```c
// param_3 < 2 (data path):
DW1 = (0 & 0xc040ffff | 0x400000) & 0x7fffffff | 0x40000000
    = 0x40400000
```

分解:
- BIT(30) = OWN bit
- BIT(22) = CT mode 标志 (属于 SD_LEN0 字段范围内，但值 = 64)

这对应 CT mode 数据帧，TXP 附加在 buf + 0x20 处。管理帧不使用此格式。

---

## 7. FUN_14005d6d8 (N6PciUpdateAppendTxD) — 仅数据帧使用

此函数在数据路径中被调用 (`param_3 < 2`)，用于构建 CT mode 的 TXP (Transmit Packet Protocol) scatter-gather 扩展头。

**管理帧不调用此函数** — 管理帧使用 SF (Short Frame inline) mode，TXD + 帧内联在一个 DMA buffer 中。

---

## 8. 修复建议

### DMA 描述符: 无需修改

当前代码完全正确，与 Windows 二进制级别完全匹配。

### 仍需排查的根因 (按优先级)

1. **🔴 DMASHDL init 覆盖固件配置** (`docs/win_re_hif_ctrl_investigation.md`)
   - Windows 只做 `0xd6060 |= 0x10101`
   - 我们做了 full reconfig (15+ 寄存器写入)

2. **🔴 BSS_INFO 缺 RATE/PROTECT/IFS_TIME TLV**
   - Windows 发 14 个 TLV，我们只发 3 个

3. **🟡 PLE/PSE 诊断**
   - TX 后读 PLE_QUEUE_EMPTY/PSE_QUEUE_EMPTY 确认帧去向

---

## 附录: 关键函数地址

| 地址 | 函数名 | 说明 |
|------|--------|------|
| 0x14000cc44 | FUN_14000cc44 | 描述符计数检查包装函数 |
| 0x1400532e4 | NdisCommonHifPciFreeDescriptorRequest | 实际空闲描述符计数检查 |
| 0x14005d1a4 | N6PciTxSendPkt | DMA 描述符写入 + ring kick |
| 0x14005d6d8 | N6PciUpdateAppendTxD | CT mode TXP 构建 (仅数据帧) |
| 0x1400359cc | FUN_1400359cc | mgmt 帧队列追加 |
| 0x140009a18 | (ring kick) | 写 CIDX 寄存器 |
