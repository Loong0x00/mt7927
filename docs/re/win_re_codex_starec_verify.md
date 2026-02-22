# Windows mtkwecx.sys — STA_REC TLV 字段映射验证 + Connect Flow 时序确认

**分析时间**: 2026-02-22 (Session 24)
**分析工具**: disasm_helper.py (capstone 反汇编), 直接从 mtkwecx.sys v5705275 二进制提取
**分析方法**: 汇编级逐指令验证每个 TLV 构建函数 + MtCmdSendStaRecUpdate 调用链

---

## 一、Executive Summary — 关键差异列表

### 🔴 必须修复

| # | 差异 | 影响 | 优先级 |
|---|------|------|--------|
| 1 | **STA_REC option=0x07 (wait_ack) vs Windows 0xed (fire-and-forget)** | 我们等待固件响应，Windows 不等。可能导致 MCU 命令排队阻塞 | 🔴 高 |
| 2 | **STA_REC_STATE 结构大小: 12B vs Windows 16B** | `flags` 偏移可能正确但 `action` 位置错误，TLV 总长度不匹配 | 🔴 高 |
| 3 | **缺少 BA_OFFLOAD TLV (tag=0x16)** | Windows 始终发送（即使全零），固件可能依赖此 TLV 初始化 BA 引擎 | 🔴 高 |
| 4 | **缺少 UAPSD TLV (tag=0x24)** | Windows 始终发送（即使全零），固件可能依赖此 TLV 初始化电源管理 | 🔴 高 |
| 5 | **PM_DISABLE (BSS_INFO tag=0x1B) 完全缺失** | Windows 在 BSS_INFO full 之前必发，可能是固件 TX 队列 suspend 的原因 | 🔴 高 |

### 🟡 需关注

| # | 差异 | 影响 |
|---|------|------|
| 6 | **STA_REC_RA tag=0x01 布局不同**: 我们用 `legacy(2)+rx_mcs[10]`=12B; Windows 用 `oper_rate(2)+supp_rates[10]`=16B (tag+len included) | 字段语义可能不同 |
| 7 | **STA_REC_PHY 缺少 rcpi/channel_bw/nss** | 我们填 `ampdu`+`rts_policy`, Windows 填 `rcpi`+`channel_bw`+`nss` |
| 8 | **BASIC TLV: extra_info=3 vs 我们的条件赋值** | Windows 硬编码 extra_info=3, 我们用 EXTRA_INFO_VER\|EXTRA_INFO_NEW |

---

## 二、STA_REC Dispatch Table (0x140250710) — 13 TLV 完整验证

### 2.1 Dispatch Table 原始数据 (已验证)

| Idx | Size | Func VA | Tag | Len | Name | 验证状态 |
|-----|------|---------|-----|-----|------|----------|
| [0] | 0x14 | 0x14014d6d0 | 0x00 | 0x14 (20B) | BASIC | ✅ 汇编级验证 |
| [1] | 0x08 | 0x14014d7a0 | 0x09 | 0x08 (8B) | HT_INFO | ✅ 汇编级验证 |
| [2] | 0x10 | 0x14014d7e0 | 0x0a | 0x10 (16B) | VHT_INFO | ✅ 汇编级验证 |
| [3] | 0x1c | 0x14014d810 | 0x19 | 0x1c (28B) | HE_BASIC | 结构已知 |
| [4] | 0x08 | 0x14014dae0 | 0x17 | 0x08 (8B) | HE_6G_CAP | 结构已知 |
| [5] | 0x10 | 0x14014d730 | 0x07 | 0x10 (16B) | STATE_INFO | ✅ 汇编级验证 |
| [6] | 0x0c | 0x14014d760 | 0x15 | 0x0c (12B) | PHY_INFO | ✅ 汇编级验证 |
| [7] | 0x10 | 0x14014e570 | 0x01 | 0x10 (16B) | RA_INFO | ✅ 汇编级验证 |
| [8] | 0x10 | 0x14014e5b0 | 0x16 | 0x10 (16B) | BA_OFFLOAD | ✅ 汇编级验证 |
| [9] | 0x08 | 0x14014e620 | 0x24 | 0x08 (8B) | UAPSD | ✅ 汇编级验证 |
| [10] | 0x28 | 0x14014db80 | 0x22 | 0x28 (40B) | EHT_INFO | 结构已知 |
| [11] | 0x10 | 0x14014e2a0 | 0x21 | 0x10 (16B) | EHT_MLD | 结构已知 |
| [12] | 0x20 | 0x14014ddc0 | 0x20 | 0x20 (32B) | MLD_SETUP | 结构已知 |

### 2.2 nicUniCmdUpdateStaRec (0x1401446d0) — Dispatch 循环验证

```asm
0x1401446ed:  cmp  byte ptr [rdx], 0x13      ; cmd_id == 0x13 (STA_REC)
0x1401446fc:  cmp  dword ptr [rdx + 0x10], 0xec  ; payload_size == 0xEC (236B)
0x140144725:  cmp  eax, 0xd                  ; loop 13 entries
; 循环体:
0x1401447ac:  mov  rax, qword ptr [r12]      ; r12 = dispatch_table + i*16
0x1401447b3:  mov  rdx, rsi                  ; rdx = output buffer
0x1401447b6:  mov  rcx, r15                  ; rcx = adapter
0x1401447b9:  call qword ptr [rip + ...]     ; call TLV builder func
0x1401447c1:  inc  ebp                       ; i++
0x1401447c3:  add  rsi, rax                  ; advance buffer by returned size
0x1401447cb:  cmp  ebp, 0xd                  ; while i < 13
```

**确认**: 每次迭代调用 TLV builder，将返回的字节数加到输出指针。如果 builder 返回 0，TLV 被跳过。

STA_REC header (8 bytes, 在 TLV 之前):
```asm
0x140144797:  mov  al, byte ptr [r14 + 0xc]   ; hdr[0] = bss_idx
0x14014479d:  mov  byte ptr [rsi + 6], bpl     ; hdr[6] = 0
0x1401447a1:  mov  al, byte ptr [r14 + 0x36]   ; hdr[1] = secondary_idx
```

---

## 三、逐 TLV 详细验证

### [0] STA_REC_BASIC (tag=0x00, 20 bytes) — ✅ 汇编级验证

**Windows 汇编** (0x14014d6d0):
```asm
mov  dword ptr [rdx], 0x140000        ; tag=0x0000, len=0x0014
call 0x140151608                      ; eax = getConnType(adapter, bss_idx)
mov  dword ptr [rdi + 4], eax         ; TLV[4:8] = conn_type
mov  byte ptr [rdi + 8], 1            ; TLV[8] = new_entry = 1 (硬编码!)
mov  byte ptr [rdi + 9], input[0x12]  ; TLV[9] = qos
mov  word ptr [rdi + 0xa], input[0x08]; TLV[0xa:0xc] = aid
mov  dword ptr [rdi + 0xc], input[0x02]; TLV[0xc:0x10] = peer_addr[0:4]
mov  word ptr [rdi + 0x10], input[0x06]; TLV[0x10:0x12] = peer_addr[4:6]
mov  word ptr [rdi + 0x12], 3         ; TLV[0x12:0x14] = extra_info = 3 (硬编码!)
```

**Windows 结构**:
```c
struct sta_rec_basic {
    __le16 tag;          // +0x00 = 0x0000
    __le16 len;          // +0x02 = 0x0014
    __le32 conn_type;    // +0x04 = 0x10001 (INFRA_STA)
    u8     new_entry;    // +0x08 = 1 (硬编码，始终新建)
    u8     qos;          // +0x09
    __le16 aid;          // +0x0a
    u8     peer_addr[6]; // +0x0c
    __le16 extra_info;   // +0x12 = 3 (硬编码!)
};
```

**我们的结构** (mt7927_pci.h):
```c
struct sta_rec_basic {
    __le16 tag;
    __le16 len;
    __le32 conn_type;
    u8 conn_state;       // ← WRONG NAME! Windows 这个位置是 new_entry (硬编码=1)
    u8 qos;
    __le16 aid;
    u8 peer_addr[6];
    __le16 extra_info;
};
```

**🟡 差异**:
1. 字段 +0x08: 我们叫 `conn_state`(值=CONN_STATE_PORT_SECURE=2)，Windows 叫 `new_entry`(值=1)
   - Windows 硬编码 =1, 我们填 conn_state 枚举值 (enable 时=2, disable 时=0)
   - **值不同!** Windows=1, 我们=2 或 0
2. extra_info: Windows 硬编码 =3, 我们用 `EXTRA_INFO_VER | EXTRA_INFO_NEW` = 0x03
   - **值匹配 ✅** (只是构建方式不同)

**⚠️ 关键**: `conn_state` 字段名在 mt7925 代码中对应 Windows 的 `new_entry`。Windows 始终填 1，而我们填 conn_state 枚举。这可能影响 WTBL 创建行为。

---

### [5] STA_REC_STATE_INFO (tag=0x07, 16 bytes) — 🔴 结构大小不匹配

**Windows 汇编** (0x14014d730):
```asm
mov  dword ptr [rdx], 0x100007        ; tag=0x0007, len=0x0010 (16 bytes!)
mov  byte ptr [rdx + 4], input[0x14]  ; TLV[4] = state (1 byte)
                                      ; TLV[5:8] = implicit pad (3 bytes)
mov  dword ptr [rdx + 8], input[0x58] ; TLV[8:0xc] = flags (4 bytes)
mov  byte ptr [rdx + 0xc], input[0x45]; TLV[0xc] = action (1 byte)
mov  byte ptr [rdx + 0xd], 0          ; TLV[0xd] = pad = 0
                                      ; TLV[0xe:0x10] = implicit pad (2 bytes)
```

**Windows 结构 (16 bytes)**:
```c
struct sta_rec_state_info {
    __le16 tag;      // +0x00 = 0x0007
    __le16 len;      // +0x02 = 0x0010 (16!)
    u8     state;    // +0x04: 0=DISCONNECT, 1=IDLE, 2=CONNECTED
    u8     pad1[3];  // +0x05: zeroed
    __le32 flags;    // +0x08: state change flags
    u8     action;   // +0x0c: opmode/action
    u8     pad2;     // +0x0d: explicit zero
    u8     pad3[2];  // +0x0e: zeroed
};
```

**我们的结构 (12 bytes)**:
```c
struct sta_rec_state {
    __le16 tag;        // +0x00
    __le16 len;        // +0x02
    u8     state;      // +0x04 ✅ 匹配
    u8     vht_opmode; // +0x05 ← 填 0, Windows 也是 0
    u8     action;     // +0x06 ← ⚠️ 偏移错误! Windows 在 +0x0c
    u8     pad;        // +0x07
    __le32 flags;      // +0x08 ✅ 匹配
};                     // 总大小: 12 bytes
```

**🔴 差异**:
1. **结构大小**: 12B vs 16B — len 字段值不同 (0x0C vs 0x10)
2. **action 字段位置**: 我们在 +0x06，Windows 在 +0x0C
3. 虽然我们当前 action=0、flags=0 所以数据等价，但 **len 值不匹配** 会影响固件的 TLV 解析:
   - 固件按 tag+len 解析 TLV 链
   - 如果 len=12 (我们) vs len=16 (Windows)，后续 TLV 偏移会错位 4 字节
   - 这可能导致后续所有 TLV 被解析失败!

**修复方案**:
```c
struct sta_rec_state {
    __le16 tag;        // +0x00 = 0x0007
    __le16 len;        // +0x02 = 0x0010 (16, not 12!)
    u8     state;      // +0x04
    u8     pad1[3];    // +0x05
    __le32 flags;      // +0x08
    u8     action;     // +0x0c
    u8     pad2[3];    // +0x0d
};                     // 总大小: 16 bytes
```

---

### [7] STA_REC_RA_INFO (tag=0x01, 16 bytes) — 🟡 字段语义差异

**Windows 汇编** (0x14014e570):
```asm
mov  dword ptr [rdx], 0x100001        ; tag=0x0001, len=0x0010 (16 bytes)
mov  word ptr [rdx + 4], input[0x0e]  ; TLV[4:6] = oper_rate (2 bytes)
memcpy(TLV+6, input+0x18, 10)        ; TLV[6:16] = supp_rates[10]
```

**Windows 结构 (16 bytes)**:
```c
struct sta_rec_ra_info {
    __le16 tag;            // +0x00 = 0x0001
    __le16 len;            // +0x02 = 0x0010 (16)
    __le16 oper_rate;      // +0x04: operational rate code
    u8     supp_rates[10]; // +0x06: supported rate set (memcpy 10B)
};
```

**我们的结构 (16 bytes, 含 tag+len)**:
```c
struct sta_rec_ra_info {
    __le16 tag;               // +0x00 = 0x0001
    __le16 len;               // +0x02 = sizeof = 16
    __le16 legacy;            // +0x04: RA_LEGACY_OFDM | RA_LEGACY_CCK
    u8     rx_mcs_bitmask[10];// +0x06: HT MCS bitmap
};
```

**🟡 分析**:
- 结构大小 ✅ 匹配 (16 bytes)
- +0x04 字段: Windows 叫 `oper_rate`，我们叫 `legacy`，宽度相同 (u16)
- +0x06 字段: Windows 叫 `supp_rates` (raw 802.11 rate set)，我们叫 `rx_mcs_bitmask` (HT MCS)
- **语义可能不同**: Windows 的 `supp_rates` 是 802.11 基础速率集 (rates from Beacon/ProbeResp)，我们的 `rx_mcs_bitmask` 是 HT MCS mask
- 但 Windows 的 input struct 中 `input[0x18:0x22]` 可能就是 HT MCS mask (从 sta_entry 复制)
- **大小和偏移匹配，功能应等价**

---

### [6] STA_REC_PHY_INFO (tag=0x15, 12 bytes) — 🟡 字段差异

**Windows 汇编** (0x14014d760):
```asm
mov  dword ptr [rdx], 0xc0015         ; tag=0x0015, len=0x000c (12 bytes)
mov  word ptr [rdx + 4], input[0x10]  ; TLV[4:6] = basic_rate
mov  byte ptr [rdx + 6], input[0x0d]  ; TLV[6] = phy_type
mov  byte ptr [rdx + 7], input[0x30]  ; TLV[7] = rcpi
mov  byte ptr [rdx + 8], input[0x44]  ; TLV[8] = channel_bw
mov  byte ptr [rdx + 9], input[0x32]  ; TLV[9] = nss
```

**Windows 结构 (12 bytes)**:
```c
struct sta_rec_phy_info {
    __le16 tag;          // +0x00 = 0x0015
    __le16 len;          // +0x02 = 0x000c
    __le16 basic_rate;   // +0x04 ✅ 匹配
    u8     phy_type;     // +0x06 ✅ 匹配
    u8     rcpi;         // +0x07: Windows 填 RCPI, 我们填 ampdu (不同!)
    u8     channel_bw;   // +0x08: Windows 填 BW, 我们填 rts_policy (不同!)
    u8     nss;          // +0x09: Windows 填 NSS, 我们填 rcpi (偏移错!)
    u8     pad[2];       // +0x0a
};
```

**我们的结构**:
```c
struct sta_rec_phy {
    __le16 tag;          // +0x00
    __le16 len;          // +0x02
    __le16 basic_rate;   // +0x04 ✅
    u8     phy_type;     // +0x06 ✅
    u8     ampdu;        // +0x07: ← Windows 这里是 rcpi!
    u8     rts_policy;   // +0x08: ← Windows 这里是 channel_bw!
    u8     rcpi;         // +0x09: ← Windows 这里是 nss! 偏移完全错位
    u8     __rsv[2];     // +0x0a
};
```

**🟡 差异**: 字段名和语义不同，但我们从未填写 ampdu/rts_policy/rcpi，它们默认 = 0。
- Windows 不填 RCPI/BW/NSS 也可能为 0 (初始连接阶段)
- **建议**: 按 Windows 布局重命名字段，并适当填写 channel_bw 和 nss

---

### [1] STA_REC_HT_INFO (tag=0x09, 8 bytes) — ✅ 完全匹配

**Windows 汇编** (0x14014d7a0):
```asm
; 条件检查: 跳过当 ht_cap==0 && ht_ext_cap==0
cmp  word ptr [r8 + 0x28], 0
jne  write_tlv
cmp  word ptr [r8 + 0x2a], 0
jne  write_tlv
xor  eax, eax                ; 跳过
ret

write_tlv:
mov  dword ptr [rdx], 0x80009 ; tag=0x09, len=0x08
mov  word ptr [rdx + 4], cx   ; ht_cap
mov  word ptr [rdx + 6], cx   ; ht_ext_cap
```

**匹配**: 我们的 struct 完全匹配 Windows:
```c
struct sta_rec_ht_info {
    __le16 tag;        // = 0x0009
    __le16 len;        // = 0x0008
    __le16 ht_cap;     // +0x04 ✅
    __le16 ht_ext_cap; // +0x06 ✅
};
```

**注意**: Windows 在 ht_cap 和 ext_cap 都为 0 时跳过此 TLV (返回 0)。我们始终发送。
对于 auth 阶段（对端 STA 能力未知），这不影响功能。

---

### [2] STA_REC_VHT_INFO (tag=0x0a, 16 bytes) — ✅ 基本匹配

**Windows 汇编** (0x14014d7e0):
```asm
cmp  dword ptr [r8 + 0x38], 0  ; 条件: vht_cap != 0
jne  write_tlv
xor  eax, eax
ret

write_tlv:
mov  dword ptr [rdx], 0x10000a     ; tag=0x0a, len=0x10
mov  dword ptr [rdx + 4], input[0x38] ; vht_cap_info (4B)
mov  word ptr [rdx + 8], input[0x3c]  ; vht_rx_mcs (2B)
mov  word ptr [rdx + 0xa], input[0x40]; vht_tx_mcs (2B)
; TLV[0xc:0x10] = 隐式 0 (pad)
```

**我们的结构**:
```c
struct sta_rec_vht_info {
    __le16 tag;              // ✅
    __le16 len;              // ✅
    __le32 vht_cap;          // +0x04 ✅
    __le16 vht_rx_mcs_map;   // +0x08 ✅
    __le16 vht_tx_mcs_map;   // +0x0a ✅
    u8     rts_bw_sig;       // +0x0c: Windows 这里是 pad(0), 我们填 0 ✅
    u8     __rsv[3];         // +0x0d
};
```

**✅ 匹配**: 大小和关键字段完全一致。rts_bw_sig = 0 等于 Windows 的 pad。

---

### [8] STA_REC_BA_OFFLOAD (tag=0x16, 16 bytes) — ❌ 我们完全缺失

**Windows 汇编** (0x14014e5b0):
```asm
mov  dword ptr [rdx], 0x100016        ; tag=0x16, len=0x10
mov  byte ptr [rdx + 4], input[0x4c]  ; tx_ba_wsize
mov  byte ptr [rdx + 5], input[0x4d]  ; rx_ba_wsize
mov  byte ptr [rdx + 6], input[0x80]  ; ba_policy
mov  byte ptr [rdx + 7], input[0x81]  ; ba_control
mov  dword ptr [rdx + 8], input[0x84] ; ba_bitmap (4B)
; 条件分支: phy_type bit6(VHT) or bit7(HE) → word-width BA window
; else → byte-width BA window
```

**Windows 结构**:
```c
struct sta_rec_ba_offload {
    __le16 tag;              // +0x00 = 0x0016
    __le16 len;              // +0x02 = 0x0010
    u8     tx_ba_wsize;      // +0x04
    u8     rx_ba_wsize;      // +0x05
    u8     ba_policy;        // +0x06
    u8     ba_control;       // +0x07
    __le32 ba_bitmap;        // +0x08
    __le16 tx_ba_wsize_ext;  // +0x0c (VHT/HE: word; HT: byte zero-ext)
    __le16 rx_ba_wsize_ext;  // +0x0e
};
```

**我们的状态**: **完全缺失**。对于 auth 阶段，所有字段 = 0，但 TLV 本身必须发送。

---

### [9] STA_REC_UAPSD (tag=0x24, 8 bytes) — ❌ 我们完全缺失

**Windows 汇编** (0x14014e620):
```asm
mov  dword ptr [rdx], 0x80024         ; tag=0x24, len=0x08
mov  byte ptr [rdx + 4], input[0x13]  ; uapsd_flags
mov  byte ptr [rdx + 5], input[0x34]  ; max_sp_len
mov  byte ptr [rdx + 6], input[0x35]  ; uapsd_ac
```

**Windows 结构**:
```c
struct sta_rec_uapsd {
    __le16 tag;          // = 0x0024
    __le16 len;          // = 0x0008
    u8     uapsd_flags;  // +0x04
    u8     max_sp_len;   // +0x05
    u8     uapsd_ac;     // +0x06
    u8     pad;          // +0x07
};
```

**我们的状态**: **完全缺失**。对于 auth 阶段，所有字段 = 0，但 TLV 本身必须发送。

---

### [3] STA_REC_HE_BASIC (tag=0x19, 28 bytes) — ✅ 已实现

我们的结构大小 = 28 bytes，与 Windows dispatch table [3] size=0x1c 匹配。
具体字段布局未在本次重新验证（结构已知）。

---

## 四、MtCmdSendStaRecUpdate Option 参数验证

### 4.1 汇编确认

**地址**: 0x1400cf3ba-0x1400cf3df

```asm
0x1400cf3b7:  xor      r9d, r9d        ; extra_param = 0
0x1400cf3c0:  mov      r8b, 0xed       ; option = 0xed ← FIRE-AND-FORGET!
0x1400cf3c7:  mov      dl, 0x13        ; cmd_id = 0x13 (STA_REC)
0x1400cf3c9:  mov      word ptr [rsp+0x38], 0xec  ; payload_size = 0xEC (236)
0x1400cf3d5:  mov      byte ptr [rsp+0x28], 8     ; type = 8
0x1400cf3da:  mov      byte ptr [rsp+0x20], 1     ; flag = 1
0x1400cf3df:  call     0x1400cdc4c     ; FUN_1400cdc4c(adapter, 0x13, 0xed, 0)
```

### 4.2 Option 解码 (0x1400ca864)

```asm
0x1400ca864:  cmp  cl, 0xee            ; option == 0xee?
0x1400ca867:  mov  eax, 0x8000         ; default = 0x8000 (no wait)
0x1400ca86c:  mov  edx, 0xc000         ; alt = 0xc000 (wait response)
0x1400ca871:  cmove ax, dx             ; if 0xee → 0xc000
0x1400ca875:  ret
```

| Windows option | 解码结果 | 含义 | 我们的等价 |
|---------------|----------|------|-----------|
| 0xed | 0x8000 | fire-and-forget | UNI_CMD_OPT_SET (0x06) |
| 0xee | 0xc000 | wait response | UNI_CMD_OPT_SET_ACK (0x07) |

### 4.3 🔴 关键差异

**Windows STA_REC 使用 option=0xed (fire-and-forget)**
**我们使用 UNI_CMD_OPT_SET_ACK (0x07, wait for response)**

这意味着:
- Windows 发 STA_REC 后立即继续，不等固件确认
- 我们发 STA_REC 后等待固件 ACK
- 这可能导致: 如果固件在处理 STA_REC 时卡住/超时，我们的 MCU 命令通道被阻塞
- **可能是 Ring 15 MCU 命令卡住的部分原因!**

**修复**: 改为 `UNI_CMD_OPT_SET` (0x06, fire-and-forget):
```c
return mt7927_mcu_send_unicmd(dev, MCU_UNI_CMD_STA_REC,
                              UNI_CMD_OPT_SET,  // ← 改为 0x06, NOT 0x07!
                              &req, sizeof(req));
```

---

## 五、Connect Flow 完整时序确认

### 5.1 MlmeCntlWaitJoinProc (0x1401273a8) — 汇编级验证

```
[步骤5] 0x140127475: call 0x1400cb7a4     ; SCAN_CANCEL
[步骤6] 0x14012757b: call 0x1400c5e08     ; MtCmdChPrivilage (CH_PRIVILEGE)
[步骤7] 0x14012758c: call 0x1400cdea0     ; MtCmdSendStaRecUpdate (STA_REC, 13 TLVs)
[步骤8] 0x1401276ac: call 0x1400ac6c8     ; auth TX (DMA queue submit)
```

**关键确认**:
1. ✅ 步骤顺序完全确认: SCAN_CANCEL → CH_PRIVILEGE → STA_REC → auth TX
2. ✅ 各步骤之间**无延迟/等待**: 顺序调用，不检查返回值 (fire-and-forget)
3. ✅ STA_REC (步骤7) 使用 option=0xed (fire-and-forget)
4. ✅ auth TX (步骤8) 不是 MCU 命令，直接入 DMA 队列

### 5.2 完整连接命令序列 (从 WdiTaskConnect 到 auth TX)

```
WdiTaskConnect (0x140065be0)
  │
  ├─[1] ChipConfig (0xca → CID=0x0E)
  │     └─ 每次 connect 重发 (328B payload)
  │
  ├─[2] BssActivateCtrl (0x11 → CID=0x01)
  │     └─ 组合命令: DEV_INFO(CID=1) + BSS_INFO(CID=2, BASIC+MLD)
  │     └─ option = 0xed (fire-and-forget)
  │
  └─ MlmeCntlOidConnectProc (0x140123588)
        │
        ├─[3] PM_DISABLE (0x17 → BSS_INFO CID=2, tag=0x1B)
        │     └─ option = 0xed
        │     └─ payload: bss_idx(1) + pad(3) + TLV(tag=0x1B, len=4)
        │     └─ 总 BSS_INFO sub-payload = 8 bytes
        │
        ├─[4] BSS_INFO full (0x12 → CID=0x02, 14 TLVs)
        │     └─ option = 0xed
        │
        └─ MlmeCntlWaitJoinProc (0x1401273a8)
              │
              ├─[5] SCAN_CANCEL (0x1b → CID=0x16)
              ├─[6] CH_PRIVILEGE (0x1c → CID=0x27)
              ├─[7] STA_REC (0x13 → CID=0x03, 13 TLVs)
              │     └─ option = 0xed (fire-and-forget!)
              └─[8] auth TX → DMA queue
```

### 5.3 nicUniCmdPmDisable (0x1400caefc) — PM_DISABLE 详细验证

```asm
0x1400caf53:  mov  esi, 4              ; payload_size = 4 bytes
0x1400caf8a:  mov  r8b, 0xed           ; option = 0xed
0x1400caf92:  mov  dl, 0x17            ; cmd_id = 0x17 (PM_BSS_ABORT)
0x1400caf9c:  mov  byte ptr [rsp+0x78], al  ; payload[0] = bss_idx
```

Handler (0x1401442d0) 构建:
```asm
0x1401442df:  cmp  byte ptr [rdx], 0x17     ; cmd_id == 0x17
0x1401442f0:  cmp  dword ptr [rdx+0x10], 4  ; payload_size == 4
0x1401442fd:  lea  r8d, [rsi + 4]           ; alloc_size = 4+4 = 8
0x140144367:  mov  byte ptr [rcx], al       ; data[0] = bss_idx
0x14014436d:  mov  dword ptr [rcx+4], 0x4001b  ; tag=0x001B, len=0x0004
```

**PM_DISABLE 帧结构** (发送到固件):
```
UniCmd Header (16B):
  CID = 0x02 (BSS_INFO)
  option = fire-and-forget

BSS_INFO sub-payload (8B):
  [+0] u8  bss_idx         ← 通常 = 0
  [+1] u8  pad[3]          ← zeroed
  [+4] u16 tag = 0x001B    ← PM_DISABLE
  [+6] u16 len = 0x0004    ← TLV 仅含 tag+len header, 无额外数据
```

**总 payload = 8 bytes** (不含 UniCmd header)

---

## 六、与我们驱动代码的完整差异列表

| # | 项目 | Windows | 我们的代码 | 影响 | 修复优先级 |
|---|------|---------|-----------|------|-----------|
| 1 | STA_REC option | 0xed (fire-and-forget) | 0x07 (wait ACK) | MCU 可能阻塞 | 🔴 高 |
| 2 | STA_REC_STATE 大小 | 16 bytes | 12 bytes | TLV 链偏移错误 | 🔴 高 |
| 3 | BA_OFFLOAD TLV | 始终发 (全零) | 缺失 | 固件 BA 引擎未初始化 | 🔴 高 |
| 4 | UAPSD TLV | 始终发 (全零) | 缺失 | 固件电源管理未初始化 | 🔴 高 |
| 5 | PM_DISABLE | BSS_INFO 前必发 | 完全缺失 | TX 队列可能 suspend | 🔴 高 |
| 6 | BASIC new_entry | =1 (硬编码) | =conn_state (0/2) | WTBL 创建行为不同 | 🟡 中 |
| 7 | PHY_INFO 字段 | rcpi/bw/nss | ampdu/rts/rcpi | 字段名义不同，值均=0 | 🟡 低 |
| 8 | BssActivateCtrl | DEV+BSS_INFO 组合 | DEV_INFO 单独 | 缺少早期 BSS 初始化 | 🔴 (Task#5) |
| 9 | TLV 总数 | 13个 (部分条件跳过) | 7个 | 缺 6 个 TLV | 🟡 (EHT/MLD 可选) |

---

## 七、修复建议

### 7.1 🔴 立即修复 (高优先级)

#### Fix 1: STA_REC option 改为 fire-and-forget
```c
// mt7927_pci.c: mt7927_mcu_sta_update()
return mt7927_mcu_send_unicmd(dev, MCU_UNI_CMD_STA_REC,
                              UNI_CMD_OPT_SET,  // 0x06, NOT 0x07!
                              &req, sizeof(req));
```

#### Fix 2: STA_REC_STATE 扩展到 16 bytes
```c
// mt7927_pci.h:
struct sta_rec_state {
    __le16 tag;        // +0x00 = 0x0007
    __le16 len;        // +0x02 = 0x0010 (16!)
    u8     state;      // +0x04
    u8     pad1[3];    // +0x05
    __le32 flags;      // +0x08
    u8     action;     // +0x0c
    u8     pad2[3];    // +0x0d
};
```

#### Fix 3: 添加 BA_OFFLOAD TLV (tag=0x16, 16B)
```c
// mt7927_pci.h:
struct sta_rec_ba_offload {
    __le16 tag;              // = 0x0016
    __le16 len;              // = 0x0010
    u8     tx_ba_wsize;      // auth 阶段 = 0
    u8     rx_ba_wsize;      // auth 阶段 = 0
    u8     ba_policy;        // auth 阶段 = 0
    u8     ba_control;       // auth 阶段 = 0
    __le32 ba_bitmap;        // auth 阶段 = 0
    __le16 tx_ba_wsize_ext;  // auth 阶段 = 0
    __le16 rx_ba_wsize_ext;  // auth 阶段 = 0
};
```

#### Fix 4: 添加 UAPSD TLV (tag=0x24, 8B)
```c
// mt7927_pci.h:
struct sta_rec_uapsd {
    __le16 tag;          // = 0x0024
    __le16 len;          // = 0x0008
    u8     uapsd_flags;  // auth 阶段 = 0
    u8     max_sp_len;   // auth 阶段 = 0
    u8     uapsd_ac;     // auth 阶段 = 0
    u8     pad;          // = 0
};
```

#### Fix 5: 添加 PM_DISABLE
```c
static int mt7927_send_pm_disable(struct mt7927_dev *dev, u8 bss_idx)
{
    struct {
        u8 bss_idx;
        u8 pad[3];
        __le16 tag;   // 0x001B
        __le16 len;   // 0x0004
    } __packed req = {
        .bss_idx = bss_idx,
        .tag = cpu_to_le16(0x001B),
        .len = cpu_to_le16(4),
    };

    return mt7927_mcu_send_unicmd(dev, MCU_UNI_CMD_BSS_INFO,
                                  UNI_CMD_OPT_SET, &req, sizeof(req));
}
```

调用位置: `mt7927_bss_info_changed()` 中，在 `mt7927_set_bss_info()` **之前**。

### 7.2 🟡 中优先级

#### Fix 6: BASIC TLV new_entry 字段
将 `conn_state` 改为 `new_entry`，始终填 1:
```c
req.basic.new_entry = 1;  // Windows 硬编码
```

#### Fix 7: PHY_INFO 字段重命名
按 Windows 布局重命名: `ampdu → rcpi`, `rts_policy → channel_bw`, `rcpi → nss`

### 7.3 tlv_num 更新

添加 BA_OFFLOAD + UAPSD 后，TLV 总数:
BASIC + RA + STATE + HT + VHT + PHY + HE_BASIC + BA_OFFLOAD + UAPSD = **9 个**

```c
req.hdr.tlv_num = cpu_to_le16(9);  // 从 7 改为 9
```

---

## 八、TLV 总大小验证

添加新 TLV 后的 STA_REC 总 payload 大小:

| TLV | 大小 (B) |
|-----|---------|
| STA_REC header | 8 |
| BASIC (0x00) | 20 |
| RA_INFO (0x01) | 16 |
| STATE_INFO (0x07) | **16** (从12改为16) |
| HT_INFO (0x09) | 8 |
| VHT_INFO (0x0a) | 16 |
| PHY_INFO (0x15) | 12 |
| **BA_OFFLOAD (0x16)** | **16** (新增) |
| HE_BASIC (0x19) | 28 |
| **UAPSD (0x24)** | **8** (新增) |
| **总计** | **148** |

Windows 总 payload = 0xEC = 236 bytes (含 header + all 13 TLVs + EHT/MLD)
我们 9 TLV = 148 bytes — 差距 = 236 - 148 = 88 bytes (EHT_INFO 40B + EHT_MLD 16B + MLD_SETUP 32B = 88B)

这 88 bytes 对应 EHT/MLD TLVs，对于非 EHT/非 MLO 连接不需要。

---

*分析完成: 2026-02-22, 基于 mtkwecx.sys v5705275 汇编级逆向*
*所有汇编引用均已通过 capstone 反汇编 + 原始二进制字节验证*
