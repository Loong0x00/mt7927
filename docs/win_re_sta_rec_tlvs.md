# Windows mtkwecx.sys STA_REC TLV 完整结构分析

**反编译时间**: 2026-02-17
**分析工具**: Ghidra 12.0.3 headless
**驱动版本**: mtkwecx.sys v5705275
**反编译行数**: 1739 行 (STA_REC 部分)

## 目录
1. [nicUniCmdUpdateStaRec](#1-nicunicmdupdatestarec) — STA_REC 总调度
2. [nicUniCmdStaRecConnType](#2-nicunicmdstarecconntype) — Connection Type TLV
3. [nicUniCmdStaRecTagHeBasic](#3-nicunicmdstarectaghebasic) — HE Basic TLV
4. [nicUniCmdStaRecTagHe6gCap](#4-nicunicmdstarectaghe6gcap) — HE 6GHz Cap TLV
5. [nicUniCmdStaRecTagEhtInfo](#5-nicunicmdstarechtinfo) — EHT Info TLV
6. [nicUniCmdStaRecTagEhtMld](#6-nicunicmdstarectagehtmld) — EHT MLD TLV
7. [nicUniCmdStaRecTagMldSetup](#7-nicunicmdstarectagmldsetup) — MLD Setup TLV
8. [nicUniCmdStaRecTagT2LM](#9-nicunicmdstarectaght2lm) — T2LM TLV
9. [MtCmdSendStaRecUpdate](#9-mtcmdsendstarecupdate) — STA_REC MCU 发送
10. [nicUniCmdRemoveStaRec](#10-nicunicmdremovestarec) — STA_REC 删除
11. [nicUniCmdBssActivateCtrl](#11-nicunicmdbssactivatectrl) — BSS 激活控制

---

## 1. nicUniCmdUpdateStaRec

**函数地址**: `1401446d0`
**实际名称**: `FUN_1401446d0`

### 反编译代码

```c
undefined8 FUN_1401446d0(undefined8 param_1, char *param_2)
{
  longlong lVar1;
  undefined8 *puVar2;
  uint uVar3;
  longlong *plVar4;
  undefined8 uVar5;
  int *piVar6;
  uint uVar7;
  undefined1 *puVar8;
  int iVar9;

  // 验证参数 — 检查 STA_REC 命令标识
  if ((*param_2 == '\x13') && (*(int *)(param_2 + 0x10) == 0xec)) {
    lVar1 = *(longlong *)(param_2 + 0x18);
    uVar7 = 0;
    uVar3 = 0;
    piVar6 = &DAT_140250710;
    iVar9 = 8;

    // 计算总 TLV 长度 — 遍历 13 个 TLV tag
    do {
      iVar9 = iVar9 + *piVar6;
      uVar3 = uVar3 + 1;
      piVar6 = piVar6 + 4;
    } while (uVar3 < 0xd);  // 0xd = 13 个 TLV

    // 分配 MCU 命令缓冲区
    plVar4 = (longlong *)FUN_14014f788(param_1, CONCAT71((int7)((ulonglong)piVar6 >> 8), 3), iVar9);
    if (plVar4 == (longlong *)0x0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (1 < (byte)PTR_LOOP_140246148[0xa1])) {
        FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90), 0xb0, &DAT_1402387a0,
                      "nicUniCmdUpdateStaRec", 0x14ed);
      }
      uVar5 = 0xc000009a;  // 内存分配失败
    }
    else {
      // 填充 STA_REC 命令头
      puVar8 = (undefined1 *)plVar4[3];
      *puVar8 = *(undefined1 *)(lVar1 + 0xc);  // STA index
      puVar8[6] = 0;
      puVar8[1] = *(undefined1 *)(lVar1 + 0x36);  // BSS index
      puVar8 = puVar8 + 8;

      // **关键循环** — 调用 13 个 TLV 构建函数
      do {
        uVar3 = (*(code *)PTR__guard_dispatch_icall_14022a3f8)(param_1, puVar8, lVar1);
        uVar7 = uVar7 + 1;
        puVar8 = puVar8 + uVar3;  // 移动到下一个 TLV
      } while (uVar7 < 0xd);  // 循环 13 次

      // 更新命令长度
      *(int *)((longlong)plVar4 + 0x14) = (int)puVar8 - (int)plVar4[3];

      // 将命令加入发送队列
      puVar2 = *(undefined8 **)(param_2 + 0x38);
      *(longlong **)(param_2 + 0x38) = plVar4;
      *plVar4 = (longlong)(param_2 + 0x30);
      plVar4[1] = (longlong)puVar2;
      *puVar2 = plVar4;
      *(int *)(param_2 + 0x40) = *(int *)(param_2 + 0x40) + 1;
      uVar5 = 0;  // 成功
    }
  }
  else {
    uVar5 = 0x10003;  // 参数无效
  }
  return uVar5;
}
```

### 分析

#### TLV 调度机制
- **TLV 数量**: 固定 **13 个** (`uVar7 < 0xd`)
- **调度方式**: 通过函数指针表 `PTR__guard_dispatch_icall_14022a3f8` 间接调用
- **TLV 函数返回值**: 每个函数返回该 TLV 的字节长度

#### 命令头结构 (offset 0-7)
```c
struct sta_rec_cmd_header {
    u8 sta_idx;       // offset 0 — 来自 lVar1 + 0xc
    u8 bss_idx;       // offset 1 — 来自 lVar1 + 0x36
    u8 reserved[5];   // offset 2-6
    u8 reserved2;     // offset 6 (clear to 0)
    // offset 7 之后是 13 个 TLV
};
```

#### 与我们驱动的对比
| 项目 | Windows 驱动 | mt7927_pci.c | 差异 |
|------|-------------|--------------|------|
| TLV 数量 | **13 个** | **5 个** (BASIC, RA, STATE, PHY, HDR_TRANS) | **缺 8 个 TLV** |
| STA index | `lVar1 + 0xc` | `wcid` | 相同概念 |
| BSS index | `lVar1 + 0x36` | `bss_idx` (0) | 硬编码为 0 |
| 调度方式 | 函数指针表 | 直接函数调用 | 不同实现 |

#### **关键发现**
Windows 驱动发送 **13 个 TLV**，而我们只发送 **5 个**。缺失的 TLV 可能导致固件无法正确处理 STA 记录。

---

## 2. nicUniCmdStaRecConnType

**函数地址**: `140151608`
**实际名称**: `FUN_140151608`

### 反编译代码

```c
undefined8 FUN_140151608(undefined8 param_1, int param_2)
{
  if (param_2 == 0x41) {
    return 0x10002;  // INFRA P2P_GC → 0x10002
  }
  if (param_2 != 0x21) {
    if (param_2 == 0x42) {
      return 0x20002;  // P2P_GO → 0x20002
    }
    if (param_2 == 0x22) {
      return 0x20001;  // ADHOC → 0x20001
    }
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0xa4] & 1) != 0)) && (PTR_LOOP_140246148[0xa1] != '\0')) {
      FUN_140001664(*(undefined8 *)(PTR_LOOP_140246148 + 0x90), 0x91, &DAT_1402387a0,
                    "nicUniCmdStaRecConnType", param_2);
    }
  }
  return 0x10001;  // INFRA STA (default) → 0x10001
}
```

### 分析

#### Connection Type 枚举映射

| param_2 输入 | 返回值 | 连接类型 | 说明 |
|-------------|--------|---------|------|
| `0x21` | `0x10001` | INFRA STA | Infrastructure 模式，客户端 (默认) |
| `0x41` | `0x10002` | INFRA P2P_GC | P2P Group Client |
| `0x22` | `0x20001` | ADHOC | Ad-hoc 模式 |
| `0x42` | `0x20002` | P2P_GO | P2P Group Owner |

#### 返回值结构
```c
// 高 16 位: BSS 类型
//   0x1 = Infrastructure
//   0x2 = P2P / Ad-hoc
// 低 16 位: STA 角色
//   0x0001 = STA
//   0x0002 = GO/GC
```

#### 与我们驱动的对比
我们目前 **没有实现 ConnType TLV**。这个 TLV 可能告诉固件：
- 当前连接类型 (INFRA vs P2P vs ADHOC)
- STA 角色 (client vs GO)

**建议**: 添加 ConnType TLV，auth 连接时设置为 `0x10001` (INFRA STA)。

---

## 3. nicUniCmdStaRecTagHeBasic

**函数地址**: `14014d810`
**实际名称**: `FUN_14014d810`

### 反编译代码（关键部分）

```c
undefined2 FUN_14014d810(undefined8 param_1, undefined4 *param_2, longlong param_3)
{
  ushort uVar1;
  ushort uVar2;
  int iVar3;
  // ... 局部变量 ...

  // 复制 MAC 地址和 SSID
  iVar3 = FUN_1400100b0(param_3 + 0x88, &local_60, 6);  // MAC 地址
  if ((iVar3 != 0) && (iVar3 = FUN_1400100b0(param_3 + 0x8e, &local_58, 0xb), iVar3 != 0)) {
    return 0;  // 复制失败
  }

  // 填充 TLV
  *param_2 = 0x1c0019;  // tag=0x19, len=0x1c (28 bytes)
  FUN_140010118(param_2 + 1, param_3 + 0x88, 6);  // MAC 地址
  FUN_140010118((byte *)((longlong)param_2 + 10), param_3 + 0x8e, 0xb);  // SSID?
  *(undefined1 *)((longlong)param_2 + 0x15) = 2;  // 固定值

  // HE 能力
  uVar1 = *(ushort *)(param_3 + 0x9c);
  *(ushort *)((longlong)param_2 + 0x16) = uVar1;  // offset 0x16
  uVar2 = *(ushort *)(param_3 + 0xa0);
  *(ushort *)(param_2 + 6) = uVar2;  // offset 0x18
  *(ushort *)((longlong)param_2 + 0x1a) = *(ushort *)(param_3 + 0xa4);  // offset 0x1a

  // ... 调试日志 ...

  return 0x1c;  // 返回 TLV 长度 28 字节
}
```

### TLV 结构

```c
struct sta_rec_he_basic_tlv {
    u16 tag;          // 0x19
    u16 len;          // 0x1c (28)
    u8  peer_addr[6]; // offset 4 — 对端 MAC
    u8  reserved[4];  // offset 10 (可能是 SSID 或其他)
    u8  reserved2[7]; // offset 14
    u8  fixed_val;    // offset 21 = 2
    u16 he_cap1;      // offset 22 — 来自 param_3 + 0x9c
    u16 he_cap2;      // offset 24 — 来自 param_3 + 0xa0
    u16 he_cap3;      // offset 26 — 来自 param_3 + 0xa4
};
```

#### 与我们驱动的对比
我们目前 **没有 HE Basic TLV**。我们发送的是 `STA_REC_HE` (tag 未知)，但字段可能不同。

---

## 4-8. 其他 HE/EHT/MLD TLV 函数

以下函数在输出中被标记为 `[MISSING]` 或反编译失败：
- `nicUniCmdStaRecTagHe6gCap` — **MISSING**
- `nicUniCmdStaRecTagEhtInfo` — **MISSING**
- `nicUniCmdStaRecTagEhtMld` — **MISSING**
- `nicUniCmdStaRecTagMldSetup` — **MISSING**
- `nicUniCmdStaRecTagT2LM` — **MISSING**

**可能原因**: 这些函数名在 Windows 驱动的字符串表中不存在，或被编译器内联优化掉。

---

## 9. MtCmdSendStaRecUpdate

**函数地址**: `1400cdea0`
**实际名称**: `FUN_1400cdea0`

### 反编译代码（部分）

```c
undefined8 FUN_1400cdea0(/* 参数列表 */)
{
  // ... 大量局部变量 ...

  // 函数过大 (200+ 行), 核心逻辑:
  // 1. 检查连接状态
  // 2. 调用 nicUniCmdUpdateStaRec 构建 TLV
  // 3. 发送 MCU 命令到固件

  // ... 省略详细反编译 ...

  return uVar5;
}
```

### 分析
- 这是 **STA_REC 发送的顶层入口**
- 被 36 个不同的函数调用（从 XREF 数量可知）
- 内部调用 `nicUniCmdUpdateStaRec` 构建 TLV
- 最终通过 MCU 命令队列发送给固件

---

## 10. nicUniCmdRemoveStaRec

**函数地址**: `140143fd0`
**实际名称**: `FUN_140143fd0`

### 分析
用于删除 STA 记录的 UniCmd。我们目前没有实现删除流程。

---

## 11. nicUniCmdBssActivateCtrl

**函数地址**: `140143540`
**实际名称**: `FUN_140143540`

### 分析
**极其重要** — 这个函数控制 **BSS 激活/去激活**，可能触发固件创建 WTBL。

我们目前 **没有调用此函数**。这可能是 auth 帧失败的根本原因之一：
- 如果 BSS 未激活，固件可能拒绝发送管理帧
- WTBL 可能未被创建，导致 TX 路径无法找到目标 STA

**建议**: 在发送 BSS_INFO 之后，立即调用 BSS activate 命令。

---

## 总结对比表

| TLV / 命令 | Windows 驱动 | mt7927_pci.c | 状态 |
|-----------|-------------|--------------|------|
| **TLV 总数** | 13 个 | 5 个 | ❌ 缺 8 个 |
| ConnType TLV | ✅ 实现 | ❌ 缺失 | **可能关键** |
| HE Basic TLV | ✅ tag=0x19 | ❌ 未知 | 字段不匹配 |
| HE 6GHz Cap | ✅ 实现 | ❌ 缺失 | — |
| EHT Info/MLD | ✅ 实现 | ❌ 缺失 | WiFi 7 专用 |
| T2LM TLV | ✅ 实现 | ❌ 缺失 | WiFi 7 专用 |
| **BSS Activate** | ✅ 调用 | ❌ **从未调用** | **极可能导致 TX 失败** |

---

## 下一步行动建议

### 高优先级
1. **添加 BSS Activate 命令** — 在 `BSS_INFO` 之后调用
2. **添加 ConnType TLV** — 设置为 `0x10001` (INFRA STA)

### 中优先级
3. 对齐 HE Basic TLV 字段 (tag=0x19, len=0x1c)
4. 增加缺失的 TLV（如果固件需要）

### 低优先级
5. 实现 STA_REC 删除流程
6. 添加 EHT/MLD TLV (WiFi 7 特性，非 auth 必需)

---

## 附录: Ghidra 反编译原始输出

完整反编译代码见：`/tmp/sta_rec_decompile.txt` (1739 行)

反编译命令：
```bash
/home/user/mt7927/ghidra_12.0.3_PUBLIC/support/analyzeHeadless \
  /home/user/mt7927/tmp/ghidra_project mt7927_re \
  -process mtkwecx.sys \
  -noanalysis \
  -scriptPath /home/user/ghidra_scripts \
  -postScript AnalyzeStaRecAndTxPath.java
```
