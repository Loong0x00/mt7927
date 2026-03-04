# Windows RE: MLO_CTRL Handler (0x140147330) — 完整逆向分析

**分析来源**: mtkwecx.sys v5705275 (AMD-MediaTek WiFi 7 Driver)
**分析方法**: Ghidra headless 反编译 + 汇编对照
**日期**: 2026-03-04
**函数地址**: `0x140147330`
**Dispatch Table Entry**: #55 (outer_tag=0x7e, inner_CID=0x44, filter=0x00)

---

## 一、Executive Summary

MLO_CTRL 是一个 **性能指标上报命令 (Performance Indicator)**，不是 MLO 数据面激活命令。
函数名从错误日志字符串推断为 `nicUniCmdPerfInd`（源码行号 0x1a44）。

**关键发现**:
1. MLO_CTRL 构建一个 inner_CID=0x44 的 TLV，载荷为 0x54 字节
2. TLV tag+len = `0x00050002` (tag=0x0002, len=0x0005?? 见分析)
3. 从输入拷贝 8 字节头部字段 + 0x40 字节批量数据
4. 硬编码 `byte[0x10] = 4`，暗示"4 个链路"或"类型=4"
5. **这不是 MLO 链路激活命令**，而是运行时统计/性能指标上报

---

## 二、反编译代码（标注版）

```c
// 函数: nicUniCmdPerfInd (MLO_CTRL handler)
// 地址: 0x140147330
// 调用方式: dispatch(0x7e, 0xed, 0) → entry 55 → this handler
// 参数:
//   param_1 = adapter
//   param_2 = dispatch context (内部结构)
//     param_2+0x18 = 指向输入 payload 的指针
//     param_2+0x30 = linked list head
//     param_2+0x38 = linked list tail
//     param_2+0x40 = entry count

uint64_t nicUniCmdPerfInd(uint64_t param_1, int64_t param_2)
{
    uint8_t *input = *(uint8_t **)(param_2 + 0x18);  // 输入 payload

    // 分配 UniCmd 缓冲区: inner_CID=0x44, payload_size=0x54
    int64_t *entry = nicUniCmdBufAlloc(param_1, 0x44, 0x54);

    if (entry == NULL) {
        // 分配失败 — 记录 WPP trace
        // 错误消息: "nicUniCmdPerfInd", 行号=0x1a44
        WPP_LOG(0xd9, "nicUniCmdPerfInd", 0x1a44);
        return 0xC000009A;  // STATUS_INSUFFICIENT_RESOURCES
    }

    // entry[3] = 指向 TLV 数据区的指针 (entry + 0x28 处开始的 payload)
    uint8_t *data = (uint8_t *)entry[3];

    // === TLV 头部 ===
    // data[0..3] = 未设置 (由 BufAlloc 清零)
    *(uint32_t *)(data + 4) = 0x00050002;  // tag=0x0002(?), len/flags=0x0005(?)
    //   或解读为: u16 tag=0x0002, u16 len=0x0005
    //   注意: 这不符合标准 tag+len 格式 (通常 tag=u16, len=u16)
    //   实际可能是: padding/reserved[0..3] + tag_len[4..7]

    // === 从输入拷贝头部字段 ===
    data[8]     = input[0];           // 字段1: bss_idx 或 band_idx
    *(uint16_t *)(data + 10) = *(uint16_t *)(input + 2);  // 字段2: 2 字节
    *(uint32_t *)(data + 0x0c) = *(uint32_t *)(input + 4); // 字段3: 4 字节

    // === 硬编码 ===
    data[0x10] = 4;                   // num_links? type? = 4

    // === 批量拷贝 0x40 字节 ===
    memcpy(data + 0x14, input + 0x0c, 0x40);  // 64 字节性能数据

    // 链入命令队列
    uint64_t **tail = *(uint64_t ***)(param_2 + 0x38);
    *(uint64_t **)(param_2 + 0x38) = entry;
    entry[0] = param_2 + 0x30;     // flink
    entry[1] = (int64_t)tail;      // blink
    *tail = entry;                  // prev->flink = this
    *(int *)(param_2 + 0x40) += 1; // count++

    return 0;  // STATUS_SUCCESS
}
```

---

## 三、汇编对照

```asm
; === 函数入口 ===
140147330:  MOV [RSP+8], RBX          ; 保存 RBX
140147335:  MOV [RSP+0x10], RSI       ; 保存 RSI
14014733a:  PUSH RDI
14014733b:  SUB RSP, 0x30

; === 获取输入 payload 指针 ===
14014733f:  MOV RSI, [RDX+0x18]       ; RSI = input payload ptr
140147343:  MOV RDI, RDX              ; RDI = dispatch context

; === 调用 nicUniCmdBufAlloc(adapter, CID=0x44, size=0x54) ===
140147346:  MOV DL, 0x44              ; inner_CID = 0x44
140147348:  MOV R8D, 0x54             ; payload_size = 0x54 (84 bytes)
14014734e:  CALL 0x14014f788          ; nicUniCmdBufAlloc
140147353:  MOV RBX, RAX              ; RBX = entry (or NULL)
140147356:  TEST RAX, RAX
140147359:  JNZ  0x1401473b0          ; → 成功路径

; === 失败路径 (entry == NULL) ===
; ... WPP trace 错误记录 ...
1401473a9:  MOV EAX, 0xC000009A       ; NTSTATUS = 资源不足
1401473ae:  JMP 0x140147400           ; → 返回

; === 成功路径: 构建 TLV payload ===
1401473b0:  MOV RCX, [RAX+0x18]       ; RCX = data = entry->payload_ptr
1401473b4:  LEA RDX, [RSI+0x0c]       ; RDX = &input[0x0c] (memcpy 源)
1401473b8:  MOV R8D, 0x40             ; R8 = 0x40 = 64 bytes (memcpy 长度)

; TLV 值填充:
1401473be:  MOV dword [RCX+4], 0x500002   ; data[4..7] = 0x00500002
;   *** 注意: 汇编确认值为 0x00500002, 不是 0x00050002! ***
;   解读: u16 tag = 0x0002, u16 len = 0x0050 (80 字节)
;   0x50 = 80, 即 TLV payload 从 data[8] 开始, 长 80 字节
;   总共 8 (header) + 80 (payload) = 88 字节... 但分配只有 0x54=84
;   可能: tag=0x0002 是子 TLV tag, len=0x0050 包含自身 header

1401473c5:  MOV AL, [RSI]             ; AL = input[0]
1401473c7:  MOV [RCX+8], AL           ; data[8] = input[0] (bss_idx/band_idx)

1401473ca:  MOVZX EAX, word [RSI+2]   ; AX = input[2..3]
1401473ce:  MOV [RCX+0xa], AX         ; data[0xa..0xb] = input[2..3]

1401473d2:  MOV EAX, dword [RSI+4]    ; EAX = input[4..7]
1401473d5:  MOV [RCX+0xc], EAX        ; data[0xc..0xf] = input[4..7]

1401473d8:  MOV byte [RCX+0x10], 4    ; data[0x10] = 4 (硬编码)

1401473dc:  ADD RCX, 0x14             ; RCX = &data[0x14] (memcpy 目标)
1401473e0:  CALL 0x140010118          ; memcpy(data+0x14, input+0x0c, 0x40)

; === 链入命令队列 ===
1401473e5:  LEA RAX, [RDI+0x30]       ; RAX = &ctx->list_head
1401473e9:  MOV RCX, [RAX+8]          ; RCX = list_head->blink (tail)
1401473ed:  MOV [RAX+8], RBX          ; list_head->blink = new_entry
1401473f1:  MOV [RBX], RAX            ; new_entry->flink = list_head
1401473f4:  MOV [RBX+8], RCX          ; new_entry->blink = old_tail
1401473f8:  MOV [RCX], RBX            ; old_tail->flink = new_entry
1401473fb:  INC dword [RDI+0x40]      ; ctx->entry_count++

; === 返回成功 ===
1401473fe:  XOR EAX, EAX              ; return 0
140147400:  MOV RBX, [RSP+0x40]
140147405:  MOV RSI, [RSP+0x48]
14014740a:  ADD RSP, 0x30
14014740e:  POP RDI
14014740f:  RET
```

---

## 四、TLV Wire Format

### 4.1 外层: UniCmd Header

```
UniCmd Header:
  CID = 0x44
  option = 0x06 (set, fire-and-forget)
  S2D = 0xa0
```

### 4.2 TLV Payload 布局 (0x54 = 84 字节)

```
Offset  Size  Field                 Value/Source
------  ----  --------------------  ----------------------------
+0x00   4     reserved/header       (清零)
+0x04   4     tag_len               0x00500002
                u16 tag = 0x0002     (子 TLV 标识)
                u16 len = 0x0050     (payload 长度 = 80 字节)
+0x08   1     bss_idx               input[0]
+0x09   1     (padding)             0 (清零)
+0x0a   2     field_1               input[2..3]  (u16, 可能是 sta_idx 或 link_bitmap)
+0x0c   4     field_2               input[4..7]  (u32, 可能是 perf_flags 或 counters)
+0x10   1     num_entries           4 (硬编码)
+0x11   3     (padding)             0 (清零)
+0x14   64    perf_data[64]         memcpy from input[0x0c], 64 字节批量数据
```

**总计**: 4 (reserved) + 4 (tag_len) + 1 + 1 + 2 + 4 + 1 + 3 + 64 = 84 = 0x54 字节

### 4.3 tag_len 解读

汇编确认 `MOV dword [RCX+4], 0x500002`:
- Little-endian 存储: `02 00 50 00`
- 解析为 u16 tag = `0x0002`, u16 len = `0x0050` (80)
- len=0x50 覆盖从 +0x08 到 +0x53 (80 字节), 即从 bss_idx 到 perf_data 结尾

---

## 五、输入 Payload 结构推断

调用者提供的 input buffer (至少 0x4C = 76 字节):

```
Offset  Size  Field           Used as
------  ----  --------------- -------------------------
+0x00   1     bss_idx         → data[0x08]
+0x01   1     (unknown)       不使用
+0x02   2     field_1         → data[0x0a..0x0b]
+0x04   4     field_2         → data[0x0c..0x0f]
+0x08   4     (gap)           不使用
+0x0c   64    perf_data       → data[0x14..0x53] (批量拷贝)
```

---

## 六、函数名推断

在分配失败路径中，WPP trace 日志打印:
```c
FUN_140001664(log_ctx, 0xd9, &DAT_1402387a0, "nicUniCmdPerfInd", 0x1a44);
```

- 函数名: **`nicUniCmdPerfInd`** (Performance Indicator)
- 源码行号: **0x1a44 = 6724**
- 源码文件: `nic_uni_cmd_event.c` (从 nicUniCmdBufAlloc 的路径推断:
  `E:\worktmp\...\Common\nic_uni_cmd_event.c`)

**结论**: 这个命令叫 **UNI_CMD_PERF_IND** (Performance Indicator)，不是 MLO 链路控制命令。

---

## 七、调用时机分析

### 7.1 Dispatch Table 入口

```
Entry 55: outer_tag=0x7e, inner_CID=0x44, filter=0x00
```

通过标准路径调用:
```c
FUN_1400cdc4c(adapter, 0x7e, 0xed, 0);
// → dispatch lookup(0x7e) → entry 55 → handler 0x140147330
```

### 7.2 调用者搜索结果

在已导出的所有 59 个函数中，**没有找到直接调用 MLO_CTRL/PerfInd 的代码**。
这意味着调用者在 connect 流程之外，可能是:

1. **周期性定时器回调** — 类似 Linux 的 `ieee80211_sta_stats_timer`
2. **RX 统计处理路径** — 当 RX 性能数据更新时上报
3. **连接建立后的运行时路径** — 不在初始 connect 序列中

### 7.3 与 WdiTaskConnect 的关系

WdiTaskConnect 中的 `MOV EDX, 0x7e` (地址 0x140066571) 是 WPP trace message ID,
**不是 dispatch 调用**。完整上下文:
```asm
MOV EDX, 0x7e           ; WPP trace event ID
LEA R9, [0x140214f90]   ; format string
LEA R8, [0x140230730]   ; module string
CALL 0x140012e48        ; WPP trace function
```

---

## 八、与 AUR 驱动的对比

### 8.1 AUR 驱动中的 MLO 命令

AUR 驱动 (`/usr/src/mediatek-mt7927-2.1/mt76/`) 中:
- **STA_REC_MLD (0xE0)**: 存在 ✅
- **EHT_MLD (0xFB)**: 存在 ✅
- **BSS_MLD (0x1A)**: 存在 ✅
- **UNI_CMD_PERF_IND (CID=0x44)**: **完全不存在** ❌

搜索 AUR 源码:
```bash
grep -r "0x44" /usr/src/mediatek-mt7927-2.1/mt76/mt7925/mcu.*
# 无 CID=0x44 相关的 MCU 命令
```

### 8.2 影响评估

**对 MLO 数据面不工作的影响: 极低**

原因:
1. `nicUniCmdPerfInd` 是性能指标上报，不是 MLO 激活命令
2. 输入包含 bss_idx + 64 字节性能数据，这是运行时统计
3. 硬编码的 `num_entries=4` 可能对应 4 个性能指标类别
4. 缺少此命令不会阻止 MLO 链路建立
5. MLO 不工作的真正原因仍然是 `MT792x_CHIP_CAP_MLO_EN` 门控和 band_idx 映射

---

## 九、相关函数分析

### 9.1 BssActivateCtrl MLO 路径 (0x140143540)

BssActivateCtrl 的 MLD mode==3 分支已在 `win_re_connect_flow_complete.md` 中详细分析。
关键点:
- 当 `activate=1` (byte[1]!=0) 时, 额外发送 MLD TLV (tag=0x1A, len=0x14)
- MLD TLV 由 `nicUniCmdBssInfoMld` (0x14014fad0) 构建
- 检查 `adapter->mld_mode == 3` 来决定是否填充 MLD 字段

### 9.2 STA_REC dispatch MLO 分支 (0x1401446d0)

STA_REC 的 13 TLV dispatch 中包含:
- **MLD_SETUP (tag=0x20)**: 设置 MLD 链路映射，最多遍历 N 个链路
- **EHT_MLD (tag=0x21)**: EHT MLD 能力信息
- 这两个 TLV 只在 `mld_mode == 3` 时被填充

### 9.3 EHT_MLD builder (0x14014e2a0)

```c
// 检查 sta_entry->mld_mode:
if (mld_mode == 3) {
    // MLD 活跃: 填充 MLD 信息
    data = { tag=0x21, len=0x10, ... }
    data[4] = mld_entry[0xf];    // link_bitmap
    data[5] = mld_entry[0x10];   // primary_link_id
    memcpy(data+7, mld_entry+0x11, 3);  // 3 字节
    memcpy(data+10, mld_entry+0x14, 2); // 2 字节 (mld_id)
} else if (mld_mode == 0) {
    // Non-MLD: 跳过
} else {
    // 其他模式: 跳过
}
```

### 9.4 MLD_SETUP builder (0x14014ddc0)

```c
// 检查 mld_mode:
if (mld_mode == 3) {
    data[0] = 0x20;  // tag
    data[1] = (num_links + 5) * 4;  // len = 动态计算
    data[2..5] = mld_entry[3..6];   // MLD group info
    data[8..9] = link IDs

    // 遍历每个链路:
    for (i = 0; i < num_links; i++) {
        link_entry = find_link_entry(adapter, i);
        // 填充: band_idx, wlan_idx
        per_link_data[0] = link_entry->band_idx;
        per_link_data[1] = link_entry->sta_wlan_idx;
    }
}
```

---

## 十、结论与建议

### 10.1 MLO_CTRL 的真实身份

| 属性 | 值 |
|------|-----|
| 函数名 | `nicUniCmdPerfInd` |
| 外部标签 | 0x7e |
| 内部 CID | 0x44 |
| 功能 | 性能指标上报 (Performance Indicator) |
| 调用时机 | 连接后运行时 (非 connect 序列) |
| Payload 大小 | 0x54 (84 字节) |
| 核心数据 | bss_idx + 64 字节性能数据 |

### 10.2 对 Linux 驱动的实现建议

**优先级: 低** — 此命令不影响 MLO 链路建立或数据面功能。

如果未来需要实现 (用于性能优化):
```c
// CID = 0x44, 子 TLV tag = 0x0002
struct uni_cmd_perf_ind {
    __le16 tag;         // 0x0002
    __le16 len;         // 0x0050 (80)
    u8     bss_idx;
    u8     rsv;
    __le16 field_1;     // 用途待确认
    __le32 field_2;     // 用途待确认
    u8     num_entries; // = 4
    u8     rsv2[3];
    u8     perf_data[64]; // 性能统计数据
} __packed;

// 发送方式:
mt76_mcu_send_msg(dev, MCU_UNI_CMD(0x44), &cmd, sizeof(cmd), false);
```

### 10.3 MLO 不工作的根本原因不在此

MLO 数据面不工作的真正原因仍然是:
1. **`MT792x_CHIP_CAP_MLO_EN` 门控** — 固件能力标志未被设置/检查
2. **band_idx 映射问题** — MT6639 需要显式 band_idx, 不接受 0xFF
3. **marcin-fm 的 STR MLO 补丁未发布** — 核心 MLO 激活逻辑尚缺

---

*逆向来源: mtkwecx.sys v5705275, Ghidra headless 反编译 + 汇编验证, 2026-03-04*
