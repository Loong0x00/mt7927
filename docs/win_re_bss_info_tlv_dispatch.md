# Windows RE: BSS_INFO TLV Dispatch Table 完整逆向

**日期**: 2026-02-21 (Session 18)
**文件**: `mtkwecx.sys` (ImageBase=0x140000000)
**目标**: 逆向 BSS_INFO (CID=0x26) 命令的 14 个 TLV handler，重点获取 RATE/PROTECT/IFS_TIME 字段

---

## 1. 分发函数概览

- **nicUniCmdSetBssInfo**: VA=`0x14013e320`，构建完整 BSS_INFO 命令
- **Dispatch Table**: VA=`0x14023fac8`，stride=16字节，结构 `{ u64 handler_ptr; u64 alloc_size; }`
- **总分配大小**: `4 + Σ(alloc_size) + 40` = 约 260 字节缓冲区
- **实际 TLV 数**: 16 个（entry[1] 一次输出 RLM+PROTECT+IFS_TIME 三个）

---

## 2. 完整 14 Entry 映射表

| Entry | Handler VA | Alloc Size | TLV Tag(s) | 名称 |
|-------|-----------|-----------|-----------|------|
| [0]  | 0x1401423c0 | 0x2c (44B)| tag=0x0000 | **BASIC** |
| [1]  | 0x140145f90 | ~44B      | tag=0x0002+0x0003+0x0017 | **RLM+PROTECT+IFS_TIME** (三合一) |
| [2]  | 0x1401460f0 | 0x10 (16B)| tag=0x000b | **RATE** |
| [3]  | 0x1401464a0 | 0x08 (8B) | tag=0x0010 | **SEC** (安全/加密) |
| [4]  | 0x140146890 | 0x08 (8B) | tag=0x000f | **QBSS_LOAD** |
| [5]  | 0x140146a20 | 0x28 (40B)| tag=0x000d | **SAP** |
| [6]  | 0x140146a60 | 0x08 (8B) | tag=0x000e | **P2P/VHT** |
| [7]  | 0x140146a80 | 0x08 (8B) | tag=0x0005 | **HE_BASIC** |
| [8]  | 0x140146c10 | 0x10 (16B)| tag=0x0004 | **HT/BSS_COLOR** |
| [9]  | 0x140146d50 | 0x08 (8B) | tag=0x001e | **EHT/EML_CAP** |
| [10] | 0x140146f40 | 0x08 (8B) | tag=0x0006 | **MBSSID/band** |
| [11] | 0x140146f60 | 0x14 (20B)| tag=0x000c | **BSS_COLOR/WAPI** |
| [12] | 0x140146f80 | 0x08 (8B) | tag=0x001a | **MLD** (stub→0x140149568) |
| [13] | 0x140146f90 | 0x08 (8B) | tag=0x0018 | **MLD_BSSID** |

---

## 3. 关键 TLV 精确 Wire 格式

### 3.1 Entry[1]: nicUniCmdSetBssRlm (VA=0x14014a8fc)

**一次函数调用输出 3 个 TLV，总计 44 字节**

Stub `0x140145f90`: `add r8, 0x44` → `jmp 0x14014a8fc`
（所有偏移相对于 `bss_info_ptr + 0x44`）

#### TLV RLM (tag=0x0002, len=0x0010, 16字节)

```c
struct bss_tlv_rlm {
    u16 tag;              // = 0x0002
    u16 len;              // = 0x0010 (16)
    u8  primary_channel;  // bss_info[+0x46]
    u8  center_freq_seg0; // bss_info[+0x54]
    u8  center_freq_seg1; // bss_info[+0x55]
    u8  bw_mode;          // 枚举转换: 0→1, 1→2, 2→3, 3→6, 4/5→7
                          //   (from bss_info[+0x4f])
                          //   特殊: if bss_info[+0x47]==0 && bw!=0 → bw_mode=1
    u8  phy_type;         // bss_info[+0x58]
    u8  rlm_param;        // bss_info[+0x59]
    u8  dot11_mode;       // bss_info[+0x50]
    u8  prim_channel_idx; // bss_info[+0x47]
    u8  band;             // bss_info[+0x45] (0=2.4GHz, 1=5GHz)
    u8  reserved[3];      // = 0
};
```

#### TLV PROTECT (tag=0x0003, len=0x0008, 8字节)

```c
struct bss_tlv_protect {
    u16 tag;     // = 0x0003
    u16 len;     // = 0x0008 (8)
    u8  flags;   // 位域:
                 //   bit1 (0x02): non_erp_present (bss_info[+0x49] == 1)
                 //   bit2 (0x04):                 (bss_info[+0x49] == 2)
                 //   bit3 (0x08):                 (bss_info[+0x49] == 3 or 4)
                 //   bit5 (0x20): erp_ie_present  (bss_info[+0x48] != 0)
                 //   bit7 (0x80): short_slot_time (bss_info[+0x4a] == 1)
    u8  reserved[3]; // = 0
};
```

**5GHz 典型值**: flags=0x00（无 ERP 保护）

#### TLV IFS_TIME (tag=0x0017, len=0x0014, 20字节)

```c
struct bss_tlv_ifs_time {
    u16 tag;      // = 0x0017
    u16 len;      // = 0x0014 (20)
    u8  valid;    // = 1 (硬编码)
    u8  reserved; // = 0
    u16 sifs;     // (bss_info[+0x52] != 0) ? 9 : 20
                  //   非零(短时隙/5GHz) → sifs=9
                  //   零(长时隙/2.4GHz legacy) → sifs=20
    u8  data[12]; // = 0
};
```

---

### 3.2 Entry[2]: RATE TLV (VA=0x1401460f0)

```c
struct bss_tlv_rate {
    u16 tag;               // = 0x000b
    u16 len;               // = 0x0010 (16)
    u16 operational_rates; // bss_info[+0x2c] — BSS 运营速率集位图
    u16 extended_rates;    // bss_info[+0x2e] — 扩展速率集
    u8  reserved[8];       // = 0
};  // 16 bytes total
```

**注意**: `operational_rates` 和 `extended_rates` 的具体编码格式需进一步确认（可能是 802.11 速率集 bitmap 或驱动内部格式）。对于 5GHz auth 帧，至少需要包含 6Mbps 基本速率。

---

### 3.3 Entry[0]: BASIC TLV (VA=0x1401423c0)

标准 BASIC TLV，我们已实现。Windows 分配 44 字节，tag=0x0000，len=0x0020 (32字节)。

---

### 3.4 Entry[12]: MLD TLV (VA=0x140146f80 → 0x140149568)

我们已实现 MLD TLV（tag=0x001a）。该 handler 是一个 thunk，跳转到实际实现 `0x140149568`。

---

## 4. 我们驱动的 BSS_INFO 现状 vs Windows

| TLV | Tag | 我们发送 | Windows 发送 | 影响 |
|-----|-----|---------|------------|------|
| BASIC | 0x0000 | ✅ | ✅ | — |
| RLM   | 0x0002 | ✅ | ✅ | — |
| PROTECT | 0x0003 | ❌ | ✅ | **可能影响帧保护逻辑** |
| IFS_TIME | 0x0017 | ❌ | ✅ | **可能影响帧间间隔** |
| RATE  | 0x000b | ❌ | ✅ | **固件可能无法选择速率** |
| SEC   | 0x0010 | ❌ | ✅ | 加密配置（open auth=0） |
| HE_BASIC | 0x0005 | ❌ | ✅ | HE 能力（auth 帧可能不需要） |
| MLD   | 0x001a | ✅ | ✅ | — |
| MLD_BSSID | 0x0018 | ❌ | ✅ | 次要 |
| SAP/QBSS/P2P/EHT | 各种 | ❌ | ✅ | 通常非必须 |

---

## 5. 实现建议（按优先级）

### 立即添加（可能解除 TX 阻塞）

#### PROTECT TLV 实现
```c
// 5GHz auth: no ERP protection needed
u8 protect_flags = 0x00;
// 2.4GHz BSS with ERP stations: set appropriate bits
// For testing, use 0x00 unconditionally first
struct {
    __le16 tag;    // 0x0003
    __le16 len;    // 0x0008
    u8  flags;     // 0x00 for 5GHz
    u8  reserved[3];
} tlv_protect;
```

#### IFS_TIME TLV 实现
```c
struct {
    __le16 tag;    // 0x0017
    __le16 len;    // 0x0014
    u8  valid;     // 1
    u8  reserved;  // 0
    __le16 sifs;   // 9 for 5GHz (short slot), 20 for 2.4GHz legacy
    u8  padding[12]; // 0
} tlv_ifs_time;
```

#### RATE TLV 实现
```c
// 5GHz 802.11a 基本速率: 6/12/24 Mbps mandatory
// operational_rates 编码待确认，先用 0 测试
struct {
    __le16 tag;               // 0x000b
    __le16 len;               // 0x0010
    __le16 operational_rates; // TBD
    __le16 extended_rates;    // TBD
    u8  reserved[8];          // 0
} tlv_rate;
```

---

## 6. nicUniCmdSetBssRlm 调用关系

```
nicUniCmdSetBssInfo (0x14013e320)
  └─ dispatch_table[1].handler (0x140145f90)
       └─ jmp nicUniCmdSetBssRlm (0x14014a8fc)
            ├─ writes RLM  TLV (16B) at [out+0]
            ├─ writes PROTECT TLV (8B)  at [out+16]
            └─ writes IFS_TIME TLV (20B) at [out+24]
```

**关键**: Windows 在一次 BSS_INFO 命令中同时发送 RLM+PROTECT+IFS_TIME，**三者必须同时存在**。

---

## 7. 结论

Windows 驱动发 14 个 handler（实际输出 16 个 TLV）。我们当前只发 3 个（BASIC + RLM + MLD）。

**最高优先级补充**（按可能影响 TX 的重要程度）：
1. **PROTECT** (tag=0x0003) — 保护模式，固件帧调度依赖
2. **IFS_TIME** (tag=0x0017) — SIFS/slot time，帧发送时序
3. **RATE** (tag=0x000b) — BSS 合法速率集，固件需选速率
4. **SEC** (tag=0x0010) — 安全配置（open auth 时值为 0，可能安全发空）

以上 4 个 TLV 应与 BASIC+RLM+MLD 一起发送，将 TLV 数从 3 个扩展到 7 个。

---

*逆向来源: mtkwecx.sys binary analysis via pefile+capstone, 2026-02-21*
