# Windows RE: SET_DOMAIN / BAND_CONFIG / DMASHDL 精确 Payload 格式

**分析日期**: 2026-02-21
**来源**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (PE + capstone 汇编级逆向)
**分析方法**: pefile 解析 PE 节, capstone 反汇编, 手工追踪指令级数据流

---

## 关键更正: nicUniCmdBufAlloc 实际结构

之前分析 (`docs/win_re_scan_domain_band_payloads.md`) **错误地**将 `alloc_size - 0x18` 作为 firmware payload 大小。

**正确结构** (从 `nicUniCmdBufAlloc @ 0x14014f788` 汇编验证):

```c
// nicUniCmdBufAlloc(adapter, inner_CID, size_param) 实现:
total_alloc = size_param + 0x28    // ExAllocatePoolWithTag(total_alloc, 'MK54')
alloc_buf[0x18] = &alloc_buf[0x28] // 存储 payload 指针
alloc_buf[0x28+0x10] = inner_CID
alloc_buf[0x28+0x14] = size_param
alloc_buf[0x28+0x20] = 0xff
// 返回 alloc_buf

// Handler 读取:
rdi = *(alloc_buf + 0x18) = alloc_buf + 0x28  // payload 起始地址
// Handler 写入 rdi[0x00..size_param-1]
```

**正确 firmware payload 大小**:
- SET_DOMAIN: `size_param = 0x4c = 76 bytes` (之前文档说 52 bytes → **错误**)
- BAND_CONFIG: `size_param = 0x5c = 92 bytes` (之前文档说 68 bytes → **错误**)
- SCAN_CFG: `size_param = 0x150 = 336 bytes` (与我们发送的一致 → **正确**)

---

## 1. SET_DOMAIN (outer=0x07, inner CID=0x03)

### Handler 信息
- **Handler VA**: `0x140145d30`
- **Input 结构**: `cmp byte ptr [rdx], 0x07` (outer_tag check)
- **Input 大小验证**: `cmp dword ptr [rdx + 0x10], 0x40` → 输入必须 0x40 = 64 bytes
- **Input 数据指针**: `r15 = qword ptr [rdx + 0x18]`
- **Alloc 调用**: `nicUniCmdBufAlloc(adapter, CID=0x03, size=0x4c)` → 76 bytes payload
- **Caller function VA**: `0x1400cf890..0x1400cf925` (nicSetDomainInfo 类似函数)
- **调用 nicUniCmdAllocEntry**: `0x1400cf8c5` (dl=7, r8b=0xed, [rsp+0x38]=0x40)

### 完整 Firmware Payload 格式 (76 bytes = 0x4c)

从 handler `0x140145dd1..0x140145f1b` 汇编逐字节验证:

```c
struct set_domain_fw_payload {    // 76 bytes, 直接发给固件
    u8  country_a;    // [0x00] = input[0x0a]  (alpha2 字符 1)
    u8  country_b;    // [0x01] = input[0x0e]  (alpha2 字符 2)
    u8  _rsv1[4];     // [0x02..0x05] = 0 (未写入)
    u8  _rsv2;        // [0x06] = 0 (显式置零: `mov byte ptr [rdi+6], 0`)
    u8  _rsv3;        // [0x07] = 0 (未写入)
    u32 flags;        // [0x08..0x0b] = 0x00440027 (硬编码)
                      //   LE bytes: [0x08]=0x27, [0x09]=0x00, [0x0a]=0x44, [0x0b]=0x00
    u8  desc_0;       // [0x0c] = input[0x00]  (regulatory descriptor byte 0)
    u8  desc_1;       // [0x0d] = input[0x01]
    u8  desc_2;       // [0x0e] = input[0x02]
    u8  desc_3;       // [0x0f] = input[0x03]
    u32 desc_4to7;    // [0x10..0x13] = input[0x04..0x07]  (dword)
    u16 desc_8to9;    // [0x14..0x15] = input[0x08..0x09]  (word)
    u8  country_a2;   // [0x16] = input[0x0a]  (alpha2[0], 再次)
    u8  desc_b;       // [0x17] = input[0x0b]
    u8  desc_c;       // [0x18] = input[0x0c]
    u8  desc_d;       // [0x19] = input[0x0d]
    u8  country_b2;   // [0x1a] = input[0x0e]  (alpha2[1], 再次)
    u8  no_ir;        // [0x1b] = 1 (如果 indoor mode) 或 input[0x0f]
    u8  ch_data1[32]; // [0x1c..0x3b] = memcpy(input[0x10..0x2f], 0x20 bytes)
    u8  ch_data2[16]; // [0x3c..0x4b] = memcpy(input[0x30..0x3f], 0x10 bytes)
} __packed;
// Total = 76 bytes
```

**关键汇编指令对应** (handler @ 0x140145dd1):
```asm
mov al, [r15 + 0x0a] → [rdi + 0x00]     ; alpha2[0]
mov al, [r15 + 0x0e] → [rdi + 0x01]     ; alpha2[1]
mov [rdi + 6], 0                          ; explicit zero
mov dword [rdi + 8], 0x440027             ; flags = 0x00440027 LE
mov al, [r15 + 0x00..0x03] → [rdi + 0x0c..0x0f]  ; 4 bytes
mov eax, [r15 + 0x04] → [rdi + 0x10]    ; dword
movzx eax, [r15 + 0x08] → [rdi + 0x14] ; word
mov al, [r15 + 0x0a..0x0e] → [rdi + 0x16..0x1a]  ; 5 bytes
mov al, [r15 + 0x0f] → [rdi + 0x1b]    ; no_ir (or 1 if indoor)
lea rcx, [rdi + 0x1c]; lea rdx, [r15 + 0x10]; r8d=0x20 → memcpy 32B
lea rcx, [rdi + 0x3c]; lea rdx, [r15 + 0x30]; r8d=0x10 → memcpy 16B
```

### input 结构 (64 bytes，Windows 内部格式)

从调用上下文 `0x1400cf890` 分析:
- `input[0x0a]` = alpha2 字符 1 (e.g., 'C')
- `input[0x0e]` = alpha2 字符 2 (e.g., 'N')
- `input[0x00..0x09]` = regulatory domain descriptor (10 bytes，来自 NDIS 配置)
- `input[0x0b..0x0f]` = 更多 descriptor 字节 + no_ir flag
- `input[0x10..0x2f]` = 32 bytes 信道数据 (动态构建)
- `input[0x30..0x3f]` = 16 bytes 更多信道数据

### 我们当前的代码 vs Windows 格式

**当前代码** (`src/mt7927_pci.c ~1858`):
```c
// 发送可变长 channel list TLV: ~400+ bytes — 完全错误!
struct {
    u8 alpha2[4];   // "CN\0\0"
    u8 bw_2g, bw_5g, bw_6g, pad;
} + TLV(n_channels × struct_entry) /* 完全不同格式 */
```

**应该发送**: 76 bytes 固定格式，如上结构定义。

### 推荐的 Minimal Fix Payload (世界域 "00")

```c
struct set_domain_fw_payload {
    .country_a   = '0',    // world domain
    .country_b   = '0',
    ._rsv1       = {0},
    ._rsv2       = 0,
    ._rsv3       = 0,
    .flags       = 0x00440027,  // hardcoded: LE bytes {0x27,0x00,0x44,0x00}
    .desc_0to3   = {0, 0, 0, 0},   // regulatory descriptor zeros
    .desc_4to7   = 0,
    .desc_8to9   = 0,
    .country_a2  = '0',
    .desc_b_to_d = {0, 0, 0},
    .country_b2  = '0',
    .no_ir       = 0,
    .ch_data1    = {channel bitmap 32B for 2.4G+5G},
    .ch_data2    = {extra channel data 16B},
};
```

**⚠️ 关键**: `ch_data1/ch_data2` 不能全零！固件会认为没有合法信道 → scan 仍失败。需要进一步 RE 信道数据格式，或从 cfg80211 regulatory 数据构建。

---

## 2. BAND_CONFIG (outer=0x93, inner CID=0x49)

### Handler 信息
- **Handler VA**: `0x140146950`
- **Input 结构**: `cmp byte ptr [rdx], 0x93` (outer_tag check)
- **Input 大小验证**: `cmp dword ptr [rdx + 0x10], 0x54` → 输入必须 0x54 = 84 bytes
- **Input 数据指针**: `rsi = qword ptr [rdx + 0x18]`
- **Alloc 调用**: `nicUniCmdBufAlloc(adapter, CID=0x49, size=0x5c)` → 92 bytes payload
- **外层函数 VA**: `0x1400d15d0` (nicBandConfig)
- **nicBandConfig 调用者**: `0x14000b4e8` (OID handler dispatch)

### 实际调用场景 (从 0x14000b4e8 逆向)

```
// OID dispatch case 1: band_idx=0 (2.4GHz), r8w=0
xor r8d, r8d; xor edx, edx; call nicBandConfig  // 但! cmp dl,1 → jne → 不发 BAND_CONFIG

// OID dispatch case 2: band_idx=1 (5GHz), r8w=0
xor r8d, r8d; mov dl, 1; call nicBandConfig      // cmp dl,1 → je → 发 BAND_CONFIG
```

**关键**: BAND_CONFIG (outer=0x93) 仅在 band_idx=1 (5GHz) 时发送！
2.4GHz 走不同路径 (0x1400c0dd7 之后的命令)。

### nicBandConfig (0x1400d15d0) 逻辑

```c
void nicBandConfig(adapter, rdx_band_data_84B) {
    r12d = 0x54;                   // input_size = 84
    memset(rsp+0x60, 0, 0x54);    // zero local buffer
    memcpy(rsp+0x60, rdx, 0x54);  // copy input to local
    // check chip_id: 0x6639/0x738/0x7927/0x7925/0x717
    // rcx = [rbx + 0x14c0] (adapter)
    nicUniCmdAllocEntry(adapter, outer=0x93, option=0xed,
        input=rsp+0x60, input_size=0x54);
}
```

### input 结构在调用点 (0x1400c0dcd) 的值

从 `0x1400c0ce8` 函数分析 (参数: rcx=container, dl=band_idx, r8w=capability):

```c
// 在 cmp dl,1 为真时进入 BAND_CONFIG 路径
// esi = 2 (from `mov esi, 2` at 0x1400c0d36, 始终设置)
// edi = 0 (from `xor edi, edi` at 0x1400c0d7d)
// r15d = r8w (from function arg r8)
// r14b = dl = 1 (band_idx)

input[0x00] = 0           // not set
input[0x01] = 1           // [rbp-0x28] = 1 (硬编码)
input[0x02..0x03] = 0    // not set
input[0x04..0x05] = 2    // word [rbp-0x25] = si = 2 (硬编码)
input[0x06] = 0           // [rbp-0x23] = dil = 0
input[0x07] = 0           // not set
input[0x08..0x09] = 2    // word [rbp-0x21] = si = 2 (硬编码)
input[0x0a..0x0b] = r15w // [rbp-0x1f] = r15w (= r8w parameter, 典型值=0)
input[0x0c..0x13] = 0    // [rbp-0x1d] = rdi = 0 (qword)
input[0x14..0x15] = 0    // [rbp-0x15] = di = 0
// input[0x16..0x53] = 0 (未设置, 已 memset)
```

### 完整 Firmware Payload 格式 (92 bytes = 0x5c)

从 handler `0x1401469e1..0x140146a12` 汇编逐字节验证:

```c
struct band_config_fw_payload {   // 92 bytes
    u8  _rsv[4];      // [0x00..0x03] = 0 (未写入)
    u32 flags;        // [0x04..0x07] = 0x00580000 (硬编码)
                      //   LE bytes: [0x04]=0x00, [0x05]=0x00, [0x06]=0x58, [0x07]=0x00
    u8  _rsv2;        // [0x08] = 0 (未写入)
    u8  field_09;     // [0x09] = input[0x01]  (典型值=1)
    u8  _rsv3[2];     // [0x0a..0x0b] = 0 (未写入)
    u8  field_0c;     // [0x0c] = input[0x04]  (典型值=2)
    u8  field_0d;     // [0x0d] = input[0x05]  (典型值=0)
    u8  field_0e;     // [0x0e] = input[0x06]  (典型值=0)
    u8  _rsv4;        // [0x0f] = 0 (未写入)
    u8  data[14];     // [0x10..0x1d] = memcpy(input[0x08..0x15], 14 bytes)
                      //   典型值: {0x02,0x00, r15w_lo,r15w_hi, 0,0,0,0,0,0,0,0,0,0}
    u8  _unused[62];  // [0x1e..0x5b] = 0 (未写入, 全零)
} __packed;
// Total = 92 bytes
```

**关键汇编指令对应** (handler @ 0x1401469e1):
```asm
mov rcx, [rax + 0x18]              ; rcx = payload_ptr (= alloc+0x28)
lea rdx, [rsi + 8]                 ; rdx = input+8
mov r8d, 0xe                       ; r8 = 14 (memcpy size)
mov dword [rcx + 4], 0x580000      ; [0x04..0x07] = 0x00580000 LE
mov al, [rsi + 1] → [rcx + 9]     ; input[0x01] → firmware[0x09]
mov al, [rsi + 4] → [rcx + 0xc]   ; input[0x04] → firmware[0x0c]
mov al, [rsi + 5] → [rcx + 0xd]   ; input[0x05] → firmware[0x0d]
mov al, [rsi + 6] → [rcx + 0xe]   ; input[0x06] → firmware[0x0e]
add rcx, 0x10                      ; rcx = payload+0x10
call memcpy(rcx, rdx, 0xe)        ; input[8..21] → firmware[0x10..0x1d]
```

### 典型 Firmware Payload Bytes (5GHz band, r8w=0)

```
0x00: 00 00 00 00 00 00 58 00  00 01 00 00 02 00 00 00
0x10: 02 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0x20: 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
      ... (全零到 0x5b)
```

### 我们当前的代码 vs Windows 格式

**当前代码** (`src/mt7927_pci.c ~1959`):
```c
// 发送 RTS threshold TLV: 16 bytes — 完全错误！格式和大小都不对
struct {
    u8 band_idx; u8 _rsv[3];
    __le16 tag = UNI_BAND_CONFIG_RTS_THRESHOLD;
    __le16 len; __le32 len_thresh; __le32 pkt_thresh;
} = 16 bytes
```

**应该发送**: 92 bytes 固定格式，如上结构定义。

---

## 3. DMASHDL 寄存器 0xd6060 — BIT(0)/BIT(8)/BIT(16) 含义

### Binary 验证

函数 @ `0x1401d7738` (PostFwDownloadInit 第一步, 调用者 `0x1401cf5ee`):

```asm
0x1401d773e  and dword [rsp+0x38], 0       ; 清零 read 结果 buffer
0x1401d7743  lea r8, [rsp+0x38]            ; r8 = 结果指针
0x1401d7748  mov edx, 0x7c026060           ; 编码地址 (→ BAR0+0xd6060)
0x1401d7750  call 0x1400099ac             ; readl(adapter, 0x7c026060, &result)
0x1401d7755  mov r8d, [rsp+0x38]           ; r8 = 读取值
0x1401d775a  mov edx, 0x7c026060
0x1401d775f  or r8d, 0x10101              ; 仅 OR 三个 bit!
0x1401d7769  mov [rsp+0x38], r8d           ; 存回
0x1401d776e  call 0x140009a18             ; writel(adapter, 0x7c026060, new_value)
0x1401d7778  ret
```

**操作**: `readl(0xd6060) |= 0x10101`
- BIT(0) = 0x000001
- BIT(8) = 0x000100
- BIT(16) = 0x010000

### 寄存器意义

MT_HIF_DMASHDL_QUEUE_MAP0 (0xd6060) 是 DMASHDL Queue Map 寄存器。
三个 bit 各控制一个 HW 队列的使能:
- BIT(0): 使能 DMASHDL 队列 0 (对应某 TX 队列)
- BIT(8): 使能 DMASHDL 队列 1 (or queue group 1)
- BIT(16): 使能 DMASHDL 队列 2 (or queue group 2)

**关键**: Windows 用的是 **OR** 操作 (只加bit，不清其他bit), 而我们当前做了 full reconfig (15+ 寄存器全写)。

### 我们的当前问题

```c
// 当前错误做法 (src/mt7927_pci.c ~xxx):
mt7927_dmashdl_init(dev);  // full reconfig — 覆盖固件自己的配置!

// Windows 正确做法:
writel(readl(bar0 + 0xd6060) | 0x10101, bar0 + 0xd6060);
```

---

## 4. SCAN_CFG (outer=0xca, inner CID=0x0e) — 确认正确

**好消息**: 我们当前 SCAN_CFG 发送 336 bytes 与 Windows 的 `size_param=0x150=336` 完全吻合！

Handler @ `0x140143a70` (outer_tag=0xca) 验证:
- `nicUniCmdBufAlloc(adapter, CID=0x0e, size=0x150)` → **336 bytes payload**
- payload[4..7] = 0x014c0002 (hardcoded dword)
- 我们的 {tag=0x0002, len=0x014c} LE = dword 0x014c0002 → **匹配！**

唯一潜在问题: payload[0x08..0x0b] (config_id/type/resp_type) 从 Windows 输入结构读取，可能需要非零值。**但格式正确**。

---

## 5. 修复优先级

| 修复项 | 影响 | 优先级 |
|--------|------|--------|
| SET_DOMAIN: 用 76-byte 固定格式替换当前 TLV 格式 | scan 无法工作 | 🔴 最高 |
| SET_DOMAIN: channel data 字段填充 | scan 找到 0 信道 | 🔴 最高 |
| DMASHDL: 替换 full reconfig 为 `|= 0x10101` | TX 可能静默失败 | 🔴 高 |
| BAND_CONFIG: 用 92-byte 格式替换 RTS TLV | 5GHz TX 配置错误 | 🟡 中 |
| SCAN_CFG: 检查 config_id 非零值 | 可能影响 scan 类型 | 🟡 低 |

---

## 6. 代码修复指引

### SET_DOMAIN 修复 (src/mt7927_pci.c)

```c
/* 替换当前可变长 TLV 格式, 使用 Windows 精确格式 */
static int mt7927_mcu_set_domain(struct mt7927_dev *dev)
{
    struct {
        u8  country_a;    /* [0x00] alpha2[0] */
        u8  country_b;    /* [0x01] alpha2[1] */
        u8  _rsv1[6];     /* [0x02..0x07] = 0 */
        __le32 flags;     /* [0x08..0x0b] = 0x00440027 */
        u8  desc[10];     /* [0x0c..0x15] = regulatory descriptor */
        u8  country_a2;   /* [0x16] alpha2[0] again */
        u8  desc2[3];     /* [0x17..0x19] more descriptor */
        u8  country_b2;   /* [0x1a] alpha2[1] again */
        u8  no_ir;        /* [0x1b] = 0 (or 1 if indoor) */
        u8  ch_data1[32]; /* [0x1c..0x3b] channel data */
        u8  ch_data2[16]; /* [0x3c..0x4b] extra channel data */
    } __packed req = {
        .country_a  = '0',   /* world domain */
        .country_b  = '0',
        .flags      = cpu_to_le32(0x00440027),
        /* alpha2 duplicate */
        .country_a2 = '0',
        .country_b2 = '0',
        /* ch_data: 需要正确的 channel bitmap — 关键待解决! */
    };
    /* Total: 76 bytes */
    return mt7927_mcu_send_msg(dev, MCU_CMD(SET_DOMAIN),
                                &req, sizeof(req), false);
}
```

### BAND_CONFIG 修复 (src/mt7927_pci.c)

```c
/* 替换当前 RTS threshold TLV, 使用 Windows 精确格式 */
static int mt7927_mcu_set_band_config(struct mt7927_dev *dev, u8 band_idx)
{
    struct {
        u8  _rsv[4];      /* [0x00..0x03] = 0 */
        __le32 flags;     /* [0x04..0x07] = 0x00580000 */
        u8  _rsv2;        /* [0x08] = 0 */
        u8  field_09;     /* [0x09] = 1 */
        u8  _rsv3[2];     /* [0x0a..0x0b] = 0 */
        u8  field_0c;     /* [0x0c] = 2 */
        u8  _rsv4[2];     /* [0x0d..0x0e] = 0 */
        u8  _rsv5;        /* [0x0f] = 0 */
        u8  data[14];     /* [0x10..0x1d] = {2, 0, 0, ...} */
        u8  _unused[62];  /* [0x1e..0x5b] = 0 */
    } __packed req = {
        .flags    = cpu_to_le32(0x00580000),
        .field_09 = 1,
        .field_0c = 2,
        .data     = { 0x02, 0x00 },   /* word 2 at data[0..1] */
    };
    /* Total: 92 bytes */
    return mt7927_mcu_send_msg(dev, MCU_CMD(BAND_CONFIG),
                                &req, sizeof(req), false);
}
```

### DMASHDL 修复 (src/mt7927_pci.c)

```c
/* PostFwDownloadInit Step 1: 替换 full reconfig */
static void mt7927_dmashdl_partial_init(struct mt7927_dev *dev)
{
    u32 val;
    /* Windows: readl(0xd6060) |= 0x10101 */
    val = mt7927_mmio_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0);
    dev_dbg(dev->mt76.dev, "DMASHDL_MAP0 before: 0x%08x\n", val);
    val |= 0x10101;  /* BIT(0) | BIT(8) | BIT(16) */
    mt7927_mmio_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP0, val);
}
/* 删除原来的 mt7927_dmashdl_init() 全量写入调用 */
```

---

## 7. SET_DOMAIN Channel Data 格式分析

⚠️ **未解决**: `ch_data1[32]` 和 `ch_data2[16]` 的具体格式未从 Windows binary 完全逆向。

**已知**:
- 这 48 bytes 来自 Windows 输入结构的 `input[0x10..0x3f]`
- 该输入由 `0x1400cf200` 附近的函数从 regulatory domain 配置动态构建
- 涉及 adapter+0x2e0..0x2e5 等 regulatory 相关字段

**下一步**:
1. 分析 `0x1400cf200` 函数: 它从 `[rcx + 0x2e0..0x2e5]` 读取 regulatory 参数并构建 channel data
2. 或从 MT6639 Android 驱动中找 `wlanoidSetCountryCode` 对应的 firmware payload 格式
3. 或发送 "world domain" 的所有信道都开放的 bitmap

---

## 8. 相关文件

- `docs/win_re_scan_domain_band_payloads.md` — 之前分析 (payload 大小有误, 已在本文更正)
- `docs/win_re_cid_mapping.md` — CID dispatch table
- `src/mt7927_pci.c:~1858` — SET_DOMAIN 当前实现 (需替换)
- `src/mt7927_pci.c:~1959` — BAND_CONFIG 当前实现 (需替换)
- `src/mt7927_pci.c` — PostFwDownloadInit DMASHDL init (需修改)
