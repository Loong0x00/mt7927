# Windows RE: RX Packet Filter 固件命令分析

**分析日期**: 2026-02-22  
**目标二进制**: `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` (PE32+ x86_64, 2.5MB)  
**分析工具**: Python3 + Capstone 反汇编 + OpenAI Codex CLI 交叉验证  

## 摘要

RX Packet Filter 命令通过 `UNI_CMD_BAND_CONFIG (CID=0x08)` + `TLV tag=0x0002 (SET_RX_FILTER)` 发送，**与 MT6639 参考代码完全一致**。payload 为 12 字节 TLV，包含 band_idx + tag + len + 32-bit filter_flags。

## 1. 关键函数和地址

| 函数名 | VA | 文件偏移 | 职责 |
|--------|-----|---------|------|
| `MtCmdSetCurrentPacketFilter` | `0x1400d0904` | `0x0cfd04` | 上层入口：修改 filter bits，调用 nicUniCmdAllocEntry |
| `nicUniCmdSetRxFilter` | `0x140143cd0` | `0x1430d0` | Dispatch handler：构建 TLV 并加入命令队列 |
| `nicUniCmdAllocEntry` | `0x1400cdc4c` | `0x0cd04c` | 分配 UniCmd 条目并触发 dispatch |

## 2. Dispatch Table 映射

从 dispatch table (`0x1402507e0`) 第 27 项提取：

```
Entry #27:
  outer_tag = 0x0a
  inner_CID = 0x08  (= BAND_CONFIG)
  handler   = 0x140143cd0  (nicUniCmdSetRxFilter)
```

**关键**: inner_CID = 0x08 = 我们驱动的 `MT_MCU_CLASS_BAND_CONFIG` (mt7925 值)。

## 3. MtCmdSetCurrentPacketFilter 函数分析

### 3.1 函数签名

```c
// VA: 0x1400d0904
// rcx = adapter, rdx = port, r8d = filter_flags
NTSTATUS MtCmdSetCurrentPacketFilter(void *adapter, void *port, uint32_t filter_flags);
```

### 3.2 Filter Bit 修改逻辑

```c
// 伪代码 (从反汇编还原):
void MtCmdSetCurrentPacketFilter(adapter, port, filter_flags) {
    uint32_t ebx = filter_flags;
    
    // 保存原始 filter 到 port 结构体
    port->PacketFilter = ebx;  // offset +0x3B4
    
    // 强制设置 BROADCAST bit
    if (!(ebx & 0x08)) {       // 如果 BROADCAST 未设置
        // 打印日志 "添加 BROADCAST"
        ebx |= 0x08;           // 强制添加 BROADCAST
    }
    
    // 强制设置 MULTICAST bit
    if (!(ebx & 0x02)) {       // 如果 MULTICAST 未设置
        // 打印日志 "添加 MULTICAST"
        ebx |= 0x02;           // 强制添加 MULTICAST
    }
    
    // 构建 payload (68 字节, 全零初始化)
    uint8_t payload[0x44] = {0};
    *(uint32_t*)payload = ebx;  // 前 4 字节 = 修改后的 filter
    
    // 发送 UniCmd
    nicUniCmdAllocEntry(
        port,               // rcx
        0x0a,               // dl  = outer_tag
        0xed,               // r8b = option (fire-and-forget)
        0,                  // r9d = 0
        1,                  // [rsp+0x20] = 1
        0x10,               // [rsp+0x28] = 0x10
        &payload,           // [rsp+0x30] = payload ptr
        0x44,               // [rsp+0x38] = payload size (68 bytes)
        0, 0, 0             // [rsp+0x40..0x50] = 0
    );
}
```

### 3.3 关键汇编摘录

```asm
; 保存 filter 到 port 结构体
0x1400d0b12: mov dword ptr [rbp + 0x3b4], ebx    ; port->PacketFilter = filter

; 检查 BROADCAST bit
0x1400d0b18: test edi, edi                         ; edi = (ebx & 8)
0x1400d0b1a: jne  0x1400d0b4f                      ; 如果有 BROADCAST, 跳过
0x1400d0b4c: or   ebx, 8                           ; 强制添加 BROADCAST

; 检查 MULTICAST bit
0x1400d0b4f: test bl, 2                            ; 测试 MULTICAST
0x1400d0b52: jne  0x1400d0b89                      ; 如果有 MULTICAST, 跳过
0x1400d0b86: or   ebx, 2                           ; 强制添加 MULTICAST

; 准备 nicUniCmdAllocEntry 参数
0x1400d0ba9: mov  dl, 0xa                          ; outer_tag = 0x0a
0x1400d0b9f: mov  r8b, 0xed                        ; option = 0xed (fire-and-forget)
0x1400d0ba2: mov  word ptr [rsp + 0x38], 0x44      ; payload size = 68
0x1400d0bbd: mov  dword ptr [rsp + 0x60], ebx      ; payload[0] = filter_flags
0x1400d0bc1: call 0x1400cdc4c                       ; nicUniCmdAllocEntry
```

## 4. nicUniCmdSetRxFilter Dispatch Handler 分析

### 4.1 完整反汇编 (VA: 0x140143cd0)

```asm
; 验证 outer_tag 和 payload size
0x140143cdf: cmp  byte ptr [rdx], 0xa              ; outer_tag == 0x0a?
0x140143ce5: jne  0x140143d92                       ; 不匹配 -> 返回错误
0x140143ceb: cmp  dword ptr [rdx + 0x10], 0x44     ; payload_size == 0x44?
0x140143cef: jne  0x140143d92                       ; 不匹配 -> 返回错误

; 获取 payload 指针
0x140143cf5: mov  rdi, qword ptr [rdx + 0x18]      ; rdi = payload_ptr

; 分配 TLV 缓冲区 (12 bytes)
0x140143cf9: mov  edx, 8                            ; dl = 0x08 (inner CID!)
0x140143cfe: lea  r8d, [rdx + 4]                    ; r8d = 12 (TLV 大小)
0x140143d02: call 0x14014f788                        ; TLV alloc

; 构建 TLV
0x140143d62: mov  rcx, qword ptr [rax + 0x18]      ; rcx = TLV data buffer
0x140143d66: mov  byte ptr [rcx], 0xff              ; [+0] = 0xFF (band_idx = ALL)
0x140143d69: mov  dword ptr [rcx + 4], 0x80002     ; [+4] = tag=0x0002, len=0x0008
0x140143d70: mov  eax, dword ptr [rdi]              ; eax = payload[0] = filter_flags
0x140143d72: mov  dword ptr [rcx + 8], eax          ; [+8] = filter_flags

; 链入命令队列
0x140143d75: lea  rax, [rbx + 0x30]
0x140143d79..0x140143d88: (双向链表插入操作)
0x140143d8b: inc  dword ptr [rbx + 0x40]            ; TLV 计数 +1
0x140143d8e: xor  eax, eax                          ; 返回 STATUS_SUCCESS
```

### 4.2 TLV 结构 (12 字节)

```
偏移  大小  字段           值           说明
----  ----  ----           --           ----
+0    1     band_idx       0xFF         所有 band (ALL_BANDS)
+1    3     padding        0x000000     对齐填充
+4    2     tag (LE16)     0x0002       SET_RX_FILTER
+6    2     len (LE16)     0x0008       TLV 数据长度 (含 tag+len 自身)
+8    4     filter (LE32)  variable     NDIS packet filter flags
```

### 4.3 完整命令结构 (发送到固件)

```
+----------------------------------------------+
| UniCmd Header (16 bytes)                      |
|   CID = 0x08 (BAND_CONFIG)                   |
|   option = 0xED (fire-and-forget)             |
|   len = 16 + 12 = 28                         |
+----------------------------------------------+
| Fixed Field (4 bytes)                         |
|   [+0] band_idx = 0xFF                       |
|   [+1..3] padding = 0x000000                 |
+----------------------------------------------+
| TLV: SET_RX_FILTER (8 bytes)                 |
|   [+4] tag = 0x0002 (LE16)                   |
|   [+6] len = 0x0008 (LE16)                   |
|   [+8] filter_flags (LE32)                    |
+----------------------------------------------+
```

**注意**: band_idx 属于 `UNI_CMD_BAND_CONFIG` 的 fixed field (与 mt6639 `struct UNI_CMD_BAND_CONFIG` 的 `ucDbdcIdx` 字段对应)。TLV 的 len=0x0008 包含 tag(2) + len(2) + filter(4) = 8 字节。

## 5. Filter Flag 常量

```c
// NDIS Packet Filter Types (与 MT6639 wlan_oid.h 完全一致)
#define PARAM_PACKET_FILTER_DIRECTED       0x00000001  // 单播
#define PARAM_PACKET_FILTER_MULTICAST      0x00000002  // 组播
#define PARAM_PACKET_FILTER_ALL_MULTICAST  0x00000004  // 所有组播
#define PARAM_PACKET_FILTER_BROADCAST      0x00000008  // 广播
#define PARAM_PACKET_FILTER_PROMISCUOUS    0x00000020  // 混杂模式

// Windows 驱动强制行为:
// - BROADCAST (0x08) 总是被强制设置
// - MULTICAST (0x02) 总是被强制设置
// -> 最小 filter 值 = 0x0A (MULTICAST | BROADCAST)

// 正常 STA 模式 filter:
// DIRECTED | MULTICAST | BROADCAST = 0x0000000B
```

## 6. 调用时机

### 6.1 调用者列表

| 调用者 VA | 上下文 | filter 值 |
|-----------|--------|-----------|
| `0x14007e9d3` | 连接/端口切换 | 从另一 port 复制 (`[rax+0x3B8]`) |
| `0x1400b5b00` | 初始化/错误路径 | `0xC000000C` (特殊值) |
| `0x1400b5ca4` | 正常连接流程 | 从 port 读取 (`[rdi+0x3B8]`) |

### 6.2 调用时序 (caller3 分析: 0x1400b5ca4)

```
caller3 位于连接建立流程中:
1. 检查 adapter 状态
2. 如果状态不是 4 (SCAN) 或 7 (xxx):
   -> 读取 port->PacketFilter (offset +0x3B8)
   -> 调用 MtCmdSetCurrentPacketFilter
3. 这发生在 BSS_INFO/STA_REC 之后, auth 之前
```

### 6.3 caller2 分析: 初始化路径 (0x1400b5b00)

```
caller2 位于某个初始化/重置路径:
1. 先用 filter = 0xC000000C 调用 MtCmdSetCurrentPacketFilter
   (0xC 的低位 = BROADCAST | ALL_MULTICAST)
   (高位 0xC0000000 可能是 NDIS 特殊标志)
2. 然后调用其他连接初始化函数
```

## 7. 与 MT6639 参考代码对比

### MT6639 代码 (nic_uni_cmd_event.h)

```c
struct UNI_CMD_BAND_CONFIG {
    uint8_t ucDbdcIdx;      // band index
    uint8_t aucPadding[3];  // 填充
    uint8_t aucTlvBuffer[]; // TLV 数据
};

enum ENUM_UNI_CMD_BAND_CONFIG_TAG {
    UNI_CMD_BAND_CONFIG_TAG_SET_RX_FILTER = 2,  // tag = 0x0002
};

struct UNI_CMD_BAND_CONFIG_SET_RX_FILTER {
    uint16_t u2Tag;              // 0x0002
    uint16_t u2Length;           // 0x0008
    uint32_t u4RxPacketFilter;   // filter flags
};
```

### 对比结果

| 字段 | MT6639 | Windows RE | 匹配? |
|------|--------|-----------|--------|
| CID | UNI_CMD_ID_BAND_CONFIG (0x08) | inner_CID = 0x08 | **完全匹配** |
| Fixed field: band_idx | ucDbdcIdx | byte[+0] = 0xFF | 匹配 (Windows 用 ALL_BANDS) |
| TLV tag | 0x0002 | 0x0002 | **完全匹配** |
| TLV len | 0x0008 | 0x0008 | **完全匹配** |
| Filter field | u4RxPacketFilter | dword[+8] | **完全匹配** |
| Filter constants | PARAM_PACKET_FILTER_xxx | 相同 NDIS 常量 | **完全匹配** |

## 8. 驱动实现建议

### 8.1 C 代码结构体

```c
/* RX Packet Filter - CID=0x08 (BAND_CONFIG), tag=0x02 (SET_RX_FILTER)
 * 来源: Windows RE 0x140143cd0 (nicUniCmdSetRxFilter) + MT6639 参考代码
 */
struct mt7927_rx_filter_tlv {
    u8   band_idx;          /* +0: 0xFF = all bands */
    u8   _rsv[3];           /* +1: padding */
    __le16 tag;             /* +4: 0x0002 */
    __le16 len;             /* +6: 0x0008 */
    __le32 filter_flags;    /* +8: NDIS packet filter */
} __packed;
```

### 8.2 发送函数

```c
static int mt7927_mcu_set_rx_filter(struct mt7927_dev *dev, u32 filter)
{
    struct mt7927_rx_filter_tlv req = {
        .band_idx = 0xff,                       /* ALL_BANDS */
        .tag = cpu_to_le16(0x0002),             /* SET_RX_FILTER */
        .len = cpu_to_le16(0x0008),             /* 8 bytes */
        .filter_flags = cpu_to_le32(filter),
    };
    
    return mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_BAND_CONFIG,
                                   UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
                                   &req, sizeof(req));
}
```

### 8.3 configure_filter 回调

```c
static void mt7927_configure_filter(struct ieee80211_hw *hw,
                                    unsigned int changed_flags,
                                    unsigned int *total_flags,
                                    u64 multicast)
{
    struct mt7927_dev *dev = mt7927_hw_dev(hw);
    u32 filter = 0;
    
    /* mac80211 filter flags -> NDIS packet filter */
    filter |= PARAM_PACKET_FILTER_DIRECTED;    /* 0x01: 始终接收单播 */
    filter |= PARAM_PACKET_FILTER_BROADCAST;   /* 0x08: 始终接收广播 */
    
    if (*total_flags & FIF_ALLMULTI)
        filter |= PARAM_PACKET_FILTER_ALL_MULTICAST;  /* 0x04 */
    else
        filter |= PARAM_PACKET_FILTER_MULTICAST;      /* 0x02 */
    
    if (*total_flags & FIF_PROMISC_IN_BSS)
        filter |= PARAM_PACKET_FILTER_PROMISCUOUS;    /* 0x20 */
    
    *total_flags &= (FIF_ALLMULTI | FIF_BCN_PRBRESP_PROMISC);
    
    mt7927_mcu_set_rx_filter(dev, filter);
}
```

### 8.4 正常 STA 模式典型 filter

```
filter = 0x0000000B = DIRECTED(0x01) | MULTICAST(0x02) | BROADCAST(0x08)
```

## 9. 相关字符串 (strings 提取)

```
MtCmdSetCurrentPacketFilter         - 上层设置函数
nicUniCmdSetRxFilter                - dispatch handler (TLV 构建)
nicUniCmdSuspendWowClsFilter        - WoW 相关 filter
EvtNetAdapterSetReceiveFilter       - NDIS 接收 filter 回调
FuncSetScanFilter                   - 扫描 filter
FuncResetScanFilter                 - 重置扫描 filter
BSSCheck6GFilterAction              - 6GHz filter
MlmeConfigIcsFilter                 - ICS filter
WoWLANHwFilterOffset                - WoW 硬件 filter
WowPktFilterUCConfig                - WoW 单播 filter
WowPktFilterMCConfig                - WoW 组播 filter
WowPktFilterUniCastConfigConnac3x   - CONNAC3 WoW 单播
WowPktFilterMultiCastConfigConnac3x - CONNAC3 WoW 组播
WDI_TLV_PACKET_FILTER_PARAMETERS    - WDI TLV
WDI_TLV_RECEIVE_FILTER_FIELD        - WDI 接收 filter
```

## 10. 分析方法论

1. **字符串搜索**: `strings mtkwecx.sys | grep -i filter` 定位函数名
2. **交叉引用**: 在 `.text` 段搜索 LEA RIP-relative 指令引用字符串 VA
3. **函数定位**: 通过 CC padding 向上搜索找到函数入口
4. **Dispatch table**: 解析 `0x1402507e0` 处的 13 字节条目，找到 outer_tag=0x0a 对应的 handler
5. **参数追踪**: 反汇编 caller 和 handler，追踪寄存器和栈参数
6. **Codex 交叉验证**: 使用 OpenAI Codex CLI (default model) 独立验证 disassembly 结果

所有分析结果与 MT6639 参考代码 (`nic_uni_cmd_event.h`) 100% 一致。
