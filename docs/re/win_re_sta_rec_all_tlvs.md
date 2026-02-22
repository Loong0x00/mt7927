# Windows mtkwecx.sys STA_REC 全部 TLV 构建函数反汇编分析

**分析时间**: 2026-02-22 (Session 24)
**分析工具**: disasm_helper.py (capstone 反汇编)
**驱动版本**: mtkwecx.sys v5.7.0.5275
**分析方法**: 直接读取 STA_REC dispatch table 0x140250710，逐函数反汇编

---

## Dispatch Table 概览 (0x140250710)

13 个 TLV 构建函数，每条目 16 字节: `[size(4), pad(4), func_ptr(8)]`

| 索引 | Size | 函数地址 | Tag | Len | 名称 | 状态 |
|------|------|----------|-----|-----|------|------|
| [0] | 0x14 | 0x14014d6d0 | 0x00 | 0x14 (20B) | **BASIC** | ✅ 本次分析 |
| [1] | 0x08 | 0x14014d7a0 | 0x09 | 0x08 (8B) | **HT_INFO** | ✅ 本次分析 |
| [2] | 0x10 | 0x14014d7e0 | 0x0a | 0x10 (16B) | **VHT_INFO** | ✅ 本次分析 |
| [3] | 0x1c | 0x14014d810 | 0x19 | 0x1c (28B) | HE_BASIC | 已知 |
| [4] | 0x08 | 0x14014dae0 | 0x17 | 0x08 (8B) | HE_6G_CAP | 已知 |
| [5] | 0x10 | 0x14014d730 | 0x07 | 0x10 (16B) | **STATE_INFO** | ✅ 本次分析 |
| [6] | 0x0c | 0x14014d760 | 0x15 | 0x0c (12B) | **PHY_INFO** | ✅ 本次分析 |
| [7] | 0x10 | 0x14014e570 | 0x01 | 0x10 (16B) | **RA_INFO** | ✅ 本次分析 |
| [8] | 0x10 | 0x14014e5b0 | 0x16 | 0x10 (16B) | **BA_OFFLOAD** | ✅ 本次分析 |
| [9] | 0x08 | 0x14014e620 | 0x24 | 0x08 (8B) | **UAPSD** | ✅ 本次分析 |
| [10] | 0x28 | 0x14014db80 | 0x22 | 0x28 (40B) | EHT_INFO | 已知 |
| [11] | 0x10 | 0x14014e2a0 | 0x21 | 0x10 (16B) | EHT_MLD | 已知 |
| [12] | 0x20 | 0x14014ddc0 | 0x20 | 0x20 (32B) | MLD_SETUP | 已知 |

**所有 TLV 函数签名**: `u16 func(adapter *param_1, u8 *param_2, u8 *param_3)`
- param_1: adapter 指针
- param_2: TLV 输出缓冲区
- param_3: 0xEC 字节 flat input struct (由 MtCmdSendStaRecUpdate 构建)
- 返回值: TLV 实际写入字节数 (0 = 跳过此 TLV)

---

## [0] STA_REC_BASIC — tag=0x00, len=0x14 (20 bytes)

**函数地址**: `0x14014d6d0`

### 反汇编
```asm
mov  dword ptr [rdx], 0x140000        ; tag=0x00, len=0x14
movzx edx, byte ptr [r8 + 1]          ; edx = input[1] (bss_idx)
call  0x140151608                      ; eax = getConnType(adapter, bss_idx)
mov  dword ptr [rdi + 4], eax          ; TLV[4:8] = conn_type
mov  byte ptr [rdi + 8], 1             ; TLV[8] = 1 (new_entry flag, 硬编码)
mov  al, byte ptr [rbx + 0x12]         ; al = input[0x12]
mov  byte ptr [rdi + 9], al            ; TLV[9] = input[0x12]
movzx eax, word ptr [rbx + 8]          ; eax = input[0x08:0x0a]
mov  word ptr [rdi + 0xa], ax          ; TLV[0xa:0xc] = input[0x08] (AID)
mov  eax, dword ptr [rbx + 2]          ; eax = input[0x02:0x06]
mov  dword ptr [rdi + 0xc], eax        ; TLV[0xc:0x10] = input[0x02] (peer MAC bytes 0-3)
movzx eax, word ptr [rbx + 6]          ; eax = input[0x06:0x08]
mov  word ptr [rdi + 0x10], ax         ; TLV[0x10:0x12] = input[0x06] (peer MAC bytes 4-5)
mov  word ptr [rdi + 0x12], 3          ; TLV[0x12:0x14] = 3 (extra_info, 硬编码)
```

### TLV 结构 (20 bytes)
```c
struct sta_rec_basic_tlv {        // 偏移 (含 tag+len header)
    __le16 tag;          // +0x00 = 0x0000
    __le16 len;          // +0x02 = 0x0014 (20)
    __le32 conn_type;    // +0x04 — getConnType(bss_idx) 返回值
    u8     new_entry;    // +0x08 = 1 (硬编码，始终创建新条目)
    u8     qos;          // +0x09 — input[0x12] (QoS capability)
    __le16 aid;          // +0x0a — input[0x08] (Association ID)
    u8     peer_addr[6]; // +0x0c — input[0x02:0x08] (对端 MAC 地址)
    __le16 extra_info;   // +0x12 = 3 (硬编码, 0x03 = EXTRA_MAN?)
};
```

### conn_type 映射 (辅助函数 0x140151608)
```
bss_idx → conn_type:
  0x21 → 0x10001  (INFRA STA, 默认值)
  0x41 → 0x10002  (INFRA P2P_GC)
  0x22 → 0x20001  (ADHOC/AP)
  0x42 → 0x20002  (P2P_GO)
  其他  → 0x10001  (默认 INFRA STA)

高16位: 网络类型 (0x01=Infra, 0x02=P2P/IBSS)
低16位: 角色 (0x01=AP/STA, 0x02=GC/GO)
```

### 关键发现
- **conn_type = 0x10001** 是 INFRA STA 连接的默认值
- **extra_info = 3** 始终硬编码
- **new_entry = 1** 始终硬编码
- peer MAC 地址存储在 input[0x02:0x08] (6 bytes)

---

## [1] STA_REC_HT_INFO — tag=0x09, len=0x08 (8 bytes)

**函数地址**: `0x14014d7a0`

### 反汇编
```asm
cmp  word ptr [r8 + 0x28], 0           ; 检查 input[0x28] (HT cap)
jne  write_tlv
cmp  word ptr [r8 + 0x2a], 0           ; 检查 input[0x2a] (HT ext cap)
jne  write_tlv
xor  eax, eax                          ; 两者都为 0 → 返回 0 (跳过 TLV)
ret

write_tlv:
mov  dword ptr [rdx], 0x80009          ; tag=0x09, len=0x08
mov  eax, 8                            ; 返回 8 字节
movzx ecx, word ptr [r8 + 0x28]        ; ecx = input[0x28]
mov  word ptr [rdx + 4], cx            ; TLV[4:6] = HT cap info
movzx ecx, word ptr [r8 + 0x2a]        ; ecx = input[0x2a]
mov  word ptr [rdx + 6], cx            ; TLV[6:8] = HT ext cap
```

### TLV 结构 (8 bytes)
```c
struct sta_rec_ht_info_tlv {
    __le16 tag;          // +0x00 = 0x0009
    __le16 len;          // +0x02 = 0x0008
    __le16 ht_cap;       // +0x04 — input[0x28] (HT Capabilities Info)
    __le16 ht_ext_cap;   // +0x06 — input[0x2a] (HT Extended Capabilities)
};
```

### 关键发现
- **条件性 TLV**: 如果 HT cap 和 ext cap 都为 0，则跳过 (返回 0)
- 仅在对端支持 HT 时才发送
- 非 HT AP (802.11a/g only) 连接不会包含此 TLV

---

## [2] STA_REC_VHT_INFO — tag=0x0a, len=0x10 (16 bytes)

**函数地址**: `0x14014d7e0`

### 反汇编
```asm
cmp  dword ptr [r8 + 0x38], 0          ; 检查 input[0x38] (VHT cap info)
jne  write_tlv
xor  eax, eax                          ; VHT cap = 0 → 跳过 TLV
ret

write_tlv:
mov  dword ptr [rdx], 0x10000a         ; tag=0x0a, len=0x10
mov  eax, 0x10                         ; 返回 16 字节
mov  ecx, dword ptr [r8 + 0x38]        ; ecx = input[0x38] (4 bytes)
mov  dword ptr [rdx + 4], ecx          ; TLV[4:8] = VHT cap info
movzx ecx, word ptr [r8 + 0x3c]        ; ecx = input[0x3c]
mov  word ptr [rdx + 8], cx            ; TLV[8:0xa] = VHT RX MCS map
movzx ecx, word ptr [r8 + 0x40]        ; ecx = input[0x40]
mov  word ptr [rdx + 0xa], cx          ; TLV[0xa:0xc] = VHT TX MCS map
; TLV[0xc:0x10] = 隐式 0 (pad)
```

### TLV 结构 (16 bytes)
```c
struct sta_rec_vht_info_tlv {
    __le16 tag;           // +0x00 = 0x000a
    __le16 len;           // +0x02 = 0x0010
    __le32 vht_cap_info;  // +0x04 — input[0x38] (VHT Capabilities Info, 4 bytes)
    __le16 vht_rx_mcs;    // +0x08 — input[0x3c] (VHT RX MCS Map)
    __le16 vht_tx_mcs;    // +0x0a — input[0x40] (VHT TX MCS Map)
    u8     pad[4];        // +0x0c — 未写入 (zeroed)
};
```

### 关键发现
- **条件性 TLV**: 如果 VHT cap info 为 0，跳过
- 仅在对端支持 VHT (802.11ac) 时发送
- pad 区域 4 字节未显式写入

---

## [5] STA_REC_STATE_INFO — tag=0x07, len=0x10 (16 bytes)

**函数地址**: `0x14014d730`

### 反汇编
```asm
mov  dword ptr [rdx], 0x100007         ; tag=0x07, len=0x10
mov  eax, 0x10                         ; 返回 16 字节
mov  cl, byte ptr [r8 + 0x14]          ; cl = input[0x14]
mov  byte ptr [rdx + 4], cl            ; TLV[4] = input[0x14] (state)
mov  ecx, dword ptr [r8 + 0x58]        ; ecx = input[0x58] (4 bytes)
mov  dword ptr [rdx + 8], ecx          ; TLV[8:0xc] = input[0x58] (flags)
mov  cl, byte ptr [r8 + 0x45]          ; cl = input[0x45]
mov  byte ptr [rdx + 0xc], cl          ; TLV[0xc] = input[0x45] (action)
mov  byte ptr [rdx + 0xd], 0           ; TLV[0xd] = 0 (显式 pad)
```

### TLV 结构 (16 bytes)
```c
struct sta_rec_state_info_tlv {
    __le16 tag;      // +0x00 = 0x0007
    __le16 len;      // +0x02 = 0x0010 (16)
    u8     state;    // +0x04 — input[0x14] (STA state: 0=DISCONNECT, 1=IDLE, 2=CONNECTED)
    u8     pad1[3];  // +0x05 — 未写入 (zeroed)
    __le32 flags;    // +0x08 — input[0x58] (state flags, 4 bytes)
    u8     action;   // +0x0c — input[0x45] (opmode/action)
    u8     pad2;     // +0x0d = 0 (显式清零)
    u8     pad3[2];  // +0x0e — 未写入 (zeroed)
};
```

### 关键发现
- **非条件性 TLV**: 始终写入 (不检查条件)
- **与 CLAUDE.md Session 23 修复一致**: state 在 offset+4, flags 在 offset+8
- CLAUDE.md 记录的 wire format `07 00 10 00 00 00 00 00 02 00 00 00 00 00 00 00`:
  - state=0 在 TLV[4], flags=0x02000000 在 TLV[8:12]? 或 state=0, flags 包含 state=2?

**重要**: 反汇编与 CLAUDE.md 记载的布局有差异:
- CLAUDE.md: `tag(2)+len(2)+flags(4)+state(1)+...` (flags在+4, state在+8)
- 反汇编: `tag(2)+len(2)+state(1)+pad(3)+flags(4)+action(1)+pad(3)` (state在+4, flags在+8)
- **反汇编确认: state 在 offset+4 (1 byte), flags 在 offset+8 (4 bytes)**
- CLAUDE.md 的 "flags 在 offset+4" 描述有误, 实际 **state 在 +4, flags 在 +8**

---

## [6] STA_REC_PHY_INFO — tag=0x15, len=0x0c (12 bytes)

**函数地址**: `0x14014d760`

### 反汇编
```asm
mov  dword ptr [rdx], 0xc0015          ; tag=0x15, len=0x0c
mov  eax, 0xc                          ; 返回 12 字节
movzx ecx, word ptr [r8 + 0x10]        ; ecx = input[0x10] (2 bytes)
mov  word ptr [rdx + 4], cx            ; TLV[4:6] = basic_rate (rate bitmap)
mov  cl, byte ptr [r8 + 0xd]           ; cl = input[0x0d]
mov  byte ptr [rdx + 6], cl            ; TLV[6] = phy_type
mov  cl, byte ptr [r8 + 0x30]          ; cl = input[0x30]
mov  byte ptr [rdx + 7], cl            ; TLV[7] = rcpi
mov  cl, byte ptr [r8 + 0x44]          ; cl = input[0x44]
mov  byte ptr [rdx + 8], cl            ; TLV[8] = channel_bw
mov  cl, byte ptr [r8 + 0x32]          ; cl = input[0x32]
mov  byte ptr [rdx + 9], cl            ; TLV[9] = nss
```

### TLV 结构 (12 bytes)
```c
struct sta_rec_phy_info_tlv {
    __le16 tag;          // +0x00 = 0x0015
    __le16 len;          // +0x02 = 0x000c (12)
    __le16 basic_rate;   // +0x04 — input[0x10] (basic rate bitmap)
    u8     phy_type;     // +0x06 — input[0x0d] (PHY type: a/b/g/n/ac/ax)
    u8     rcpi;         // +0x07 — input[0x30] (RCPI value)
    u8     channel_bw;   // +0x08 — input[0x44] (channel bandwidth: 0=20M, 1=40M, 2=80M...)
    u8     nss;          // +0x09 — input[0x32] (number of spatial streams)
    u8     pad[2];       // +0x0a — 未写入 (zeroed)
};
```

### 关键发现
- **非条件性 TLV**: 始终写入
- 包含物理层关键参数: 速率、PHY 类型、带宽、RCPI、空间流数
- phy_type 来自 input[0x0d] — 这可能与 BSS_INFO 中的 conn_state 标志有关

---

## [7] STA_REC_RA_INFO — tag=0x01, len=0x10 (16 bytes)

**函数地址**: `0x14014e570`

### 反汇编
```asm
mov  dword ptr [rdx], 0x100001         ; tag=0x01, len=0x10
movzx eax, word ptr [r8 + 0xe]         ; eax = input[0x0e] (2 bytes)
mov  word ptr [rdx + 4], ax            ; TLV[4:6] = oper_rate
lea  rdx, [r8 + 0x18]                  ; src = input + 0x18
mov  r8d, 0xa                          ; size = 10 bytes
lea  rcx, [rbx + 6]                    ; dst = TLV + 6
call 0x140010118                        ; memcpy(TLV+6, input+0x18, 10)
; 返回 len (从 TLV header 读取)
```

### TLV 结构 (16 bytes)
```c
struct sta_rec_ra_info_tlv {
    __le16 tag;            // +0x00 = 0x0001
    __le16 len;            // +0x02 = 0x0010 (16)
    __le16 oper_rate;      // +0x04 — input[0x0e] (operational rate code)
    u8     supp_rates[10]; // +0x06 — input[0x18:0x22] (supported rate set, memcpy 10B)
};
```

### 关键发现
- **非条件性 TLV**: 始终写入
- tag=0x01 — Rate Adaptation 信息
- supp_rates 是 10 字节: 这与 802.11 supported rate element 的最大长度一致
- 通过 memcpy 复制而非逐字段填充

---

## [8] STA_REC_BA_OFFLOAD — tag=0x16, len=0x10 (16 bytes)

**函数地址**: `0x14014e5b0`

### 反汇编
```asm
mov  dword ptr [rdx], 0x100016         ; tag=0x16, len=0x10
mov  r9d, 0x10                         ; 返回值 = 16
mov  al, byte ptr [r8 + 0x4c]
mov  byte ptr [rdx + 4], al            ; TLV[4] = input[0x4c] (tx_ba_wsize)
mov  al, byte ptr [r8 + 0x4d]
mov  byte ptr [rdx + 5], al            ; TLV[5] = input[0x4d] (rx_ba_wsize)
mov  al, byte ptr [r8 + 0x80]
mov  byte ptr [rdx + 6], al            ; TLV[6] = input[0x80] (ba_policy)
mov  al, byte ptr [r8 + 0x81]
mov  byte ptr [rdx + 7], al            ; TLV[7] = input[0x81] (ba_control)

mov  eax, dword ptr [r8 + 0x84]
mov  dword ptr [rdx + 8], eax          ; TLV[8:0xc] = input[0x84] (ba_bitmap, 4 bytes)

; 条件分支: 根据 PHY 能力选择窗口大小来源
mov  al, byte ptr [r8 + 0xd]           ; al = input[0x0d] (phy_type)
test al, 0x40                          ; 检查 bit6 (VHT flag)
jne  use_wide_ba
test al, al
jns  use_narrow_ba                     ; 检查 bit7 (HE/EHT flag)

use_wide_ba:                           ; VHT 或 HE: 使用 word-width BA 窗口
movzx eax, word ptr [r8 + 0x5c]
mov  word ptr [rdx + 0xc], ax          ; TLV[0xc:0xe] = input[0x5c] (word, tx_ba_wsize_ext)
movzx eax, word ptr [r8 + 0x5e]
mov  word ptr [rdx + 0xe], ax          ; TLV[0xe:0x10] = input[0x5e] (word, rx_ba_wsize_ext)
jmp  done

use_narrow_ba:                         ; legacy HT: 使用 byte → zero-extend 到 word
movzx ecx, byte ptr [r8 + 0x5c]
mov  word ptr [rdx + 0xc], cx          ; TLV[0xc:0xe] = zero-extend input[0x5c] (byte)
movzx eax, byte ptr [r8 + 0x5d]
mov  word ptr [rdx + 0xe], ax          ; TLV[0xe:0x10] = zero-extend input[0x5d] (byte)

done:
mov  eax, r9d                          ; 返回 16
```

### TLV 结构 (16 bytes)
```c
struct sta_rec_ba_offload_tlv {
    __le16 tag;              // +0x00 = 0x0016
    __le16 len;              // +0x02 = 0x0010 (16)
    u8     tx_ba_wsize;      // +0x04 — input[0x4c] (TX BA window size, 基础)
    u8     rx_ba_wsize;      // +0x05 — input[0x4d] (RX BA window size, 基础)
    u8     ba_policy;        // +0x06 — input[0x80] (BA policy)
    u8     ba_control;       // +0x07 — input[0x81] (BA control flags)
    __le32 ba_bitmap;        // +0x08 — input[0x84] (BA TID bitmap, 4 bytes)
    __le16 tx_ba_wsize_ext;  // +0x0c — 根据 PHY 能力选择:
                             //         VHT/HE: input[0x5c] as word
                             //         HT:     input[0x5c] as byte, zero-extended
    __le16 rx_ba_wsize_ext;  // +0x0e — 同上, input[0x5e] or input[0x5d]
};
```

### 关键发现
- **非条件性 TLV**: 始终写入
- **有条件分支**: 根据 input[0x0d] (phy_type) bit6/bit7 选择 BA 窗口大小的读取方式
  - VHT (bit6=1) 或 HE (bit7=1): 从 input[0x5c/0x5e] 读取 word (支持 >256 的窗口)
  - HT only: 从 input[0x5c/0x5d] 读取 byte 并零扩展
- Block Ack offload 是固件级别的 BA 管理功能
- **对于 auth 阶段**: 此 TLV 的值应全为 0 (无 BA session), 但仍必须发送

---

## [9] STA_REC_UAPSD — tag=0x24, len=0x08 (8 bytes)

**函数地址**: `0x14014e620`

### 反汇编
```asm
mov  dword ptr [rdx], 0x80024          ; tag=0x24, len=0x08
mov  eax, 8                            ; 返回 8 字节
mov  cl, byte ptr [r8 + 0x13]
mov  byte ptr [rdx + 4], cl            ; TLV[4] = input[0x13] (uapsd_flags)
mov  cl, byte ptr [r8 + 0x34]
mov  byte ptr [rdx + 5], cl            ; TLV[5] = input[0x34] (max_sp_len)
mov  cl, byte ptr [r8 + 0x35]
mov  byte ptr [rdx + 6], cl            ; TLV[6] = input[0x35] (uapsd_ac)
```

### TLV 结构 (8 bytes)
```c
struct sta_rec_uapsd_tlv {
    __le16 tag;          // +0x00 = 0x0024 (36)
    __le16 len;          // +0x02 = 0x0008
    u8     uapsd_flags;  // +0x04 — input[0x13] (UAPSD trigger/delivery enabled ACs)
    u8     max_sp_len;   // +0x05 — input[0x34] (Max SP Length)
    u8     uapsd_ac;     // +0x06 — input[0x35] (UAPSD AC bitmap)
    u8     pad;          // +0x07 — 未写入 (zeroed)
};
```

### 关键发现
- **非条件性 TLV**: 始终写入
- UAPSD (Unscheduled Automatic Power Save Delivery) 用于省电
- 对于初始 auth 连接，这些值通常为 0

---

## Flat Input Struct (param_3) 字段映射

根据所有 TLV 构建函数的反汇编，重建 0xEC 字节 flat input struct 的字段分布:

| Offset | Size | 来源 TLV | 字段名 |
|--------|------|----------|--------|
| 0x00 | 1 | (header) | sta_idx |
| 0x01 | 1 | BASIC | bss_idx → conn_type 查找 |
| 0x02 | 4 | BASIC | peer_addr[0:4] (MAC bytes 0-3) |
| 0x06 | 2 | BASIC | peer_addr[4:6] (MAC bytes 4-5) |
| 0x08 | 2 | BASIC | aid (Association ID) |
| 0x0a | 3 | — | (gap) |
| 0x0d | 1 | PHY/BA | phy_type (bit6=VHT, bit7=HE) |
| 0x0e | 2 | RA_INFO | oper_rate |
| 0x10 | 2 | PHY_INFO | basic_rate |
| 0x12 | 1 | BASIC | qos (QoS capability flag) |
| 0x13 | 1 | UAPSD | uapsd_flags |
| 0x14 | 1 | STATE_INFO | state (0=DISCONNECT, 1=IDLE, 2=CONNECTED) |
| 0x15 | 3 | — | (gap) |
| 0x18 | 10 | RA_INFO | supp_rates[10] (supported rate set) |
| 0x22 | 6 | — | (gap) |
| 0x28 | 2 | HT_INFO | ht_cap (HT Capabilities Info) |
| 0x2a | 2 | HT_INFO | ht_ext_cap (HT Extended Capabilities) |
| 0x2c | 4 | — | (gap) |
| 0x30 | 1 | PHY_INFO | rcpi (received channel power indicator) |
| 0x31 | 1 | — | (gap) |
| 0x32 | 1 | PHY_INFO | nss (number of spatial streams) |
| 0x33 | 1 | — | (gap) |
| 0x34 | 1 | UAPSD | max_sp_len |
| 0x35 | 1 | UAPSD | uapsd_ac |
| 0x36 | 2 | — | (gap) |
| 0x38 | 4 | VHT_INFO | vht_cap_info (VHT Capabilities Info) |
| 0x3c | 2 | VHT_INFO | vht_rx_mcs (VHT RX MCS Map) |
| 0x3e | 2 | — | (gap) |
| 0x40 | 2 | VHT_INFO | vht_tx_mcs (VHT TX MCS Map) |
| 0x42 | 2 | — | (gap) |
| 0x44 | 1 | PHY_INFO | channel_bw (bandwidth) |
| 0x45 | 1 | STATE_INFO | action (opmode/action type) |
| 0x46 | 6 | — | (gap) |
| 0x4c | 1 | BA_OFFLOAD | tx_ba_wsize |
| 0x4d | 1 | BA_OFFLOAD | rx_ba_wsize |
| 0x4e | 0x0e | — | (gap) |
| 0x5c | 2-4 | BA_OFFLOAD | tx/rx_ba_wsize_ext (大小取决于 VHT/HE) |
| 0x58 | 4 | STATE_INFO | flags (state flags) |
| 0x80 | 1 | BA_OFFLOAD | ba_policy |
| 0x81 | 1 | BA_OFFLOAD | ba_control |
| 0x84 | 4 | BA_OFFLOAD | ba_bitmap (TID bitmap) |
| 0x88 | 6 | HE_BASIC | he_peer_addr (MAC) |
| 0x8e | 11 | HE_BASIC | he_info (SSID or HE caps) |
| 0x9c | 2 | HE_BASIC | he_cap1 |
| 0xa0 | 2 | HE_BASIC | he_cap2 |
| 0xa4 | 2 | HE_BASIC | he_cap3 |
| ... | ... | EHT/MLD | (EHT/MLD fields, 已知 TLVs) |

---

## Auth 阶段必须发送的 TLV 分析

Windows 驱动始终发送全部 13 个 TLV (某些可能返回 0 长度跳过)。

对于 auth 阶段 (open system, 802.11a/g AP), 实际生效的 TLV:

| TLV | 条件 | Auth 阶段行为 |
|-----|------|--------------|
| BASIC (0x00) | 始终 | ✅ **必须**: conn_type, MAC, AID, QoS |
| RA_INFO (0x01) | 始终 | ✅ **必须**: 速率信息 |
| STATE_INFO (0x07) | 始终 | ✅ **必须**: state=2 (CONNECTED) |
| HT_INFO (0x09) | 有 HT cap | 取决于 AP: 5G AP 通常有 HT |
| VHT_INFO (0x0a) | 有 VHT cap | 取决于 AP: 5G AP 通常有 VHT |
| PHY_INFO (0x15) | 始终 | ✅ **必须**: phy_type, 带宽, NSS |
| BA_OFFLOAD (0x16) | 始终 | ✅ 写入但值为 0 (无 BA session) |
| HE_BASIC (0x19) | 始终 | ✅ 写入 HE capabilities |
| HE_6G_CAP (0x17) | 6GHz only | 通常跳过 (仅 6GHz band) |
| MLD_SETUP (0x20) | MLO | 通常跳过 (非 MLO) |
| EHT_MLD (0x21) | EHT | 通常跳过 |
| EHT_INFO (0x22) | EHT | 通常跳过 |
| UAPSD (0x24) | 始终 | ✅ 写入但值通常为 0 |

### ⚠️ 我们缺失的关键 TLV

我们当前发送 5 个 TLV: BASIC, RA, STATE, PHY, HDR_TRANS

对比 Windows:
1. **BASIC (0x00)**: ✅ 有 — 但需验证 conn_type/extra_info 字段是否正确
2. **RA_INFO (0x01)**: ✅ 有 — 但 tag 和字段布局需对齐
3. **STATE_INFO (0x07)**: ✅ 有 — Session 23 已修复
4. **HT_INFO (0x09)**: ❌ **缺失** — 连接 HT AP 时需要
5. **VHT_INFO (0x0a)**: ❌ **缺失** — 连接 VHT AP 时需要
6. **PHY_INFO (0x15)**: ✅ 有 — 需验证字段布局
7. **BA_OFFLOAD (0x16)**: ❌ **缺失** — Windows 始终发送 (即使全零)
8. **UAPSD (0x24)**: ❌ **缺失** — Windows 始终发送 (即使全零)
9. **HDR_TRANS**: ❓ Windows **无此 TLV** — 我们多发了一个 Windows 没有的 TLV!

---

## Tag 值汇总表

| Tag (hex) | Tag (dec) | 名称 | 来源 |
|-----------|-----------|------|------|
| 0x00 | 0 | STA_REC_BASIC | [0] 本次确认 |
| 0x01 | 1 | STA_REC_RA_INFO | [7] 本次确认 |
| 0x07 | 7 | STA_REC_STATE_INFO | [5] 本次确认 |
| 0x09 | 9 | STA_REC_HT_INFO | [1] 本次确认 |
| 0x0a | 10 | STA_REC_VHT_INFO | [2] 本次确认 |
| 0x15 | 21 | STA_REC_PHY_INFO | [6] 本次确认 |
| 0x16 | 22 | STA_REC_BA_OFFLOAD | [8] 本次确认 |
| 0x17 | 23 | STA_REC_HE_6G_CAP | [4] 已知 |
| 0x19 | 25 | STA_REC_HE_BASIC | [3] 已知 |
| 0x20 | 32 | STA_REC_MLD_SETUP | [12] 已知 |
| 0x21 | 33 | STA_REC_EHT_MLD | [11] 已知 |
| 0x22 | 34 | STA_REC_EHT_INFO | [10] 已知 |
| 0x24 | 36 | STA_REC_UAPSD | [9] 本次确认 |

---

## Dispatch Table 原始数据

```
0x140250710: 14 00 00 00 00 00 00 00 d0 d6 14 40 01 00 00 00  [0] BASIC
0x140250720: 08 00 00 00 00 00 00 00 a0 d7 14 40 01 00 00 00  [1] HT_INFO
0x140250730: 10 00 00 00 00 00 00 00 e0 d7 14 40 01 00 00 00  [2] VHT_INFO
0x140250740: 1c 00 00 00 00 00 00 00 10 d8 14 40 01 00 00 00  [3] HE_BASIC
0x140250750: 08 00 00 00 00 00 00 00 e0 da 14 40 01 00 00 00  [4] HE_6G_CAP
0x140250760: 10 00 00 00 00 00 00 00 30 d7 14 40 01 00 00 00  [5] STATE_INFO
0x140250770: 0c 00 00 00 00 00 00 00 60 d7 14 40 01 00 00 00  [6] PHY_INFO
0x140250780: 10 00 00 00 00 00 00 00 70 e5 14 40 01 00 00 00  [7] RA_INFO
0x140250790: 10 00 00 00 00 00 00 00 b0 e5 14 40 01 00 00 00  [8] BA_OFFLOAD
0x1402507a0: 08 00 00 00 00 00 00 00 20 e6 14 40 01 00 00 00  [9] UAPSD
0x1402507b0: 28 00 00 00 00 00 00 00 80 db 14 40 01 00 00 00  [10] EHT_INFO
0x1402507c0: 10 00 00 00 00 00 00 00 a0 e2 14 40 01 00 00 00  [11] EHT_MLD
0x1402507d0: 20 00 00 00 00 00 00 00 c0 dd 14 40 01 00 00 00  [12] MLD_SETUP
```
