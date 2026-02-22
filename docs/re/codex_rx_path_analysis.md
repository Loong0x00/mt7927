# mtkwecx.sys 逆向分析 — Auth TX 阻塞根因调查

**分析工具**: Python + pefile + capstone (直接汇编级逆向)
**分析对象**: `/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys`
**日期**: 2026-02-22 (Session 24 teammate analysis)
**目标**: 调查 auth 帧无法到达空中的根因 — PLE STATION_PAUSE、Ring 15 卡住

---

## 一、执行摘要

| 发现 | 结论 |
|------|------|
| PLE STATION_PAUSE 如何清除? | **固件自动清除** — Windows 从不写该寄存器，只读取诊断 |
| PLE 清除触发条件? | BSS_INFO active=1 + STA_REC 正确 → 固件清除 |
| Windows auth 前 BSS_INFO 内容? | **只有 BASIC + MLD**，不含 RATE/PROTECT/IFS_TIME！ |
| Windows 命令顺序? | DEV_INFO+BSS_INFO(basic) → CH_PRIVILEGE → STA_REC → auth TX |
| 我们的命令顺序? | DEV_INFO → BSS_INFO(全14 TLV) → STA_REC → CH_PRIVILEGE → auth TX |
| STATE_INFO 在 auth 前是多少? | **wire state=0** (DISCONNECT) |
| 我们当前发的 STATE? | state=1 (IDLE) |

**核心问题**: 我们在 auth 前发送了**完整 14-TLV BSS_INFO**（包含 RATE 等），且命令顺序与 Windows 不同。这是导致固件行为异常的直接原因。

---

## 二、任务 1: PLE STATION_PAUSE 清除机制

### 2.1 两个访问点

通过搜索 `0x820c0360` 字节序列 `60 03 0c 82`，找到 2 处访问:

| VA | 上下文 |
|----|--------|
| `0x1401e7338` | `mov edx, 0x820c0360; call 0x1400099ac` |
| `0x1401f6002` | `mov edx, 0x820c0360; call 0x1400099ac` |

### 2.2 确认: 仅 READ 操作

`0x1400099ac` 反汇编分析:

```asm
; 0x1400099ac — read_register(adapter, reg_addr, out_buf)
0x1400099ac: mov qword ptr [rsp + 8], rbx
0x1400099bb: cmp byte ptr [rcx + 0x1465e0a], 0   ; check if debug mode
0x1400099c5: mov ebx, edx   ; save reg addr
0x1400099e0: and dword ptr [rsi], 0               ; out_buf = 0 (zero first)
0x1400099e3: mov r8, rsi                          ; r8 = out_buf ptr
0x1400099e6: mov rcx, qword ptr [rdi + 0x1f80]    ; rcx = BAR0 base
0x1400099ed: mov edx, ebx                         ; edx = reg_addr
0x1400099ef: call 0x140054ee4                      ; BAR0_READ(bar0_base, reg_addr, out_buf)
```

**结论**: `0x1400099ac` = `read_register()` — 这两处都是 **READ** 操作。

### 2.3 访问模式分析

两处访问都在诊断转储函数中，读取一系列 PLE 寄存器:
```
0x820c0300, 0x820c0304, 0x820c0308,  ← PLE control regs
0x820c0004,                          ← PSE FL control
0x820c0360 (STATION_PAUSE0),        ← 只读诊断
0x820c0600, 0x820c0604, ...         ← 更多 PLE regs
```

**关键结论**: **Windows 驱动从不写 PLE_STATION_PAUSE0**。该寄存器由固件自动管理。PLE pause 是固件响应 BSS_INFO/STA_REC MCU 命令的结果。

---

## 三、任务 2: Windows 连接前的完整 MCU 命令序列

### 3.1 调用图

```
WdiTaskConnect (0x140065be0)
  │
  └─[1] MtCmdActivateDeactivateNetwork (0x1400c558c)
          └─ BssActivateCtrl: DEV_INFO(CID=1) + BSS_INFO(CID=2, BASIC+MLD ONLY)
               ↑ 原子批次提交，两个命令同时入队
  │
  └─ MlmeCntlOidConnectProc → MlmeCntlWaitJoinProc
          │
          ├─[2] MtCmdChPrivilage (0x1400c5e08) — ROC 请求
          │       └─ nicUniCmdChReqPrivilege: inner CID=0x27
          │
          ├─[3] MtCmdSendStaRecUpdate (0x1400cdea0) — STA_REC
          │       └─ 13 TLVs, STATE_INFO state=0 (DISCONNECT)
          │
          ├─[4] FUN_1400ac6c8(adapter, bss, 2, 0) — auth 触发
          │
          └─ state=5 → MlmeCntlWaitAuthProc
                  └─[5] FUN_1400ac6c8(adapter, bss, 1, sae_flag) — auth 帧 TX
```

### 3.2 BSS_INFO 双阶段发送

Windows **发送 BSS_INFO 两次**:

| 阶段 | 时机 | 内容 | TLV 数 |
|------|------|------|--------|
| **早期** | BssActivateCtrl (pre-auth) | BASIC + MLD | **2** |
| **完整** | MtCmdSetBssInfo (post-assoc) | 全部 14 TLVs | **14** |

**关键**: auth 帧发出前，固件只收到 BASIC + MLD 两个 TLV！**不含 RATE、PROTECT、IFS_TIME 等**。

### 3.3 我们的命令序列 vs Windows

| 步骤 | Windows | 我们的驱动 |
|------|---------|-----------|
| 1 | DEV_INFO + BSS_INFO(BASIC+MLD) ← 原子批次 | DEV_INFO (单独) |
| 2 | CH_PRIVILEGE (ROC 请求) | BSS_INFO (全 14 TLVs!) |
| 3 | STA_REC (STATE=0, DISCONNECT) | STA_REC (STATE=1, IDLE) |
| 4 | Auth TX | CH_PRIVILEGE (ROC 请求) |
| 5 | — | Auth TX |

**差异**:
1. BSS_INFO 内容错误: 我们发 14 TLVs，Windows 只发 BASIC+MLD
2. 命令顺序错误: Windows CH_PRIVILEGE 在 STA_REC 之前
3. STATE_INFO 值错误: 我们发 1(IDLE)，Windows 发 0(DISCONNECT)

---

## 四、任务 3: RX Filter 分析

### 4.1 .rdata 中没有独立的 SET_RX_FILTER 字符串

在 .rdata 中搜索 'Rx', 'RX', 'Filter' 相关字符串，**没有找到 nicUniCmdSetRxFilter 或 nicSetRxFilter**。

找到的相关字符串:
```
0x14022a170: "rx:"
0x14022b168: "MTCX_RXQUEUE"
0x140235fc8: "WDI_TLV_PACKET_FILTER_PARAMETERS"  ← WDI layer (NDIS interface)
0x1402382e8: "WDI_TLV_RECEIVE_FILTER_FIELD"
0x14023abc8: "MDP RX Q"
0x14023abe8: "SEC RX Q"
```

**结论**: SET_RX_FILTER 是 Windows 通过 WDI 层（NDIS 接口）实现的，不是直接的 UniCmd MCU 命令。固件 RX filter 由 BSS_INFO + STA_REC 的 BASIC TLV 中的 `bssid` 和 `wlan_idx` 字段隐式配置。

---

## 五、任务 4: STA_REC STATE_INFO 详细分析

### 5.1 STATE_INFO TLV wire 格式 (已从汇编确认)

```c
struct sta_rec_state_info_tlv {
    __le16 tag;      // +0x00 = 0x0007
    __le16 len;      // +0x02 = 0x0010 (16)
    u8     state;    // +0x04 — ← 状态字段在 offset+4
    u8     pad1[3];  // +0x05~07
    __le32 flags;    // +0x08 — flags 字段
    u8     action;   // +0x0c — 动作字段
    u8     pad2[3];  // +0x0d~0f
};
```

### 5.2 State 值映射 (汇编直接验证)

`MtCmdSendStaRecUpdate` 函数中的关键代码 (@ 0x1400ce1c3):

```asm
mov eax, dword ptr [r13 + 0x4b4]  ; eax = sta_entry内部状态
cmp eax, 1                         ; if internal=1 → wire=0
je  store_zero
cmp eax, 2                         ; if internal=2 → wire=1
jne check_three
mov byte ptr [rbp - 0x4c], 1      ; ← wire state=1 (IDLE)
jmp done
check_three:
cmp eax, 3                         ; if internal=3 → wire=2
jne store_zero
mov byte ptr [rbp - 0x4c], 2      ; ← wire state=2 (CONNECTED)
jmp done
store_zero:
mov byte ptr [rbp - 0x4c], 0      ; ← wire state=0 (DISCONNECT)
```

**内部状态 → wire 状态映射**:
| 内部状态 | Wire STATE_INFO 值 | 含义 |
|---------|-------------------|------|
| 0 | 0 | DISCONNECT |
| 1 | 0 | DISCONNECT (pre-join) |
| **2** | **1** | **IDLE** |
| **3** | **2** | **CONNECTED** |

**在 auth 阶段**: 内部状态=1 (JOIN 发起) → **wire state=0 (DISCONNECT)**

**我们当前发送 wire state=1 (IDLE) 是错误的**。Auth 阶段应该发送 wire state=0 (DISCONNECT)。

### 5.3 ROC GRANT handler (0x14014fe60)

ROC grant 后的处理:
```asm
mov dl, 0x27     ; inner CID = 0x27 (CH_PRIVILEGE)
call 0x14014f788 ; nicUniCmdAllocEntry(adapter, 0x27, 0x10)
; 填充 ROC grant 确认:
mov dword ptr [rax + 4], 0xc0001  ; flags
mov byte ptr [rax + 8], bss_idx
mov byte ptr [rax + 9], band_idx
mov byte ptr [rax + 0xa], activate_type  ; 0xFF/0xFE/原始值
```

ROC GRANT handler **不直接解除 PLE pause**，只是将 ROC grant 事件通知 TX 路径。固件通过 BSS_INFO+STA_REC 正确配置后自动清除 PLE pause。

---

## 六、根因总结与修复方向

### 6.1 根因 (按优先级)

**根因 1: BSS_INFO 内容错误 (最高优先级)**
我们在 auth 前发送完整 14-TLV BSS_INFO（含 RATE、PROTECT 等），而 Windows 只发 BASIC + MLD。RATE TLV 在 AP 未认证时指定速率，可能导致固件进入错误状态 → Ring 15 阻塞。

**根因 2: 命令顺序错误**
Windows 顺序: `DEV_INFO+BSS_INFO(basic)` → `CH_PRIVILEGE` → `STA_REC`
我们顺序: `DEV_INFO` → `BSS_INFO(full)` → `STA_REC` → `CH_PRIVILEGE`

**根因 3: STA_REC STATE 错误**
Auth 阶段应发 wire state=0 (DISCONNECT)，我们发的是 1 (IDLE)。

### 6.2 修复方案

**修复 1**: 将 BSS_INFO 拆分为两阶段:
- **Pre-auth**: 只发 BASIC + MLD TLV (与 Windows BssActivateCtrl 一致)
- **Post-assoc**: 发完整 14 TLVs (含 RATE、PROTECT 等)

**修复 2**: 调整命令顺序:
```
connect_callback():
  1. DEV_INFO + BSS_INFO(BASIC+MLD)  ← 原子或快速连续
  2. CH_PRIVILEGE (ROC 请求)
  3. STA_REC (STATE=0, 13 TLVs)
  4. (等 ROC grant → auth TX)
```

**修复 3**: STA_REC STATE_INFO: auth 阶段 state=0 (DISCONNECT)，assoc 完成后 state=2 (CONNECTED)。

### 6.3 BSS_INFO BASIC + MLD 内容 (auth 前)

来自 win_re_connect_flow_complete.md § 3.3，auth 前 BSS_INFO 内容:

```c
// BASIC TLV (tag=0x0000, len=0x0020):
bss_idx, ownmac_idx×2, sco=0xFF,
conn_type=0x10001 (INFRA STA),
active=1, network_type, bssid[6],
sta_type, bcn_interval, dtim,
phy_mode, mbss_flags=0x00FE,
wlan_idx, band_info

// MLD TLV (tag=0x001A, len=0x0014):
由 nicUniCmdBssInfoMld() 填充
```

**注意**: 不发 RATE TLV (tag=0x??), PROTECT TLV (tag=0x??), IFS_TIME TLV 等。

---

## 七、附: alloc_cmd_entry 函数 (0x14014f788)

```c
// nicUniCmdAllocEntry(adapter, inner_cid, payload_size)
// 返回: cmd_entry 指针，data 在 entry+0x28
void* nicUniCmdAllocEntry(adapter, u8 inner_cid, u32 payload_size) {
    magic_tag = 0x34354b4d;  // "4KM5" MediaTek allocation tag
    entry = ExAllocatePoolWithTag(NonPagedPool, payload_size + 0x28, magic_tag);
    if (!entry) return NULL;
    memset(entry, 0, payload_size + 0x28);
    entry->data_ptr = entry + 0x28;  // [+0x18]
    entry->payload_size = payload_size;  // [+0x14]
    entry->inner_cid = inner_cid;  // [+0x10]
    return entry;
}
```

---

*分析完成: 2026-02-22*
