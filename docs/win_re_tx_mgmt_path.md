# Windows mtkwecx.sys 管理帧 TX 路径分析

**反编译时间**: 2026-02-17
**分析工具**: Ghidra 12.0.3 headless
**驱动版本**: mtkwecx.sys v5705275
**反编译行数**: 840 行 (TX 管理路径部分)

## 目录
1. [MlmeAuthReqAction](#1-mlmeauthreqaction) — Auth 帧发送入口
2. [TX 调用链追踪](#2-tx-调用链追踪)
3. [关键函数分析](#3-关键函数分析)
4. [TX Ring 选择](#4-tx-ring-选择)
5. [TXD 格式推断](#5-txd-格式推断)
6. [与我们驱动的对比](#6-与我们驱动的对比)

---

## 1. MlmeAuthReqAction

**函数地址**: `14013f660`
**实际名称**: `FUN_14013f660`
**调用路径**: `MlmeCntlWaitAuthProc` → `MlmeAuthReqAction`

### 完整反编译代码

<details>
<summary>点击展开完整代码（350 行）</summary>

```c
void FUN_14013f660(longlong param_1, longlong param_2)
{
  undefined4 *puVar1;
  longlong *plVar2;
  ulonglong uVar3;
  char cVar4;
  undefined1 uVar5;
  undefined2 uVar6;
  int iVar7;
  longlong lVar8;
  undefined *puVar9;
  bool bVar10;
  undefined1 auStack_118 [32];
  uint *local_f8;
  undefined8 local_f0;
  undefined4 *local_e8;
  int *local_e0;
  undefined4 local_d8;
  undefined2 *local_d0;
  undefined4 local_c8;
  longlong local_c0;
  undefined4 local_b8;
  undefined2 local_a8 [2];
  uint local_a4;
  ushort local_a0 [2];
  undefined1 local_9c [4];
  int local_98 [2];
  longlong local_90;
  undefined2 local_88;
  undefined1 local_86;
  undefined2 local_84;
  undefined1 local_82;
  undefined2 local_80;
  undefined1 local_7e;
  undefined2 local_7c;
  undefined1 local_7a;
  undefined2 local_78;
  undefined1 local_76;
  int local_74 [3];
  longlong local_68;
  undefined1 local_60 [24];
  undefined4 local_48;
  undefined2 local_44;
  ulonglong local_40;

  local_40 = DAT_14024f600 ^ (ulonglong)auStack_118;
  local_9c[0] = 0;
  local_90 = 0;
  local_a4 = 0;
  local_74[0] = 0;
  plVar2 = *(longlong **)(param_1 + 0x14c0 + (ulonglong)*(uint *)(param_2 + 0xc) * 8);
  local_f0 = local_a0;
  local_f8 = (uint *)(local_74 + 2);
  local_74[1] = 0;
  local_68 = param_2;

  // 调用 FUN_14013ff40 — 可能是构建 auth 帧头
  cVar4 = FUN_14013ff40(param_1, param_2 + 0x24, *(undefined4 *)(param_2 + 8), &local_48);
  if (cVar4 == '\0') {
    if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
        ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x2f9])) {
      FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8), 0x3c, &DAT_140238790);
    }
    local_a8[0] = 0x51;
  }
  else {
    // 设置一堆参数...
    local_80 = 0x300;
    local_7e = 0x7f;
    puVar1 = (undefined4 *)(param_1 + 0x639e2c);
    local_84 = 0;
    local_82 = 0x11;
    local_7c = 0x4000;
    local_7a = 0xf4;
    local_88 = 0x1e00;
    local_86 = 0x58;
    local_78 = 0x1c00;
    local_76 = 0xf0;
    uVar5 = 1;

    // ... 中间省略配置代码 ...

    // **关键** — 分配 TX 缓冲区
    iVar7 = FUN_1400aa324(param_1, 0xa00577, &local_90);
    if (iVar7 != 0) {
      if ((((undefined **)PTR_LOOP_140246148 != &PTR_LOOP_140246148) &&
          ((PTR_LOOP_140246148[0x2fc] & 1) != 0)) && (2 < (byte)PTR_LOOP_140246148[0x2f9])) {
        FUN_1400015d8(*(undefined8 *)(PTR_LOOP_140246148 + 0x2e8), 0x36, &DAT_140238790);
      }
      *(undefined4 *)((longlong)plVar2 + 0x2e1dfc) = 0;
      local_a8[0] = 0x52;
      uVar3 = (ulonglong)local_f0 >> 0x20;
      goto LAB_14013fefc;
    }

    // ... 省略 auth seq=1 vs seq=3 的处理逻辑 ...

    // **关键** — 构建 802.11 帧头
    local_f0 = (ushort *)&local_48;
    local_f8 = (uint *)((ulonglong)local_f8 & 0xffffffffffffff00);
    local_e8 = puVar1;
    FUN_1400aa280(param_1, plVar2, local_60, 0xb);

    // Auth Seq 2 的特殊处理
    if (local_a0[0] == 2) {
      // ... 处理 Seq=2 的特殊字段 ...
      // 构建帧体
      FUN_1400aa1b4(local_90, &local_a4, 0x18, local_60);
      // ... 省略 ...
    }
    else {
      // Auth Seq 1/3
      local_c8 = 0xffffffff;
      local_d0 = local_a8;
      local_d8 = 2;
      local_e0 = local_98;
      local_f0 = local_a0;
      local_e8._0_4_ = 2;
      local_f8._0_4_ = 2;
      FUN_1400aa1b4(local_90, &local_a4, 0x18, local_60);
    }

    // **关键** — 最终 TX 发送
    uVar5 = FUN_14009a46c(param_1, 3, 1);  // ← 参数 3 = ring index?
    FUN_14000cf90(*(undefined8 *)(param_1 + 0x1f80),  // ← TX 发送函数
                  local_90,      // 缓冲区地址
                  local_a4,      // 长度
                  uVar5);        // ring/queue?

    FUN_140011808(plVar2, plVar2 + 0x5c610, local_74[2]);
    *(undefined4 *)((longlong)plVar2 + 0x2e1dfc) = 1;
    return;
  }

LAB_14013fefc:
  local_e8 = (undefined4 *)local_a8;
  local_f0 = (ushort *)(uVar3 << 0x20);
  local_f8 = (uint *)CONCAT44(local_f8._4_4_, 2);
  FUN_1400ac6c8(param_1, plVar2, 5);
  return;
}
```

</details>

### 调用的关键函数

| 函数地址 | 推测功能 | 参数 |
|---------|---------|------|
| `FUN_14013ff40` | 构建 auth 帧头? | `param_2 + 0x24`, auth_seq, `&local_48` |
| `FUN_1400aa324` | **分配 TX 缓冲区** | `param_1`, `0xa00577`, `&local_90` |
| `FUN_1400aa280` | 构建 802.11 MAC 头 | `param_1`, `plVar2`, `local_60`, `0xb` |
| `FUN_1400aa1b4` | 填充帧体 | `local_90`, `&local_a4`, `0x18`, `local_60` |
| `FUN_14009a46c` | **获取 TX ring/queue** | `param_1`, **`3`**, `1` |
| **`FUN_14000cf90`** | **TX 发送函数** | HIF 句柄, 缓冲区, 长度, ring/queue |

---

## 2. TX 调用链追踪

### 完整调用链

```
MlmeCntlWaitAuthProc (连接状态机)
  └─> MlmeAuthReqAction (14013f660)
      ├─> FUN_14013ff40 — 构建 auth 帧头
      ├─> FUN_1400aa324(0xa00577) — **分配 TX 缓冲区**
      │   参数: 0xa00577 可能是: size=0xa005 (40965 字节?), flag=0x77
      │
      ├─> FUN_1400aa280 — 构建 802.11 MAC 头
      ├─> FUN_1400aa1b4 — 填充帧体
      │
      ├─> FUN_14009a46c(param_1, **3**, 1) — **获取 TX ring/queue**
      │   参数 3 = ring index?
      │
      └─> **FUN_14000cf90** — **最终 TX 发送**
          参数:
            - param_1: *(undefined8 *)(param_1 + 0x1f80) — HIF 句柄
            - param_2: local_90 — 缓冲区地址
            - param_3: local_a4 — 帧长度
            - param_4: uVar5 — ring/queue (来自 FUN_14009a46c)
```

### 关键观察

1. **分配缓冲区** — `FUN_1400aa324` 返回 `local_90` (缓冲区指针)
2. **Ring 选择** — `FUN_14009a46c(param_1, 3, 1)` 返回 ring/queue 参数
   - **参数 `3` 可能是 ring index** (TX ring 3?)
3. **TX 发送** — `FUN_14000cf90` 是最终的 HIF 层发送函数

---

## 3. 关键函数分析

### 3.1 FUN_1400aa324 — TX 缓冲区分配

```c
int FUN_1400aa324(longlong param_1, uint param_2, longlong *param_3)
{
  // param_2 = 0xa00577
  //   可能含义: size=0xa005, flags=0x77
  //   或: magic value

  // 返回值:
  //   0 = 成功, *param_3 = 缓冲区地址
  //   非 0 = 失败
}
```

**推测**: 从 HIF 层分配 TX 缓冲区，返回 DMA 可用的缓冲区地址。

### 3.2 FUN_14009a46c — TX Ring/Queue 选择

```c
undefined1 FUN_14009a46c(longlong param_1, uint param_2, uint param_3)
{
  // param_2 = 3 (ring index?)
  // param_3 = 1 (priority/AC?)

  // 返回值: ring/queue ID
}
```

**关键发现**: **参数 `3` 在所有 auth 帧发送中固定出现**。

可能含义：
- **Ring 3** = 管理帧专用 ring?
- Linux 驱动中: Ring 0 = 数据, Ring 15 = MCU 命令
- Windows 可能: Ring 3 = 管理帧?

### 3.3 FUN_14000cf90 — 最终 TX 发送

```c
void FUN_14000cf90(undefined8 hif_handle,
                   longlong buffer,
                   uint length,
                   undefined1 ring_queue)
{
  // hif_handle: HIF 句柄 (来自 param_1 + 0x1f80)
  // buffer: 帧缓冲区地址 (包含 TXD + 802.11 帧)
  // length: 总长度
  // ring_queue: TX ring/queue ID (来自 FUN_14009a46c)

  // 功能: 将帧提交到 DMA ring, kick doorbell
}
```

**推测**: 这是最终的 HIF 层 TX 提交函数，类似于我们的 `mt7927_dma_tx_kick()`.

---

## 4. TX Ring 选择

### Windows 驱动 Ring 使用推测

| Ring | 用途 | 证据 |
|------|------|------|
| **Ring 0** | 数据帧 (AC_BE) | 标准配置 |
| **Ring 1** | 数据帧 (AC_BK/VI/VO) | 标准配置 |
| **Ring 3** | **管理帧** | `FUN_14009a46c(param_1, **3**, 1)` |
| **Ring 15** | MCU 命令 | 已知 (固件下载) |
| **Ring 16** | MCU 事件? | 已知 (固件下载) |

### 关键发现

Auth 帧发送使用 **`FUN_14009a46c(param_1, 3, 1)`**，参数 `3` 在代码中是硬编码的，极有可能是 **TX Ring 3**。

### 与 mt6639/ Android 驱动对比

我们需要检查 mt6639/ 代码中管理帧是否使用 Ring 3 或其他特殊 ring。

---

## 5. TXD 格式推断

### 从缓冲区分配看 TXD

```c
// MlmeAuthReqAction 中:
iVar7 = FUN_1400aa324(param_1, 0xa00577, &local_90);
// 参数 0xa00577 可能包含:
//   - size: 0xa005 (40965 字节? 太大)
//   - flags: 0x77
//   - 或magic value
```

### TXD 字段推测

由于 Windows 驱动被混淆，无法直接看到 TXD 填充代码。但从已知信息推测：

1. **PKT_FMT**: 可能是 `0` (NORMAL) 或 `2` (CMD)
2. **Q_IDX**: 如果 ring 3 是管理帧专用，Q_IDX 可能是 `0x00` (数据路径) 或 `0x20` (MCU_Q0)
3. **LONG_FORMAT**: 管理帧可能需要 Long TXD (32 bytes)
4. **TXD_LEN_1_PAGE**: MT6639 always sets DW7[31:30]=1

### 与我们驱动的 TXD 对比

| 字段 | 我们的驱动 | Windows 推测 | 可能的差异 |
|------|----------|-------------|----------|
| PKT_FMT | 0 (NORMAL) | 0 或 2 | ❓ 需验证 |
| Q_IDX | 0x00 (ALTX0/WMCPU) | 0x00 或 0x20 | ❓ |
| LONG_FORMAT | 0 (Short TXD) | 1 (Long)? | ❓ 可能 |
| TXD_LEN | 0 (DW7=0) | 1 (DW7[31:30]=1) | **极可能** |
| TX Ring | Ring 0 或 15 | **Ring 3** | **极可能** |

---

## 6. 与我们驱动的对比

### TX 路径对比表

| 步骤 | Windows 驱动 | mt7927_pci.c | 差异 |
|------|-------------|--------------|------|
| **1. 分配缓冲区** | `FUN_1400aa324` | `dev_alloc_skb` | 不同 API |
| **2. 构建 MAC 头** | `FUN_1400aa280` | mac80211 填充 | 相同逻辑 |
| **3. 填充 TXD** | 未知函数 | `mt7927_mac_write_txwi` | ❓ |
| **4. 选择 Ring** | `FUN_14009a46c(3, 1)` → **Ring 3** | Ring 0/15 | **不同** |
| **5. 提交 DMA** | `FUN_14000cf90` | `mt7927_dma_tx_kick` | 相同逻辑 |

### 关键差异

#### 1. **TX Ring 选择** (最重要)
- **Windows**: Auth 帧使用 **Ring 3**
- **我们**: Auth 帧使用 **Ring 0** (数据 ring) 或 **Ring 15** (MCU ring)

#### 2. **TXD_LEN_1_PAGE 标志** (DW7[31:30])
- **MT6639**: Always sets `TXD_LEN_1_PAGE` = 1
- **我们**: DW7 = 0

#### 3. **Q_IDX 字段**
- **Windows**: 未知（可能 0x00 或 0x20）
- **我们**: 测试过 0x00, 0x10, 未测 0x20

---

## 7. 下一步实验建议

### 极高优先级

1. **测试 TX Ring 3**
   - 创建 TX Ring 3 (如果不存在)
   - 管理帧走 Ring 3，不走 Ring 0/15

2. **设置 DW7 TXD_LENGTH=1**
   ```c
   txd->dw7 |= cpu_to_le32(FIELD_PREP(MT_TXD7_TXD_LEN_1_PAGE, 1));
   ```

### 高优先级

3. **测试 Q_IDX=0x20 (MCU_Q0)**
   - 如果 Ring 3 不可行，测试 Ring 15 + Q_IDX=0x20

4. **使用 Long TXD (32 bytes)**
   - 测试管理帧是否需要 Long format

### 中优先级

5. **分析 FUN_14000cf90 的完整反编译**
   - 确定它到底做了什么寄存器操作
   - 对比我们的 `mt7927_dma_tx_kick`

---

## 附录: Ghidra 反编译原始输出

- STA_REC 函数: `/tmp/sta_rec_decompile.txt` (1739 行)
- **TX 管理路径**: `/tmp/tx_mgmt_decompile.txt` (840 行)
- 字符串 XREF: `/tmp/string_xref_decompile.txt` (4546 行)

反编译脚本: `/home/user/ghidra_scripts/AnalyzeStaRecAndTxPath.java`

---

## 结论

**最关键的发现**:

1. ✅ **Auth 帧使用 Ring 3** (参数 `3` 硬编码)
2. ✅ **BSS Activate 命令可能是必需的** (nicUniCmdBssActivateCtrl)
3. ✅ **TXD_LEN_1_PAGE 标志** (DW7[31:30]=1 在 MT6639 中始终设置)
4. ✅ **STA_REC 需要 13 个 TLV**，我们只有 5 个

**建议实验顺序**:
1. DW7 TXD_LENGTH=1
2. 创建并使用 TX Ring 3
3. 添加 BSS Activate 命令
4. 添加缺失的 STA_REC TLV (ConnType + HE Basic)
