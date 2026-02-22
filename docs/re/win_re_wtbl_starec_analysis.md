# Windows mtkwecx.sys WTBL + STA_REC 写入路径分析

**反编译时间**: 2026-02-17
**分析工具**: Ghidra 12.0.3 headless
**驱动版本**: mtkwecx.sys v5705275
**焦点**: WTBL[1] DW0=0 问题 — 为什么 AP MAC 地址没写入 WTBL?

---

## 核心发现

### 1. WTBL 写入机制: 固件自动从 STA_REC BASIC 创建

**没有独立的 WTBL 写入命令**。Windows 驱动和 MT6639 都通过 STA_REC BASIC TLV 的 `aucPeerMacAddr` + `EXTRA_INFO_NEW` 标志让固件自动创建 WTBL 条目。

关键证据:
- MT6639 `nicUniCmdStaRecTagBasic()` (nic_uni_cmd_event.c:2275):
  ```c
  tag->u2Tag = UNI_CMD_STAREC_TAG_BASIC;  // 0x00
  COPY_MAC_ADDR(tag->aucPeerMacAddr, cmd->aucMacAddr);
  tag->u2ExtraInfo = STAREC_COMMON_EXTRAINFO_V2 | STAREC_COMMON_EXTRAINFO_NEWSTAREC;  // 0x03
  ```
- `UNI_CMD_STAREC_TAG_WTBL (0x0d)` 存在于枚举中但 **MT6639 不在 arUpdateStaRecTable 中使用**
- Windows 驱动 `nicUniCmdUpdateStaRec` 遍历 13 个 TLV，但没有 WTBL 专用 TLV

### 2. ⚠️ conn_type 值不一致 — 可能导致 WTBL 创建失败

| 驱动 | STA_REC conn_type | BSS_INFO conn_type |
|------|-------------------|-------------------|
| **Windows** | **0x10001** (INFRA_STA) | 未确认 |
| **MT6639 源码** | 0x10002 (INFRA_AP) | INFRA_STA |
| **我们的驱动** | **0x10002** (INFRA_AP) | 0x10001 (INFRA_STA) |

Windows `nicUniCmdStaRecConnType`:
```c
// param_2 = 0x21 (STA_TYPE_LEGACY_AP)
return 0x10001;  // CONNECTION_INFRA_STA !!
```

**我们的驱动**: `CONNECTION_INFRA_AP = 0x10002` (跟随 mt76 惯例)

**这可能是关键差异!** 固件内部可能用 conn_type 决定如何处理 WTBL 条目。如果值不匹配固件预期，WTBL 可能不会正确创建。

### 3. BSS_ACTIVATE_CTRL 序列 — Windows 发送 DEVINFO + BSS_INFO 组合命令

Windows `nicUniCmdBssActivateCtrl` (0x140143540) 在连接时发送:

**激活顺序 (ucActive=1)**:
1. **DEVINFO ACTIVE** (CID=0x01) — 先发
   ```
   header: OwnMacIdx, DbdcIdx=0xFF(BAND_AUTO)
   tag=0 (ACTIVE), len=12
   ucActive=1, OwnMacAddr[6]
   ```
2. **BSS_INFO BASIC+MLD** (CID=0x02) — 后发
   ```
   header: BssIdx
   BASIC tag: Active, OwnMacIdx, HwBssIdx, ConnectionType, ConnState,
              WmmIdx, PhyMode, BcnInterval, DtimPeriod, BcMcWlanIdx,
              StaRecIdxOfAP=0xFFFE(NOT_FOUND)
   MLD tag: GroupMldId=0xFF, OwnMldId=BssIdx, OmRemapIdx=0xFF
   ```

**去激活顺序 (ucActive=0)**: BSS_INFO 先, DEVINFO 后 (反序)

**我们的驱动**:
- `add_interface()` → DEVINFO + BSS_INFO (一次性)
- `sta_state(NONE→AUTH)` → BSS_INFO + STA_REC (**不重发 DEVINFO**)

### 4. mldStarecAlloc (0x14017fb48) — 本地数据结构分配

```c
// 遍历 4 个 MLD STA_REC slot (每个 0xDE 字节)
do {
    entry = param_1 + 0x1BA4 + index * 0xDE;
    if (!entry[0]) {  // 未使用
        memset(entry, 0, 0xDE);
        entry[0] = 1;           // in_use = TRUE
        entry[1] = index;       // MLD STA index
        entry[2] = band_idx;   // 来自 param_5+2
        memcpy(entry+3, mac, 6); // **MAC 地址!** 来自 param_7
        entry[0x16] = wlan_idx; // 来自 param_6+0x2D08
        break;
    }
} while (++index < 4);
```

**不发送 MCU 命令** — 仅分配本地数据结构。MAC 地址存入 offset 3。

### 5. mldStarecRegister (0x14017ffa8) — 链接 MLD STA_REC 到 STA

```c
// 验证: 复制 MAC 地址对比 (param_3+0x902 → 6 字节)
// 如果 MLD entry 有效且 band 匹配:
*(param_3 + 0x8FA) = mld_entry[1];     // 存储 STA index
mld_entry[0x10] = *(param_3 + 0x909);  // 复制某字段
mld_entry[0x28] |= BIT(*(param_3+0x908) & 0x1F);  // 设置 bitmap
FUN_140180288(param_1, param_2, mld_entry, param_4);  // 更新内部状态
```

**也不发送 MCU 命令** — 仅链接内部数据结构。
调用者: `FUN_14017b810` (连接设置主函数)

### 6. MtCmdSendStaRecUpdate (0x1400cdea0) — STA_REC 发送入口

超大函数 (~500行反编译)。核心流程:
1. 从内部 STA 记录收集所有字段
2. 填充 local variables 用于 STA_REC 命令
3. 特殊处理: 检查芯片 ID (包括 0x6639, 0x7927, 0x7925)
4. 调用 `FUN_1400cdc4c` (实际 MCU 命令发送) — 参数 `(ctx, 0x13, 0xED, 0)`
   - 0x13 = UCmkd command class
   - 0xED = STA_REC command
5. 之前调用 `nicUniCmdUpdateStaRec` 构建 13 个 TLV

### 7. DEVINFO 命令头格式差异

**Windows**:
```
byte 0: OwnMacIdx
byte 1: DbdcIdx (0xFF = BAND_AUTO)
byte 2-3: reserved
```

**我们的驱动**: `u8 rsv[4] = {0, 0, 0, 0}` — **缺少 OwnMacIdx 和 DbdcIdx!**

但 DEVINFO 只在 `add_interface` 发一次，此时 omac_idx=0，所以可能无影响。

---

## 连接序列对比

### Windows 驱动
```
1. nicUniCmdBssActivateCtrl:
   → DEVINFO ACTIVE (own MAC, band)
   → BSS_INFO BASIC + MLD (BSS context, conn_type, conn_state)
2. mldStarecAlloc (本地分配 MLD slot, 存 peer MAC)
3. mldStarecRegister (链接到 STA record)
4. MtCmdSendStaRecUpdate:
   → STA_REC (13 TLVs: BASIC 含 peer_addr, conn_type=0x10001,
              extra_info=0x03, conn_state=CONNECTED)
5. Auth 帧发送
```

### MT6639 Android 驱动
```
1. nicActivateNetwork → nicUniCmdBssActivateCtrl:
   → DEVINFO ACTIVE
   → BSS_INFO BASIC + MLD
2. aisFsmStateInit_JOIN:
   → cnmStaRecAlloc (分配 WTBL index via secPrivacySeekForEntry)
   → cnmStaSendUpdateCmd → nicUniCmdUpdateStaRec:
      STA_REC (10 TLVs: BASIC + HT + VHT + HE + HE_6G + STATE +
               PHY + RA + BA_OFFLOAD + UAPSD)
      BASIC: peer_addr=AP_MAC, conn_type=CONNECTION_INFRA_AP(0x10002),
             conn_state=STATE_CONNECTED(1), extra_info=0x03(VER|NEW)
3. Auth 帧发送
```

### 我们的驱动
```
1. add_interface:
   → DEVINFO ACTIVE (CID=0x01)
   → BSS_INFO (CID=0x02, 6 TLVs: BASIC+RLM+PROTECT+IFS+RATE+MLD)
2. sta_state NOTEXIST→NONE:
   → BSS_INFO (再次发送, 同上)
   → STA_REC (5 TLVs: BASIC+RA+STATE+PHY+HDR_TRANS)
     BASIC: peer_addr=sta->addr, conn_type=0x10002(INFRA_AP),
            conn_state=1(CONNECT), extra_info=0x03(VER|NEW)
3. Auth 帧发送
→ TXFREE stat=1, WTBL[1] DW0=0x00000000
```

---

## WTBL[1] DW0=0 的可能原因 (按优先级排序)

### 🔴 高优先级

1. **conn_type 不匹配**
   - Windows 发 0x10001 (INFRA_STA)，我们发 0x10002 (INFRA_AP)
   - 固件可能根据 conn_type 决定 WTBL 创建逻辑
   - **建议**: 改为 0x10001 测试

2. **STA_REC 缺少关键 TLV**
   - 我们: 5 TLV, Windows: 13 TLV, MT6639: 10 TLV
   - 缺少的 TLV 可能导致固件拒绝创建 WTBL
   - **关键缺失**: HT_INFO(0x09), VHT_INFO(0x0A), HE_BASIC(0x19), HE_6G_CAP(0x17)
   - **建议**: 至少添加 HT_INFO + VHT_INFO

3. **sta_req_hdr 的 wlan_idx 问题**
   - 如果 wlan_idx 不正确, 固件写 WTBL 到错误位置
   - MT6639: `WCID_SET_H_L(uni_cmd->ucWlanIdxHnVer, uni_cmd->ucWlanIdxL, cmd->ucWlanIndex)`
   - 我们: `wlan_idx_lo = idx & 0xff, wlan_idx_hi = (idx >> 8) & 0xff`
   - 注意: MT6639 使用 `ucWlanIdxHnVer` 而不是简单的高位, 可能包含版本信息

### 🟡 中优先级

4. **BSS_INFO 激活顺序**
   - Windows 在连接时重发 DEVINFO+BSS_INFO (通过 BssActivateCtrl)
   - 我们只在 add_interface 发 DEVINFO, sta_state 时不重发
   - 可能导致固件 BSS context 不完整

5. **DEVINFO 命令头缺少 OwnMacIdx/DbdcIdx**
   - 我们的 rsv[4] 全零, Windows 填入 OwnMacIdx + DbdcIdx=0xFF
   - 如果 DEVINFO header 格式不正确, 固件可能忽略整个命令

### 🟢 低优先级

6. **MT6639 WTBL 搜索 bug workaround**
   - MT6639 跳过 index 0 和每 8 的倍数 (CFG_WIFI_WORKAROUND_HWITS00012836)
   - 我们从 index 1 开始分配, 不跳过 8 的倍数
   - 不太可能是当前问题 (WTBL[1] 不是 8 的倍数)

---

## 建议修改 (按优先级)

1. **将 STA_REC conn_type 改为 0x10001** (CONNECTION_INFRA_STA)
   - 匹配 Windows 驱动实际值
   - 最简单的测试, 一行改动

2. **添加 HT_INFO (tag=0x09) + VHT_INFO (tag=0x0A) TLV 到 STA_REC**
   - MT6639 和 Windows 都发送这些 TLV
   - 固件可能需要它们来正确创建 WTBL

3. **sta_req_hdr 中 wlan_idx_hi 字段检查**
   - MT6639 用 `ucWlanIdxHnVer` — 可能高 4 位是版本号
   - 确认 wlan_idx_hi 只包含索引高位, 不含版本

4. **DEVINFO 头添加 OwnMacIdx 和 DbdcIdx=0xFF**

---

## 附录: 关键结构体

### CMD_BSS_ACTIVATE_CTRL (MT6639)
```c
struct CMD_BSS_ACTIVATE_CTRL {
    uint8_t ucBssIndex;        // 0
    uint8_t ucActive;          // 1
    uint8_t ucNetworkType;     // 2
    uint8_t ucOwnMacAddrIndex; // 3
    uint8_t aucBssMacAddr[6];  // 4-9
    uint8_t ucBMCWlanIndex;    // 10
    uint8_t ucReserved;        // 11
};
```

### UNI_CMD_STAREC_BASIC (MT6639)
```c
struct UNI_CMD_STAREC_BASIC {
    uint16_t u2Tag;            // 0x00
    uint16_t u2Length;
    uint32_t u4ConnectionType; // Windows: 0x10001, MT6639: 0x10002
    uint8_t  ucConnectionState; // STATE_CONNECTED = 1
    uint8_t  ucIsQBSS;
    uint16_t u2AID;
    uint8_t  aucPeerMacAddr[6]; // AP MAC → firmware writes to WTBL
    uint16_t u2ExtraInfo;       // 0x03 = VER | NEWSTAREC
};
```

### STA_REC TLV tag 枚举 (MT6639)
| Tag | 名称 | 我们 | MT6639 | Windows |
|-----|------|------|--------|---------|
| 0x00 | BASIC | ✅ | ✅ | ✅ |
| 0x01 | RA | ✅ | ✅ | ? |
| 0x07 | STATE_CHANGED | ✅ | ✅ | ? |
| 0x09 | HT_BASIC | ❌ | ✅ | ✅ |
| 0x0A | VHT_BASIC | ❌ | ✅ | ✅ |
| 0x0D | WTBL | ❌ | ❌ | ❌ |
| 0x15 | PHY_INFO | ✅ | ✅ | ? |
| 0x16 | BA_OFFLOAD | ❌ | ✅ | ? |
| 0x17 | HE_6G_CAP | ❌ | ✅ | ✅ |
| 0x19 | HE_BASIC | ❌ | ✅ | ✅ |
| 0x20 | MLD_SETUP | ❌ | ❌ | ✅ |
| 0x21 | EHT_MLD | ❌ | ❌ | ✅ |
| 0x24 | UAPSD | ❌ | ✅ | ? |
| 0x2B | HDR_TRANS | ✅ | ❌ | ? |
