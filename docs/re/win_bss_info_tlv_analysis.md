# Windows BSS_INFO TLV 结构深度分析

**分析日期**: 2026-02-17
**分析人**: Ghidra RE Agent
**原始反编译**: `/home/user/mt7927/docs/win_re_bss_info_tlvs.md`

---

## 执行摘要

通过 Ghidra headless 反编译分析 Windows mtkwecx.sys 驱动，成功提取 **11 个 BSS_INFO 相关函数**的完整反编译代码。

**关键发现**：
1. **BSS_INFO Basic TLV (tag=0)** 的连接状态字段位于 **offset 8 (4 bytes)**，由函数 `nicUniCmdBssInfoConnType` 计算
2. 连接类型枚举值：`0x10001` (client), `0x20000`/`0x20001`/`0x20002` (AP modes)
3. MLD TLV (tag=0x1A) 长度为 0x14 (20 bytes)
4. **未找到** 10 个 TLV Tag 函数（可能未被字符串引用，或使用不同命名）

---

## 1. nicUniCmdBssInfoTagBasic (Tag 0x00)

### 函数信息
- **地址**: `14014c610`
- **原型**: `undefined2 FUN_14014c610(longlong *param_1, undefined4 *param_2, undefined1 *param_3)`
- **参数**:
  - `param_1`: BSS context 结构体指针
  - `param_2`: **输出** TLV buffer（将被填充）
  - `param_3`: 输入参数结构体

### TLV 结构（推断）

```c
struct BSS_INFO_BASIC_TLV {
    // Offset 0-3: TLV header
    u16 tag;           // 0x0000
    u16 length;        // 0x0020 (32 bytes)

    // Offset 4: BSS Index
    u8 bss_index;      // param_3[0x5e]

    // Offset 5-6: 未明确字段
    u8 field_5;        // param_1[0x2d2] or lVar7[0x20]
    u8 field_6;        // 同上

    // Offset 7: Connection type byte
    u8 conn_type_byte; // 根据 param_3[0x5a] 转换：
                       //   0x04 -> 0xFF
                       //   0x03 -> 0xFE
                       //   其他 -> 原值

    // Offset 8-11: CONNECTION TYPE (关键字段!)
    u32 connection_type;  // 来自 FUN_14014fa20 (nicUniCmdBssInfoConnType)
                          // 值:
                          //   0x00010001 = Infrastructure Client
                          //   0x00020000 = AP Mode 1 (adhoc=8)
                          //   0x00020001 = AP Mode 2 (ibss=5 or adhoc=0x20)
                          //   0x00020002 = AP Mode 3 (ibss=4 or adhoc=0x10)

    // Offset 12 (0xC):
    u8 field_12;       // ~(param_1[0x2e6964] >> 7) & 1

    // Offset 13 (0xD):
    u8 field_13;       // param_3[0x5b]

    // Offset 14-17 (0xE-0x11):
    u32 field_14;      // param_3[0x24] (4 bytes)

    // Offset 18-19 (0x12-0x13):
    u16 field_18;      // param_3[0x28] (2 bytes)

    // Offset 20-21 (0x14-0x15):
    u16 field_20;      // param_3[0x3a] (byte -> ushort)

    // Offset 22-23 (0x16-0x17):
    u16 dtim_period;   // param_1[0xb8612] or 100 (if conn_type==0x20002)

    // Offset 24 (0x18):
    u8 dtim_count;     // param_1[0xb862d] or param_1[0x2e6109]

    // Offset 25 (0x19):
    u8 capability_low; // FUN_14014fdfc() result low byte

    // Offset 26-27 (0x1A-0x1B):
    u16 field_26;      // lVar8[9] (from STA or BSS object)

    // Offset 28-29 (0x1C-0x1D):
    u16 field_28;      // param_3[0x34]

    // Offset 30 (0x1E):
    u8 capability_high; // FUN_14014fdfc() result high byte

    // Offset 31 (0x1F):
    u8 field_31;       // param_2[3] 第 193 行输出日志
} __packed;
```

### 关键代码分析

#### Line 138: TLV Header
```c
*param_2 = 0x200000;  // Little-endian: tag=0x0000, length=0x0020
```

#### Line 139-156: BSS Index + 未明确字段
```c
*(undefined1 *)(param_2 + 1) = param_3[0x5e];  // offset 4: bss_index

// offset 5-6: 根据条件从不同来源获取
if ((lVar7 == 0) || ((int)param_1[2] != 1)) {
    *(undefined1 *)((longlong)param_2 + 5) = *(undefined1 *)((longlong)param_1 + 0x2d2);
    uVar2 = *(undefined1 *)((longlong)param_1 + 0x2d2);
} else {
    *(undefined1 *)((longlong)param_2 + 5) = *(undefined1 *)(lVar7 + 0x20);
    uVar2 = *(undefined1 *)(lVar7 + 0x20);
}
*(undefined1 *)((longlong)param_2 + 6) = uVar2;

// offset 7: connection type byte transformation
cVar3 = param_3[0x5a];
if (cVar3 == '\x04') {
    cVar3 = -1;  // 0xFF
} else if (cVar3 == '\x03') {
    cVar3 = -2;  // 0xFE
}
*(char *)((longlong)param_2 + 7) = cVar3;
```

#### Line 157-158: **CONNECTION TYPE** (最关键!)
```c
iVar5 = FUN_14014fa20(param_1);  // 调用 nicUniCmdBssInfoConnType
param_2[2] = iVar5;              // 存储到 offset 8 (4 bytes)
```

**这是我们一直在找的连接状态字段！**

---

## 2. nicUniCmdBssInfoConnType (连接类型计算)

### 函数信息
- **地址**: `14014fa20`
- **原型**: `undefined8 FUN_14014fa20(longlong param_1)`
- **作用**: 根据 BSS context 计算连接类型值

### 逻辑分析

```c
undefined8 FUN_14014fa20(longlong param_1)
{
  int iVar1 = *(int *)(param_1 + 0x10);  // BSS 模式
  int iVar2;

  // Case 1: Infrastructure Client
  if ((iVar1 == 1) && (*(int *)(param_1 + 0x28) == 0)) {
    return 0x10001;  // ← Infrastructure 模式
  }

  // Case 2-4: AP 模式
  if (*(int *)(param_1 + 0x28) == 1) {
    iVar2 = *(int *)(param_1 + 0x2dc);

    if (iVar2 == 8) {
      return 0x20000;  // ← AP Mode 1
    }
    if ((iVar1 == 5) || (iVar2 == 0x20)) {
      return 0x20001;  // ← AP Mode 2
    }
    if ((iVar1 == 4) || (iVar2 == 0x10)) {
      return 0x20002;  // ← AP Mode 3 (会调整 DTIM period = 100)
    }
  }

  return 0;  // 未连接
}
```

### Connection Type 枚举（推断）

```c
enum BSS_CONNECTION_TYPE {
    CONN_TYPE_NONE         = 0x00000000,  // 未连接
    CONN_TYPE_INFRA_CLIENT = 0x00010001,  // Infrastructure Client (STA mode)
    CONN_TYPE_AP_MODE1     = 0x00020000,  // AP mode (adhoc type 8)
    CONN_TYPE_AP_MODE2     = 0x00020001,  // AP mode (ibss=5 or adhoc=0x20)
    CONN_TYPE_AP_MODE3     = 0x00020002,  // AP mode (ibss=4 or adhoc=0x10)
};
```

**关键发现**：
- Infrastructure Client (STA) 模式 = `0x10001`
- 这与我们之前看到的 MT6639 Android 代码中的 `MEDIA_STATE_CONNECTED = 0` **不同**！
- Windows 驱动使用的是 **connection type 枚举**，不是简单的 connected/disconnected 状态

---

## 3. nicUniCmdBssInfoMld (Tag 0x1A)

### 函数信息
- **地址**: `14014fad0`
- **TLV Header**: `*param_2 = 0x14001a` → tag=0x1A, length=0x14

### TLV 结构（推断）

```c
struct BSS_INFO_MLD_TLV {
    u16 tag;           // 0x001A
    u16 length;        // 0x0014 (20 bytes)

    // Offset 4: Group MLD ID
    u8 group_mld_id;   // 0xFF (default) or *(lVar3 + 2)

    // Offset 5: Own MLD ID
    u8 own_mld_id;     // *(param_1 + 0x24) or BSS index

    // Offset 6-9: 未明确
    u32 field_6;       // *(param_1 + 0x2cc)

    // Offset 10-11:
    u16 field_10;      // param_1[0x5a]

    // Offset 12: 未明确
    u8 field_12;       // 0

    // Offset 13-14: OmRemapIdx
    u16 om_remap_idx;  // 0xFFFF (default)

    // Offset 15-23: MLD specific fields (条件填充)
    u8 field_15;       // *(lVar3 + 3)
    u8 field_16;       // *(lVar4 + 0x8fb)
    u8 field_17;       // *(lVar3 + 0xd)
    u8 field_18;       // 1/2 based on param_1[0xb95fc]
    // ... more fields
} __packed;
```

### 关键代码

```c
*param_2 = 0x14001a;  // TLV header

// 默认值
*(undefined1 *)(param_2 + 1) = 0xff;         // group_mld_id = 0xFF
*(undefined1 *)((longlong)param_2 + 5) = *(undefined1 *)((longlong)param_1 + 0x24);  // own_mld_id
*(undefined4 *)((longlong)param_2 + 6) = *(undefined4 *)((longlong)param_1 + 0x2cc);
*(short *)((longlong)param_2 + 10) = (short)param_1[0x5a];
*(undefined1 *)(param_2 + 4) = 0;
param_2[3] = 0xffff;  // om_remap_idx = 0xFFFF
```

---

## 4. 其他找到的 TLV 函数

### 4.1 nicUniCmdBssInfoTagHe (Tag 0x05)
- **地址**: `14014cd50`
- **TLV Header**: `*param_2 = 0x100005` → tag=0x05, length=0x10
- **用途**: HE (WiFi 6) capability

### 4.2 nicUniCmdBssInfoTagEht (Tag 0x0C)
- **地址**: `14014d150`
- **用途**: EHT (WiFi 7) capability

### 4.3 nicUniCmdBssInfoTagBssColor (Tag 0x09)
- **地址**: `14014d010`
- **用途**: BSS Color (WiFi 6)

### 4.4 nicUniCmdBssInfoTagSTAIoT
- **地址**: `14014d350`

---

## 5. 缺失的 TLV 函数（未找到）

以下 10 个函数**未在字符串表中找到**，可能原因：
1. 函数名被编译器优化移除
2. 使用了不同的命名
3. 函数被内联
4. 通过函数指针表间接调用

**缺失列表**：
- `nicUniCmdBssInfoTagRlm` (RLM TLV)
- `nicUniCmdBssInfoTagProtect` (Protection TLV)
- `nicUniCmdBssInfoTagIfsTime` (IFS Time TLV)
- **`nicUniCmdBssInfoTagRate`** ← **最关键**（固件可能需要这个来知道发送速率）
- `nicUniCmdBssInfoTagSec` (Security TLV)
- `nicUniCmdBssInfoTagQbss` (QBSS TLV)
- `nicUniCmdBssInfoTagSap` (SAP TLV)
- `nicUniCmdBssInfoTagP2P` (P2P TLV)
- `nicUniCmdBssInfoTag11vMbssid` (11v MBSSID TLV)
- `nicUniCmdBssInfoTagWapi` (WAPI TLV)

---

## 6. 与 Linux 驱动的对比

| 字段 | Windows (反编译) | 我们的驱动 | 差异 |
|------|-----------------|-----------|------|
| **连接状态字段** | `connection_type` @ offset 8 | `ucConnectionState` @ offset ? | **完全不同的语义** |
| 连接状态值 (STA) | `0x00010001` | `0` (DISCONNECT) | **我们用错了枚举！** |
| BSS Basic TLV tag | `0x0000` | `0x0000` | ✅ 相同 |
| BSS Basic TLV length | `0x0020` (32 bytes) | 需确认 | ? |
| MLD TLV tag | `0x001A` | `0x001A` | ✅ 相同 |
| MLD TLV length | `0x0014` (20 bytes) | `0x0014` | ✅ 相同 |

---

## 7. 最重要的发现：Connection Type 字段

### 当前驱动的错误

我们的驱动在发送 auth 帧之前，BSS_INFO 命令中：
```c
// 错误！使用了 Android MT6639 的枚举
ucConnectionState = MEDIA_STATE_DISCONNECTED = 0;
```

### Windows 驱动的实际做法

```c
// 正确！使用 connection_type 枚举
connection_type = CONN_TYPE_INFRA_CLIENT = 0x00010001;  // STA 模式
```

### 推测：为什么 auth 帧失败

固件可能检查 `connection_type` 字段来决定：
1. **是否允许发送管理帧**
2. **使用什么 TX queue**
3. **使用什么发送速率**

如果 `connection_type == 0`（我们当前的值），固件可能认为：
- "这个 BSS 没有连接类型，不应该发送帧"
- 或者 "connection_type=0 是无效配置，拒绝发送"

**stat=1** (TXFREE 失败) 很可能是固件返回的 "配置错误，拒绝发送" 状态码。

---

## 8. 下一步行动建议

### 高优先级修复

1. **修改 BSS_INFO Basic TLV 结构**
   - 添加 `connection_type` 字段（4 bytes @ offset 8）
   - 在 STA 模式下设置为 `0x00010001`

2. **查找 RATE TLV**
   - 可能需要通过 IDA Pro 的函数调用图分析
   - 或者查找 `nicUniCmdSetBssInfo` 的函数指针表
   - RATE TLV 可能是固件知道发送速率的关键

3. **对比 MT6639 Android 驱动的 BSS_INFO 结构**
   - 确认 `connection_type` 字段的偏移和大小
   - 查看 MT6639 是否也使用 `0x00010001` 作为 STA 模式值

### 中优先级

4. **验证 BSS Basic TLV 的完整结构**
   - 手工分析反编译代码，确认所有 32 bytes 的布局
   - 与 MT6639 Android 代码对比验证

5. **查找其他缺失的 TLV**
   - 特别是 RLM, Protect, IfsTime, Sec 等

---

## 9. Ghidra 分析统计

- **目标函数**: 21 个
- **找到**: 11 个 (52.4%)
- **缺失**: 10 个 (47.6%)
- **完整反编译**: 11 个
- **分析时间**: ~5 分钟（headless mode）

**找到的函数**：
1. ✅ nicUniCmdSetBssInfo
2. ✅ nicUniCmdBssInfoTagBasic ← **最关键**
3. ✅ nicUniCmdBssInfoTagHe
4. ✅ nicUniCmdBssInfoTagEht
5. ✅ nicUniCmdBssInfoTagBssColor
6. ✅ nicUniCmdBssInfoConnType ← **最关键**
7. ✅ nicUniCmdBssInfoMld
8. ✅ nicUniCmdBssInfoTagSTAIoT
9. ✅ MtCmdSetBssInfo
10. ✅ nicUniCmdSetBssRlm
11. ✅ nicUniCmdSetBssRlmImpl

---

## 附录：完整反编译代码

详见：`/home/user/mt7927/docs/win_re_bss_info_tlvs.md`
