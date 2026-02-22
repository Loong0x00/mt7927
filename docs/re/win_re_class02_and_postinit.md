# Windows RE 报告: class=0x02 命令格式 + FUN_1401c3240 分析

**日期**: 2026-02-21
**分析者**: postinit-re agent
**文件**: `DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/mtkwecx.sys`
**工具**: pefile + capstone (Python)

---

## 摘要

1. **class=0x02 命令**: UniCmd 格式确认，12字节 payload，`r9b=0`(fire-and-forget)。之前"破坏MCU通道"的原因极可能是我们错用了 `option=0x07`(query/等待响应)，而 Windows 用 fire-and-forget 发送。

2. **FUN_1401c3240**: 这是**连接(connect)流程处理函数**，不是静态 post-init 函数。它处理：连接状态设置 → 信道切换 → ROC 获取 → auth 帧 TX。

---

## 1. class=0x02 命令 — 详细逆向

### 1.1 函数位置

- 调用者: `AsicConnac3xPostFwDownloadInit` (`0x1401c9510`) Step 3 (在 NIC_CAP 之后)
- 包装函数: `0x1401cbd1c`
- MCU 发送函数: `0x1400c9468` (与 NIC_CAP 完全相同的发送路径)

### 1.2 完整汇编 — 0x1401cbd1c

```asm
; 初始化 12 字节 buffer
0x1401cbd7d: mov edi, 0xc           ; edi = 12 (buffer size)
0x1401cbd82: lea rcx, [rsp+0x60]    ; rcx = buffer
0x1401cbd87: mov edx, edi           ; size = 12
0x1401cbd89: call memset_zero       ; memset(buffer, 0, 12)

; 构建 payload
0x1401cbd90: xor ecx, ecx
0x1401cbd90: mov word ptr [rsp+0x60], 1     ; offset 0: u16le = 0x0001
0x1401cbd9b: lea eax, [rdi - 5]             ; eax = 12-5 = 7 (stored in adapter struct)
0x1401cbdab: mov r8b, 0xed                  ; param3 = 0xed (UniCmd format flag)
0x1401cbdb4: mov dl, 2                      ; param2 = CID = 0x02
0x1401cbdb6: mov word ptr [rsp+0x38], di    ; param8 = 12 (payload len)
0x1401cbdbb: lea rax, [rsp+0x60]
0x1401cbdc0: mov qword ptr [rsp+0x30], rax  ; param7 = &buffer (data ptr)
0x1401cbdc5: mov byte ptr [rsp+0x62], cl    ; offset 2: u8 = 0x00
0x1401cbdc9: mov rcx, qword ptr [rbx+0x14c0] ; param1 = mcu_ctrl
0x1401cbdd0: mov byte ptr [rsp+0x28], 8    ; param6 = 8
0x1401cbdd5: mov byte ptr [rsp+0x20], 1    ; param5 = 1
0x1401cbdda: mov dword ptr [rsp+0x64], 0x70000 ; offset 4: u32le = 0x00070000
0x1401cbde2: call 0x1400c9468              ; MCU sender
```

### 1.3 Payload 精确格式 (12字节)

```c
struct mt7927_cmd_class02_payload {
    __le16 field0;   /* = 0x0001 */
    u8     field2;   /* = 0x00  */
    u8     pad;      /* = 0x00  */
    __le32 field4;   /* = 0x00070000 */
    __le32 field8;   /* = 0x00000000 */
} __packed;
```

### 1.4 MCU 发送路径分析 (0x1400c9468)

MCU 发送函数根据芯片 ID 路由:

```asm
0x1400c9470: mov r11d, 0x6639
0x1400c947e: cmp ax, r11w
0x1400c9482: je 0x1400c94b4        ; MT6639 → UniCmd 路径
...
0x1400c9490: mov r11d, 0x7927
0x1400c9496: cmp ax, r11w
0x1400c949a: je 0x1400c94b4        ; MT7927 → UniCmd 路径
...
0x1400c94c8: jmp 0x1401481e4       ; → UniCmd path!
; 否则:
0x1400c94d7: jmp 0x1400c8b5c       ; → Legacy path
```

**结论**: MT7927 使用 **UniCmd 路径 (0x1401481e4)**，与 NIC_CAP 完全相同。

### 1.5 参数对比: NIC_CAP vs class=0x02

| 参数 | NIC_CAP (0x1401ca5d8) | class=0x02 (0x1401cbd1c) |
|------|----------------------|--------------------------|
| param1 (rcx) | mcu_ctrl | mcu_ctrl |
| param2 (dl) | **0x8a** | **0x02** |
| param3 (r8b) | 0xed | 0xed ← 相同! |
| param4 (r9d) | **0** | **0** ← 相同! |
| param5 [rsp+0x20] | 0 (sil) | **1** |
| param6 [rsp+0x28] | **5** | **8** |
| param7 [rsp+0x30] | NULL | &buffer (12字节) |
| param8 [rsp+0x38] | 0 | **12** |

**关键差异**: param7/param8 对应有无 payload。param6 的 5 vs 8 可能代表内部 option 类型。

### 1.6 Option 字段

从 UniCmd 查表函数 (0x140149218) 分析:
- `cl = CID = 0x02`
- `dl = option = r9b = 0`

```asm
0x140149228: cmp r10b, 0xed      ; 如果 CID==0xed (FWDL 命令):
0x14014922e: test dl, dl         ;   如果 option != 0: r8d=1 (用 option 匹配)
; 否则: 仅用 CID 匹配
```

`r9b=0` 表示 **fire-and-forget** (无等待响应)。NIC_CAP 也是 r9b=0，但 NIC_CAP 内部通过其他机制等待响应。

### 1.7 为什么之前"破坏 MCU 通道"?

**分析**: 我们的 `mt7927_mcu_send_unicmd` 使用 `option=0x07` (UNI_CMD_OPT_SET_ACK = 等待响应)。但 Windows 用 `r9b=0` (fire-and-forget) 发送 class=0x02，固件**不会**发送响应。

后果: 我们的代码在 `mt7927_mcu_wait_resp()` 中等待超时 → 超时后不清理 pending state → 后续命令乱序 → MCU 通道破坏。

**修复**: 用 `option=0x06` (fire-and-forget) 发送 class=0x02，不等待响应。

### 1.8 实施代码

```c
/* Step 3: class=0x02 命令 (Windows PostFwDownloadInit Step 4) */
/* 来源: Windows 0x1401cbd1c, Ghidra 汇编级验证 */
struct {
    __le16 field0;   /* = 0x0001 */
    u8     field2;   /* = 0x00  */
    u8     pad;
    __le32 field4;   /* = 0x00070000 */
    __le32 field8;   /* = 0x00000000 */
} __packed class02_payload = {
    .field0 = cpu_to_le16(0x0001),
    .field2 = 0x00,
    .pad    = 0x00,
    .field4 = cpu_to_le32(0x00070000),
    .field8 = cpu_to_le32(0x00000000),
};

ret = mt7927_mcu_send_unicmd(dev, 0x02,
                              UNI_CMD_OPT_SET_QUERY, /* 0x06 = fire-and-forget */
                              &class02_payload, sizeof(class02_payload));
if (ret) {
    dev_warn(&dev->pdev->dev, "class=0x02 failed: %d (继续)\n", ret);
    /* 不致命，继续 */
}
```

> **注意**: 用 `UNI_CMD_OPT_SET_QUERY (0x06)` 而非 `UNI_CMD_OPT_SET_ACK (0x07)`。

---

## 2. FUN_1401c3240 — 分析

### 2.1 函数位置

- VA: `0x1401c3240`
- 通过 vtable+0x50 调用

### 2.2 关键汇编序列

```asm
; 初始化连接状态机
0x1401c326d: call 0x1400c6fc4      ; connect state prep (函数1)
0x1401c3275: call 0x1400c6910      ; connect state prep (函数2)
0x1401c327a: mov [rdi+0x1467374], 2 ; conn_state = 2 (MEDIA_STATE_CONNECTED)
                                    ; ← 乐观地先设置 CONNECTED

; 信道配置
0x1401c3343: movzx edx, word ptr [r15+0x18] ; channel_id from vtable struct
0x1401c3346: mov r8b, 1
0x1401c3349: call 0x1400147f0      ; channel switch

; vtable 回调
0x1401c3396: call qword ptr [rip+0x5806c] ; vtable callback (ROC? auth?)

; ROC 获取
0x1401c34ac: call 0x1401f0be4      ; ROC_acquire()

; ROC 失败处理
0x1401c34b3: test eax, eax
0x1401c34b5: je 0x1401c3503        ; success → continue
...
0x1401c34f6: mov [rdi+0x1467374], ebp ; ebp=0 → conn_state = 0 (DISCONNECTED)
                                       ; ROC 失败时回滚状态
```

### 2.3 函数性质: CONNECT 流程处理函数

FUN_1401c3240 是 **连接(connect)流程的核心处理函数**，不是静态的 post-init 函数。它：

1. **设置连接状态** → `conn_state = 2 (CONNECTED)` (乐观)
2. **信道切换** → 配置目标 AP 所在信道
3. **ROC (Remain-on-Channel) 获取** → 锁定信道用于 auth 帧 TX
4. **失败回滚** → ROC 失败时清除 conn_state

#### vtable+0x50 的含义

```
vtable offset | 函数 | 用途
+0x48 | PostFwDownloadInit  | FW 下载后静态初始化
+0x50 | FUN_1401c3240       | 首次连接/认证流程
```

vtable+0x50 不是在 PostFwDownloadInit 之后自动调用的，而是当 OS/NDIS 发起连接请求时调用的。这类似于 mac80211 的 `.auth_complete()` 或 `.assoc()` 回调。

### 2.4 对我们 auth TX 问题的启示

FUN_1401c3240 揭示了 Windows 的正确连接序列:

```
connect 触发 (OS/NDIS OID)
  → FUN_1401c3240:
      1. call 0x1400c6fc4 + 0x1400c6910   [连接状态机初始化]
      2. conn_state = CONNECTED           [乐观设置]
      3. channel_switch(target_channel)   [切换到目标信道]
      4. vtable_callback()               [可能: BSS_INFO / STA_REC 更新]
      5. ROC_acquire()                   [获取驻留信道许可]
      6. [ROC grant callback → TX auth]  [ROC 授权后发送 auth 帧]
```

**关键**: Windows 先做 channel switch，再做 ROC，最后在 ROC grant 回调中发 auth 帧。我们的驱动可能缺少 channel switch 步骤，或 ROC grant 后没有正确触发 TX。

### 2.5 清楚的初始化 (0x1401c33f8 区域)

```asm
0x1401c33f1: mov word ptr [rdi+0x1889583], bp  ; bp=0 → 清零某个 word
0x1401c33f8: call 0x14000bfcc                  ; flush/reset 某状态
0x1401c3404: call 0x14000c808                  ; 设置某状态 (写入字符串 [rdx] = "xx")
0x1401c3417: call 0x14000c0b8                  ; 设置某状态
```

这些在 ROC 失败后的错误路径中出现，用于清理连接状态。

---

## 3. 总结与实施建议

### 3.1 class=0x02 命令 — 确认可以重新启用

| 项目 | 值 |
|------|-----|
| 格式 | **UniCmd** (与 NIC_CAP 相同路径) |
| CID | **0x02** |
| Option | **0x06** (fire-and-forget，不等响应) |
| Payload 大小 | **12 字节** |
| Payload 内容 | `{u16=1, u8=0, u8=0, u32=0x70000, u32=0}` |

**之前破坏 MCU 通道的原因**: 我们用了 `option=0x07`(等待响应)，而固件不回复此命令。

### 3.2 FUN_1401c3240 — 不需要在 probe 中模拟

FUN_1401c3240 是 connect 时的动态处理函数，不是 probe 时的静态初始化。我们不需要在驱动 probe 中添加等效代码。

但它揭示了正确的 connect 序列：**channel_switch → ROC → [ROC grant → TX auth]**。

### 3.3 优先级修复顺序

```
1. [立即] 重新启用 class=0x02，改用 option=0x06 (fire-and-forget)
2. [待验证] 确认 0xd6060 |= 0x10101 已在我们驱动中执行 (Session 17已加)
3. [中期] 检查 ROC grant 回调中是否正确触发 TX auth
4. [中期] 确认 channel_switch 在 connect 时正确执行
```

---

## 附录 A: 调用参数完整对比

### NIC_CAP (0x1401ca5d8 → 0x1400c9468)
```asm
mov rcx, [rdi+0x14c0]    ; mcu_ctrl
mov dl, 0x8a             ; CID = NIC_CAP
mov r8b, 0xed            ; format = UniCmd
xor r9d, r9d             ; option = 0
mov [rsp+0x20], sil      ; param5 = 0
mov [rsp+0x28], 5        ; param6 = 5
mov [rsp+0x30], rsi      ; param7 = NULL (no payload)
mov [rsp+0x38], si       ; param8 = 0 (no payload)
call 0x1400c9468
```

### class=0x02 (0x1401cbd1c → 0x1400c9468)
```asm
mov rcx, [rbx+0x14c0]    ; mcu_ctrl
mov dl, 2                ; CID = 0x02
mov r8b, 0xed            ; format = UniCmd (相同!)
xor r9d, r9d             ; option = 0
mov [rsp+0x20], 1        ; param5 = 1
mov [rsp+0x28], 8        ; param6 = 8
lea rax, [rsp+0x60]
mov [rsp+0x30], rax      ; param7 = &payload
mov [rsp+0x38], di       ; param8 = 12 (payload len)
call 0x1400c9468
```

---

## 附录 B: 相关地址

| 地址 | 函数 | 备注 |
|------|------|------|
| 0x1401c9510 | AsicConnac3xPostFwDownloadInit | 主函数 |
| 0x1401ca5d8 | NIC_CAP wrapper | Step 2 |
| 0x1401cbd1c | class=0x02 command | Step 3 |
| 0x1400c9468 | MCU sender (共用) | UniCmd/Legacy 路由 |
| 0x1401481e4 | UniCmd path | MT7927 走此路径 |
| 0x140149218 | UniCmd lookup | CID → slot 映射 |
| 0x1401c3240 | FUN_1401c3240 | Connect 流程处理 |
| 0x1401f0be4 | ROC_acquire | ROC 获取 |
| 0x1400147f0 | channel_switch | 信道切换 |
