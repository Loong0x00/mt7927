# Windows RE: EFUSE_CTRL (outer=0x58, inner=0x05) 分析

**分析日期**: 2026-02-21
**来源**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (pefile + capstone)
**目的**: 定位 bisect 根因 — EFUSE_CTRL 导致 scan 失败

---

## 1. Bisect 结论摘要

| Test | 改动 | scan 结果 |
|------|------|---------|
| Test 1+2 | SCAN_CFG/SET_DOMAIN/BAND_CONFIG 回退 | ❌ 失败 |
| Test 4 | 只回退 CONFIG (0x0b→0x02) | ❌ 失败 |
| Test 5 | 只回退 WFDMA_CFG (0x0d→0xc0) | ❌ 失败 |
| **Test 6** | **只回退 EFUSE_CTRL (0x05→0x2d)** | **✅ 22 BSS，scan 恢复** |

**根因锁定：EFUSE_CTRL (inner CID=0x05) 的 payload 格式完全错误，导致固件处理后破坏 scan 所需配置。**

---

## 2. Dispatch Table 中的 EFUSE_CTRL 记录

来源：dispatch table @ `0x1402507e0`，每条记录 13 字节格式：
```
[0]    outer_tag
[1]    0x00 (padding)
[2]    inner_CID
[3]    extra_param (通常0，STA_REC 为 0xa8)
[4]    0x00 (padding)
[5:13] handler_VA (8 bytes, little-endian)
```

EFUSE_CTRL 条目 (@ `0x14025099a`):
```
58 00 05 00 00 d0 4c 14 40 01 00 00 00
outer=0x58, inner=0x05, extra_param=0x00, handler=0x140144cd0
```

---

## 3. EFUSE_CTRL Handler 反汇编分析 (VA 0x140144cd0)

```asm
0x140144cd0: mov  qword ptr [rsp+8], rbx
0x140144cda: cmp  byte ptr [rdx], 0x58          ; verify outer_tag == 0x58
0x140144cdd: mov  rbx, rdx                      ; save cmd_ptr
0x140144ce6: cmp  dword ptr [rdx+0x10], 0x44    ; ← INPUT SIZE CHECK: must be 0x44 = 68 bytes
0x140144cf0: mov  rdi, qword ptr [rdx+0x18]     ; rdi = &input_data
0x140144cf4: mov  r8d, 0x10                     ; alloc param = 0x10
0x140144cfa: mov  dl, 5                         ; inner_CID = 5
0x140144cfc: call 0x14014f788                   ; nicUniCmdBufAlloc(ctx, CID=5, 0x10)
; --- payload construction (alloc_result + 0x18) ---
0x140144d5e: mov  rcx, qword ptr [rax+0x18]     ; rcx = payload area
0x140144d62: mov  al, byte ptr [rdi]             ; al = input[0]
0x140144d64: mov  byte ptr [rcx], al             ; payload[0x00] = input[0]
0x140144d66: mov  dword ptr [rcx+4], 0xc0000    ; payload[0x04..0x07] = 0x000c0000 (硬编码!)
0x140144d6d: mov  al, byte ptr [rdi+1]           ; al = input[1]
0x140144d70: mov  byte ptr [rcx+8], al           ; payload[0x08] = input[1]
0x140144d73: mov  al, byte ptr [rdi+2]           ; al = input[2]
0x140144d76: mov  byte ptr [rcx+9], al           ; payload[0x09] = input[2]
0x140144d7d: mov  byte ptr [rcx+0xa], r8b        ; payload[0x0a] = r8b
```

### Handler 期望的固件 payload 结构

```c
struct efuse_ctrl_fw_payload {   // 固件接收的 payload
    u8  field_00;    // [0x00] = input[0]   (第一个字段)
    u8  _rsv[3];     // [0x01..0x03] = 0    (未设置)
    u32 flags;       // [0x04..0x07] = 0x000c0000 (硬编码!)
    u8  field_08;    // [0x08] = input[1]
    u8  field_09;    // [0x09] = input[2]
    u8  field_0a;    // [0x0a] = r8b (= 0x10 = alloc size?)
    // 其余字节未设置
};
```

**输入结构 (handler 期望的 68-byte input)**:
- input[0] → payload[0]: EEPROM mode (0=内建eFuse, 1=BIN文件, 最大4)
- input[1] → payload[8]: 未知字节参数（来自调用方的 R8B 参数）
- input[2] → payload[9]: 0（通常）
- input[3..67]: 其余字段全为 0（handler 只读 input[0..2]）

**注意**：handler 只从 68-byte input 中读取 input[0], input[1], input[2] 三个字节，其余字段均为 0（memset 初始化）。

---

## 4. EFUSE_CTRL Sender 函数 (FUN_1400c3238) 调用链分析

**函数 VA**: `0x1400c3238`
**参数**: RCX=adapter, RDX=data_struct_ptr, R8B=某字节参数

```c
// 函数逻辑 (伪代码)
void SendEfuseCtrl(ctx, data_ptr, param_byte) {
    u8 input_buf[68] = {0};  // memset(0x44 bytes)

    // FUN_14018b14c: 读取 data_ptr->offset_0x24, clamp 到 4
    input_buf[0] = min(data_ptr->field_0x24, 4);  // EEPROM mode
    input_buf[1] = param_byte;                     // from R8B caller param

    // 调用 nicUniCmdAllocEntry:
    // rcx = ctx->field_0x14c0
    // dl = 0x58 (outer_tag EFUSE_CTRL)
    // r8b = 0xed (target)
    // r9d = 0 (extra_param)
    // [rsp+0x20] = 1, [rsp+0x28] = 8
    // [rsp+0x30] = &input_buf, [rsp+0x38] = 0x44 (input size=68)
    nicUniCmdAllocEntry(ctx->ctx_field, 0x58, 0xed, 0, ..., &input_buf, 68, ...);
}
```

**调用方搜索结果**：
- 无直接 E8 CALL 引用（不是普通函数调用）
- 无 .data/.rdata 中的函数指针引用
- **结论**：该函数通过运行时写入的函数指针调用（可能是 NDIS/WDI 回调），probe 阶段不在调用链上

---

## 5. PostFwDownloadInit 中的 EFUSE_CTRL

**结论：Windows PostFwDownloadInit 完全不发送 EFUSE_CTRL (outer=0x58)！**

PostFwDownloadInit (VA `0x1401c9510`) 的完整序列（来自 `docs/references/ghidra_post_fw_init.md`）：
```
1. Clear flag
2. WRITE 0x7c026060 |= 0x10101
3. MCU cmd class=0x8a  (NIC_CAP)
4. MCU cmd class=0x02  (CLASS02, payload={1,0,0x70000})
5. MCU cmd class=0xc0  (WFDMA_CFG, payload={0x820cc800, 0x3c200})
6. AsicConnac3xDownloadBufferBin (class=0xed, subcmd=0x21, optional)
7. MCU cmd class=0x28  (DBDC, MT6639/MT7927 only)
8. KeStallExecutionProcessor(1ms)
9. MCU cmd class=0xca  (SetPassiveToActiveScan)
10. MCU cmd class=0xca (SetFWChipConfig)
11. MCU cmd class=0xca (SetLogLevelConfig)
```

**EFUSE_CTRL (outer=0x58) 不在此序列中。**

---

## 6. 我们当前的错误实现

```c
// src/mt7927_pci.c:1989 — mt7927_mcu_set_eeprom()
/* 参考: mt76/mt7925/mcu.c mt7925_mcu_set_eeprom() ← 违反禁止参考 mt7925 的规则！ */
struct {
    u8   rsv[4];        // = 0
    __le16 tag;         // = 0x0002 (UNI_EFUSE_BUFFER_MODE)
    __le16 len;         // = sizeof-4 = 4
    u8   buffer_mode;   // = 0 (EE_MODE_EFUSE)
    u8   format;        // = 1 (EE_FORMAT_WHOLE)
    __le16 buf_len;     // = 0
} req;                  // sizeof = 12 bytes
```

**错误点**：
1. **来源错误**：从 mt7925 抄来，mt7927 的 EFUSE_CTRL 格式完全不同
2. **Payload 格式错误**：Windows handler 期望 field_00/flags/field_08/field_09，我们发 TLV 格式
3. **调用时机错误**：Windows probe 阶段根本不发送 EFUSE_CTRL
4. **发送后果**：固件处理了错误格式的 payload，`payload[4..7] = 0x000c0000` 是 Windows 期望的硬编码值，我们发的完全不同 → 固件可能错误配置了 EEPROM 访问模式，破坏 RF 校准数据读取 → scan 失败

---

## 7. 修复方案

### 立即修复 (推荐)：删除 EFUSE_CTRL 调用

**理由**：Windows probe 阶段不发送，固件默认使用 eFuse 模式读取校准数据，不需要我们显式设置。

```c
// src/mt7927_pci.c: PostFwDownloadInit 末尾
// 删除以下行:
// mt7927_mcu_set_eeprom(dev);
```

**效果**：等同于 bisect Test 6（用旧 outer tag 0x2d 时固件忽略），但更干净——完全不发送，无副作用。

### 长远研究

如果 EFUSE_CTRL 在连接流程中必须发送：
1. 需要 RE 找到 Windows 在什么场景调用 outer=0x58 命令（搜索调用链）
2. 需要确认 68-byte input 结构的完整含义（input[0..2] 是什么字段）
3. 注意：EFUSE_CTRL dispatch table 中 extra_param=0x00（STA_REC 的是 0xa8），说明它是普通命令，不需要特殊 option 参数

---

## 8. 相关文件

- `src/mt7927_pci.c:1989` — mt7927_mcu_set_eeprom() (需要删除/注释调用处)
- `src/mt7927_pci.c:1826` — mt7927_mcu_set_eeprom() 调用处 (PostFwDownloadInit Step 10)
- `docs/references/ghidra_post_fw_init.md` — PostFwDownloadInit 完整序列
- `docs/win_re_cid_mapping.md` — dispatch table 完整表
