# Windows mtkwecx.sys — 完整连接流程深度逆向分析

**分析来源**: mtkwecx.sys v5705275 (AMD-MediaTek WiFi 7 Driver)
**分析方法**: pefile/capstone 汇编级逆向 + CID dispatch table 直接提取
**日期**: 2026-02-22 (Session 24)
**目的**: 彻底分析 BssActivateCtrl、option=0xed 编码、BSS_INFO/MLD 时序、完整连接命令序列

---

## 一、Executive Summary

### 关键新发现

1. **BssActivateCtrl (0x140143540) 是组合命令** — 同时发送 DEV_INFO + BSS_INFO(BASIC+MLD)，两个子命令作为原子批次提交
2. **option=0xed 的完整含义已解明**:
   - 0xed 既是内部选项码（fire-and-forget），也是 dispatch table 的特殊 outer_tag
   - 作为选项: decode(0xed) → 0x8000 flag（不等待响应）
   - 作为 outer_tag: 用 extra_param 匹配 filter 字段，路由到特殊处理器
   - **我们的 0x06 (UNI_CMD_OPT_SET) 语义等价**
3. **BSS_INFO 发送两次**:
   - 第一次: BssActivateCtrl 中，只含 BASIC + MLD 两个 TLV（网络激活时的早期初始化）
   - 第二次: MtCmdSetBssInfo 中，包含完整 14 个 TLV（连接配置）
4. **CID dispatch table 完整 58 条目已解析**，包含 10 个 0xed 特殊条目
5. **FUN_1400ac6c8 是 TX 队列提交函数**，不是 MCU 命令，直接将 auth 帧入队 DMA

---

## 二、CID Dispatch Table 完整解析 (0x1402507E0, 58 条目)

### 2.1 表结构

每条目 13 字节: `outer_tag(2) + inner_CID(2) + filter(1) + handler_VA(8)`

### 2.2 Dispatch 路由算法 (FUN_14014f720)

```c
int dispatch_lookup(u8 cmd_id, u8 extra_param) {
    int use_filter = (cmd_id == 0xed && extra_param != 0) ? 1 : 0;

    for (int i = 0; i < 58; i++) {
        if (table[i].inner_CID == 0xa5) continue;  // 跳过 sentinel 条目

        if (!use_filter) {
            if (table[i].outer_tag == cmd_id) return i;  // 普通匹配
        } else {
            if (table[i].filter == extra_param) return i; // filter 匹配
        }
    }
    return -1;  // 未找到
}
```

**关键**: 当 cmd_id=0xed 且 extra_param≠0 时，使用 FILTER 字段匹配而非 outer_tag。
这就是为什么 STA_REC 有两个入口:
- 普通路径: `dispatch(0x13, 0xed, 0)` → outer_tag=0x13 → entry 22
- 特殊路径: `dispatch(0xed, 0xed, 0xa8)` → filter=0xa8 → entry 49

### 2.3 完整条目列表

| # | outer | inner | filter | Handler VA | 命令名 | 备注 |
|---|-------|-------|--------|-----------|--------|------|
| 0 | 0x8a | 0x0e | 0x00 | 0x140144b20 | NIC_CAP | 查询固件能力 |
| 1 | 0x02 | 0x0b | 0x00 | 0x140144be0 | CLASS_02 (Config) | PostFwDownloadInit step3 |
| 2 | 0xc0 | 0x0d | 0x00 | 0x140145670 | LOG_CONFIG | 日志配置 |
| 3 | 0x28 | 0x28 | 0x00 | 0x140143db0 | DBDC | 双频段配置 |
| 4 | 0xca | 0x0e | 0x00 | 0x140143a70 | CHIP_CONFIG/SCAN | 芯片/扫描配置 |
| 5 | 0xc5 | 0x0b | 0x00 | 0x140144890 | CHIP_CONFIG_2 | |
| 6 | 0x0f | 0x15 | 0x00 | 0x140143fc0 | WSYS_CONFIG | |
| 7 | 0x5d | 0x2c | 0x00 | 0x140146850 | HIF_CTRL | suspend/resume |
| 8 | 0x70 | 0x0e | 0x00 | 0x140143950 | NIC_CAP_V2 | |
| **9** | **0x11** | **0x01** | 0x00 | **0x140143540** | **BssActivateCtrl** | **DEV_INFO+BSS_INFO 组合** |
| 10 | 0x03 | 0x16 | 0x00 | 0x140143020 | SCAN_REQ | |
| 11 | 0x1b | 0x16 | 0x00 | 0x1401431d0 | SCAN_CANCEL | |
| 12 | 0x1c | 0x27 | 0x00 | 0x140144820 | CH_PRIVILEGE | 信道请求 |
| 13 | 0x05 | 0x02 | 0x00 | 0x140144db0 | BSS_INFO_HE_SUB | BSS_INFO HE 子命令 |
| 14 | 0x17 | 0x02 | 0x00 | 0x1401442d0 | BSS_INFO_PM | PM 相关 BSS_INFO |
| **15** | **0x12** | **0x02** | 0x00 | **0x1401444a0** | **BSS_INFO (full)** | **14 TLV dispatch** |
| 16 | 0x16 | 0x02 | 0x00 | 0x1401443b0 | BSS_INFO_SUB_1 | |
| 17 | 0x19 | 0x02 | 0x00 | 0x1401445e0 | BSS_INFO_RLM | RLM+PROTECT+IFS 三合一 |
| 18 | 0x18 | 0x02 | 0x00 | 0x140144110 | BSS_INFO_SUB_2 | |
| 19 | 0x1e | 0x02 | 0x00 | 0x140144e80 | BSS_INFO_SUB_3 | |
| 20 | 0x15 | **0xa5** | 0x00 | 0x140142fc0 | (SKIPPED) | inner=0xa5 sentinel |
| **21** | **0x14** | **0x03** | 0x00 | **0x140143fd0** | **RemoveStaRec** | 断连时清理 STA |
| **22** | **0x13** | **0x03** | 0x00 | **0x1401446d0** | **STA_REC (full)** | **13 TLV dispatch** |
| 23 | 0x07 | 0x03 | 0x00 | 0x140145d30 | SET_DOMAIN | regulatory |
| 24 | 0x08 | 0x03 | 0x00 | 0x140145f70 | WFDMA_CFG_EXT | |
| 25 | 0xfd | 0x03 | 0x00 | 0x140146060 | THERMAL | 温度管理 |
| 26 | 0x1d | 0x04 | 0x00 | 0x140145550 | STAREC_PHY_EXT | |
| 27 | 0x0a | 0x08 | 0x00 | 0x140143cd0 | BAND_CONFIG | 频段配置 |
| 28 | 0x81 | 0x23 | 0x00 | 0x140146790 | BFEE_CTRL | beamforming |
| 29 | 0xce | 0x22 | 0x00 | 0x1401463e0 | SMART_ANT | |
| 30 | 0x85 | 0x23 | 0x00 | 0x140146620 | BF_CTRL | beamforming |
| 31 | 0x61 | 0x16 | 0x00 | 0x1401432b0 | SCHED_SCAN | |
| 32 | 0x62 | 0x16 | 0x00 | 0x140143390 | SCHED_SCAN_2 | |
| 33 | 0xc4 | 0x0e | 0x00 | 0x140143bf0 | FW_LOG | |
| 34 | 0x58 | 0x05 | 0x00 | 0x140144cd0 | EFUSE_CTRL | |
| 35 | 0x04 | 0x0f | 0x00 | 0x140143ef0 | POWER_CTRL | |
| 36 | 0xfc | 0x24 | 0x00 | 0x1401459e0 | TESTMODE | |
| 37 | 0x2a | 0x2f | 0x00 | 0x140145ae0 | ROAM | |
| 38 | 0x2b | 0x38 | 0x00 | 0x140145ba0 | OFFLOAD | |
| 39 | 0x79 | 0x27 | 0x00 | 0x140145c60 | CH_ABORT | 信道释放 |
| **40** | **0xb1** | **0x25** | **0xa8** | 0x1401458d0 | **STA_REC (direct)** | 直接路径，非 legacy |
| 41 | 0xb0 | 0x43 | 0x00 | 0x1401457c0 | PP_CTRL | |
| 42 | 0xf6 | 0x07 | 0x00 | 0x1401461c0 | DOMAIN_EXT | |
| 43 | 0xc1 | 0x0a | 0x00 | 0x140144970 | PERF_CTRL | |
| 44 | 0xed | 0x2d | 0x21 | 0x140146d60 | (special: filter=0x21) | |
| 45 | 0xed | 0x14 | 0x94 | 0x140144f80 | (special: filter=0x94) | |
| 46 | 0xed | 0x33 | 0x1e | 0x1401450d0 | (special: filter=0x1e) | |
| 47 | 0xed | 0x13 | 0x81 | 0x140145230 | (special: filter=0x81) | |
| 48 | 0xed | 0x1a | 0x3c | 0x140145460 | (special: filter=0x3c) | |
| 49 | 0xed | 0x25 | 0xa8 | 0x140146ec0 | (special: STA_REC alt) | |
| 50 | 0xed | 0x7b | 0xbf | 0x140147010 | (special: filter=0xbf) | |
| 51 | 0xed | 0xa4 | 0xc0 | 0x140147160 | (special: filter=0xc0) | |
| 52 | 0x93 | 0x49 | 0x00 | 0x140146950 | BAND_CONFIG_V2 | |
| 53 | 0x4c | 0x4a | 0x00 | 0x140146a50 | BSS_INFO_PROTECT | 独立 CID! |
| 54 | 0x8f | 0x19 | 0x00 | 0x140146290 | MU_CTRL | |
| 55 | 0x7e | 0x44 | 0x00 | 0x140147330 | MLO_CTRL | |
| 56 | 0xed | 0x2d | 0x01 | 0x140147410 | (special: EFUSE var1) | |
| 57 | 0xed | 0x2d | 0x4f | 0x140147550 | (special: EFUSE var2) | |

### 2.4 关键发现

1. **Entry 9 (outer=0x11)**: `nicUniCmdBssActivateCtrl` — 不是简单的 DEV_INFO，而是 DEV_INFO + BSS_INFO 组合！
2. **Entry 20 (inner=0xa5)**: sentinel 条目，被 dispatch 算法跳过
3. **Entry 40 (outer=0xb1, filter=0xa8)**: STA_REC 的直接路径（非 legacy wrapper），handler 不同于 entry 22
4. **0xed 系列 (entries 44-51, 56-57)**: 10 个特殊条目，使用 filter 字段做二级路由

---

## 三、nicUniCmdBssActivateCtrl 深度分析 (0x140143540)

### 3.1 入口条件

```c
// 从 MtCmdActivateDeactivateNetwork 调用:
// FUN_1400cdc4c(bss, 0x11, 0xed, 0)
// → dispatch lookup(0x11) → entry 9 → handler=0x140143540

// 入口检查:
if (input->cmd_id != 0x11) return 0x10003;      // 必须是 DEV_INFO
if (input->payload_len != 0x0c) return 0x10003;  // payload 必须 12 字节
```

### 3.2 子命令 1: DEV_INFO (CID=1)

```c
entry1 = nicUniCmdAllocEntry(adapter, CID=1, size=0x10);  // 16 字节

// DEV_INFO TLV 构建 (从 input payload rbp 读取):
data = entry1->data;
data[0x00] = rbp[3];          // band_idx
data[0x01] = (rbp[0] != 4) ? 0xFF : 0xFE;  // activate: if mode!=AP → 0xFF(activate)
data[0x04] = 0x000C0000;      // conn_info = 0xC0000 (dword)
data[0x08] = rbp[1];          // ownmac_idx
data[0x09] = rbp[0xb];        // phy_idx
data[0x0a] = rbp[4..8];       // bssid/conn_info (4 bytes)
data[0x0e] = rbp[8..10];      // bssid_hi (2 bytes)
```

### 3.3 子命令 2: BSS_INFO (CID=2)

**大小**: activate 时 = 0x38 (56B), deactivate 时 = 0x24 (36B)

```c
// 大小计算:
int size = 0x24;  // base = 4 (header) + 32 (BASIC TLV)
if (activate) size += 0x14;  // + 20 (MLD TLV)

entry2 = nicUniCmdAllocEntry(adapter, CID=2, size);
```

**BSS_INFO 布局** (r15 = entry2->data):

```
[+0x00] u8  bss_idx            ← rbp[0]
[+0x01] u8  pad[3]
--- BASIC TLV (tag=0x0000, len=0x0020, 32 bytes) ---
[+0x04] u32 tag_len = 0x00200000
[+0x08] u8  bss_idx            ← rbp[0]
[+0x09] u8  ownmac_idx_1       ← rbp[3]
[+0x0a] u8  ownmac_idx_2       ← rbp[3] (duplicate)
[+0x0b] u8  sco = 0xFF
[+0x0c] u32 conn_type          ← nicUniCmdBssInfoConnType(adapter) → 0x10001 for STA
[+0x10] u8  active             ← ~(bss->0x2e6964 >> 7) & 1
[+0x11] u8  network_type       ← bss->0x5caa80
[+0x12] u8  bssid[6]           ← from sta_entry or bss->0x2e0
[+0x18] u16 sta_type           ← rbp[0xa]
[+0x1a] u16 bcn_interval       ← bss->0x5c3090
[+0x1c] u8  dtim               ← bss->0x5c3168
[+0x1d] u8  phy_mode_lo        ← nicUniCmdBssInfoPhyMode() result lo
[+0x1e] u16 mbss_flags = 0x00FE
[+0x20] u16 wlan_idx           ← sta_entry->0x4ac or 3
[+0x22] u8  phy_mode_hi        ← result byte 1
[+0x23] u8  band_info          ← rbp[0xb]
--- MLD TLV (tag=0x001A, len=0x0014, 20 bytes, ONLY when activate) ---
[+0x24] 由 nicUniCmdBssInfoMld(adapter, &data[0x24], bss_idx) 填充
```

### 3.4 原子提交

两个子命令链接到同一个命令列表，然后一起提交:

```c
// 伪代码 (从 0x1401438a6 汇编):
list_head = &cmd_queue->list;  // [rdi + 0x30]

if (activate) {
    // DEV_INFO 先, BSS_INFO 后
    list_insert_tail(list_head, entry1);  // rsi = DEV_INFO
    list_insert_tail(list_head, entry2);  // r14 = BSS_INFO
} else {
    // BSS_INFO 先, DEV_INFO 后
    list_insert_tail(list_head, entry2);
    list_insert_tail(list_head, entry1);
}
cmd_queue->count += 2;
```

### 3.5 对我们驱动的影响

**我们当前行为**: 分别发送 DEV_INFO 和 BSS_INFO(14 TLVs)
**Windows 行为**: BssActivateCtrl 组合发送 DEV_INFO + BSS_INFO(BASIC+MLD)，之后 MtCmdSetBssInfo 再发完整 BSS_INFO(14 TLVs)

**差异**:
- Windows 的 BSS_INFO 发送了两次（先简单版，后完整版）
- 第一次（BssActivateCtrl 中）可能是固件的"早期初始化"
- 我们只发一次完整版，跳过了早期初始化

**建议**: 可能需要在 DEV_INFO 后、完整 BSS_INFO 前，先发一个只含 BASIC+MLD 的简化 BSS_INFO。

---

## 四、Option=0xed 完整编码分析

### 4.1 调用约定

```c
// 调用: FUN_1400cdc4c(bss_ptr, legacy_cmd_id, option, extra_param)
//   rcx = bss_ptr
//   dl  = legacy_cmd_id (0x11=DEV_INFO, 0x12=BSS_INFO, 0x13=STA_REC)
//   r8b = option (0xed = fire-and-forget, 0xee = wait-for-response)
//   r9b = extra_param (0 for normal, 非零用于 0xed 特殊路由)
```

### 4.2 Option 解码函数 (FUN_1400ca864)

```c
// 地址: 0x1400ca864
// 极简函数，只有 4 条指令:
uint16_t decode_option(uint8_t option) {
    if (option == 0xee) return 0xc000;  // 等待响应
    else               return 0x8000;  // 不等待响应 (fire-and-forget)
}
```

```asm
0x1400ca864:  cmp      cl, 0xee        ; option == 0xee?
0x1400ca867:  mov      eax, 0x8000     ; default = 0x8000 (no response)
0x1400ca86c:  mov      edx, 0xc000     ; alt = 0xc000 (wait response)
0x1400ca871:  cmove    ax, dx          ; if 0xee → 0xc000
0x1400ca875:  ret
```

### 4.3 Option 在 UniCmd 帧中的使用

在 `nicUniCmdSendCmd` (0x14014eb0c):

```c
// 1. 解码 option → flag word
uint16_t flag = decode_option(0xed);  // → 0x8000
frame->flag_word = flag;              // [rsi + 0x3a]

// 2. 存储 raw option 到帧管理结构
frame->option = 0xed;                 // [rsi + 0x0a]

// 3. 不直接进入 UniCmd wire header!
// UniCmd wire header 的 option 字段由其他逻辑构建
```

### 4.4 与我们驱动的对比

| 功能 | Windows 值 | 我们的值 | 等价? |
|------|-----------|---------|-------|
| 设置(不等响应) | 0xed | 0x06 (UNI_CMD_OPT_SET) | **✅ 语义等价** |
| 设置(等待响应) | 0xee | 0x07 (UNI_CMD_OPT_SET_ACK) | **✅ 语义等价** |

### 4.5 0xed 作为 Dispatch Table outer_tag

当 `FUN_1400cdc4c` 的 cmd_id 参数本身是 0xed 时（不是 option！），dispatch 算法进入特殊模式:

```c
// 特殊调用: FUN_1400cdc4c(bss, 0xed, 0xed, extra_param)
// extra_param 用于匹配 filter 字段
//
// 例: extra_param=0xa8 → entry 49 → STA_REC 特殊处理器
// 例: extra_param=0x21 → entry 44 → EFUSE 特殊处理器
```

这些特殊路径可能用于固件更新、MLD 操作等非标准场景。**正常连接流程不使用这些路径。**

---

## 五、完整连接命令序列 (从 WdiTaskConnect 到 Auth TX)

### 5.1 调用流程图

```
WdiTaskConnect (0x140065be0)
  │
  ├─[1] ChipConfig (0x1400484c0)
  │       └─ CMD_ID=0xca, CID=0x0E, payload=328B
  │       └─ 每次 connect 重发
  │
  ├─[2] MtCmdActivateDeactivateNetwork (0x1400c558c)
  │       └─ FUN_1400cdc4c(bss, 0x11, 0xed, 0)
  │       └─ Dispatch → entry 9 → nicUniCmdBssActivateCtrl
  │       └─ 发送: DEV_INFO(CID=1) + BSS_INFO(CID=2, BASIC+MLD)
  │       └─ 条件: bss_state==5 && connected==0
  │
  └─ MlmeCntlOidConnectProc (0x140123588)
        │
        ├─[3] nicUniCmdPmDisable (0x1400caefc)
        │       └─ BSS_INFO CID=0x02, tag=0x1B (PM_DISABLE)
        │       └─ payload: 只有 tag+len header (4字节)
        │
        ├─[4] MtCmdSetBssInfo (0x1400cf928)
        │       └─ FUN_1400cdc4c(bss, 0x12, 0xed, 0)
        │       └─ Dispatch → entry 15 → nicUniCmdSetBssInfo
        │       └─ BSS_INFO CID=0x02, 14 TLVs via dispatch table
        │
        ├─ (state setup, MLD check, channel info prep)
        │
        └─ MlmeCntlWaitJoinProc (0x1401273a8)
              │
              ├─[5] SCAN_CANCEL (0x1400c5e08 area)
              │       └─ CMD_ID=0x1b, CID=0x16
              │
              ├─[6] MtCmdChPrivilage (0x1400c5e08)
              │       └─ Dispatch → entry 12 → nicUniCmdChReqPrivilege
              │       └─ CID=0x27, tag=0, len=0x18 per channel
              │
              ├─[7] MtCmdSendStaRecUpdate (0x1400cdea0)
              │       └─ FUN_1400cdc4c(bss, 0x13, 0xed, 0)
              │       └─ Dispatch → entry 22 → nicUniCmdUpdateStaRec
              │       └─ STA_REC CID=0x03, 13 TLVs via dispatch table
              │
              └─[8] FUN_1400ac6c8(adapter, bss, 2, 0)
                      └─ TX 队列提交 (NOT MCU command!)
                      └─ 将 auth 帧入队 DMA ring
                      └─ 触发: 0x14008d69c (信号 TX 队列)
```

### 5.2 MlmeCntlOidConnectProc 汇编确认

```asm
; 步骤 3: PM_DISABLE (必须在 BSS_INFO full 之前!)
0x140123776:  xor      r8d, r8d         ; r8=0
0x140123779:  mov      rdx, r13         ; rdx=bss
0x14012377c:  mov      rcx, r15         ; rcx=adapter
0x14012377f:  call     0x1400caefc      ; nicUniCmdPmDisable

; 步骤 4: BSS_INFO full (14 TLVs)
0x140123784:  xor      r9d, r9d         ; r9=0
0x140123787:  xor      r8d, r8d         ; r8=0
0x14012378a:  mov      rdx, r13         ; rdx=bss
0x14012378d:  mov      rcx, r15         ; rcx=adapter
0x140123790:  call     0x1400cf928      ; MtCmdSetBssInfo → 14 TLVs
```

### 5.3 BSS_INFO 时序总结

BSS_INFO (CID=0x02) 在连接过程中发送 **三次**:

| 顺序 | 来源 | TLV 内容 | 目的 |
|------|------|---------|------|
| **第1次** | BssActivateCtrl (步骤2) | BASIC + MLD (2个) | 网络早期初始化 |
| **第2次** | nicUniCmdPmDisable (步骤3) | PM_DISABLE (1个, tag=0x1B) | 禁用省电 |
| **第3次** | MtCmdSetBssInfo (步骤4) | 14个完整TLV | 完整 BSS 配置 |

---

## 六、nicUniCmdBssInfoMld (0x14014fad0) 调用路径分析

### 6.1 调用位置

MLD TLV 有两个调用路径:

1. **BssActivateCtrl (0x140143540)** — 直接调用
   ```asm
   0x14014375e:  call     0x14014fad0      ; nicUniCmdBssInfoMld(bss, &data[0x24], bss_idx)
   ```
   条件: 仅当 activate (byte ptr [rbp+1] != 0)

2. **BSS_INFO dispatch table entry[12]** — 作为 14 TLV 之一
   - Handler: 0x14014d340 → 内部调用 0x14014fad0
   - 在 MtCmdSetBssInfo 的 14 TLV 遍历中被调用

### 6.2 MLD TLV 构建逻辑

```c
// 函数: nicUniCmdBssInfoMld (0x14014fad0)
// 参数: rcx=adapter, rdx=output_buf, r8b=bss_idx

void nicUniCmdBssInfoMld(adapter, out, bss_idx) {
    // 1. 写入 tag+len
    *(uint32_t*)out = 0x0014001A;  // tag=0x1A, len=0x14

    // 2. 查找 MLD entry
    sta_entry = find_sta_entry(adapter, bss_idx);
    mld_entry = find_mld_entry(adapter, sta_entry);

    // 3. 检查 MLD 模式
    if (adapter->mld_mode_flag == 0  // [adapter + 0x5cafd8]
        || sta_entry == NULL
        || mld_entry == NULL) {
        // Non-MLD fallback:
        out[4]  = 0xFF;             // group_mld_id = no MLD
        out[5]  = bss_idx;          // own_mld_id = bss_idx
        out[6..11] = adapter->mac_addr;  // [adapter + 0x2cc] (6 bytes)
        out[12] = 0;                // remap_idx
        out[16] = 0xFF;             // no setup
        out[17] = 0xFF;             // no setup
        return;
    }

    // 4. MLD mode == 3:
    if (adapter->mld_mode == 3) {   // [adapter + 0x14647ac]
        out[4]  = mld_entry[2];     // group_mld_id
        out[12] = mld_entry[3];     // remap_idx
        out[13] = sta_entry[0x8fb]; // eml_cap
        out[14] = mld_entry[0xd];   // mld_type
        out[15] = linkmap_enable;   // conditional
        out[17] = mld_entry[1];     // setup_wlan_id
        out[16] = mld_entry[0x10];  // ???
    } else {
        out[4]  = 0xFF;
        out[12..13] = 0xFFFF;
        out[16] = 0;
    }

    // 5. Common fields (MLD or not):
    out[5]  = sta_entry[0x908] + 0x20;  // own_mld_id
    out[6..9]  = mld_entry[4..7];       // mld_addr[0:4]
    out[10..11] = mld_entry[8..9];      // mld_addr[4:6]
}
```

### 6.3 对我们驱动的影响

我们在 BSS_INFO 的 MLD TLV 中设置:
- `group_mld_id = 0xFF` (no MLD) ✅
- `own_mld_id = vif->bss_idx` ✅
- `mld_addr = bss MAC` ✅

这与 Windows Non-MLD 路径一致。**MLD TLV 实现正确。**

---

## 七、FUN_1400ac6c8 — Auth 帧 TX 触发函数

### 7.1 函数签名

```c
// 地址: 0x1400ac6c8
// 参数:
//   rcx = adapter
//   rdx = bss_ptr (r13)
//   r8d = action_type (2=channel_setup, 1=auth)
//   r9d = sae_flag
//   [rsp+0xb0] = timeout (5th param)
//   [rsp+0xb8] = extra_data
//   [rsp+0xc0] = data_buf_ptr

bool tx_enqueue(adapter, bss, action_type, sae_flag, ...) {
    // 1. 检查 adapter 状态
    uint32_t flags = adapter->tx_flags;  // [rcx + 0x1310]
    if (flags & 0x40000140) {
        // TX 被禁止 (suspend/reset/error)
        return false;
    }

    // 2. 检查 timeout 参数
    uint32_t timeout = stack_param5;
    if (timeout > 0x800) {
        log_warning("timeout too large: %d", timeout);
        return false;
    }

    // 3. 检查 TX 队列是否满
    tx_queue = &adapter->tx_queue;  // [rcx + 0x1fa8]
    if (is_queue_full(tx_queue)) {
        log_warning("queue full");
        return false;
    }

    // 4. 入队
    int slot = tx_queue->write_idx;
    tx_queue->write_idx = (slot + 1) % 200;  // 0xC8 = 200 slots
    tx_queue->count++;

    // 5. 填充队列条目
    entry = &tx_queue->entries[slot];  // slot * 0x828 bytes per entry
    entry->valid = 1;
    entry->action_type = action_type;
    entry->sae_flag = sae_flag;
    entry->timeout = timeout;
    entry->bss_id = bss->id;
    // ... copy more fields

    // 6. 触发 TX worker
    signal_tx_worker(adapter);  // 0x14008d69c

    return true;
}
```

### 7.2 关键发现

**FUN_1400ac6c8 不是 MCU 命令！** 它是 TX 队列入队函数:
- 将 auth 帧放入内部 TX 队列
- 触发 TX worker 线程处理队列
- TX worker 最终构建 TXD 并提交到 DMA ring

**对我们的影响**: 我们使用 mac80211 的 `ieee80211_tx` 提交 auth 帧，这与 Windows 的内部队列机制不同，但最终都走 DMA ring，功能等价。

---

## 八、完整 Legacy CMD_ID → CID 映射 (已验证)

从 dispatch table 直接提取，以下是所有 legacy CMD_ID 到 UniCmd CID 的映射:

| Legacy CMD_ID | UniCmd CID | 命令 | 我们使用的 CID | 差异 |
|--------------|-----------|------|---------------|------|
| 0x02 | 0x0b | CLASS_02 (Config) | — (跳过) | 需要恢复 |
| 0x03 | 0x16 | SCAN_REQ | 0xca → 0x0e | 不同路径 |
| 0x04 | 0x0f | POWER_CTRL | — | 不使用 |
| 0x05 | 0x02 | BSS_INFO_HE | — | 子命令 |
| 0x07 | 0x03 | SET_DOMAIN | 0x15 (mt7925) | **不同!** |
| 0x08 | 0x03 | WFDMA_CFG | — | 不使用 |
| 0x0a | 0x08 | BAND_CONFIG | 0x08 (mt7925) | ✅ 匹配 |
| 0x0f | 0x15 | WSYS_CONFIG | — | 不使用 |
| **0x11** | **0x01** | **DEV_INFO** | **0x01** | **✅** |
| **0x12** | **0x02** | **BSS_INFO** | **0x02** | **✅** |
| **0x13** | **0x03** | **STA_REC** | **0x03** | **✅** |
| 0x14 | 0x03 | RemoveStaRec | — | 不使用 |
| 0x1b | 0x16 | SCAN_CANCEL | 0x16 | ✅ |
| 0x1c | 0x27 | CH_PRIVILEGE | 0x27 | ✅ |

### 8.1 SET_DOMAIN CID 差异

**重要**: Windows 的 SET_DOMAIN (legacy CMD_ID=0x07) 使用 inner_CID=0x03 (与 STA_REC 相同的 CID)！
我们使用 mt7925 的 CID=0x15。

但 mt7925 的 CID=0x15 已验证工作（scan 成功），所以不需要改。可能固件对这两个 CID 都接受。

---

## 九、UniCmd Header Wire Format

### 9.1 帧结构 (从 nicUniCmdSendCmd 0x14014eb0c)

```
[r14 + 0x00..0x1F] = TXD (32 bytes, 8 DWs)
[r14 + 0x20]       = UniCmd Header 开始

UniCmd Header Layout:
[+0x00] u16 len            = payload_len + header_overhead
[+0x02] u16 CID/seq        = sequence number
[+0x05] u8  S2D            = 0xa0 (src=EXT_CMD_REQ, dst=MCU)
[+0x07] u8  seq_num        = from seq_generator (FUN_14009a46c)
[+0x0a] u8  reserved       = 0
[+0x0b] u8  flags          = computed (includes S2D routing bits)
[+0x10] ...                = payload data (copy from handlers)
```

### 9.2 S2D (Source to Destination) 编码

```c
// S2D = 0xa0:
//   Source = 0xa (EXT_CMD_REQ)
//   Destination = 0x0 (MCU)
//
// 高 4 bits = source, 低 4 bits = destination
// 0xa0 = src=MCU_EXT_CMD, dst=MCU
```

### 9.3 TXD DW0 (UniCmd 专用)

```c
// 从 nicUniCmdSendCmd:
dw0 = (total_len & 0x7FFFFF) | 0x41000000;
//   bits [22:0]  = total length
//   bit  [24]    = 1 (UniCmd marker)
//   bit  [30]    = 1 (?)
```

---

## 十、对我们驱动的影响分析

### 10.1 关键差异总结

| 项目 | Windows 行为 | 我们的行为 | 影响 |
|------|-------------|-----------|------|
| BssActivateCtrl | DEV_INFO + BSS_INFO(BASIC+MLD) 组合发送 | DEV_INFO 单独发送 | **可能影响固件初始化** |
| BSS_INFO 发送次数 | 3次 (BASIC+MLD → PM_DISABLE → full 14 TLVs) | 2次 (PM_DISABLE → full 14 TLVs) | **缺少早期初始化** |
| Option | 0xed (fire-and-forget) | 0x06 (UNI_CMD_OPT_SET) | ✅ 语义等价 |
| ChipConfig | 每次 connect 重发 | 只在 init 发一次 | 低影响 |
| Auth TX | TX 队列入队 → DMA | mac80211 → DMA | ✅ 等价 |

### 10.2 建议修复 (优先级)

#### 🔴 高优先级: 实现 BssActivateCtrl 组合命令

在 `mt7927_mcu_set_dev_info()` 中，除了 DEV_INFO，还应同时发送 BSS_INFO(BASIC + MLD):

```c
static int mt7927_mcu_bss_activate(struct mt7927_dev *dev,
                                    struct ieee80211_vif *vif,
                                    bool enable)
{
    // 步骤 1: 构建并发送 DEV_INFO (CID=1)
    // ... (现有代码)

    // 步骤 2: 构建简化版 BSS_INFO (CID=2, 只含 BASIC + MLD)
    if (enable) {
        struct sk_buff *skb;
        // 分配: 4 (header) + 32 (BASIC) + 20 (MLD) = 56 bytes
        skb = mt7927_mcu_alloc_uni_cmd(dev, MCU_UNI_CMD_BSS_INFO, 56);
        // 填充 BASIC TLV (tag=0, len=0x20)
        // 填充 MLD TLV (tag=0x1a, len=0x14)
        mt7927_mcu_send_cmd(dev, skb, false);
    }

    return 0;
}
```

#### 🟡 中优先级: Config 命令恢复

CLASS_02 (CID=0x0b) 在 Session 20 被跳过。需要恢复，使用 option=0x06 (不等响应)。

---

## 附录 A: FUN_1400cdcc0 — RemoveStaRec 包装器

```c
// 地址: 0x1400cdcc0
// 用于网络停用时清理 STA 记录

void remove_sta_rec_wrapper(adapter, cmd_id, option, extra) {
    sta_entry = find_sta_entry(adapter, extra);
    if (!sta_entry) return;

    // 构建清理参数
    struct {
        u8 cmd_id;     // [+0] = cmd_id (from param)
        u8 option;     // [+1] = option
        u8 extra;      // [+2] = extra
        u8 pad;        // [+3] = 0
    } params;

    // 调用 dispatcher 发送 RemoveStaRec
    FUN_1400cdc4c(sta_entry, 0x14, 0xed, 0);
    // → dispatch entry 21: outer=0x14, inner=0x03, handler=0x140143fd0
}
```

---

## 附录 B: nicUniCmdSendCmd 帧发送流程

```
nicUniCmdSendCmd (0x14014eb0c)
  │
  ├─ 检查 adapter->uni_cmd_supported
  ├─ 检查 chip_id (6639/738/7927/7925/717)
  │
  ├─ 获取 header overhead: FUN_1401cc290(adapter) → 0x30 或 0x40
  │
  ├─ 分配 TX buffer: FUN_1400c5ad8(adapter, total_size)
  │
  ├─ 填充帧管理结构 (rsi):
  │     [+0x08] = 0 (seq)
  │     [+0x0a] = 0xed (option raw)
  │     [+0x0b] = CID from dispatch
  │     [+0x3a] = decode_option(0xed) → 0x8000 (flag)
  │     [+0x7c] = 1 (cmd_count)
  │
  ├─ 构建 TXD (r14 = buffer):
  │     DW0 = (total_len & 0x7FFFFF) | 0x41000000
  │     DW1 = bit14 set, bit15 clear
  │
  ├─ 构建 UniCmd header (r14 + 0x20):
  │     [+0x00] = len
  │     [+0x05] = 0xa0 (S2D)
  │     [+0x07] = seq_num
  │     [+0x0b] = flags
  │
  ├─ 复制 payload → [r14 + 0x30]
  │
  └─ 提交到 MCU: FUN_1400c8340(adapter, ctx, frame)
```

---

*逆向来源: mtkwecx.sys v5705275 binary analysis (pefile + capstone), 2026-02-22*
