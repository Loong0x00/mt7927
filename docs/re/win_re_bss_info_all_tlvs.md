# Windows RE: BSS_INFO 全部 14 个 TLV 完整逆向

**日期**: 2026-02-22 (Session 24)
**二进制**: `mtkwecx.sys` (ImageBase=0x140000000)
**工具**: `tools/disasm_helper.py` (pefile + capstone)
**分析方法**: 直接反汇编，非 Ghidra 反编译

---

## 1. Dispatch Table 概览

**Dispatch Table VA**: `0x1402505b0`
**Entry 数量**: 14 (循环 `uVar3 < 0xe`)
**Entry 格式**: 每 16 字节 `[u32 alloc_size] [u32 reserved] [u64 func_ptr]`
**总 TLV 数**: 17 (entry[1] 输出 3 个 TLV: RLM+PROTECT+IFS_TIME)

### 分发函数调用流程

```
nicUniCmdSetBssInfo (0x1401444a0)
  → 遍历 dispatch_table[0..13]
  → 每个 handler(param_1=adapter, param_2=output_buf, param_3=bss_info_ptr)
  → handler 返回值 = 实际写入字节数 (从 tag+len 头的 len 字段读取)
```

---

## 2. 完整 14 Entry 映射表

| Idx | AllocSize | Func VA | Tag(s) | Len(s) | 名称 | 状态 |
|-----|-----------|---------|--------|--------|------|------|
| 0 | 0x20 | `0x14014c610` | 0x0000 | 0x20 | **BASIC** | 已知 |
| 1 | 0x2c | `0x14014cc80`→`0x140150edc` | 0x0002+0x0003+0x0017 | 0x10+0x08+0x14 | **RLM+PROTECT+IFS_TIME** | **本次逆向** |
| 2 | 0x10 | `0x14014cc90` | 0x000b | 0x10 | **RATE** | **本次逆向** |
| 3 | 0x08 | `0x14014ccb0` | 0x0010 | 0x08 | **SEC** | **本次逆向** |
| 4 | 0x08 | `0x14014ccd0` | 0x000f | 0x08 | **QBSS** | **本次逆向** |
| 5 | 0x28 | `0x14014ccf0` | 0x000d | 0x28 | **SAP** (带速率拷贝) | **本次逆向** |
| 6 | 0x08 | `0x14014cd30` | 0x000e | 0x08 | **P2P** | **本次逆向** |
| 7 | 0x10 | `0x14014cd50` | 0x0005 | 0x10 | **HE** | 已知 |
| 8 | 0x08 | `0x14014d010` | 0x0004 | 0x08 | **BSS_COLOR** | 已知 |
| 9 | 0x10 | `0x14014d150` | 0x001E | 0x10 | **EHT** | 已知 |
| 10 | 0x08 | `0x14014d300` | 0x0006 | 0x08 | **11V_MBSSID** | **本次逆向** |
| 11 | 0x08 | `0x14014d320` | 0x000c | 0x08 | **UNKNOWN_0x0C** | **本次逆向** |
| 12 | 0x14 | `0x14014d340`→`0x14014fad0` | 0x001a | 0x14 | **MLD** | **本次逆向** |
| 13 | 0x08 | `0x14014d350` | 0x0018 | 0x08 | **STAIoT** | 已知 |

---

## 3. 已知 TLV (跳过详细分析)

### Entry[0]: BASIC (tag=0x0000, len=0x20)

已实现，标准 BSS_INFO_BASIC TLV。

### Entry[7]: HE (tag=0x0005, len=0x10)

已知，HE capability 信息。

### Entry[8]: BSS_COLOR (tag=0x0004, len=0x08)

已知，BSS color 信息。

### Entry[9]: EHT (tag=0x001E, len=0x10)

已知，EHT/WiFi 7 capability 信息。

### Entry[13]: STAIoT (tag=0x0018, len=0x08)

已知，STA IoT 标记。

---

## 4. Entry[1]: RLM + PROTECT + IFS_TIME (三合一, 总 0x2c 字节)

**Func**: `0x14014cc80` — trampoline: `add r8, 0x44; jmp 0x140150edc`
**实际函数**: `0x140150edc` (nicUniCmdSetBssRlmImpl)

所有输入偏移 = bss_info_ptr + 0x44 (trampoline 提前加了 0x44)

### 4.1 TLV RLM (tag=0x0002, len=0x10, offset 0x00-0x0F)

```c
struct bss_tlv_rlm {       // 16 bytes
    __le16 tag;             // +0x00: = 0x0002
    __le16 len;             // +0x02: = 0x0010
    u8  primary_channel;    // +0x04: bss[+0x46]
    u8  center_freq_seg0;   // +0x05: bss[+0x54]
    u8  center_freq_seg1;   // +0x06: bss[+0x55]
    u8  bw_mode;            // +0x07: 从 bss[+0x53] 转换:
                            //   0 → (bss[+0x47]!=0 ? 1 : 0)
                            //   1 → 2
                            //   2 → 3
                            //   3 → 6
                            //   4 → 7
    u8  phy_type;           // +0x08: bss[+0x58]
    u8  rlm_param;          // +0x09: bss[+0x59]
    u8  dot11_mode;         // +0x0A: bss[+0x50]
    u8  prim_channel_idx;   // +0x0B: bss[+0x47]
    u8  band;               // +0x0C: bss[+0x45]  (0=2.4G, 1=5G)
    u8  reserved[3];        // +0x0D: = 0
};
```

**反汇编关键代码**:
```asm
0x140150ef4:  mov dword ptr [rdx], 0x100002      ; tag=0x0002, len=0x10
0x140150f0b:  mov byte ptr [rbx+4], dl           ; primary_channel = input[2]
0x140150f13:  mov byte ptr [rbx+5], r8b          ; center_freq_seg0 = input[0x10]
0x140150f20:  mov byte ptr [rbx+6], r9b          ; center_freq_seg1 = input[0x11]
0x140150f27:  mov byte ptr [rbx+8], al           ; phy_type = input[0x14]
0x140150f2d:  mov byte ptr [rbx+9], al           ; rlm_param = input[0x15]
0x140150f33:  mov byte ptr [rbx+0xa], al         ; dot11_mode = input[0xc]
0x140150f39:  mov byte ptr [rbx+0xb], al         ; prim_channel_idx = input[3]
0x140150f3f:  mov byte ptr [rbx+0xc], al         ; band = input[1]
0x140150f42:  mov byte ptr [rbx+7], 0            ; bw_mode 初始化为 0
; ... switch on input[0xf] 设置 bw_mode
```

### 4.2 TLV PROTECT (tag=0x0003, len=0x08, offset 0x10-0x17)

```c
struct bss_tlv_protect {    // 8 bytes
    __le16 tag;             // +0x10: = 0x0003
    __le16 len;             // +0x12: = 0x0008
    u32    flags;           // +0x14: 位域:
                            //   bit5 (0x20): erp_present   — if bss[+0x48] != 0
                            //   bit1 (0x02): non_erp=1     — if bss[+0x49] == 1
                            //   bit2 (0x04): non_erp=2     — if bss[+0x49] == 2
                            //   bit3 (0x08): non_erp=3     — if bss[+0x49] == 3
                            //   bit7 (0x80): short_slot     — if bss[+0x4a] == 1
};
```

**反汇编关键代码**:
```asm
0x140150ffd:  mov dword ptr [r8], 0x80003        ; tag=0x0003, len=0x08
0x140151011:  or  dword ptr [rcx], 0x20          ; if input[4] != 0: flags |= 0x20
; switch on input[5]:
;   1 → flags |= 0x02
;   2 → flags |= 0x04
;   3 → flags |= 0x08
0x14015103a:  bts dword ptr [rcx], 7             ; if input[6] == 1: flags |= 0x80
```

**5GHz 典型值**: `flags = 0x00` (无 ERP, 短时隙由 AP 决定)

### 4.3 TLV IFS_TIME (tag=0x0017, len=0x14, offset 0x18-0x2B)

```c
struct bss_tlv_ifs_time {   // 20 bytes
    __le16 tag;             // +0x18: = 0x0017
    __le16 len;             // +0x1A: = 0x0014
    u8  valid;              // +0x1C: = 1 (硬编码)
    u8  pad1;               // +0x1D: = 0
    u8  pad2;               // +0x1E: = 0
    u8  pad3;               // +0x1F: = 0
    __le16 sifs;            // +0x20: bss[+0x52] != 0 → 9, == 0 → 20
                            //   5GHz/short_slot → sifs = 9
                            //   2.4GHz legacy   → sifs = 20
    u8  pad[10];            // +0x22: = 0
};
```

**反汇编关键代码**:
```asm
0x140151046:  mov dword ptr [r8], 0x140017       ; tag=0x0017, len=0x14
0x14015104d:  mov byte ptr [r8+4], 1             ; valid = 1
; sifs 计算:
0x140151055:  neg cl                             ; CF = (input[0xe] != 0)
0x140151057:  sbb dx, dx                         ; dx = CF ? -1 : 0
0x14015105d:  and dx, 0xfff5                     ; dx = CF ? 0xFFF5(-11) : 0
0x140151062:  add dx, ax                         ; ax=0x14, dx = CF ? 9 : 20
0x140151065:  mov word ptr [r9+0x10], dx         ; store sifs
```

---

## 5. Entry[2]: RATE (tag=0x000b, len=0x10)

**Func**: `0x14014cc90` (仅 7 条指令)

```c
struct bss_tlv_rate {       // 16 bytes
    __le16 tag;             // +0x00: = 0x000b
    __le16 len;             // +0x02: = 0x0010
    __le16 operational_rates; // +0x04: bss[+0x2c] (word)
    __le16 extended_rates;  // +0x06: bss[+0x2e] (word)
    u8  reserved[8];        // +0x08: = 0
};
```

**反汇编**:
```asm
0x14014cc90:  mov dword ptr [rdx], 0x10000b      ; tag=0x000b, len=0x10
0x14014cc9b:  movzx ecx, word ptr [r8+0x2c]      ; operational_rates
0x14014cca0:  mov word ptr [rdx+4], cx
0x14014cca4:  movzx ecx, word ptr [r8+0x2e]      ; extended_rates
0x14014cca9:  mov word ptr [rdx+6], cx
0x14014ccad:  ret
```

**注意**: `operational_rates` 和 `extended_rates` 是 802.11 速率集 bitmap:
- 5GHz 802.11a: 至少包含 6/12/24 Mbps 必选速率
- 2.4GHz: 包含 1/2/5.5/11 Mbps + OFDM 速率

---

## 6. Entry[3]: SEC (tag=0x0010, len=0x08)

**Func**: `0x14014ccb0` (仅 6 条指令)

```c
struct bss_tlv_sec {        // 8 bytes
    __le16 tag;             // +0x00: = 0x0010
    __le16 len;             // +0x02: = 0x0008
    u8  cipher_type;        // +0x04: bss[+0x35]
    u8  group_cipher;       // +0x05: bss[+0x36]
    u8  reserved[2];        // +0x06: = 0
};
```

**反汇编**:
```asm
0x14014ccb0:  mov dword ptr [rdx], 0x80010       ; tag=0x0010, len=0x08
0x14014ccbb:  mov cl, byte ptr [r8+0x35]         ; cipher_type
0x14014ccbf:  mov byte ptr [rdx+4], cl
0x14014ccc2:  mov cl, byte ptr [r8+0x36]         ; group_cipher
0x14014ccc6:  mov byte ptr [rdx+5], cl
0x14014ccc9:  ret
```

**Open auth 时**: `cipher_type = 0, group_cipher = 0` (无加密)

---

## 7. Entry[4]: QBSS (tag=0x000f, len=0x08)

**Func**: `0x14014ccd0` (仅 4 条指令)

```c
struct bss_tlv_qbss {       // 8 bytes
    __le16 tag;             // +0x00: = 0x000f
    __le16 len;             // +0x02: = 0x0008
    u8  qos_enabled;        // +0x04: bss[+0x2a]
    u8  reserved[3];        // +0x05: = 0
};
```

**反汇编**:
```asm
0x14014ccd0:  mov dword ptr [rdx], 0x8000f       ; tag=0x000f, len=0x08
0x14014ccdb:  mov cl, byte ptr [r8+0x2a]         ; qos_enabled
0x14014ccdf:  mov byte ptr [rdx+4], cl
0x14014cce2:  ret
```

---

## 8. Entry[5]: SAP (tag=0x000d, len=0x28)

**Func**: `0x14014ccf0` (含子函数调用)

```c
struct bss_tlv_sap {        // 40 bytes
    __le16 tag;             // +0x00: = 0x000d
    __le16 len;             // +0x02: = 0x0028
    u8  num_sta;            // +0x04: bss[+0x3b]
    u8  pad[2];             // +0x05: = 0
    u8  rate_count;         // +0x07: bss[+0x03]
    u8  rate_data[32];      // +0x08: memcpy(dst, bss+4, bss[3])
                            //        拷贝 bss[3] 字节的速率列表
};
```

**反汇编**:
```asm
0x14014ccf6:  mov dword ptr [rdx], 0x28000d      ; tag=0x000d, len=0x28
0x14014ccff:  mov al, byte ptr [r8+0x3b]         ; num_sta
0x14014cd06:  mov byte ptr [rdx+4], al
0x14014cd09:  mov al, byte ptr [r8+3]            ; rate_count
0x14014cd0d:  mov byte ptr [rdx+7], al
0x14014cd10:  lea rdx, [r9+4]                    ; src = bss + 4
0x14014cd14:  movzx r8d, byte ptr [r8+3]         ; size = bss[3] (rate_count)
0x14014cd19:  lea rcx, [rbx+8]                   ; dst = output + 8
0x14014cd1d:  call 0x140010118                    ; safe_memcpy(dst, src, size)
```

**说明**: `0x140010118` 是带 NULL 检查的 memcpy 包装函数:
```asm
0x140010118:  sub rsp, 0x28
0x14001011c:  test rcx, rcx   ; if dst == NULL → skip
0x140010124:  test rdx, rdx   ; if src == NULL → skip
0x140010129:  test r8d, r8d   ; if size == 0  → skip
0x14001012e:  call 0x14020d600 ; → RtlCopyMemory
```

---

## 9. Entry[6]: P2P (tag=0x000e, len=0x08)

**Func**: `0x14014cd30` (仅 4 条指令)

```c
struct bss_tlv_p2p {        // 8 bytes
    __le16 tag;             // +0x00: = 0x000e
    __le16 len;             // +0x02: = 0x0008
    __le32 p2p_info;        // +0x04: bss[+0x40] (dword)
};
```

**反汇编**:
```asm
0x14014cd30:  mov dword ptr [rdx], 0x8000e       ; tag=0x000e, len=0x08
0x14014cd3b:  mov ecx, dword ptr [r8+0x40]       ; p2p_info (4 bytes)
0x14014cd3f:  mov dword ptr [rdx+4], ecx
0x14014cd42:  ret
```

---

## 10. Entry[10]: 11V_MBSSID (tag=0x0006, len=0x08)

**Func**: `0x14014d300` (仅 6 条指令)

```c
struct bss_tlv_mbssid {     // 8 bytes
    __le16 tag;             // +0x00: = 0x0006
    __le16 len;             // +0x02: = 0x0008
    u8  max_bssid_indicator; // +0x04: bss[+0x66]
    u8  mbssid_index;       // +0x05: bss[+0x67]
    u8  reserved[2];        // +0x06: = 0
};
```

**反汇编**:
```asm
0x14014d300:  mov dword ptr [rdx], 0x80006       ; tag=0x0006, len=0x08
0x14014d30b:  mov cl, byte ptr [r8+0x66]         ; max_bssid_indicator
0x14014d30f:  mov byte ptr [rdx+4], cl
0x14014d312:  mov cl, byte ptr [r8+0x67]         ; mbssid_index
0x14014d316:  mov byte ptr [rdx+5], cl
0x14014d319:  ret
```

---

## 11. Entry[11]: UNKNOWN_0x0C (tag=0x000c, len=0x08)

**Func**: `0x14014d320` (仅 4 条指令)

```c
struct bss_tlv_0c {         // 8 bytes
    __le16 tag;             // +0x00: = 0x000c
    __le16 len;             // +0x02: = 0x0008
    u8  flag;               // +0x04: bss[+0x38]
    u8  reserved[3];        // +0x05: = 0
};
```

**反汇编**:
```asm
0x14014d320:  mov dword ptr [rdx], 0x8000c       ; tag=0x000c, len=0x08
0x14014d32b:  mov cl, byte ptr [r8+0x38]         ; single byte flag
0x14014d32f:  mov byte ptr [rdx+4], cl
0x14014d332:  ret
```

**推测**: 可能是 WAPI 或 BCN_PROT 相关的安全标志。

---

## 12. Entry[12]: MLD (tag=0x001a, len=0x14)

**Func**: `0x14014d340` — trampoline: `mov r8b, [r8]; jmp 0x14014fad0`
**实际函数**: `0x14014fad0` (nicUniCmdBssInfoMld)

这是最复杂的 TLV 构建函数，涉及多个子函数调用来查找 MLD link 和 BSS 信息。

```c
struct bss_tlv_mld {        // 20 bytes
    __le16 tag;             // +0x00: = 0x001a
    __le16 len;             // +0x02: = 0x0014
    u8  link_id;            // +0x04: link_info[2] (或 0xFF 如无 MLD)
    u8  group_mld_id;       // +0x05: sta_rec[0x908] + 0x20
    __le32 own_mac_low;     // +0x06: link_info[4:7] (MAC 地址低 4 字节)
    __le16 own_mac_hi;      // +0x0A: link_info[8:9] (MAC 地址高 2 字节)
    u8  band_idx;           // +0x0C: link_info[3]
    u8  omac_idx;           // +0x0D: sta_rec[0x8fb]
    u8  remap_idx;          // +0x0E: link_info[0xd]
    u8  eml_mode;           // +0x0F: 基于 MLD capability:
                            //   cap==1 → 1, cap==2/3 → 0, 其他不变
    u8  str_opmode;         // +0x10: link_info[0x10]
    u8  link_opmode;        // +0x11: link_info[1]
    u8  pad[2];             // +0x12: = 0
};
```

**Fallback 路径 (无 MLD 信息)**:
```c
link_id     = 0xFF;
group_mld_id = adapter[0x24];
own_mac_low  = adapter[0x2cc] (4 bytes);
own_mac_hi   = adapter[0x2d0] (2 bytes);
str_opmode   = 0;
band_idx/omac/remap = 0xFFFF (标记无效);
```

**关键反汇编** (主路径):
```asm
0x14014fafe:  mov dword ptr [rdx], 0x14001a      ; tag=0x001a, len=0x14
0x14014fb0a:  call 0x14018b15c                    ; lookup_link_by_band(adapter, band_idx)
; ... 多个子函数查找 link_info 和 sta_rec ...
0x14014fb78:  mov byte ptr [r13+4], cl           ; link_id = link_info[2]
0x14014fb7f:  mov byte ptr [r13+0xc], al         ; band_idx = link_info[3]
0x14014fb89:  mov byte ptr [r13+0xd], al         ; omac_idx = sta_rec[0x8fb]
0x14014fb90:  mov byte ptr [r13+0xe], al         ; remap_idx = link_info[0xd]
0x14014fc2f:  mov byte ptr [r13+5], al           ; group_mld_id = sta_rec[0x908] + 0x20
0x14014fc36:  mov dword ptr [r13+6], eax         ; own_mac_low = link_info[4:7]
0x14014fc3e:  mov word ptr [r13+0xa], ax         ; own_mac_hi = link_info[8:9]
```

---

## 13. Tag 值与 mt76 枚举对照

| Windows Tag | mt76 connac_mcu.h 枚举 | mt76 名称 | 匹配? |
|-------------|------------------------|-----------|-------|
| 0x0000 | 0 | BASIC | ✅ |
| 0x0002 | 2 | RLM | ✅ |
| 0x0003 | — | PROTECT (mt76 无此 tag) | ⚠️ Windows 特有 |
| 0x0004 | 4 | BSS_COLOR | ✅ |
| 0x0005 | 5 | HE_BASIC | ✅ |
| 0x0006 | 6 | 11V_MBSSID | ✅ |
| 0x000b | 11 | RATE | ✅ |
| 0x000c | — | ??? (mt76 无此 tag) | ⚠️ Windows 特有 |
| 0x000d | — | SAP (mt76 无此 tag) | ⚠️ Windows 特有 |
| 0x000e | — | P2P (mt76 无此 tag) | ⚠️ Windows 特有 |
| 0x000f | 15 | QBSS | ✅ |
| 0x0010 | 16 | SEC | ✅ |
| 0x0017 | 23 | IFS_TIME | ✅ |
| 0x0018 | — | STA_IOT (mt76 connac 无此 tag) | ⚠️ |
| 0x001a | 26 | MLD | ✅ |
| 0x001e | 30 | EHT | ✅ |

---

## 14. 当前驱动实现状态 vs Windows

| Tag | TLV 名称 | 我们发送? | Windows 发送? | 对 TX 影响 |
|-----|---------|----------|-------------|-----------|
| 0x0000 | BASIC | ✅ | ✅ | — |
| 0x0002 | RLM | ✅ | ✅ | — |
| 0x0003 | PROTECT | ❌ | ✅ | **可能影响帧保护模式** |
| 0x0017 | IFS_TIME | ❌ | ✅ | **可能影响帧间间隔** |
| 0x000b | RATE | ❌ | ✅ | **固件需要知道合法速率集** |
| 0x0010 | SEC | ❌ | ✅ | **加密配置 (open=0)** |
| 0x000f | QBSS | ❌ | ✅ | QoS 相关 |
| 0x000d | SAP | ❌ | ✅ | 速率列表拷贝 |
| 0x000e | P2P | ❌ | ✅ | P2P 配置 (非 STA 可跳过) |
| 0x0005 | HE | ❌ | ✅ | HE 能力 |
| 0x0004 | BSS_COLOR | ❌ | ✅ | BSS color (HE/EHT) |
| 0x001E | EHT | ❌ | ✅ | EHT 能力 |
| 0x0006 | 11V_MBSSID | ❌ | ✅ | MBSSID 指示 |
| 0x000c | UNKNOWN_0x0C | ❌ | ✅ | 用途未明 |
| 0x001a | MLD | ✅ | ✅ | — |
| 0x0018 | STAIoT | ❌ | ✅ | IoT 标记 |

---

## 15. 实现建议 (按优先级)

### 立即添加 (可能影响 TX 成功)

#### 1. PROTECT (tag=0x0003) — **最重要**
```c
// 5GHz: 无 ERP 保护，flags = 0
// 2.4GHz: 根据 BSS 情况设置 ERP bits
u8 tlv_protect[] = {
    0x03, 0x00, 0x08, 0x00,  // tag=3, len=8
    0x00, 0x00, 0x00, 0x00   // flags=0 (5GHz)
};
```

#### 2. IFS_TIME (tag=0x0017) — **帧时序关键**
```c
u8 tlv_ifs_time[] = {
    0x17, 0x00, 0x14, 0x00,  // tag=0x17, len=0x14
    0x01, 0x00, 0x00, 0x00,  // valid=1
    0x09, 0x00,              // sifs=9 (5GHz)
    0x00, 0x00, 0x00, 0x00,  // pad
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00               // pad
};
```

#### 3. RATE (tag=0x000b) — **固件选速率**
```c
u8 tlv_rate[] = {
    0x0b, 0x00, 0x10, 0x00,  // tag=0x0b, len=0x10
    0x??, 0x??,              // operational_rates (TBD)
    0x??, 0x??,              // extended_rates (TBD)
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};
```

#### 4. SEC (tag=0x0010) — **安全配置**
```c
u8 tlv_sec[] = {
    0x10, 0x00, 0x08, 0x00,  // tag=0x10, len=0x08
    0x00,                    // cipher_type=0 (open)
    0x00,                    // group_cipher=0 (none)
    0x00, 0x00               // reserved
};
```

### 可选添加 (对 TX 可能不影响)

#### 5. QBSS (tag=0x000f)
```c
u8 tlv_qbss[] = {
    0x0f, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00   // qos_enabled=0
};
```

#### 6. 11V_MBSSID (tag=0x0006)
```c
u8 tlv_mbssid[] = {
    0x06, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00   // max_bssid=0, index=0
};
```

---

## 16. Dispatch Table 原始数据

```
0x1402505b0: 20 00 00 00 00 00 00 00 10 c6 14 40 01 00 00 00  [0] BASIC
0x1402505c0: 2c 00 00 00 00 00 00 00 80 cc 14 40 01 00 00 00  [1] RLM+PROTECT+IFS
0x1402505d0: 10 00 00 00 00 00 00 00 90 cc 14 40 01 00 00 00  [2] RATE
0x1402505e0: 08 00 00 00 00 00 00 00 b0 cc 14 40 01 00 00 00  [3] SEC
0x1402505f0: 08 00 00 00 00 00 00 00 d0 cc 14 40 01 00 00 00  [4] QBSS
0x140250600: 28 00 00 00 00 00 00 00 f0 cc 14 40 01 00 00 00  [5] SAP
0x140250610: 08 00 00 00 00 00 00 00 30 cd 14 40 01 00 00 00  [6] P2P
0x140250620: 10 00 00 00 00 00 00 00 50 cd 14 40 01 00 00 00  [7] HE
0x140250630: 08 00 00 00 00 00 00 00 10 d0 14 40 01 00 00 00  [8] BSS_COLOR
0x140250640: 10 00 00 00 00 00 00 00 50 d1 14 40 01 00 00 00  [9] EHT
0x140250650: 08 00 00 00 00 00 00 00 00 d3 14 40 01 00 00 00  [10] MBSSID
0x140250660: 08 00 00 00 00 00 00 00 20 d3 14 40 01 00 00 00  [11] 0x0C
0x140250670: 14 00 00 00 00 00 00 00 40 d3 14 40 01 00 00 00  [12] MLD
0x140250680: 08 00 00 00 00 00 00 00 50 d3 14 40 01 00 00 00  [13] STAIoT
```

---

*逆向来源: mtkwecx.sys 二进制，pefile+capstone 反汇编，2026-02-22*
