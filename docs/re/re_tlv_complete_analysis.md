# MT7927 Windows RE: BSS_INFO / STA_REC / BssActivateCtrl 完整 TLV 分析

> **来源**: Codex Agent A (Ghidra 反编译) + Agent B (汇编级) 自动化 RE 分析
> **目标**: 字节级布局 → 驱动差距分析 → 修复方案
> **日期**: 2026-02-22 (Session 25)
> **Windows 驱动**: mtkwecx.sys v5705275

---

## 第一章: BSS_INFO TLV 完整布局

Windows `nicUniCmdSetBssInfo` (0x1401444a0) 发送 **14 个 TLV** (由 14 次间接调用填充)。

BSS_INFO 命令头:
```
offset 0x00: bss_idx      (1B) — BSS 索引
offset 0x01-0x03: reserved (3B) — 未写入，可能是对齐
offset 0x04+: TLV 数组
```

### 1.1 BSS_BASIC (tag=0x0000, len=0x0020, 32B)

函数: `nicUniCmdBssInfoTagBasic` (0x14014c610)

| Offset | Size | 名称 | 值/来源 | 备注 |
|--------|------|------|---------|------|
| 0x00 | 2 | tag | 0x0000 | BSS_BASIC |
| 0x02 | 2 | len | 0x0020 | 32 bytes |
| 0x04 | 1 | active/bss_idx | param3[0x5e] | BSS 索引 |
| 0x05 | 1 | omac_idx | adapter→0x2d2 或 link→0x20 | OWN_MAC 索引 |
| 0x06 | 1 | hw_bss_idx | = omac_idx (dup) | 固件读为 omac |
| 0x07 | 1 | band_idx | param3[0x5a] → 映射 (0x04→0xFF, 0x03→0xFE) | 频段索引 |
| 0x08 | 4 | conn_type | FUN_14014fa20() 返回值 | 连接类型 (如 0x00080015) |
| 0x0C | 1 | active_flag | ~(bss→0x2e6964 >> 7) & 1 | **关键**: 1=激活 |
| 0x0D | 1 | wmm_idx/network_type | param3[0x5b] | WMM 索引 |
| 0x0E | 4 | bssid_part | param3+0x24 (dword) | BSSID 部分 |
| 0x12 | 2 | bcn_interval | param3+0x28 (u16) | Beacon 间隔 |
| 0x14 | 2 | sta_type | param3[0x3a] (u8→u16) | STA 类型 |
| 0x16 | 2 | dtim/timer | param1[0xb8612] → 可被覆盖为 0x64 | DTIM/定时器 |
| 0x18 | 1 | phy_mode | param1[0xb862d] → 可被覆盖 | PHY 模式 |
| 0x19 | 1 | uVar9_low | FUN_14014fdfc() 低字节 | BW info |
| 0x1A | 2 | wlan_idx | link→0x09 (u8→u16) | **WLAN index for TX** |
| 0x1C | 2 | nonht_basic_phy | param3[0x34] (u8→u16) | 基本 PHY |
| 0x1E | 1 | uVar9_high | FUN_14014fdfc() >> 8 | BW info 高字节 |
| 0x1F | 1 | link_idx/band_info | (隐含 — 32B 结束) | 频段信息 |

**Agent A/B 一致性**: 两者均确认 tag=0x0000, len=0x0020, 字段偏移匹配。

### 1.2 BSS_RLM + BSS_PROTECT + BSS_IFS_TIME (组合 builder)

函数: `RLM_PROTECT_IFS_builder` (0x14014cc80) → 调用 `IFS_TIME_builder` (0x140150edc)

IFS_TIME_builder 实际构建 **3 个子 TLV**:

#### 1.2.1 RLM TLV (tag=0x0002, len=0x0010, 16B)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0002 |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 1 | control_channel | param3[2] |
| 0x05 | 1 | center_chan | param3[0x10] |
| 0x06 | 1 | center_chan2 | param3[0x11] |
| 0x07 | 1 | bw_mapped | 映射表: 0→0x01, 1→0x02, 2→0x03, 3→0x06, 4→0x07 |
| 0x08 | 1 | tx_streams | param3[0x14] |
| 0x09 | 1 | rx_streams | param3[0x15] |
| 0x0A | 1 | ht_op_info | param3[0xc] |
| 0x0B | 1 | sco | param3[3] |
| 0x0C | 1 | band | param3[1] |
| 0x0D-0x0F | 3 | pad | 0 |

#### 1.2.2 PROTECT TLV (tag=0x0003, len=0x0008, 8B)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0003 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 4 | protect_mode | 条件 OR: ERP→0x20, 长slot→0x02/0x04/0x08, OBSS→0x80 |

#### 1.2.3 IFS_TIME TLV (tag=0x0017, len=0x0014, 20B)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0017 |
| 0x02 | 2 | len | 0x0014 |
| 0x04 | 1 | slot_valid | 0x01 (固定) |
| 0x05-0x07 | 3 | reserved | 0 |
| 0x08 | 2 | slot_time | 2.4GHz: 0x0014(20μs), 5GHz: 0x0009(9μs) |
| 0x0A-0x13 | 10 | reserved | 0 (sifs/rifs/eifs 未设) |

**注意**: RLM+PROTECT+IFS_TIME 三个 TLV 由同一个 builder (0x140150edc) 构建，总返回长度 44B (16+8+20)。

### 1.3 BSS_RATE (tag=0x000B, len=0x0010, 16B)

函数: `FUN_14014cc90` (0x14014cc90)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x000B |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 2 | operational_rate | param3+0x2c (u16) — 所有支持速率 bitmap |
| 0x06 | 2 | basic_rate | param3+0x2e (u16) — 基本速率 bitmap |
| 0x08-0x0F | 8 | reserved | 未显式写入 (bc/mc_trans 等) |

**Agent A/B 一致**: 常量 0x0010000B, 两个 u16 字段确认。

### 1.4 BSS_COLOR (tag=0x0004, len=0x0008, 8B)

函数: `FUN_14014d010` (0x14014d010)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0004 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | enable | ~(link→0x5c2 >> 7) & 1 |
| 0x05 | 1 | bss_color | link→0x5c2 & 0x3F |
| 0x06-0x07 | 2 | pad | 0 |

### 1.5 BSS_HE (tag=0x0005, len=0x0010, 16B)

函数: `FUN_14014cd50` (0x14014cd50)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0005 |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 2 | txop_rts_threshold | (link→0x5c0 & 0x3F) << 4 | (link→0x5bf >> 4) |
| 0x06 | 1 | default_pe_duration | link→0x5bf & 0x07 |
| 0x07 | 1 | er_su_disable | link→0x5c1 & 0x01 |
| 0x08 | 2 | max_nss_mcs[0] | link→0x5c3 (u16) |
| 0x0A | 2 | max_nss_mcs[1] | link→0x5c3 (u16) — 重复 |
| 0x0C | 2 | max_nss_mcs[2] | link→0x5c3 (u16) — 重复 |
| 0x0E | 1 | he_bss_flags | param3[0x3e] (可能 OR 0x02/0x01) |
| 0x0F | 1 | pad | 0 |

### 1.6 BSS_MBSSID (tag=0x0006, len=0x0008, 8B)

函数: `FUN_14014d300` (0x14014d300)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0006 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | max_bssid_indicator | param3+0x66 |
| 0x05 | 1 | mbssid_index | param3+0x67 |
| 0x06-0x07 | 2 | pad | 0 |

### 1.7 BSS_UNKNOWN_0C (tag=0x000C, len=0x0008, 8B)

函数: `FUN_14014d320` (0x14014d320)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x000C |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | flag | param3+0x38 |
| 0x05-0x07 | 3 | pad | 0 |

### 1.8 BSS_SAP (tag=0x000D, len=0x0028, 40B)

函数: `FUN_14014ccf0` (0x14014ccf0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x000D |
| 0x02 | 2 | len | 0x0028 |
| 0x04 | 1 | num_sta | param3+0x3b |
| 0x05-0x06 | 2 | pad | 0 |
| 0x07 | 1 | rate_count | param3[3] (长度) |
| 0x08 | N | rate_data | memcpy(param3+4, len=param3[3]) |

### 1.9 BSS_P2P (tag=0x000E, len=0x0008, 8B)

函数: `FUN_14014cd30` (0x14014cd30)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x000E |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 4 | private_data | param3+0x40 (dword) |

### 1.10 BSS_QBSS (tag=0x000F, len=0x0008, 8B)

函数: `FUN_14014ccd0` (0x14014ccd0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x000F |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | is_qbss | param3+0x2a |
| 0x05-0x07 | 3 | pad | 0 |

### 1.11 BSS_SEC (tag=0x0010, len=0x0008, 8B)

函数: `FUN_14014ccb0` (0x14014ccb0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0010 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | auth_mode | param3+0x35 |
| 0x05 | 1 | enc_status | param3+0x36 |
| 0x06-0x07 | 2 | pad | 0 |

### 1.12 BSS_IOT (tag=0x0018, len=0x0008, 8B)

函数: `FUN_14014d350` (0x14014d350)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0018 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | iot_ap_bmp | param3+0x31 |
| 0x05 | 1 | iot_flag | param3+0x3d |
| 0x06-0x07 | 2 | pad | 0 |

### 1.13 BSS_MLD (tag=0x001A, len=0x0014, 20B)

函数: `nicUniCmdBssInfoMld` (0x14014fad0)

| Offset | Size | 名称 | 值/来源 | 备注 |
|--------|------|------|---------|------|
| 0x00 | 2 | tag | 0x001A | |
| 0x02 | 2 | len | 0x0014 | 20 bytes |
| 0x04 | 1 | link_id | 0xFF (非 MLD) 或 link_info[2] | |
| 0x05 | 1 | group_mld_id | adapter→0x24 或 sta_rec[0x908]+0x20 | 非 MLD: bss_idx |
| 0x06 | 4 | own_mld_addr[0:3] | adapter→0x2CC 或 link_info+4 | MAC 地址低 4B |
| 0x0A | 2 | own_mld_addr[4:5] | adapter[0x5A] 或 link_info+8 | MAC 地址高 2B |
| 0x0C | 1 | band_idx | 0xFF (非 MLD) 或 link_info[3] | |
| 0x0D | 1 | omac_idx | 0xFF (非 MLD) 或 sta_rec[0x8fb] | |
| 0x0E | 1 | remap_idx | 0x00 (非 MLD 清零) 或 link_info[0xd] | |
| 0x0F | 1 | eml_mode | 0x00 或 0x01 (条件) | |
| 0x10 | 1 | str_opmode | 0x00 (非 MLD) 或 link_info[0x10] | |
| 0x11 | 1 | link_opmode | link_info[1] (MLD only) | |
| 0x12-0x13 | 2 | pad | 0 | |

### 1.14 BSS_EHT (tag=0x001E, len=0x0010, 16B)

函数: `FUN_14014d150` (0x14014d150)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x001E |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 1 | eht_flag_bit0 | link→0x772 & 0x01 |
| 0x05 | 1 | eht_flag_bit1 | (link→0x772 >> 1) & 0x01 |
| 0x06 | 1 | eht_field_1 | link→0x777 |
| 0x07 | 1 | eht_field_2 | link→0x778 |
| 0x08 | 1 | eht_field_3 | link→0x779 |
| 0x09 | 1 | pad | 0 |
| 0x0A | 2 | eht_mcs | link→0x77a (u16) |
| 0x0C-0x0F | 4 | pad | 0 |

### BSS_INFO TLV 总大小 (Windows)

```
BASIC(32) + RLM(16) + PROTECT(8) + IFS_TIME(20) + RATE(16)
+ COLOR(8) + HE(16) + MBSSID(8) + 0C(8) + SAP(40) + P2P(8)
+ QBSS(8) + SEC(8) + IOT(8) + MLD(20) + EHT(16)
= 命令头(4) + 232 bytes TLV = ~236 bytes total
```

---

## 第二章: STA_REC TLV 完整布局

Windows `nicUniCmdUpdateStaRec` (0x1401446d0) 发送 **13 个 TLV**。

STA_REC 命令头 (8B):
```
offset 0x00: bss_idx       (1B) — from link+0x0c
offset 0x01: wlan_idx      (1B) — from link+0x36
offset 0x02-0x05: reserved (4B)
offset 0x06: flag          (1B) — 0x00
offset 0x07: reserved      (1B)
```

### 2.1 STA_BASIC (tag=0x0000, len=0x0014, 20B)

函数: `FUN_14014d6d0` (0x14014d6d0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0000 |
| 0x02 | 2 | len | 0x0014 |
| 0x04 | 4 | conn_type | FUN_140151608(adapter, param3[1]) — 映射 |
| 0x08 | 1 | conn_state | 0x01 (固定) |
| 0x09 | 1 | qos | param3+0x12 |
| 0x0A | 2 | aid | param3+0x08 (u16) |
| 0x0C | 4 | peer_addr[0:3] | param3+0x02 (dword) |
| 0x10 | 2 | peer_addr[4:5] | param3+0x06 (u16) |
| 0x12 | 2 | extra_info | 0x0003 (固定 = VER \| NEW) |

**重要发现**: Windows STA_BASIC 只有 **20 字节** (不是 mt7925 的 20B)，`extra_info` 固定为 0x0003。

### 2.2 STA_REC_RA (tag=0x0001, len=0x0010, 16B)

函数: `FUN_14014e570` (0x14014e570)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0001 |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 2 | legacy_rate | param3+0x0e (u16) |
| 0x06 | 10 | rx_mcs_bitmask | memcpy(param3+0x18, 10B) |

### 2.3 STA_REC_STATE (tag=0x0007, len=0x0010, 16B)

函数: `FUN_14014d730` (0x14014d730)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0007 |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 1 | state | param3+0x14 |
| 0x05-0x07 | 3 | pad | 0 |
| 0x08 | 4 | flags | param3+0x58 (dword) |
| 0x0C | 1 | action | param3+0x45 |
| 0x0D | 1 | pad | 0x00 (显式清零) |
| 0x0E-0x0F | 2 | pad | 0 |

### 2.4 STA_REC_HT (tag=0x0009, len=0x0008, 8B)

函数: `HT_INFO_builder` (0x14014d7a0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0009 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 2 | ht_cap | param3+0x28 (u16) |
| 0x06 | 2 | ht_ext_cap | param3+0x2a (u16) |

**条件**: 仅当 ht_cap 和 ht_ext_cap 不全为 0 时构建。

### 2.5 STA_REC_VHT (tag=0x000A, len=0x0010, 16B)

函数: `VHT_INFO_builder` (0x14014d7e0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x000A |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 4 | vht_cap | param3+0x38 (dword) |
| 0x08 | 2 | rx_mcs_map | param3+0x3c (u16) |
| 0x0A | 2 | tx_mcs_map | param3+0x40 (u16) |
| 0x0C-0x0F | 4 | pad | 0 |

**条件**: 仅当 vht_cap != 0 时构建。

### 2.6 STA_REC_PHY (tag=0x0015, len=0x000C, 12B)

函数: `PHY_INFO_builder` (0x14014d760)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0015 |
| 0x02 | 2 | len | 0x000C |
| 0x04 | 2 | basic_rate | param3+0x10 (u16) |
| 0x06 | 1 | phy_type | param3+0x0d |
| 0x07 | 1 | ampdu | param3+0x30 |
| 0x08 | 1 | rts_policy | param3+0x44 |
| 0x09 | 1 | rcpi | param3+0x32 |
| 0x0A-0x0B | 2 | pad | 0 |

### 2.7 STA_REC_BA_OFFLOAD (tag=0x0016, len=0x0010, 16B)

函数: `BA_OFFLOAD_builder` (0x14014e5b0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0016 |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 1 | tx_ba_wsize | param3+0x4c |
| 0x05 | 1 | rx_ba_wsize | param3+0x4d |
| 0x06 | 1 | ba_policy | param3+0x80 |
| 0x07 | 1 | ba_control | param3+0x81 |
| 0x08 | 4 | ba_bitmap | param3+0x84 (dword) |
| 0x0C | 2 | tx_ba_wsize_ext | param3+0x5c (条件宽度) |
| 0x0E | 2 | rx_ba_wsize_ext | param3+0x5d/0x5e (条件) |

### 2.8 STA_REC_HE_6G_CAP (tag=0x0017, len=0x0008, 8B)

函数: `FUN_14014dae0` (0x14014dae0)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0017 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 2 | he_6g_cap | param3+0xa8 (u16) |
| 0x06-0x07 | 2 | pad | 0 |

**条件**: 仅当 he_6g_cap != 0 时构建 (6GHz 专用)。

### 2.9 STA_REC_HE_BASIC (tag=0x0019, len=0x001C, 28B)

函数: `HE_BASIC_STA_builder` (0x14014d810)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0019 |
| 0x02 | 2 | len | 0x001C |
| 0x04 | 6 | mac_cap | memcpy(param3+0x88, 6B) |
| 0x0A | 11 | phy_cap | memcpy(param3+0x8e, 11B) |
| 0x15 | 1 | pkt_ext | 0x02 (固定) |
| 0x16 | 2 | rx_mcs_80 | param3+0x9c (u16) |
| 0x18 | 2 | rx_mcs_160 | param3+0xa0 (u16) |
| 0x1A | 2 | rx_mcs_80p80 | param3+0xa4 (u16) |

### 2.10 STA_REC_MLD (tag=0x0020, len=不定)

函数: `EHT_MLD_builder` (0x14014e2a0) — Agent B 确认

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0021 |
| 0x02 | 2 | len | 0x0010 |
| 0x04 | 1 | mld_addr_from_sta | sta[0xf] |
| 0x05 | 1 | link_info | sta[0x10] |

**注意**: Agent B 确认常量 0x00100021, 可能是 STA_REC_MLD 的别名。

### 2.11 STA_REC_EHT (tag=0x0022, len=0x0028, 40B)

函数: `FUN_14014db80` (0x14014db80)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0022 |
| 0x02 | 2 | len | 0x0028 |
| 0x04 | 1 | const_ff | 0xFF |
| 0x05 | 1 | pad | 0 |
| 0x06 | 2 | eht_cap_u16 | param3+0xc8 (u16) |
| 0x08 | 8 | eht_block0 | param3+0xca (qword) |
| 0x10 | 8 | eht_block1 | param3+0xd2 (qword) |
| 0x18 | 4 | eht_block2 | param3+0xda (dword) |
| 0x1C-0x24 | 9 | eht_bytes | param3+0xde..0xe6 (逐字节) |
| 0x25-0x27 | 3 | pad | 0 |

### 2.12 STA_REC_UAPSD (tag=0x0024, len=0x0008, 8B)

函数: `FUN_14014e620` (0x14014e620)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0024 |
| 0x02 | 2 | len | 0x0008 |
| 0x04 | 1 | uapsd_flags | param3+0x13 |
| 0x05 | 1 | max_sp_len | param3+0x34 |
| 0x06 | 1 | uapsd_ac | param3+0x35 |
| 0x07 | 1 | pad | 0 |

### 2.13 STA_REC_MLD_SETUP (tag=0x0020, len=不定)

函数: `MLD_SETUP_builder` (0x14014ddc0) — Agent B 独有

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 2 | tag | 0x0020 |
| 0x02 | 2 | len | 动态 |
| 0x04 | 4 | mld_addr_part | 条件 |
| 0x08 | 2 | aid | 条件 |
| 0x0A | 2 | setup_wcid | 条件 |
| 0x0C | 2 | primary_link | 条件 |
| 0x0E | 2 | secondary_link | 条件 |
| 0x10 | 1 | mld_type | 条件 |
| 0x14+ | N | link_entries | 循环: 每 4B 一个 entry |

### Windows STA_REC 13 个 TLV 完整列表

```
 1. STA_BASIC     (tag=0x00, 20B)
 2. STA_RA        (tag=0x01, 16B)
 3. STA_STATE     (tag=0x07, 16B)
 4. STA_HT        (tag=0x09, 8B)   — 条件
 5. STA_VHT       (tag=0x0A, 16B)  — 条件
 6. STA_PHY       (tag=0x15, 12B)
 7. STA_BA_OFFLOAD(tag=0x16, 16B)
 8. STA_HE_6G_CAP (tag=0x17, 8B)   — 条件 (6GHz only)
 9. STA_HE_BASIC  (tag=0x19, 28B)
10. STA_MLD_SETUP (tag=0x20, 变长)
11. STA_EHT_MLD   (tag=0x21, 16B)
12. STA_EHT       (tag=0x22, 40B)
13. STA_UAPSD     (tag=0x24, 8B)
```

---

## 第三章: BssActivateCtrl 布局

函数: `nicUniCmdBssActivateCtrl` (0x140143540)

此函数构建 **两个独立 segment**，加入链表后发送:

### 3.1 Segment 1 — DEV_INFO (0x10 bytes)

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 1 | bss_index | input[3] |
| 0x01 | 1 | mapped_type | 0xFE if input[0]==4, else 0xFF |
| 0x02-0x03 | 2 | pad | 0 |
| 0x04 | 2 | tlv_tag | 0x0000 |
| 0x06 | 2 | tlv_len | 0x000C |
| 0x08 | 1 | activate_flag | input[1] (1=激活, 0=去激活) |
| 0x09 | 1 | param_byte | input[0xb] |
| 0x0A | 4 | param_dword | input+4 (u32) |
| 0x0E | 2 | param_word | input+8 (u16) |

### 3.2 Segment 2 — BSS_INFO 最小集 (0x24 或 0x38 bytes)

基础部分 (0x24 = 36 bytes):

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x00 | 1 | bss_type | input[0] |
| 0x01-0x03 | 3 | pad | 0 |
| 0x04 | 2 | tlv_tag | 0x0000 |
| 0x06 | 2 | tlv_len | 0x0020 |
| 0x08 | 1 | activate_flag | input[1] |
| 0x09 | 1 | bss_index | input[3] |
| 0x0A | 1 | bss_index_dup | input[3] |
| 0x0B | 1 | const_ff | 0xFF |
| 0x0C | 4 | token | FUN_14014fa20() 返回值 |
| 0x10 | 1 | flag_from_adapter | ~(adapter→0x2e6964 >> 7) & 1 |
| 0x11 | 1 | ctx_byte | adapter[0xb9550] |
| 0x12 | 4 | conn_type | adapter[0x5c] 或 link→0x2ac |
| 0x16 | 2 | bcn_interval | adapter→0x2e4 或 link→0x2b0 |
| 0x18 | 2 | sta_type | input[10] (u8→u16) |
| 0x1A | 2 | dtim | adapter[0xb8612] |
| 0x1C | 1 | phy_mode | adapter[0xb862d] |
| 0x1D | 1 | bw_info_low | uVar7 low byte (default 0x10) |
| 0x1E | 2 | mbss_flags | 0x00FE |
| 0x20 | 2 | selected_val | 0x0003 (if no link) 或 link→0x4ac |
| 0x22 | 1 | bw_info_high | uVar7 >> 8 |
| 0x23 | 1 | param_byte | input[0xb] |

额外 MLD 部分 (0x14 bytes, 仅当 activate_flag != 0):

| Offset | Size | 名称 | 值/来源 |
|--------|------|------|---------|
| 0x24 | 20 | MLD TLV | FUN_14014fad0() 填充 |

---

## 第四章: 当前驱动代码差距分析

### 4.1 BSS_INFO 差距

| # | TLV | Windows | 驱动当前 | 差距 |
|---|-----|---------|----------|------|
| 1 | BASIC (0x00) | 32B, 全字段 | 32B, 大部分正确 | **conn_type** 需验证 (我们用 CONNECTION_INFRA_STA 常量, Windows 用 FUN_14014fa20 动态计算) |
| 2 | RLM (0x02) | 16B | 16B | **bw 字段映射不同**: Windows 用 0→1, 1→2, 2→3, 3→6, 4→7 映射; 我们直接写 0 (CMD_CBW_20MHZ) |
| 3 | PROTECT (0x03) | 8B | 8B | **基本正确** — auth 阶段 protect_mode=0 |
| 4 | IFS_TIME (0x17) | 20B, 含 slot_valid=1 | 20B, slot_valid=1 | **结构体大小不匹配**: 我们 struct 为 20B, 但 Windows 返回 20B — 匹配 |
| 5 | RATE (0x0B) | 16B, operational + basic | 16B, 已填充 | **基本正确** |
| 6 | COLOR (0x04) | 8B | 8B | OK |
| 7 | HE (0x05) | 16B | 16B | **max_nss_mcs 全零** — Windows 从 link→0x5c3 填 (相同值重复 3 次) |
| 8 | SAP (0x0D) | 40B | 40B 但全零 | OK for STA mode |
| 9 | P2P (0x0E) | 8B | 8B | OK |
| 10 | QBSS (0x0F) | 8B, is_qbss from ctx | 8B, is_qbss=1 | OK |
| 11 | SEC (0x10) | 8B | 8B | OK for auth |
| 12 | MBSSID (0x06) | 8B | 8B | OK |
| 13 | 0C (0x0C) | 8B | 8B | OK |
| 14 | IOT (0x18) | 8B, 2 字段 | 8B, 全零 | OK for non-IoT |
| 15 | MLD (0x1A) | 20B | 20B | **大致正确**, group_mld_id 可能需调整 |
| 16 | EHT (0x1E) | 16B | 16B | OK for auth (全零) |

**BSS_INFO 结论**: 驱动已发送全部 14+2 个 TLV (比 Windows 还多 — 含 RLM 分拆)。**主要差距在 BASIC TLV 的 conn_type/packed_field 计算**。

### 4.2 STA_REC 差距

| # | TLV | Windows (13个) | 驱动当前 (9个) | 差距 |
|---|-----|----------------|----------------|------|
| 1 | BASIC (0x00) | 20B, extra=0x0003 | 20B, extra=0x0003 | **conn_state=1**: Windows 固定 0x01, 我们用 PORT_SECURE(2) |
| 2 | RA (0x01) | 16B | 16B | OK |
| 3 | STATE (0x07) | 16B | 16B | OK |
| 4 | HT (0x09) | 8B, 条件 | 8B | OK |
| 5 | VHT (0x0A) | 16B, 条件 | 16B | OK |
| 6 | PHY (0x15) | 12B | 12B | OK |
| 7 | BA_OFFLOAD (0x16) | 16B | 16B | OK |
| 8 | HE_6G_CAP (0x17) | 8B, 6GHz条件 | **缺失** | **需添加** (6GHz 连接时) |
| 9 | HE_BASIC (0x19) | 28B | 28B | OK |
| 10 | MLD_SETUP (0x20) | 变长 | **缺失** | **需添加** (即使非 MLO) |
| 11 | EHT_MLD (0x21) | 16B | **缺失** | **需添加** |
| 12 | EHT (0x22) | 40B | **缺失** | **需添加** |
| 13 | UAPSD (0x24) | 8B | 8B | OK |

**STA_REC 结论**: 缺少 4 个 TLV (HE_6G_CAP, MLD_SETUP, EHT_MLD, EHT)。

### 4.3 STA_REC_BASIC conn_state 差距

**关键发现**: Windows STA_BASIC offset 0x08 固定为 `0x01`, 我们用 `CONN_STATE_PORT_SECURE(2)`.

Agent A 证据:
```c
// FUN_14014d6d0 offset 0x08:
*(undefined1 *)(param_2 + 2) = 1;  // 固定写 0x01
```

这意味着 Windows 在 STA_REC 中 conn_state **始终** 为 1 (CONNECT), 不是 2 (PORT_SECURE).

### 4.4 TXD DW0 差距 (已修复)

**原 bug**: `0x84000000` → GENMASK(31,25)=66 或 5-bit=4, 都不是 ALTX0(0x10)
**已修复为**: `FIELD_PREP(MT_TXD0_Q_IDX, 0x10)` = `0x20000000`

代码确认 (mt7927_mac.c line 358):
```c
txwi[0] = cpu_to_le32(FIELD_PREP(MT_TXD0_Q_IDX, 0x10) |
                      (skb->len + MT_TXD_SIZE));
```
这已经正确。

### 4.5 MtCmdSetBssInfo (大 payload) vs TLV dispatch

Windows 有两条 BSS_INFO 路径:
1. **MtCmdSetBssInfo** (0x1400cf928): 构建 116B 扁平 payload, 用于旧路径
2. **nicUniCmdSetBssInfo** (0x1401444a0): 14 TLV dispatch, 用于 UniCmd 路径

我们用的是 UniCmd 路径 (正确)。但 MtCmdSetBssInfo 中的某些字段语义 (如 offset 0x34=phy_type, 0x35=channel_info, 0x3A=peer_addr_flag) 可以帮助理解 BASIC TLV 中未确认字段的含义。

### 4.6 MtCmdSendStaRecUpdate (大 payload, 236B)

Windows 也有扁平 payload 路径 (0x1400cdea0, 236B), 其中关键字段:
- offset 0x37: `0xFF` (wlan_idx fallback)
- offset 0x44: 0x02 或 0x03 (NSS count)
- offset 0x84: rate_cap (u32)
- offset 0x80-0x81: 多次条件写入 (capability flags)

这些字段映射到 TLV 路径的各个 TLV 中。

---

## 第五章: 修复方案

### 优先级 P0: TXD DW0 修复 [已完成]

**状态**: 代码已修复为 `FIELD_PREP(MT_TXD0_Q_IDX, 0x10)`
**位置**: `src/mt7927_mac.c` line 358
**效果**: Q_IDX 从错误的 4/66 修正为 0x10 (ALTX0)

### 优先级 P1: STA_REC_BASIC conn_state 修复

**问题**: 当前 `conn_state = CONN_STATE_PORT_SECURE (2)`, Windows 固定用 `1` (CONNECT)

**位置**: `src/mt7927_pci.c` line 2803

**当前代码**:
```c
req.basic.conn_state = CONN_STATE_PORT_SECURE;
```

**修复为**:
```c
req.basic.conn_state = 1;  /* Windows RE: 固定 0x01 (CONNECT) */
```

**预期效果**: 固件可能对 conn_state=2 的 STA 有不同处理 (如限制 TX 权限)。改为 1 可能解除限制。

### 优先级 P2: 添加 STA_REC_MLD_SETUP (tag=0x20)

**问题**: Windows 始终发送 MLD_SETUP TLV, 即使非 MLO 连接

**位置**: `src/mt7927_pci.h` (添加 struct) + `src/mt7927_pci.c` mt7927_mcu_sta_update()

**建议结构** (最小非 MLO 模式):
```c
struct sta_rec_mld_setup {
    __le16 tag;       /* 0x0020 */
    __le16 len;       /* 动态, 最小 0x14 */
    u8 mld_addr[6];
    __le16 aid;
    __le16 setup_wcid;
    __le16 primary_link;
    __le16 secondary_link;
    u8 mld_type;
    u8 pad[3];
} __packed;
```

**注意**: 此 TLV 结构复杂 (含循环 link entries), 非 MLO 时可能可以跳过。需验证固件是否严格要求。

### 优先级 P2: 添加 STA_REC_EHT_MLD (tag=0x21, 16B)

**建议结构**:
```c
struct sta_rec_eht_mld {
    __le16 tag;       /* 0x0021 */
    __le16 len;       /* 0x0010 */
    u8 mld_addr_from_sta;
    u8 link_info;
    u8 pad[10];
} __packed;
```

auth 阶段全零即可。

### 优先级 P2: 添加 STA_REC_EHT (tag=0x22, 40B)

**建议结构**:
```c
struct sta_rec_eht_info {
    __le16 tag;       /* 0x0022 */
    __le16 len;       /* 0x0028 */
    u8 const_ff;      /* 0xFF */
    u8 pad1;
    __le16 eht_cap;
    u8 eht_data[36];  /* auth 阶段全零 */
} __packed;
```

### 优先级 P3: STA_REC option 参数

**问题**: Windows RE 确认 STA_REC 用 `option=0xed` (非标准值), 我们用 `0x07` (SET_ACK)

**Windows RE 证据** (CID 映射表):
```
outer_tag=0xb1: DL=0xb1, R8=0xed, R9=0xa8
R8=0xed = option 参数
```

`0xed` 的含义未完全确认, 可能包含 `SET | ACK | NEED_RESP` 以外的标志。暂不修改。

### 优先级 P3: BSS_INFO RLM bw 映射

**问题**: 我们直接写 `bw=0` (20MHz), Windows 用映射表:
```
param3[0xf]: 0→0x01, 1→0x02, 2→0x03, 3→0x06, 4→0x07
```

对于 20MHz 连接，Windows 实际写的是 `0x01` 而不是 `0x00`。

**位置**: `src/mt7927_pci.c` line 2590

**修复为**:
```c
req.rlm.bw = 1;  /* Windows RE: 20MHz = 0x01, not 0x00 */
```

### 优先级 P3: BSS_BASIC phymode 精确值

Windows 通过复杂函数计算 phymode, 我们硬编码了简单值。当前值 (0x31/0x0f) 来自 mt7925 参考，可能不完全匹配 Windows 的动态计算。低优先级。

---

## 附录 A: TLV Tag ID 快速参考

### BSS_INFO TLV Tags

| Tag | Hex | 名称 | Len | 构建函数 |
|-----|-----|------|-----|----------|
| 0 | 0x0000 | BASIC | 32 | 0x14014c610 |
| 2 | 0x0002 | RLM | 16 | 0x140150edc (组合) |
| 3 | 0x0003 | PROTECT | 8 | 0x140150edc (组合) |
| 4 | 0x0004 | BSS_COLOR | 8 | 0x14014d010 |
| 5 | 0x0005 | HE | 16 | 0x14014cd50 |
| 6 | 0x0006 | MBSSID | 8 | 0x14014d300 |
| 11 | 0x000B | RATE | 16 | 0x14014cc90 |
| 12 | 0x000C | UNKNOWN_0C | 8 | 0x14014d320 |
| 13 | 0x000D | SAP | 40 | 0x14014ccf0 |
| 14 | 0x000E | P2P | 8 | 0x14014cd30 |
| 15 | 0x000F | QBSS | 8 | 0x14014ccd0 |
| 16 | 0x0010 | SEC | 8 | 0x14014ccb0 |
| 23 | 0x0017 | IFS_TIME | 20 | 0x140150edc (组合) |
| 24 | 0x0018 | IoT | 8 | 0x14014d350 |
| 26 | 0x001A | MLD | 20 | 0x14014fad0 |
| 30 | 0x001E | EHT | 16 | 0x14014d150 |

### STA_REC TLV Tags

| Tag | Hex | 名称 | Len | 构建函数 |
|-----|-----|------|-----|----------|
| 0 | 0x0000 | BASIC | 20 | 0x14014d6d0 |
| 1 | 0x0001 | RA | 16 | 0x14014e570 |
| 7 | 0x0007 | STATE | 16 | 0x14014d730 |
| 9 | 0x0009 | HT | 8 | 0x14014d7a0 |
| 10 | 0x000A | VHT | 16 | 0x14014d7e0 |
| 21 | 0x0015 | PHY | 12 | 0x14014d760 |
| 22 | 0x0016 | BA_OFFLOAD | 16 | 0x14014e5b0 |
| 23 | 0x0017 | HE_6G_CAP | 8 | 0x14014dae0 |
| 25 | 0x0019 | HE_BASIC | 28 | 0x14014d810 |
| 32 | 0x0020 | MLD_SETUP | 变长 | 0x14014ddc0 |
| 33 | 0x0021 | EHT_MLD | 16 | 0x14014e2a0 |
| 34 | 0x0022 | EHT | 40 | 0x14014db80 |
| 36 | 0x0024 | UAPSD | 8 | 0x14014e620 |

---

## 附录 B: 交叉验证说明

**Agent A** (Ghidra 反编译): 提供了完整的字段语义、常量值、条件分支逻辑
**Agent B** (汇编级): 提供了精确的指令地址、寄存器来源、MOV 指令确认

所有 TLV 的 **tag + len 常量** 两者完全一致:
- BSS_BASIC: A=0x200000, B=0x200000 ✓
- BSS_COLOR: A=0x80004, B=0x80004 ✓
- BSS_HE: A=0x100005, B=0x100005 ✓
- BSS_RATE: A=0x10000b, B 未独立确认但 Agent A 充分 ✓
- STA_BASIC: A=0x140000, B 未独立确认 ✓
- STA_STATE: A=0x100007, B 未独立确认 ✓
- STA_HE_BASIC: A=0x1c0019, B=0x1c0019 ✓
- STA_EHT: A=0x280022, B=0x280022 ✓
- BA_OFFLOAD: A=0x100016, B=0x100016 ✓
- UAPSD: A=0x80024, B=0x80024 ✓

**差异**: Agent B 有 `MLD_SETUP_builder` 和 `EHT_MLD_builder` 的独立 JSON, Agent A 没有这两个 — 说明 Agent B 的覆盖范围更广。

---

## 附录 C: 修复优先级总结

| 优先级 | 修复项 | 预期影响 | 代码位置 |
|--------|--------|----------|----------|
| **P0** | TXD DW0 Q_IDX | 管理帧送到正确队列 | mt7927_mac.c:358 [已修复] |
| **P1** | STA_REC conn_state 1→1 | 固件可能解除 TX 限制 | mt7927_pci.c:2803 |
| **P2** | 添加 STA_REC MLD/EHT TLVs | 匹配 Windows 完整 TLV 集 | mt7927_pci.c + .h |
| **P3** | RLM bw=0→1 | 20MHz 带宽映射正确 | mt7927_pci.c:2590 |
| **P3** | STA_REC option 0xed | 匹配 Windows 精确参数 | mt7927_pci.c:2951 |
