# Windows RE: BSS_INFO / STA_REC / BssActivateCtrl 逆向验证报告

日期: 2026-02-22
分析目标: mtkwecx.sys v5.7.0.5275
工具: pefile + capstone 反汇编 + 手动分析

---

## 1. conn_type 辅助函数 (0x140151608)

**完全确认 ✅** — 简单的 switch/case 函数:

```
输入 (edx)    返回值       含义
0x21         0x10001     CONNECTION_INFRA_STA (默认)
0x41         0x10002     CONNECTION_INFRA_AP
0x42         0x20002     P2P_GO / IBSS_AP
0x22         0x20001     P2P_GC / IBSS_STA
其他         0x10001     默认 = INFRA_STA
```

**与我们代码对比**: 我们使用 `CONNECTION_INFRA_STA = 0x10001` → **完全匹配** ✅

---

## 2. STA_REC BASIC TLV (0x14014d6d0)

**完全确认 ✅** — 20 字节 (0x14), 结构精确匹配。

### 反汇编逐字段映射

```
参数: rcx=unused, rdx=output_buf, r8=param_struct

mov dword ptr [rdx], 0x140000       → tag=0x0000(BASIC), len=0x0014(20B)
call conn_type_helper(param[1])     → conn_type
mov [rdi+4], eax                    → +0x04: conn_type (u32)
mov byte [rdi+8], 1                 → +0x08: conn_state = 1 (CONNECTED, 硬编码!)
mov [rdi+9], param[0x12]            → +0x09: qos
mov [rdi+0xa], word param[8:10]     → +0x0a: aid (u16)
mov [rdi+0xc], dword param[2:6]     → +0x0c: peer_addr[0:4]
mov [rdi+0x10], word param[6:8]     → +0x10: peer_addr[4:6]
mov word [rdi+0x12], 0x0003         → +0x12: extra_info = VER|NEW (硬编码!)
```

### 与我们 `struct sta_rec_basic` 对比

| 偏移 | 我们字段 | 我们值 | Windows 值 | 匹配 |
|------|---------|--------|-----------|------|
| +0x00 | tag | 0 | 0 | ✅ |
| +0x02 | len | 0x14 | 0x14 | ✅ |
| +0x04 | conn_type | 0x10001 | conn_type(0x21)=0x10001 | ✅ |
| +0x08 | conn_state | 变化(1/2) | **1 (硬编码)** | ⚠️ 见下文 |
| +0x09 | qos | sta->wme | param[0x12] | ✅ |
| +0x0a | aid | sta->aid | param[8:10] | ✅ |
| +0x0c | peer_addr | sta->addr | param[2:8] | ✅ |
| +0x12 | extra_info | 0x03 (enable时) | **0x0003 (硬编码)** | ✅ |

### 关键发现

1. **extra_info = 0x0003 (VER | NEW)**: Windows **硬编码**，不根据 enable/disable 变化。
   - 我们的代码: enable 时 0x03, disable 时 0x01 — **部分匹配**
   - 注意: 0x14014d6d0 是 BssActivateCtrl 上下文的简化构建器，完整 STA_REC 更新可能不同

2. **conn_state = 1**: 在此构建器中硬编码为 CONNECTED(1)。
   - 我们在 NOTEXIST→NONE 转换时发 conn_state=2(PORT_SECURE), 可能不匹配
   - 但这只是 BssActivateCtrl 的简化版本

---

## 3. BSS_INFO BASIC TLV (0x14014c610 + BssActivateCtrl 0x140143540)

### TLV 结构: 32 字节 (0x20), tag=0

从 BssActivateCtrl 逆向确认的精确布局 (TLV 偏移, 不含 bss_req_hdr):

```
偏移  大小  Windows 写入内容                    我们的字段名
+0x00  u16  tag = 0                             tag ✅
+0x02  u16  len = 0x20 (32)                     len ✅
+0x04  u8   active (param enable flag)          active ✅
+0x05  u8   omac_idx                            omac_idx ✅
+0x06  u8   hw_bss_idx (= omac_idx)             hw_bss_idx ✅
+0x07  u8   0xFF (硬编码, wmm 含义)             band_idx ⚠️ 名称错误但值正确
+0x08  u32  conn_type (0x10001 for STA)          conn_type ✅
+0x0c  u8   conn_state                          conn_state ✅
+0x0d  u8   adapter byte (band/wmm 相关)        wmm_idx ⚠️ 名称错误
+0x0e  u8[6] BSSID                              bssid ✅
+0x14  u16  byte from bss/param (扩展为 word)   bmc_tx_wlan_idx ⚠️
+0x16  u16  word from adapter                   bcn_interval ⚠️
+0x18  u8   byte from adapter[0x5c3168]         dtim_period ⚠️
+0x19  u8   phymode_lo (from phy_func)          phymode ⚠️
+0x1a  u16  0x00FE (默认 STA idx)               sta_idx ✅
+0x1c  u16  从 sta_rec 或默认                   nonht_basic_phy ✅
+0x1e  u8   phymode_hi (phymode >> 8)           phymode_ext ✅
+0x1f  u8   wmm/band byte (param[0xb])          link_idx ⚠️
```

### 关键问题分析

#### 问题 1: 字段名称混淆 (+0x07 和 +0x0d)

| 偏移 | 我们叫 | 实际用途 | 我们写的值 | Windows 写的值 |
|------|--------|---------|-----------|---------------|
| +0x07 | band_idx | wmm_idx | **0xFF** | **0xFF** ✅ |
| +0x0d | wmm_idx | band 相关 | mvif->wmm_idx (=0) | adapter byte |

**评估**: 值 0xFF 在 +0x07 匹配。+0x0d 我们写 wmm_idx(通常=0), Windows 写一个 adapter byte。
**影响**: 低 — 两边可能都写 0 或接近的值。

#### 问题 2: +0x14 到 +0x19 字段语义 ⚠️ 关键

**+0x14 (bmc_tx_wlan_idx, 2 bytes)**:
- 我们写: `0`
- Windows BSS_INFO_BASIC_TLV_builder 写: `word(bss_info[0x3a])` — 推测为 bcn_interval 或 STA type
- Windows BssActivateCtrl 写: `word(param[0xa])` — 推测为小数值
- **结论**: 如果这是 bcn_interval, 我们写 0 是**错误的**! 但当前代码注释说 "固件读为 sta_type, STA=0" — 需要更多验证

**+0x16 (bcn_interval, 2 bytes)**:
- 我们写: `beacon_int` (约 100)
- Windows 写: `word(adapter[0x5c3090])` — 可能是 bcn_interval 或 dtim*bcn
- **结论**: 值可能匹配 (如果 adapter 存的也是 bcn_interval)

**+0x18 (dtim_period, 1 byte)**:
- 我们写: `vif->bss_conf.dtim_period` (通常 1-2)
- Windows 写: `byte(adapter[0x5c3168])` — **不确定是 dtim 还是 phymode flag**
- **结论**: 如果这是 phymode 而非 dtim, 我们的值完全错误!

**+0x19 (phymode, 1 byte)**:
- 我们写: `0x31` (5GHz PHY_MODE_A|AN|AC)
- Windows 写: `phy_mode_from_band()` 返回值的低字节
- **结论**: 值可能匹配 (如果 phy_mode_from_band 返回 0x31 for 5GHz)

**+0x1e (phymode_ext, 1 byte)**:
- 我们写: `0`
- Windows 写: `phy_mode_from_band()` 返回值的高字节 (>> 8)
- **结论**: 如果 phy 函数返回 >0xFF 的值, 我们漏了高字节!

### 完整比较表: 我们 vs Windows

| 偏移 | 我们写 | Windows 写 | 状态 |
|------|--------|-----------|------|
| +0x04 active | mvif->bss_idx | enable flag | ⚠️ bss_idx vs boolean |
| +0x05 omac_idx | mvif->omac_idx | omac_idx | ✅ |
| +0x06 hw_bss_idx | mvif->omac_idx | =omac_idx | ✅ |
| +0x07 | 0xFF | 0xFF | ✅ |
| +0x08 conn_type | 0x10001 | 0x10001 | ✅ |
| +0x0c conn_state | 1/0 | derived | ✅ |
| +0x0d | wmm_idx(0) | adapter byte | ⚠️ |
| +0x0e bssid | bssid | bssid | ✅ |
| +0x14 | **0** | **bss_info byte** | ❓ 不确定 |
| +0x16 | **beacon_int** | **adapter word** | ❓ 可能匹配 |
| +0x18 | **dtim_period** | **adapter byte** | ❓ 可能不匹配 |
| +0x19 | **0x31** | **phymode_lo** | ❓ 可能匹配 |
| +0x1a sta_idx | 0x00FE | 0x00FE | ✅ |
| +0x1c nonht_basic_phy | wcid.idx | sta field | ❓ |
| +0x1e phymode_ext | **0** | **phymode_hi** | ⚠️ 可能漏了高字节 |
| +0x1f link_idx | band_idx | param byte | ❓ |

---

## 4. BssActivateCtrl (0x140143540) — 组合命令

### 关键发现: DEV_INFO + BSS_INFO 是**原子组合**

Windows BssActivateCtrl 在**同一个函数**中:
1. 分配 DEV_INFO 容器 (CID=1, 16 字节 payload)
2. 分配 BSS_INFO 容器 (CID=2, 36/56 字节 payload)
3. 将两者链入同一个发送队列
4. 作为**单条 UniCmd 消息**一起发送

**我们的代码**: DEV_INFO 和 BSS_INFO 分别作为**两条独立 UniCmd 消息**发送!

### DEV_INFO payload 布局 (BssActivateCtrl 版本)

```
偏移  大小  值
+0x00  u8   omac_idx (param[3])
+0x01  u8   active_flag (0xFF=enable, 0xFE=disable)
+0x02  u8[2] padding
+0x04  u32  flags = 0x000C0000
+0x08  u8   band_idx (param[1])
+0x09  u8   unknown (param[0xb])
+0x0a  u8[6] MAC address (param[4:10])
```

### 我们的 DEV_INFO 布局

```
[dev_info_hdr: 4 bytes]
  +0x00 u8  omac_idx
  +0x01 u8  band_idx
  +0x02 u8[2] padding
[dev_info_active_tlv: 12 bytes]
  +0x00 u16 tag=0
  +0x02 u16 len=12
  +0x04 u8  active
  +0x05 u8  padding
  +0x06 u8[6] MAC address
```

### ⚠️ DEV_INFO 布局差异!

Windows DEV_INFO payload (16 bytes) 与我们的 hdr+tlv (16 bytes) 结构**完全不同**:
- Windows: `[omac, active, pad2, flags(4), band, ?, MAC(6)]`
- 我们:   `[omac, band, pad2, tag(2), len(2), active, pad, MAC(6)]`

Windows payload 中有个 `flags = 0x000C0000` 字段, 我们没有! 但 DEV_INFO 在初始化阶段已经正常工作,
说明要么:
1. 固件对两种格式都接受 (unlikely)
2. 我们的 UniCmd 系统自动添加了 tag/len (possible)
3. BssActivateCtrl 的容器分配函数处理了格式转换 (likely)

### BSS 大小计算

```
if byte[rbp+1] != 0 (active/enable):
    size = 0x24 + 0x14 = 0x38 (56 bytes)
    = bss_req_hdr(4) + BASIC_TLV(32) + MLD_TLV(20 bytes)
else (deactivate):
    size = 0x24 (36 bytes)
    = bss_req_hdr(4) + BASIC_TLV(32)
```

### 激活时追加的 TLV (r15+0x24, 20 bytes)

当 enable=true 时, 调用 `0x14014fad0(adapter, &r15[0x24], bss_idx)` 写入额外 TLV。
推测是 MLD TLV 或 BSSID TLV (20 bytes, 比我们的 bss_mld_tlv 的 16 bytes 大)。

---

## 5. 差异总结和修复建议

### 高风险项 (可能影响 auth)

| # | 问题 | 风险等级 | 修复建议 |
|---|------|---------|---------|
| 1 | +0x14 bmc_tx_wlan_idx=0 vs Windows bcn_interval | **高** | 验证此字段实际含义, 如果是 bcn_interval 则改为写 beacon_int |
| 2 | +0x18 dtim_period vs phymode | **高** | 如果是 phymode, 当前值完全错误 (dtim=1 ≠ phymode=0x31) |
| 3 | +0x1e phymode_ext=0 vs phymode_hi | **中** | phy_mode_from_band 可能返回 >0xFF, 需要写高字节 |
| 4 | BssActivateCtrl 组合发送 | **中** | 当前分开发送可能影响固件内部时序 |
| 5 | +0x04 active=bss_idx vs boolean | **中** | 如果 bss_idx=0, 固件可能读为 disabled |

### 低风险项

| # | 问题 | 风险等级 | 说明 |
|---|------|---------|------|
| 6 | 字段名称 +0x07/+0x0d 交换 | 低 | 值已正确 (0xFF 和 0/wmm) |
| 7 | DEV_INFO 内部布局不同 | 低 | 已工作, 可能是容器层处理差异 |
| 8 | MLD TLV 大小 (20 vs 16) | 低 | 需要确认 Windows MLD TLV 的精确大小 |

### 建议的修复优先级

1. **立即**: 验证 +0x14/+0x16/+0x18/+0x19 的实际含义
   - 方法: 抓取 Windows 驱动实际发送的 BSS_INFO hex dump
   - 或: 反汇编 phy_mode_from_band (0x14014fdfc) 确认返回值

2. **短期**: 如果确认 +0x14=bcn_interval, +0x18=phymode:
   ```c
   // 修改 struct 或调整赋值:
   req.basic.bmc_tx_wlan_idx = cpu_to_le16(vif->bss_conf.beacon_int); // +0x14
   req.basic.bcn_interval = cpu_to_le16(vif->bss_conf.dtim_period);   // +0x16
   req.basic.dtim_period = phymode;                                     // +0x18
   req.basic.phymode = phymode_ext;                                     // +0x19
   ```

3. **中期**: 实现 BssActivateCtrl 作为组合命令

---

## 6. phy_mode_from_band 函数 (0x14014fdfc) — 关键分析

### 函数行为: 位扩展映射

此函数将紧凑的 band capability 字节扩展为固件 phymode 位图 (u32):

```
输入位   输出效果                    推测含义
BIT(0) → 输出 BIT(1) (值 2)        HR_DSSS / 11b
BIT(1) → 输出 BIT(2) (值 4)        ERP / 11g
BIT(3) → 输出 BIT(0) (值 1)        CCK basic
BIT(4) → 输出 BIT(3)|BIT(4) (0x18) HT / 11n
BIT(5) → 输出 BIT(5) (0x20)        VHT / 11ac
BIT(6) → 输出 BIT(6)|BIT(7)|BIT(8) (0x1C0) HE / 11ax
BIT(7) → 输出 BIT(9)|BIT(10)|BIT(11) (0xE00) EHT / 11be
```

### 示例返回值

| 输入 (sta[0x4a7]) | 输出 (phymode) | 低字节 (+0x19) | 高字节 (+0x1e) |
|-------------------|----------------|----------------|----------------|
| 0x00 (无能力) | 0x0000 | 0x00 | 0x00 |
| 0x02 (ERP only) | 0x0004 | 0x04 | 0x00 |
| 0x12 (ERP+HT) | 0x001C | 0x1C | 0x00 |
| 0x32 (ERP+HT+VHT) | 0x003C | 0x3C | 0x00 |
| 0x72 (ERP+HT+VHT+HE) | 0x01FC | 0xFC | 0x01 |
| 0xF2 (全部含EHT) | 0x0FFC | 0xFC | 0x0F |
| 0x0A (BIT1+BIT3) | 0x0005 | 0x05 | 0x00 |

### ⚠️ 关键发现: 我们的 phymode 值可能完全错误!

我们硬编码 phymode = 0x31 (5GHz), 但 phy_mode_from_band 的返回值使用**完全不同的位编码**!

- **我们**: 0x31 = BIT(0)|BIT(4)|BIT(5) → 可能是 mt7925 编码 (PHY_MODE_A|VHT|HE ?)
- **Windows**: 对于 5GHz ERP+HT+VHT, 返回 0x003C = BIT(2)|BIT(3)|BIT(4)|BIT(5)
- **Windows**: 对于 5GHz 含 HE, 返回 0x01FC, **高字节=0x01, 低字节=0xFC**

如果 AP 支持 HE (802.11ax), Windows 会在 phymode_ext (+0x1e) 写入非零值!
我们始终写 0 → **丢失了 HE/EHT 能力标志!**

### 确切输入值无法确定

`sta[0x4a7]` 的值取决于连接时 AP 报告的能力。需要:
1. 找到 Windows 如何设置 sta[0x4a7] (在关联过程中从 beacon/probe response 解析)
2. 或者: 在我们的驱动中根据 AP 能力动态计算 phymode (而非硬编码)

---

## 附录A: 反汇编脚本

分析脚本: `tools/disasm_bss_sta.py`, `tools/disasm_phymode.py`
完整反汇编输出: `tools/disasm_output.txt`

## 附录B: phy_mode_from_band 完整反汇编

```asm
; 输入: cl = band capability byte
; 输出: eax = phymode bitmap (u32)
0x14014fdfc:  movzx    r9d, cl           ; r9 = input
0x14014fe00:  mov      edx, r9d
0x14014fe03:  and      edx, 1            ; BIT(0) → edx=0 or 2
0x14014fe06:  add      edx, edx
0x14014fe08:  mov      r8d, edx
0x14014fe0b:  or       r8d, 4            ; add BIT(2)
0x14014fe0f:  test     r9b, 2            ; if !(input & BIT(1))
0x14014fe13:  cmove    r8d, edx          ;   remove BIT(2)
0x14014fe17:  mov      ecx, r8d
0x14014fe1a:  or       ecx, 1            ; add BIT(0)
0x14014fe1d:  test     r9b, 8            ; if !(input & BIT(3))
0x14014fe21:  cmove    ecx, r8d          ;   remove BIT(0)
0x14014fe25:  mov      r8d, ecx
0x14014fe28:  or       r8d, 0x18         ; add BIT(3)|BIT(4)
0x14014fe2c:  test     r9b, 0x10         ; if !(input & BIT(4))
0x14014fe30:  cmove    r8d, ecx          ;   remove BIT(3)|BIT(4)
0x14014fe34:  mov      edx, r8d
0x14014fe37:  or       edx, 0x20         ; add BIT(5)
0x14014fe3a:  test     r9b, 0x20         ; if !(input & BIT(5))
0x14014fe3e:  cmove    edx, r8d          ;   remove BIT(5)
0x14014fe42:  mov      ecx, edx
0x14014fe44:  or       ecx, 0x1c0        ; add BIT(6)|BIT(7)|BIT(8)
0x14014fe4a:  test     r9b, 0x40         ; if !(input & BIT(6))
0x14014fe4e:  cmove    ecx, edx          ;   remove BIT(6)|BIT(7)|BIT(8)
0x14014fe51:  mov      eax, ecx
0x14014fe53:  or       eax, 0xe00        ; add BIT(9)|BIT(10)|BIT(11)
0x14014fe58:  test     r9b, r9b          ; if !(input & BIT(7)) (sign bit)
0x14014fe5b:  cmovns   eax, ecx          ;   remove BIT(9)|BIT(10)|BIT(11)
0x14014fe5e:  ret
```
