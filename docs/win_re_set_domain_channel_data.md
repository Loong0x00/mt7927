# Windows RE: SET_DOMAIN Channel Data (ch_data1/ch_data2) 格式分析

**分析日期**: 2026-02-21
**来源**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (PE + capstone 汇编级逆向)
**分析方法**: pefile + capstone 反汇编，手工追踪数据流

---

## 结论摘要

ch_data1[32] + ch_data2[16] = **6 × CMD_SUBBAND_INFO (每条 8 字节)**

**大小验证**:
- MAX_SUBBAND_NUM = 6 (MT6639 定义)
- 6 × 8 = 48 字节 = ch_data1[32] + ch_data2[16] — 精确匹配

---

## 1. Windows Binary 逆向路径

### 关键函数链

```
0x1400ade8c (外层循环函数)
  → 遍历信道条目，每次迭代:
  → 0x1400cf534 (nicSetDomainInfo)
    → 填充 64-byte input buffer (含 ch_data 于 [0x10..0x3f])
    → 0x1400cdc4c (nicUniCmdAllocEntry, outer=7, size=0x40)
  → 发送 SET_DOMAIN UniCmd 给固件
  → 固件端 handler: 0x140145d30
    → nicUniCmdBufAlloc(CID=0x03, size=0x4c=76)
    → 将 input[0x10..0x2f] memcpy → firmware[0x1c..0x3b] (32 bytes)
    → 将 input[0x30..0x3f] memcpy → firmware[0x3c..0x4b] (16 bytes)
```

### Handler 关键汇编 (@ 0x140145efa)

```asm
; Handler @ 0x140145d30, r15 = input data pointer, rdi = firmware payload ptr
0x140145efa: lea   rdx, [r15 + 0x10]     ; rdx = input[0x10] = ch_data1 start
0x140145efe: mov   r8d, 0x20              ; size = 32 bytes
0x140145f04: lea   rcx, [rdi + 0x1c]     ; dst = firmware[0x1c]
0x140145f08: call  0x140010118            ; memcpy(fw[0x1c], input[0x10], 32)

0x140145f0d: lea   rdx, [r15 + 0x30]     ; rdx = input[0x30] = ch_data2 start
0x140145f11: mov   r8d, 0x10             ; size = 16 bytes
0x140145f17: lea   rcx, [rdi + 0x3c]     ; dst = firmware[0x3c]
0x140145f1b: call  0x140010118            ; memcpy(fw[0x3c], input[0x30], 16)
```

**结论**: ch_data1/ch_data2 被 **直接 memcpy** 到固件 payload，无变换。

---

## 2. nicSetDomainInfo (@ 0x1400cf534) 内部数据流

函数从 `channel_data_struct` 指针 (位于 input_param[0x10..0x17]) 读取信道数据：

```asm
; input_param[0x10..0x17] 是指针 ptr_to_ch_struct
0x1400cf693: mov   rdx, qword ptr [rbp + 0x17]   ; rdx = ch_struct ptr
0x1400cf697: mov   ebx, 8
0x1400cf69c: movzx r8d, byte ptr [rdx + 6]        ; ch_struct[6] = channel type
; switch on type → multiple cases, all ultimately:
0x1400cf85e: mov   al, [rdx + 7]                  ; ch_struct[7] = count bytes
0x1400cf861: lea   rcx, [rbp - 0x29]              ; dst = buf[0x10]
0x1400cf86d: add   rdx, rbx                        ; src = ch_struct + 8
0x1400cf870: call  0x140010118                     ; memcpy(buf[0x10], ch_struct+8, count)
```

**ch_struct 格式**:
- byte[6]: channel type (switch key, 1/2/3/4/5/6/0xb/0xc/0x11)
- byte[7]: N (data bytes count, ≤24)
- byte[8..8+N-1]: channel data → copied to buf[0x10..]

---

## 3. CMD_SUBBAND_INFO 结构体 (8 bytes)

从 MT6639 Android driver 头文件验证 (与固件 ABI 相同):

```c
/* Same with DOMAIN_SUBBAND_INFO */
struct CMD_SUBBAND_INFO {
    uint8_t ucRegClass;        /* [0] IEEE regulatory class */
    uint8_t ucBand;            /* [1] BAND_NULL=0, BAND_2G4=1, BAND_5G=2 */
    uint8_t ucChannelSpan;     /* [2] CHNL_SPAN_5=1(2.4G), CHNL_SPAN_20=4(5G) */
    uint8_t ucFirstChannelNum; /* [3] first channel number */
    uint8_t ucNumChannels;     /* [4] number of channels */
    uint8_t aucReserved[3];    /* [5] DFS/passive flag (0=no, 1=yes); [6][7]=0 */
};
```

**枚举值** (Windows binary + MT6639 确认):
```c
BAND_NULL = 0  // 无效
BAND_2G4  = 1  // 2.4 GHz
BAND_5G   = 2  // 5 GHz

CHNL_SPAN_5  = 1  // 5MHz 间隔 (2.4GHz 信道: ch1/2/3/...)
CHNL_SPAN_20 = 4  // 20MHz 间隔 (5GHz 信道: ch36/40/44/...)
```

---

## 4. 大小一致性验证

```
ch_data1[32] = 4 × CMD_SUBBAND_INFO (4 × 8 = 32 bytes) ✓
ch_data2[16] = 2 × CMD_SUBBAND_INFO (2 × 8 = 16 bytes) ✓
总计        = 6 × CMD_SUBBAND_INFO = 48 bytes          ✓
MAX_SUBBAND_NUM = 6 (mt6639/include/mgmt/rlm_domain.h) ✓
```

---

## 5. CN 域 (China) 推荐信道数据

**来源**: MT6639 rlm_domain.c CountryGroup7 (含 CN):

```c
/* ch_data1[32] = subbands 0-3 */
{81,  BAND_2G4, CHNL_SPAN_5,  1,  13, 0}  // 2.4GHz ch1-13 (passive=no)
{115, BAND_5G,  CHNL_SPAN_20, 36,  4, 0}  // 5GHz UNII-Low  ch36-48
{118, BAND_5G,  CHNL_SPAN_20, 52,  4, 1}  // 5GHz UNII-Mid  ch52-64 (DFS!)
{121, BAND_NULL,0,             0,   0, 0}  // UNII-WW 不可用 (placeholder)

/* ch_data2[16] = subbands 4-5 */
{125, BAND_5G,  CHNL_SPAN_20, 149, 5, 0}  // 5GHz UNII-Upper ch149-165
{0,   BAND_NULL,0,             0,   0, 0}  // 终止符
```

### 48-byte 十六进制值

```
/* ch_data1 [0x00..0x1f] = input[0x10..0x2f] */
51 01 01 01 0d 00 00 00  /* 2.4GHz ch1-13 (RegClass=81, Band=1, Span=1) */
73 02 04 24 04 00 00 00  /* 5GHz  ch36-48 (RegClass=115, Band=2, Span=4) */
76 02 04 34 04 01 00 00  /* 5GHz  ch52-64 (RegClass=118, DFS=1) */
79 00 00 00 00 00 00 00  /* NULL placeholder (RegClass=121) */

/* ch_data2 [0x20..0x2f] = input[0x30..0x3f] */
7d 02 04 95 05 00 00 00  /* 5GHz  ch149-165 (RegClass=125, 5 channels) */
00 00 00 00 00 00 00 00  /* terminator */
```

---

## 6. 完整 76-byte 固件 payload (含信道数据)

```c
struct set_domain_fw_payload {  /* 76 bytes, 直接发给固件 */
    /* [0x00] */ uint8_t  country_a;   /* 'C' */
    /* [0x01] */ uint8_t  country_b;   /* 'N' */
    /* [0x02] */ uint8_t  _rsv1[6];    /* 0 */
    /* [0x08] */ uint32_t flags;       /* 0x00440027 (LE: 27 00 44 00) */
    /* [0x0c] */ uint8_t  desc[10];    /* regulatory descriptor (0 for default) */
    /* [0x16] */ uint8_t  country_a2;  /* 'C' (alpha2[0] 再次) */
    /* [0x17] */ uint8_t  desc2[3];    /* 0 */
    /* [0x1a] */ uint8_t  country_b2;  /* 'N' (alpha2[1] 再次) */
    /* [0x1b] */ uint8_t  no_ir;       /* 0 */
    /* [0x1c] */ uint8_t  ch_data1[32];/* 4 × CMD_SUBBAND_INFO */
    /* [0x3c] */ uint8_t  ch_data2[16];/* 2 × CMD_SUBBAND_INFO */
} __packed;
```

---

## 7. 64-byte Input Buffer (我们需要构建的)

```c
/* 这是我们发送给 UniCmd handler 的 input data (64 bytes) */
struct set_domain_input {
    uint8_t  desc[10];        /* [0x00..0x09] regulatory desc (全零) */
    uint8_t  country_a;       /* [0x0a] = 'C' */
    uint8_t  desc_b;          /* [0x0b] = 0 */
    uint8_t  desc_c;          /* [0x0c] = 0 */
    uint8_t  desc_d;          /* [0x0d] = 0 */
    uint8_t  country_b;       /* [0x0e] = 'N' */
    uint8_t  no_ir;           /* [0x0f] = 0 */
    uint8_t  ch_data1[32];    /* [0x10..0x2f] → firmware[0x1c..0x3b] */
    uint8_t  ch_data2[16];    /* [0x30..0x3f] → firmware[0x3c..0x4b] */
} __packed;  /* Total: 64 bytes */
```

### 推荐填充值 (CN 域)

```c
static const struct {
    u8 country_a;
    u8 country_b;
    u8 desc[14];     /* [0x01..0x0f] = 0 */
    /* ch_data1[32] = 4 subbands */
    u8 sb0[8];  /* 2.4GHz ch1-13 */
    u8 sb1[8];  /* 5GHz  ch36-48 */
    u8 sb2[8];  /* 5GHz  ch52-64 (DFS) */
    u8 sb3[8];  /* NULL placeholder */
    /* ch_data2[16] = 2 subbands */
    u8 sb4[8];  /* 5GHz  ch149-165 */
    u8 sb5[8];  /* terminator */
} __packed set_domain_cn = {
    .country_a = 'C',
    .country_b = 'N',  /* at offset 0x0e */
    .sb0 = {81,  1, 1,   1, 13, 0, 0, 0},
    .sb1 = {115, 2, 4,  36,  4, 0, 0, 0},
    .sb2 = {118, 2, 4,  52,  4, 1, 0, 0},
    .sb3 = {121, 0, 0,   0,  0, 0, 0, 0},
    .sb4 = {125, 2, 4, 149,  5, 0, 0, 0},
    .sb5 = {0},
};
```

**注意**: country_a 在 input[0x0a]，country_b 在 input[0x0e] (两者间隔不等，来自 handler 代码验证)。

---

## 8. 代码实现指引 (src/mt7927_pci.c)

```c
static int mt7927_mcu_set_domain(struct mt7927_dev *dev)
{
    /* 完整 64-byte input buffer */
    struct {
        u8  desc[10];      /* [0x00..0x09] regulatory descriptor */
        u8  alpha2_0;      /* [0x0a] country code char 1 */
        u8  desc_b;        /* [0x0b] */
        u8  desc_c;        /* [0x0c] */
        u8  desc_d;        /* [0x0d] */
        u8  alpha2_1;      /* [0x0e] country code char 2 */
        u8  no_ir;         /* [0x0f] indoor restriction flag */
        u8  ch_data1[32];  /* [0x10..0x2f] 4 × subband */
        u8  ch_data2[16];  /* [0x30..0x3f] 2 × subband */
    } __packed req = {};

    req.alpha2_0 = 'C';
    req.alpha2_1 = 'N';
    req.no_ir    = 0;

    /* ch_data1: subbands 0-3 */
    /* subband[0]: 2.4GHz ch1-13 */
    req.ch_data1[0]  = 81;  req.ch_data1[1] = 1;
    req.ch_data1[2]  = 1;   req.ch_data1[3] = 1;
    req.ch_data1[4]  = 13;  /* 5..7 = 0 */

    /* subband[1]: 5GHz ch36-48 */
    req.ch_data1[8]  = 115; req.ch_data1[9]  = 2;
    req.ch_data1[10] = 4;   req.ch_data1[11] = 36;
    req.ch_data1[12] = 4;   /* 13..15 = 0 */

    /* subband[2]: 5GHz ch52-64 (DFS) */
    req.ch_data1[16] = 118; req.ch_data1[17] = 2;
    req.ch_data1[18] = 4;   req.ch_data1[19] = 52;
    req.ch_data1[20] = 4;   req.ch_data1[21] = 1; /* DFS flag */

    /* subband[3]: NULL placeholder (RegClass=121) */
    req.ch_data1[24] = 121; /* 25..31 = 0 */

    /* ch_data2: subbands 4-5 */
    /* subband[4]: 5GHz ch149-165 */
    req.ch_data2[0] = 125; req.ch_data2[1] = 2;
    req.ch_data2[2] = 4;   req.ch_data2[3] = 149;
    req.ch_data2[4] = 5;   /* 5..7 = 0 */
    /* subband[5]: terminator (all zero) */

    /* UniCmd: outer_tag=0x07, inner_CID=0x03, fire-and-forget */
    return mt7927_mcu_send_msg(dev, MCU_CMD(SET_DOMAIN),
                               &req, sizeof(req), false);
}
```

---

## 9. 验证备注

### Windows Binary 直接验证 (二进制级)
- **handler @ 0x140145d30**: 确认 outer_tag=7, size_check=0x40 (64 bytes)
- **memcpy @ 0x140145f08**: 确认 input[0x10..0x2f] → firmware[0x1c..0x3b]
- **memcpy @ 0x140145f1b**: 确认 input[0x30..0x3f] → firmware[0x3c..0x4b]

### MT6639 辅助验证 (作为参考，非权威)
- `MAX_SUBBAND_NUM = 6` (rlm_domain.h line 83)
- `CMD_SUBBAND_INFO` 结构体 (nic_cmd_event.h line 1850)
- CN domain group 7 subbands (rlm_domain.c line 603-617)

### 大小数学一致性
```
Windows binary: handler copies exactly 48 bytes (32+16) from input to firmware
MT6639 data:    MAX_SUBBAND_NUM=6 × sizeof(CMD_SUBBAND_INFO)=8 = 48 bytes
→ 完全匹配 ✓
```

---

## 10. 注意事项

1. **Windows 可能多次调用 SET_DOMAIN**: 外层函数 0x1400ade8c 有循环，每个信道组调用一次。我们简化为一次发送所有 6 个 subband。

2. **DFS 信道 (ch52-64)**: `aucReserved[0]=1` 表示 DFS 信道，固件可能不主动扫描。对于连接测试，5G UNII-Low (ch36-48) 和 UNII-Upper (ch149-165) 更重要。

3. **alpha2 字段**: country_a 在 input[0x0a]，country_b 在 input[0x0e] — 注意不是连续的。Handler 从这两个偏移分别读取。

4. **全零 ch_data = 固件认为无合法信道**: 之前发送全零导致 scan 不工作的根因之一。

---

## 相关文件

- `docs/win_re_payload_formats_detailed.md` — SET_DOMAIN 完整 76-byte 格式 (本文补充信道数据部分)
- `src/mt7927_pci.c:~1858` — SET_DOMAIN 当前实现 (需替换)
- `mt6639/mgmt/rlm_domain.c:603` — CN country group 7 subband 定义
