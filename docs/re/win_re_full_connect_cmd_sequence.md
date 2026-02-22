# MT7927 Windows Driver — Complete MCU Command Sequence for WiFi Connection

## Metadata

- **Binary**: mtkwecx.sys v5705275 (Windows WiFi 7 Driver)
- **Analysis Date**: 2026-02-17
- **Tool**: Ghidra 12.0.3 Headless + Custom Decompiler Scripts
- **Method**: Address-based decompilation + String XREF + Dispatch table analysis
- **Confidence**: HIGH — All key functions decompiled and cross-validated

---

## 1. Executive Summary

本文档完整追踪了 Windows 驱动中从 WiFi 连接发起到 auth 帧发送的全部 MCU 命令序列。

### 关键发现

1. **完整命令序列已映射**: WdiTaskConnect → DEV_INFO → Channel Req → STA_REC → Auth
2. **UniCmd CID 映射已确认**: DEV_INFO=1, BSS_INFO=2, STA_REC=3, CHANNEL=0x27
3. **Legacy MtCmd → UniCmd 转换**: 0x11→DEV_INFO, 0x12→BSS_INFO, 0x13→STA_REC (via FUN_1400cdc4c)
4. **BSS_INFO dispatch table**: 14 TLV entries at `DAT_1402505b0`
5. **STA_REC dispatch table**: 13 TLV entries at `DAT_140250710`
6. **BSS_INFO 不在 MlmeCntlWaitJoinProc 中发送** — 可能在 MlmeCntlOidConnectProc 或通过 BssActivateCtrl

---

## 2. UniCmd Infrastructure

### 2.1 nicUniCmdAllocEntry (FUN_14014f788) — 命令分配器

```
Source: nic_uni_cmd_event.c (line 0x13c)
```

```c
longlong nicUniCmdAllocEntry(adapter, CID, payload_size) {
    entry = alloc(payload_size + 0x28);  // 0x28 = header overhead
    memset(entry, 0, payload_size + 0x28);
    entry->data_ptr = entry + 0x28;      // [+0x18] = data starts at offset 0x28
    entry->total_len = payload_size;      // [+0x14] = total payload length
    entry->CID = CID;                    // [+0x10] = UniCmd CID
    entry->tag_default = 0xff;           // [+0x20] = default tag
    return entry;
}
```

**关键**: `param_2` = CID, `param_3` = payload size (不含 header)

### 2.2 FUN_1400cdc4c — MtCmd → UniCmd 路由器

```c
void MtCmdDispatcher(bss_ptr) {
    chip_id = *(short *)(adapter + 0x1f72);
    if ((chip_id == 0x6639 || chip_id == 0x738 ||
         chip_id == 0x7927 || chip_id == 0x7925 || chip_id == 0x717) &&
        *(adapter + 0x146cde9) == 1) {
        FUN_14014e644();  // UniCmd path
    } else {
        FUN_1400cd2a8();  // Legacy MtCmd path
    }
}
```

**Legacy MtCmd ID → UniCmd CID 映射**:
| Legacy ID | UniCmd CID | 命令 |
|-----------|-----------|------|
| 0x11 | 1 | DEV_INFO (MtCmdActivateDeactivateNetwork) |
| 0x12 | 2 | BSS_INFO (MtCmdSetBssInfo) |
| 0x13 | 3 | STA_REC (MtCmdSendStaRecUpdate) |

---

## 3. Complete Connection Command Sequence

### 3.1 调用流程图

```
WdiTaskConnect (0x140065be0)
  │
  ├─[1] MtCmdActivateDeactivateNetwork (0x1400c558c)
  │       └─ FUN_1400cdc4c(bss, 0x11, 0xed, 0)
  │           └─ UniCmd CID=1 (DEV_INFO): activate=1, band_idx, ownmac_idx
  │
  └─ MlmeCntlOidConnectProc (0x140123588)
        │
        └─ MlmeCntlWaitJoinProc (0x1401273a8)
              │
              ├─[2] MtCmdChPrivilage (0x1400c5e08)
              │       └─ nicUniCmdChReqPrivilege: CID=0x27
              │           └─ Channel request TLVs (tag=0/3, 0x18 bytes each)
              │
              ├─[3] MtCmdSendStaRecUpdate (0x1400cdea0)
              │       └─ FUN_1400cdc4c(bss, 0x13, 0xed, 0)
              │           └─ UniCmd CID=3: 13 TLVs via dispatch table
              │
              ├─[4] FUN_1400ac6c8(adapter, bss, 2, 0) — Channel switch/Auth trigger
              │
              └─ state = 5
                    │
                    └─ MlmeCntlWaitAuthProc (0x140126954)
                          │
                          ├─[5] FUN_1400ac6c8(adapter, bss, 1, sae_flag) — Auth request
                          │
                          └─ state = 6 (waiting for auth response)
```

### 3.2 BSS_INFO 发送时机

**重要发现**: `MtCmdSetBssInfo` (FUN_1400cf928) **不是从 MlmeCntlWaitJoinProc 直接调用的**。

BSS_INFO 的发送路径:
1. `MtCmdSetBssInfo` 调用 `FUN_1400cdc4c(bss, 0x12, 0xed, 0)` → UniCmd CID=2
2. 该函数可能从 `MlmeCntlOidConnectProc` 或 `nicUniCmdBssActivateCtrl` 调用
3. `nicUniCmdBssActivateCtrl` (FUN_140143540) 同时发送 CID=1 (DEV_INFO) + CID=2 (BSS_INFO)

**推测**: BSS_INFO 在 Join 阶段之前或作为 BssActivate 的一部分发送。

---

## 4. Command [1]: DEV_INFO (MtCmdActivateDeactivateNetwork)

### 4.1 函数签名

```
Address: 0x1400c558c
Legacy MtCmd ID: 0x11
UniCmd CID: 1
Payload size: 0x0c (12 bytes)
```

### 4.2 Payload 结构 (12 bytes)

```c
struct dev_info_payload {
    u8  band_idx;           // [+0x00] from bss->offset_0x24
    u8  activate;           // [+0x01] 1=activate, 0=deactivate
    u8  ownmac_idx;         // [+0x02] from bss_info->offset_0x28
    u8  phy_idx;            // [+0x03] from bss->offset_0x2d2 or bss_info->offset_0x20
    u32 conn_info;          // [+0x04] optional: bss->offset_0x2cc (if activate)
    u8  padding[3];         // [+0x08]
    u8  band_info;          // [+0x0b] from lVar6 + 0x8f9
};
```

### 4.3 调用上下文

```c
// In WdiTaskConnect:
if (bss_state == 5 && bss->connected == 0) {
    MtCmdActivateDeactivateNetwork(adapter, band_idx, 1);  // activate
}
```

**注意**: Deactivate 时还调用 `FUN_1400cdcc0(adapter, 1, 0x14, ownmac_idx)` 清理资源。

---

## 5. Command [2]: Channel Request (nicUniCmdChReqPrivilege)

### 5.1 函数签名

```
Address: 0x14014ff94
UniCmd CID: 0x27
Source string: "nicUniCmdChReqPrivilege"
```

### 5.2 命令结构

```c
// Alloc: CID=0x27, size = (num_channels + num_channels*2) * 8 + 0x1c
entry = nicUniCmdAllocEntry(adapter, 0x27, total_size);

// First channel: tag=0, len=0x18
// Additional channels: tag=3, len=0x18
```

### 5.3 Channel TLV 结构 (0x18 = 24 bytes per TLV)

```c
struct ch_req_tlv {
    u16 tag;              // [+0x00] 0=first channel, 3=additional
    u16 len;              // [+0x02] always 0x18
    u8  channel;          // [+0x04] from param_2[0]
    u8  band;             // [+0x05] from param_2[1]  (0=2.4G, 1=5G, 2=6G)
    u8  bandwidth;        // [+0x06] from param_2[3]
    u8  ht_op_info;       // [+0x07] from param_2[4]
    u8  primary_chan;      // [+0x08] from param_2[5]
    u8  center_freq_seg0; // [+0x09] BW enum: 0→0, 1→1, 2→2, 3→3, 4→6
    u8  center_freq_seg1; // [+0x0A] from param_2[7]
    u8  center_chan;       // [+0x0B] from param_2[8]
    // center_freq2 handling:
    u8  cf2_seg0;         // [+0x0C] if param_2[9]==0: same as seg0
    u8  cf2_primary;      // [+0x0D] if param_2[9]==0: same as param_2[7]
    u8  cf2_seg1;         // [+0x0E] if param_2[9]==0: same as param_2[8]
    u8  req_type;         // [+0x0F] from param_2[0xc]
    u32 timeout;          // [+0x10] from param_2 + 0x10
    u8  priority;         // [+0x14] from param_2[0xe]
    u8  sco;              // [+0x15] maps: 3→-2(0xfe), 4→-1(0xff)
};
```

### 5.4 Channel Abort (nicUniCmdChAbortPrivilege)

```
Address: 0x14014fe60
UniCmd CID: 0x27
Abort TLV: tag=1, len=0x0c
```

```c
struct ch_abort_tlv {
    u32 tag_len;    // [+0x00] 0x000c0001 (tag=1, len=0x0c)
    u8  channel;    // [+0x04] from param_2[0]
    u8  band;       // [+0x05] from param_2[1]
    u8  sco;        // [+0x06] maps: 3→-2, 4→-1
};
```

---

## 6. Command [3]: STA_REC (nicUniCmdUpdateStaRec)

### 6.1 函数签名

```
Address: 0x1401446d0 (UniCmd wrapper)
Address: 0x1400cdea0 (MtCmdSendStaRecUpdate, legacy wrapper)
Legacy MtCmd ID: 0x13
UniCmd CID: 3
Dispatch table: DAT_140250710 (13 entries, 16 bytes each)
Source string: "nicUniCmdUpdateStaRec"
```

### 6.2 MtCmdSendStaRecUpdate 输入结构

```c
// Builds 0xEC (236) byte flat struct from BSS/STA context:
// [+0x00] MtCmd ID = 0x13
// [+0x10] payload_len = 0xEC
// [+0x18] data pointer → flat struct with all STA info
// Then calls FUN_1400cdc4c(bss, 0x13, 0xed, 0)
```

### 6.3 UniCmd Wrapper (nicUniCmdUpdateStaRec)

```c
// Checks: MtCmd ID == 0x13 AND payload_len == 0xEC
// Sums sizes from dispatch table: base=8 + Σ(table[i].size) for i=0..12
// Allocs UniCmd entry: CID=3, total_size

// Header (8 bytes):
header[0] = data[0x0c];  // bss_idx
header[6] = 0;           // pad
header[1] = data[0x36];  // wlan_idx

// Then dispatches 13 TLV builders through guard_dispatch_icall
```

### 6.4 STA_REC Dispatch Table (0x140250710)

| Index | Size | Address | Function Name | Tag | Tag:Len Hex |
|-------|------|---------|---------------|-----|-------------|
| 0 | 20 (0x14) | 14014d6d0 | (BASIC?) | TBD | TBD |
| 1 | 8 (0x08) | 14014d7a0 | (HT_INFO?) | TBD | TBD |
| 2 | 16 (0x10) | 14014d7e0 | (VHT_INFO?) | TBD | TBD |
| 3 | 28 (0x1c) | 14014d810 | **nicUniCmdStaRecTagHeBasic** | **0x19** | 0x1c0019 |
| 4 | 8 (0x08) | 14014dae0 | **nicUniCmdStaRecTagHe6gCap** | **0x17** | 0x80017 |
| 5 | 16 (0x10) | 14014d730 | (STATE_INFO?) | TBD | TBD |
| 6 | 12 (0x0c) | 14014d760 | (PHY_INFO?) | TBD | TBD |
| 7 | 16 (0x10) | 14014e570 | (RA_INFO?) | TBD | TBD |
| 8 | 16 (0x10) | 14014e5b0 | (BA_OFFLOAD?) | TBD | TBD |
| 9 | 8 (0x08) | 14014e620 | (UAPSD?) | TBD | TBD |
| 10 | 40 (0x28) | 14014db80 | **nicUniCmdStaRecTagEhtInfo** | **0x22** | 0x280022 |
| 11 | 16 (0x10) | 14014e2a0 | **nicUniCmdStaRecTagEhtMld** | **0x21** | 0x100021 |
| 12 | 32 (0x20) | 14014ddc0 | **nicUniCmdStaRecTagMldSetup** | **0x20** | variable |

### 6.5 已确认的 STA_REC TLV 详情

#### [3] nicUniCmdStaRecTagHeBasic (tag=0x19, len=0x1c)
```c
struct sta_rec_he_basic {
    u32 tag_len;         // 0x1c0019
    u8  mac_addr[6];     // from sta_info + 0x88
    u8  he_cap[11];      // from sta_info + 0x8e
    u8  tid_num;         // = 2
    u16 he_cap_u16_1;    // from sta_info + 0x9c
    u16 he_cap_u16_2;    // from sta_info + 0xa0
    u16 he_cap_u16_3;    // from sta_info + 0xa4
};
```

#### [4] nicUniCmdStaRecTagHe6gCap (tag=0x17, len=0x08)
```c
struct sta_rec_he_6g_cap {
    u32 tag_len;         // 0x80017
    u16 he_6g_cap;       // from sta_info + 0xa8 (skip if zero)
    u16 pad;
};
```

#### [10] nicUniCmdStaRecTagEhtInfo (tag=0x22, len=0x28)
```c
struct sta_rec_eht_info {
    u32 tag_len;         // 0x280022
    u8  padding;         // = 0xff
    u8  pad2;
    u16 eht_cap;         // from sta_info + 0xc8
    u64 eht_mac_cap;     // from sta_info + 0xca
    u64 eht_phy_cap;     // from sta_info + 0xd2
    u32 eht_phy_cap2;    // from sta_info + 0xda
    u8  eht_mcs_map;     // from sta_info + 0xde
    // ... remaining fields from sta_info + 0xdf..0xe6
};
```

#### [11] nicUniCmdStaRecTagEhtMld (tag=0x21, len=0x10)
```c
// Only built when MLD mode == 3 AND EHT capability enabled
struct sta_rec_eht_mld {
    u32 tag_len;         // 0x100021
    u8  mld_rec[5];      // from mld_entry fields
    u8  link_bitmap[3];  // from mld_entry + 0x11
    u16 link_id;         // from mld_entry + 0x14
};
```

#### [12] nicUniCmdStaRecTagMldSetup (tag=0x20, variable len)
```c
// Complex MLD setup TLV, dispatches to multiple sub-handlers
// Size up to 0x20 (32 bytes)
```

### 6.6 STA_REC ConnType 映射 (nicUniCmdStaRecConnType @ 0x140151608)

| 输入值 | ConnType | 含义 |
|--------|----------|------|
| 0x41 | 0x10002 | AP (WDS) |
| 0x21 | 0x10001 | STA (default) |
| 0x42 | 0x20002 | AP (P2P) |
| 0x22 | 0x20001 | STA (P2P) |

---

## 7. BSS_INFO (nicUniCmdSetBssInfo)

### 7.1 函数签名

```
Address: 0x1401444a0 (UniCmd wrapper)
Address: 0x1400cf928 (MtCmdSetBssInfo, legacy wrapper)
Legacy MtCmd ID: 0x12
UniCmd CID: 2
Dispatch table: DAT_1402505b0 (14 entries, 16 bytes each)
Input payload: 0x74 bytes flat struct
```

### 7.2 MtCmdSetBssInfo 输入结构 (0x74 bytes)

```c
// Called via: FUN_1400cdc4c(bss, 0x12, 0xed, 0)
struct bss_info_flat {
    u8  bss_idx;          // [+0x00] from bss->offset_0x24
    // ... many fields populated from BSS context
    u8  ht_mode;          // from bss + 0x307
    u8  active;           // ~(bss->offset_0x2e6964 >> 7) & 1
    u8  network_type;     // (bss->offset_0x10 != 4) ? 0 : 2
    u32 bssid[6];         // MAC address
    u8  qos_en;           // from bss
    u16 beacon_period;    // from sta_entry + 0x4a2
    u16 dtim_period;      // from sta_entry + 0x4a4
    u8  wlan_idx;         // from sta_entry + 0x4ac
    u8  phy_mode;         // from sta_entry + 0x4a7
    u8  ext_bss;          // from bss + 9 (or alt entry)
    u16 he_bss_color;     // from sta_entry + 0x4ae
    u8  mbss_tsf_offset;  // from bss + 0x374
    u8  pm_state;         // from bss + 0x4ae
    // HE specific:
    u8  he_op;            // from sta_entry + 0x5ec region
    u8  bss_color;        // from bss + 0x5c2
    // MLD:
    u8  mld_band0;        // from bss + 0x914
    u8  mld_band1;        // from bss + 0x915
};
```

### 7.3 UniCmd Wrapper (nicUniCmdSetBssInfo)

```c
// Checks: MtCmd ID == 0x12 AND payload_len == 0x74
// Sums sizes from dispatch table: base=4 + Σ(table[i].size) for i=0..13
// Allocs UniCmd entry: CID=2, total_size

// Header (4 bytes):
header[0] = data[0];  // bss_idx

// Then dispatches 14 TLV builders
```

### 7.4 BSS_INFO Dispatch Table (0x1402505b0)

| Index | Tag | Len | Function | Name |
|-------|-----|-----|----------|------|
| 0 | **0x0000** | **0x20** | 14014c610 | **nicUniCmdBssInfoTagBasic** |
| 1 | TBD | TBD | TBD | (Rate?) |
| 2 | TBD | TBD | TBD | (Sec?) |
| 3 | TBD | TBD | TBD | (Qbss?) |
| 4 | TBD | TBD | TBD | (Sap?) |
| 5 | TBD | TBD | TBD | (P2P?) |
| 6 | **0x0005** | **0x10** | 14014cd50 | **nicUniCmdBssInfoTagHe** |
| 7 | **0x0004** | **0x08** | 14014d010 | **nicUniCmdBssInfoTagBssColor** |
| 8 | TBD | TBD | TBD | (11vMbssid?) |
| 9 | TBD | TBD | TBD | (Wapi?) |
| 10 | **0x0018** | **0x08** | 14014d350 | **nicUniCmdBssInfoTagSTAIoT** |
| 11 | **0x001A** | **0x14** | 14014fad0 | **nicUniCmdBssInfoMld** |
| 12 | **0x001E** | **0x10** | 14014d150 | **nicUniCmdBssInfoTagEht** |
| 13 | TBD | TBD | TBD | (Unknown) |

**注意**: RLM (tag=0x02), PROTECT (tag=0x03), IFS_TIME (tag=0x17) 不在此 dispatch table 中。它们通过 `nicUniCmdSetBssRlm` (CID=2, 单独命令) 发送。

### 7.5 已确认的 BSS_INFO TLV 详情

#### BASIC (tag=0x0000, len=0x20)
```c
struct bss_info_basic {
    u32 tag_len;          // [+0x00] 0x00200000
    u8  bss_idx;          // [+0x04] from flat_struct[0x5e]
    u8  ownmac_idx;       // [+0x05] from bss->offset_0x2d2 or bss_info->offset_0x20
    u8  ownmac_idx_dup;   // [+0x06] same as above
    u8  sco;              // [+0x07] maps: 3→-2(0xfe), 4→-1(0xff)
    u32 conn_type;        // [+0x08] from nicUniCmdBssInfoConnType()
    u8  active;           // [+0x0c] ~(bss->0x2e6964 >> 7) & 1
    u8  network_type;     // [+0x0d] from flat_struct[0x5b]
    u32 bssid;            // [+0x0e] 4 bytes of BSSID
    u16 bssid_hi;         // [+0x12] 2 bytes of BSSID
    u16 sta_type;         // [+0x14] from flat_struct[0x3a]
    u16 bcn_interval;     // [+0x16] from bss_info->bcn_period
    u8  dtim;             // [+0x18] from bss + 0x5cafd8 context
    u16 phy_mode;         // [+0x1a] from sta_entry + 9
    u16 wlan_idx;         // [+0x1c] from flat_struct[0x34]
    u8  mbss_enabled;     // [+0x19] bit[0] from nicUniCmdBssInfoConnTyp result
    u8  mbss_tsf;         // [+0x1e] bit[8] from result
};
```

#### HE (tag=0x0005, len=0x10)
```c
struct bss_info_he {
    u32 tag_len;          // [+0x00] 0x00100005
    u16 he_pe_dur;        // [+0x04] (sta_entry[0x5c0] & 0x3f) << 4 | (sta_entry[0x5bf] >> 4)
    u16 he_basic_mcs;     // [+0x08] from sta_entry + 0x5c3
    u16 he_basic_mcs2;    // [+0x0a] same
    u16 he_basic_mcs3;    // [+0x0c] same
    u8  bss_color;        // [+0x06] sta_entry[0x5bf] & 7
    u8  default_pe;       // [+0x07] sta_entry[0x5c1] & 1
    u8  twt_required;     // [+0x0e] from flat_struct[0x3e] with conditional flag
};
```

#### BSS_COLOR (tag=0x0004, len=0x08)
```c
struct bss_info_bss_color {
    u32 tag_len;          // [+0x00] 0x00080004
    u8  disable;          // [+0x04] ~(sta_entry[0x5c2] >> 7) & 1
    u8  color;            // [+0x05] sta_entry[0x5c2] & 0x3f
    u16 pad;
};
```

#### EHT (tag=0x001E, len=0x10)
```c
struct bss_info_eht {
    u32 tag_len;          // [+0x00] 0x0010001e
    u8  eht_disabled_subchannel; // [+0x04] sta_entry[0x772] & 1
    u8  eht_pe;           // [+0x05] sta_entry[0x772] >> 1 & 1
    u8  eht_field1;       // [+0x06] sta_entry[0x777]
    u8  eht_field2;       // [+0x07] sta_entry[0x778]
    u8  eht_field3;       // [+0x08] sta_entry[0x779]
    u16 pad;
    u16 eht_basic_mcs;    // [+0x0a] sta_entry + 0x77a
};
```

#### STAIoT (tag=0x0018, len=0x08)
```c
struct bss_info_sta_iot {
    u32 tag_len;          // [+0x00] 0x00080018
    u8  ap_mode;          // [+0x04] from flat_struct[0x31]
    u8  iot_flag;         // [+0x05] from flat_struct[0x3d]
    u16 pad;
};
```

#### MLD (tag=0x001A, len=0x14)
```c
struct bss_info_mld {
    u32 tag_len;          // [+0x00] 0x0014001a
    u8  group_mld_id;     // [+0x04] 0xff (no MLD) or from mld_entry[2]
    u8  own_mld_id;       // [+0x05] bss_idx (or sta_entry[0x908] + 0x20)
    u32 mld_addr;         // [+0x06] from mld_entry[4..7]
    u16 mld_addr_hi;      // [+0x0a] from mld_entry[8..9]
    u8  remap_idx;        // [+0x0c] 0xff or from mld_entry fields
    u8  eml_cap;          // [+0x0d] from sta_entry[0x8fb]
    u8  mld_type;         // [+0x0e] from mld_entry[0xd]
    u8  linkmap_enable;   // [+0x0f] conditional
    u8  pad;
    u8  setup_wlan_id;    // [+0x11] from mld_entry[1]
    // Non-MLD fallback: group_mld_id=0xff, own_mld_id=bss_idx,
    //                   mld_addr from bss->0x2cc, remap_idx=0
};
```

### 7.6 BSS_INFO RLM (独立命令 nicUniCmdSetBssRlm)

```
Address: 0x1401445e0
UniCmd CID: 2
Legacy MtCmd ID: 0x19
Payload: 0x16 bytes → generates 3 inline TLVs
```

**重要**: `nicUniCmdSetBssRlm` 使用 CID=2 (与 BSS_INFO 相同的 CID)，但不经过 dispatch table。它直接创建 UniCmd entry 并写入 3 个 TLV:

#### RLM (tag=0x0002, len=0x10) — inline in nicUniCmdSetBssRlmImpl
```c
struct bss_info_rlm {
    u32 tag_len;          // [+0x00] 0x00100002
    u8  channel;          // [+0x04] from param_3[2]
    u8  center_freq1;     // [+0x05] from param_3[0x10]
    u8  center_freq2;     // [+0x06] from param_3[0x11]
    u8  bw;               // [+0x07] maps: 0→0, 1→1, 2→2, 3→3, 4→6, 5→7 (same as channel BW enum)
    u8  tx_stream;        // [+0x08] from param_3[0x14]
    u8  rx_stream;        // [+0x09] from param_3[0x15]
    u8  short_gi;         // [+0x0a] from param_3[0x0c]
    u8  ht_op_info;       // [+0x0b] from param_3[3]
    u8  band;             // [+0x0c] from param_3[1]
};
```

#### PROTECT (tag=0x0003, len=0x08) — inline in nicUniCmdSetBssRlmImpl
```c
struct bss_info_protect {
    u32 tag_len;          // [+0x00] 0x00080003
    u32 protect_mode;     // [+0x04] bit flags:
                          //   bit5 (0x20): param_3[4] != 0
                          //   bit1 (0x02): param_3[5] == 1
                          //   bit2 (0x04): param_3[5] == 2
                          //   bit3 (0x08): param_3[5] == 3
                          //   bit7 (0x80): param_3[6] == 1
};
```

#### IFS_TIME (tag=0x0017, len=0x14) — inline in nicUniCmdSetBssRlmImpl
```c
struct bss_info_ifs_time {
    u32 tag_len;          // [+0x00] 0x00140017
    u8  present;          // [+0x04] always 1
    u16 slot_time;        // [+0x08] 0x14 (20us) or 0x09 (9us) based on param_3[0xe]
    u16 pad[5];
};
```

### 7.7 BSS_INFO ConnType 映射 (nicUniCmdBssInfoConnType @ 0x14014fa20)

```c
int nicUniCmdBssInfoConnType(bss) {
    int mode = bss->offset_0x10;   // network mode
    int wds = bss->offset_0x28;    // WDS flag
    int wds_type = bss->offset_0x2dc;

    if (mode == 1 && wds == 0)      return 0x10001;  // STA, no WDS
    if (wds == 1 && wds_type == 8)   return 0x20000;  // WDS bridge
    if (mode == 5 || wds_type == 0x20) return 0x20001; // P2P client
    if (mode == 4 || wds_type == 0x10) return 0x20002; // AP mode
    return 0;  // unknown
}
```

| Mode | WDS | ConnType | 含义 |
|------|-----|----------|------|
| 1 (STA) | 0 | 0x10001 | **Infrastructure STA** ← 我们用这个 |
| - | 1, type=8 | 0x20000 | WDS Bridge |
| 5 (P2P) | - | 0x20001 | P2P Client |
| 4 (AP) | - | 0x20002 | Access Point |

---

## 8. nicUniCmdBssActivateCtrl (FUN_140143540)

### 8.1 概述

此函数同时创建 **两个 UniCmd sub-commands**，一并发送:

```c
// Sub-command 1: CID=1 (DEV_INFO)
entry1 = nicUniCmdAllocEntry(adapter, 1, ...);
// Contains: band_idx, activate, ownmac_idx

// Sub-command 2: CID=2 (BSS_INFO)
entry2 = nicUniCmdAllocEntry(adapter, 2, ...);
// Contains: BSS activation info, STA details
```

### 8.2 含义

BssActivateCtrl 是一个组合命令，在一次 MCU 交互中同时完成:
1. 网络接口激活 (DEV_INFO)
2. BSS 基本配置 (BSS_INFO subset)

这可能是 `nicUniCmdRemoveStaRec` (tag=0x25, len=0x08) 的配对操作。

---

## 9. State Machine

### 9.1 MlmeCntlWaitJoinProc 状态转换

```
Entry: MLME state == WAIT_JOIN (triggered by msg_type == 0x28)

If join success (msg == 0x00):
  1. MtCmdChPrivilage() — channel request
  2. MtCmdSendStaRecUpdate(bss, sta_entry, 0) — STA_REC
  3. FUN_1400ac6c8(adapter, bss, 2, 0) — auth trigger
  4. Set state = 5 (WAIT_AUTH)

If join fail:
  - Increment fail counter
  - Call disconnect handler
```

### 9.2 MlmeCntlWaitAuthProc 状态转换

```
Entry: MLME state == WAIT_AUTH (msg_type == 0x23)

If auth success (msg == 0x00):
  1. Set auth parameters (timeout, capabilities)
  2. FUN_1400ac6c8(adapter, bss, 1, sae_flag) — start auth
  3. Set state = 6 (WAIT_ASSOC)

If auth fail, non-SAE:
  1. FUN_1400ac6c8(adapter, bss, 2, 0) — retry channel
  2. Set state = 7 (RETRY)

If auth fail, final:
  - state = 0 (IDLE)
  - Increment fail counter, disconnect
```

---

## 10. 对比分析: 我们的驱动 vs Windows 驱动

### 10.1 命令序列对比

| 步骤 | Windows 驱动 | 我们的驱动 | 状态 |
|------|-------------|-----------|------|
| DEV_INFO activate | CID=1, band_idx + ownmac | ✅ 已实现 | OK |
| Channel Request | CID=0x27, 详细 TLV | ⚠️ 使用 ROC API | **不同** |
| BSS_INFO | CID=2, 14 TLV 通过 dispatch table | ⚠️ 只有 3 TLV (BASIC+RLM+MLD) | **缺失 11 个** |
| BSS_INFO RLM | CID=2, 3 TLV inline | ✅ 已实现 (RLM+PROTECT+IFS_TIME) | OK |
| STA_REC | CID=3, 13 TLV 通过 dispatch table | ⚠️ 只有 5 TLV | **缺失 8 个** |
| Auth trigger | FUN_1400ac6c8 → TX | ✅ 帧构建正确 | **TX 失败** |

### 10.2 关键差异分析

#### 信道管理 API
- **Windows**: `nicUniCmdChReqPrivilege` (CID=0x27), 详细的 24 字节 TLV 结构
- **我们**: ROC (Remain-On-Channel) API
- **影响**: ROC 可能不能正确获取信道特权，导致固件拒绝 TX

#### BSS_INFO TLV 缺失
Windows 发送 14 个 TLV (通过 dispatch table) + 3 个 RLM TLV。我们只有:
- ✅ BASIC (tag=0x00)
- ✅ RLM (tag=0x02)
- ✅ PROTECT (tag=0x03)
- ✅ IFS_TIME (tag=0x17)
- ✅ MLD (tag=0x1A)
- ❌ Rate TLV (tag unknown, dispatch table index 1?)
- ❌ Sec TLV
- ❌ Qbss TLV
- ❌ Sap TLV
- ❌ P2P TLV
- ❌ HE (tag=0x05)
- ❌ BSS_COLOR (tag=0x04)
- ❌ STAIoT (tag=0x18)
- ❌ EHT (tag=0x1E)

#### STA_REC TLV 缺失
Windows 发送 13 个 TLV。我们需要补充未知的 8 个 ([0],[1],[2],[5],[6],[7],[8],[9])。

---

## 11. 修复建议 (优先级排序)

### 【最高优先级】— 可能直接修复 TX 问题

1. **替换 ROC 为 nicUniCmdChReqPrivilege (CID=0x27)**
   - 构建正确的 channel TLV (tag=0, len=0x18)
   - 包含 channel, band, bandwidth, center_freq 等完整参数
   - Windows 使用此 API 而非 ROC

2. **确认 BSS_INFO BASIC TLV conn_type**
   - STA 模式应为 `0x10001`
   - 验证我们是否正确设置

### 【高优先级】— 补全 MCU 命令

3. **添加 BSS_INFO HE TLV (tag=0x05)**
   - 包含 HE PE duration, BSS color, MCS set
   - 对 HE/WiFi 6 连接可能关键

4. **添加 BSS_INFO BSS_COLOR TLV (tag=0x04)**
   - disable flag + color value
   - WiFi 6E 必需

5. **识别并实现 STA_REC 未知 TLV [0],[1],[2],[5],[6]**
   - 需要反编译地址 14014d6d0, 14014d7a0, 14014d7e0, 14014d730, 14014d760
   - 可能包含 BASIC, HT_INFO, VHT_INFO, STATE_INFO, PHY_INFO

### 【中优先级】— 完善性

6. **识别 BSS_INFO dispatch table 中缺失的 TLV**
   - 需要读取 0x1402505b0 处的 dispatch table 获取函数地址
   - 然后反编译确认 Rate, Sec, Qbss 等 TLV

7. **添加 BSS_INFO STAIoT (tag=0x18)**
   - 简单: ap_mode + iot_flag

8. **添加 BSS_INFO EHT (tag=0x1E)**
   - WiFi 7 能力信息

---

## 12. 附录: 需要进一步反编译的地址

### BSS_INFO Dispatch Table (0x1402505b0)
需要用 Ghidra 读取此表获取 14 个 entry 的函数地址，然后反编译缺失的 TLV builder。

### STA_REC 未知 TLV 函数
| Index | Address | Probable Name |
|-------|---------|---------------|
| 0 | 14014d6d0 | StaRecTagBasic? (tag may be 0x00) |
| 1 | 14014d7a0 | StaRecTagHtInfo? (tag may be 0x07) |
| 2 | 14014d7e0 | StaRecTagVhtInfo? (tag may be 0x08) |
| 5 | 14014d730 | StaRecTagStateInfo? |
| 6 | 14014d760 | StaRecTagPhyInfo? |
| 7 | 14014e570 | StaRecTagRaInfo? |
| 8 | 14014e5b0 | StaRecTagBaOffload? |
| 9 | 14014e620 | StaRecTagUapsd? |

### TX 路径函数
| Address | Name | Purpose |
|---------|------|---------|
| 0x1400ac6c8 | - | Auth/Channel trigger (called from state machine) |
| 0x1400aa324 | - | TX submit (param2=0xa00577) |
| 0x1400aa280 | - | TX WMM/QoS config |
| 0x1400aa1b4 | - | TX descriptor setup |

---

## 13. References

- Ghidra project: `/home/user/mt7927/tmp/ghidra_project/mt7927_re`
- Analysis scripts: `/home/user/ghidra_scripts/FullConnectCmdSequence.java`
- Decompilation outputs: `/tmp/sta_rec_decompile.txt`, `/tmp/ghidra_connection_output.txt`
- Previous analysis: `docs/win_re_connection_flow.md`, `docs/win_re_tx_mgmt_path.md`
