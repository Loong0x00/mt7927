# Windows RE: ScanCfg / SET_DOMAIN / BAND_CONFIG Payload Formats

**分析日期**: 2026-02-21
**来源**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (pefile + capstone 汇编级逆向)
**目的**: CID bisect 支持 — 记录三个命令的正确 payload 格式

---

## 快速参考: 关键差异

| 命令 | outer | inner | Windows 固件 payload 大小 | 我们的 payload 大小 | 差异 |
|------|-------|-------|--------------------------|---------------------|------|
| SCAN_CFG | 0xca | 0x0e | 0x138 bytes (0x150 alloc - 0x18 hdr) | 336 bytes | 内容差异 |
| SET_DOMAIN | 0x07 | 0x03 | 0x34 bytes (0x4c alloc - 0x18 hdr) | 可变长 (>0x34) | **格式/大小都错** |
| BAND_CONFIG | 0x93 | 0x49 | 0x44 bytes (0x5c alloc - 0x18 hdr) | 16 bytes | **格式/大小都错** |

---

## 1. SCAN_CFG (outer=0xca, inner CID=0x0e)

### Handler 信息
- **Handler VA**: `0x0140143a70`
- **Input 大小验证**: `cmp dword ptr [rdx + 0x10], 0x148` → 输入结构必须 0x148 = 328 bytes
- **额外验证**: `cmp word ptr [r14 + 4], 0x140` → payload+4 处的 word ≤ 0x140
- **分配大小**: `nicUniCmdBufAlloc(adapter, CID=0x0e, size=0x150)` → 336 bytes 总分配
- **固件 payload 位置**: `alloc_result + 0x18` → payload = 0x150 - 0x18 = **0x138 = 312 bytes**

### Windows 固件 payload 结构 (handler 在 rsi 处构建)

```c
struct chip_config_fw_payload {          // 固件接收的 payload
    u8  _rsv[4];        // [0x00..0x03]  = 0 (未设置)
    u32 flags;          // [0x04..0x07]  = 0x014c0002 (硬编码)
                        //   byte[4] = 0x02  (type field)
                        //   byte[5] = 0x00
                        //   byte[6] = 0x4c  (= 76 = alloc_size - 0x10? 或 TLV len)
                        //   byte[7] = 0x01
    u16 config_id;      // [0x08..0x09]  从 Windows input[0..1]
    u8  type;           // [0x0a]        从 Windows input[2]
    u8  resp_type;      // [0x0b]        从 Windows input[3]
    u16 data_len;       // [0x0c..0x0d]  从 Windows input[4..5], 必须 ≤ 0x140
    u8  _rsv2[2];       // [0x0e..0x0f]  = 0
    u8  data[data_len]; // [0x10..]      字符串数据 (从 Windows input[8])
};
// 总大小 = 0x138 bytes (payload area after alloc header)
```

### 我们的当前 payload (src/mt7927_pci.c line ~1716)

```c
struct {
    u8   rsv[4];        // [0x00..0x03] = 0         ← 正确
    __le16 tag;         // [0x04..0x05] = 2 (0x0002) ← 巧合正确! tag=2 → byte[4]=0x02
    __le16 len;         // [0x06..0x07] = sizeof-4   ← 巧合正确! len=332=0x014c → byte[7]=0x01
                        // 注意: {tag, len} 组合 = {0x0002, 0x014c} 在内存中是
                        //       bytes {0x02, 0x00, 0x4c, 0x01} = dword 0x014c0002 ← 匹配!
    __le16 id;          // [0x08..0x09] = 0           ← config_id = 0 (可能需要特定值)
    u8   type;          // [0x0a]       = 0           ← type = 0 (可能需要特定值)
    u8   resp_type;     // [0x0b]       = 0           ← resp_type = 0
    __le16 data_size;   // [0x0c..0x0d] = str_len     ← 正确! 匹配 data_len
    __le16 data_resv;   // [0x0e..0x0f] = 0           ← 正确
    u8   data[320];     // [0x10..]     = 字符串       ← 正确
} __packed;             // sizeof = 336 bytes
```

### 差异分析

**格式差异**: 基本正确 — tag+len 的字节布局意外地与 Windows 硬编码的 0x014c0002 吻合。
**大小差异**: 我们发 336 bytes, Windows 固件 payload 是 312 bytes。差 24 bytes (=data[320] vs data[296])。固件可能只读前 312 bytes, 尾部多余数据无害。

**重要 TODO**: Windows 的 config_id (output[0x08]) 从何处获取? 对于 PassiveToActiveScan 和 KeepFullPwr 可能是非零值。需要进一步 RE 确认输入结构内容。

---

## 2. SET_DOMAIN (outer=0x07, inner CID=0x03)

### Handler 信息
- **Handler VA**: `0x0140145d30`
- **Input 大小验证**: `cmp dword ptr [rdx + 0x10], 0x40` → 输入结构必须 0x40 = **64 bytes**
- **分配大小**: `nicUniCmdBufAlloc(adapter, CID=0x03, size=0x4c)` → 76 bytes 总分配
- **固件 payload 位置**: `alloc_result + 0x18` → payload = 0x4c - 0x18 = **0x34 = 52 bytes**

### Windows 固件 payload 结构 (handler 在 rdi 处构建)

汇编来源 (0x140145dd1 - 0x140145f4b):

```c
struct set_domain_fw_payload {           // 固件接收的 payload (52 bytes)
    u8  country_code_a;  // [0x00]  = input[0x0a] (第一个 alpha2 字符)
    u8  country_code_b;  // [0x01]  = input[0x0e] (第二个 alpha2 字符)
    u8  _rsv1[4];        // [0x02..0x05] = 0
    u8  _rsv2;           // [0x06]  = 0 (显式置零)
    u8  _rsv3;           // [0x07]  = 0
    u32 flags;           // [0x08..0x0b] = 0x00440027 (硬编码)
                         //   byte[8]  = 0x27
                         //   byte[9]  = 0x00
                         //   byte[10] = 0x44
                         //   byte[11] = 0x00
    u8  field_0c[4];     // [0x0c..0x0f] = input[0x00..0x03]
    u32 field_10;        // [0x10..0x13] = input[0x04..0x07]
    u16 field_14;        // [0x14..0x15] = input[0x08..0x09]
    u8  field_16;        // [0x16]  = input[0x0a] (alpha2[0], 再次)
    u8  field_17;        // [0x17]  = input[0x0b]
    u8  field_18;        // [0x18]  = input[0x0c]
    u8  field_19;        // [0x19]  = input[0x0d]
    u8  field_1a;        // [0x1a]  = input[0x0e] (alpha2[1], 再次)
    u8  no_ir_flag;      // [0x1b]  = input[0x0f] 或 1 (if adapter is in indoor mode)
    u8  data_32[0x20];   // [0x1c..0x3b] = memcpy from input[0x10..0x2f] (32 bytes)
    u8  data_16[0x10];   // [0x3c..0x4b] = memcpy from input[0x30..0x3f] (16 bytes)
};                       // Total = 0x4c bytes
```

**注意**: `input` 是 Windows 传给 handler 的 64-byte 内部结构, 具体字段含义需要进一步 RE。
- input[0x00..0x09]: 前 10 bytes — 可能是某种 channel domain descriptor
- input[0x0a]: alpha2[0] (country code char 1)
- input[0x0e]: alpha2[1] (country code char 2)
- input[0x10..0x2f]: 32 bytes domain data (channel bitmap? regulatory rules?)
- input[0x30..0x3f]: 16 bytes additional data

### 我们的当前 payload (src/mt7927_pci.c line ~1858)

```c
// 我们发送的结构 (变长):
struct hdr {
    u8 alpha2[4];    // "CN\0\0"
    u8 bw_2g;        // = 0 (BW_20_40)
    u8 bw_5g;        // = 3 (BW_20_40_80_160)
    u8 bw_6g;        // = 3
    u8 pad;
} + TLV(tag=2, len=variable) {
    u8 n_2ch, n_5ch, n_6ch, pad;
    struct chan_entry[n_2ch + n_5ch] {
        __le16 hw_value;
        __le16 pad;
        __le32 flags;
    }
};
// 总大小: 8 + 8 + N*8 = variable (for 14+36 channels = ~400+ bytes)
```

### 差异分析 ⚠️ 严重不匹配

- **格式**: 完全不同。我们发可变长 channel list 格式, 固件期望固定 52-byte 格式
- **大小**: 我们发 ~400+ bytes, 固件期望 52 bytes
- **内容**: alpha2 位置不同 (我们在 [0..1], Windows 在 payload[0][1])
- **关键问题**: 固件收到 CID=0x03 但格式错误, 可能静默拒绝导致信道域未初始化

**修复方向**: 需要搞清楚 Windows input[0x00..0x0f] 和 input[0x10..0x3f] 的内容。
可能需要进一步 RE Windows 中调用 SET_DOMAIN handler 的上层代码。

---

## 3. BAND_CONFIG (outer=0x93, inner CID=0x49)

### Handler 信息
- **Handler VA**: `0x0140146950`
- **Input 大小验证**: `cmp dword ptr [rdx + 0x10], 0x54` → 输入结构必须 0x54 = **84 bytes**
- **分配大小**: `nicUniCmdBufAlloc(adapter, CID=0x49, size=0x5c)` → 92 bytes 总分配
- **固件 payload 位置**: `alloc_result + 0x18` → payload = 0x5c - 0x18 = **0x44 = 68 bytes**

### Windows 固件 payload 结构 (handler 在 rcx 处构建)

汇编来源 (0x1401469e1 - 0x140146a32):

```c
struct band_config_fw_payload {          // 固件接收的 payload (68 bytes)
    u8  _rsv[4];      // [0x00..0x03] = 0 (未设置)
    u32 flags;        // [0x04..0x07] = 0x00580000 (硬编码, little-endian)
                      //   byte[4] = 0x00
                      //   byte[5] = 0x00
                      //   byte[6] = 0x58
                      //   byte[7] = 0x00
    u8  _rsv2;        // [0x08]  = 0
    u8  field_09;     // [0x09]  = input[0x01]
    u8  _rsv3[2];     // [0x0a..0x0b] = 0
    u8  field_0c;     // [0x0c]  = input[0x04]
    u8  field_0d;     // [0x0d]  = input[0x05]
    u8  field_0e;     // [0x0e]  = input[0x06]
    u8  _rsv4;        // [0x0f]  = 0
    u8  data[0x0e];   // [0x10..0x1d] = memcpy from input[0x08..0x15] (14 bytes)
    u8  _unused[0x26];// [0x1e..0x43] = 0 (not set by handler)
};                    // Total = 0x44 = 68 bytes
```

**input 结构** (Windows 传给 handler 的 84-byte 内部结构):
- input[0x01]: 某字段 → output[0x09]
- input[0x04..0x06]: 3 bytes → output[0x0c..0x0e]
- input[0x08..0x15]: 14 bytes (memcpy) → output[0x10..0x1d]
- 其余字段: 未知, 需要进一步 RE

### 我们的当前 payload (src/mt7927_pci.c line ~1959)

```c
struct {
    u8   band_idx;      // [0x00] = 0
    u8   _rsv[3];       // [0x01..0x03] = 0
    __le16 tag;         // [0x04..0x05] = 0x0008 (UNI_BAND_CONFIG_RTS_THRESHOLD)
    __le16 len;         // [0x06..0x07] = sizeof-4
    __le32 len_thresh;  // [0x08..0x0b] = 0x92b (RTS threshold)
    __le32 pkt_thresh;  // [0x0c..0x0f] = 0x02
} __packed;             // sizeof = 16 bytes
```

### 差异分析 ⚠️ 严重不匹配

- **格式**: 完全不同。我们发 RTS threshold TLV, 固件期望 band 配置结构
- **大小**: 我们发 16 bytes, 固件期望 68 bytes
- **内容**: 字段布局完全不同
- **关键问题**: 固件收到 CID=0x49 但格式/大小错误, 可能静默失败

---

## 4. 参考: WFDMA_CFG (outer=0x08, inner CID=0x03)

(与 SET_DOMAIN 共享 inner CID=0x03, 但不同 outer tag)

- **Handler VA**: `0x0140145f70`
- **Input 大小**: `cmp dword ptr [rdx + 0x10], 4` → 必须 4 bytes
- **分配大小**: `nicUniCmdBufAlloc(adapter, CID=0x03, size=0x10)` → 16 bytes
- **固件 payload** (handler 在 rcx 处构建, 8 bytes):
  ```
  [0x00] = input[0]   (band_idx or flag)
  [0x01] = input[2]   (second byte)
  [0x06] = 0          (explicit zero)
  [0x08] = 0x00018000 (flags: byte[8]=0x00, byte[9]=0x80, byte[10]=0x01, byte[11]=0x00)
  [0x0c] = input[1]
  [0x0d] = input[3]
  ```
  与 SET_DOMAIN 同 inner CID 但不同结构。

---

## 5. 修复优先级建议

### 🔴 高优先级 (可能导致 scan 静默失败):

**SET_DOMAIN (CID=0x03)**:
- 当前格式完全错误
- 固件不知道合法信道列表 → scan 可能直接失败
- **下一步**: RE 找到 Windows 调用 SET_DOMAIN 的上层代码, 确认 input[0x00..0x3f] 的内容
- 临时修复: 发送固定 52-byte 结构, 至少 alpha2 在正确位置, 其余字段设为合理默认值

### 🔴 高优先级 (可能影响 TX 但不是 scan EBUSY 根因):

**BAND_CONFIG (CID=0x49)**:
- 当前格式完全错误
- 可能影响 band 配置但扫描不依赖它
- **下一步**: RE 找到 Windows 调用 BAND_CONFIG 的上层代码

### 🟡 中优先级 (格式基本对, 细节待确认):

**SCAN_CFG (CID=0x0e)**:
- 整体结构匹配, tag+len 字节布局巧合地等于 Windows 硬编码值
- config_id (output[0x08..0x09]) = 0, 需要确认正确值
- type/resp_type = 0, 可能需要特定值

---

## 6. scan EBUSY 根因与 payload 的关系

基于 analyst task #1 分析 (mac80211 kernel code):
- EBUSY 来自 `local->scan_req != NULL` — **与 payload 格式无关**
- 可能原因:
  1. hw_scan 返回 0 但 `ieee80211_scan_completed()` 从未调用
  2. ROC 活跃时 scan 被 defer, scan_req 设置后未清除

**结论**: SET_DOMAIN/BAND_CONFIG payload 格式错误会影响 **auth TX 帧发送** (固件不知道信道/band配置), 但不是 scan EBUSY 的直接根因。Payload 修复应并行进行但 EBUSY 调试优先。

---

## 7. 相关文件

- `docs/win_re_cid_mapping.md` — 完整 CID dispatch table
- `docs/win_re_connect_flow.md` — WdiTaskConnect 中的命令序列
- `docs/win_re_hif_ctrl_investigation.md` — PostFwDownloadInit 分析
- `src/mt7927_pci.c:1707` — ScanCfg 发送代码
- `src/mt7927_pci.c:1856` — SET_DOMAIN 发送代码
- `src/mt7927_pci.c:1957` — BAND_CONFIG 发送代码
