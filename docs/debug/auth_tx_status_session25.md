# Auth TX 调查状态报告 — Session 25 (2026-02-22)

## 一、当前现象

Auth 帧发送后固件返回 TXFREE stat=1（内部丢弃），MIB TX 计数器 = 0（帧从未到达射频前端）。
此问题从 Session 3 开始，至今 **22+ session 未解决**。

### 每次 auth 的 dmesg 输出（完全一致）

```
TX mgmt->ring0: fc=0x00b0 DA=b4:ba:12:5b:63:c9 len=30 wcid=1
TX0 kick: BASE=0x0a344000 CNT=256 CIDX=1 DIDX=1
TXFREE: msdu_cnt=1 ver=5 len=20
TXFREE DW1: 0xaf050001      ← wlan_idx=1, tx_count=5
TX-FAIL: stat=1 count=15 wlan=0
  Band0: TX_OK=0 FAIL=0 RETRY=0
  Band1: TX_OK=0 FAIL=0 RETRY=0
  PLE_EMPTY=0xf133ffff PSE_EMPTY=0x9ffffffb
  PLE_STA0=0x0000000a PLE_STA1=0x00000005
  STA_PAUSE0=0x03000001 STA_PAUSE1=0x2fff0fff
```

### 关键数据点

| 指标 | 值 | 含义 |
|------|-----|------|
| TXFREE stat | 1 | 固件内部丢弃（不是空中失败） |
| TXFREE tx_count | 5 (byte2=0x05) | 固件尝试了 5 次？ |
| MIB TX Band0/Band1 | 全 0 | 帧从未送到射频 |
| PLE_EMPTY | 0xf133ffff | PLE 有空闲（不是资源耗尽） |
| PSE_EMPTY | 0x9ffffffb | PSE 有空闲 |
| STA_PAUSE0 | 0x03000001 | BIT(0)+BIT(24)+BIT(25) 被 pause |
| STA_PAUSE1 | 0x2fff0fff | 大量队列被 pause |
| WTBL[1] BAND | 0 | 2.4GHz，但连接目标是 5GHz（band=1） |
| WTBL[1] DW0 | 0x3040c963 | RV=1(有效), MAC 正确 |
| WTBL[1] DW2 | 0x0c200000 | QoS=1, HT=1, VHT=0, HE=0 |

---

## 二、已确认解决的问题（不再是根因候选）

### 2.1 TXD 格式（Session 17-22 排除）

TXD DW0-DW7 全部经 Ghidra 汇编级验证，与 Windows 完全匹配：

```
DW0: 0x2000003e  (Q_IDX=0x10=ALTX0, PKT_FMT=CT, TX_BYTES=62)
DW1: 0x800c9001  (WLAN_IDX=1, OWN_MAC=0, HDR_FORMAT)
DW2: 0x001e000b  (FIXED_RATE, FC_TYPE=0, FC_STYPE=0xb=auth)
DW3: 0x1000f000  (REM_TX_COUNT=30, REMAINING_LIFE_TIME)
DW4: 0x00000000
DW5: 0x00000600  (TX_STATUS_HOST + TX_STATUS_MCU)
DW6: 0x000b0018  (OFDM 6Mbps fixed rate)
DW7: 0x00000000  (TXD_LEN=0, 不加密)
```

验证文档：`docs/win_re_dw2_dw6_verified.md`

### 2.2 TXP CT mode 格式（Session 24 排除）

```
TXP: 00008000 00000000 5705b000 0000801e
     ^^^^^^^^                    ^^^^^^^^
     msdu_id[0]=token(0)|VALID   len=30|LAST
```

散列表结构正确，DMA 地址有效，长度匹配。

### 2.3 DMA Ring 0 提交（Session 22 排除）

- Ring 0 CT mode 正确工作
- CIDX 写入后 DIDX 跟进 → DMA 硬件消费了描述符
- TXFREE ~43ms 后返回 → 固件确实处理了这个帧

### 2.4 MCU 命令 CID（Session 20 排除）

用 mt7925 验证过的 CID 值工作正常（扫描成功证明）。
Windows inner CID 在 Session 19 测试导致扫描回归，已回退。

### 2.5 DMASHDL 配置（Session 23 排除）

简化为 Windows 风格（只写 QMAP0 |= 0x10101），STA_PAUSE 从 0xf133ffff 改善到 0x03000001。
扫描从 21 BSS 提升到 130 BSS。但 auth 仍失败。

### 2.6 EFUSE_CTRL（Session 20 排除）

CID=0x2d，mt7925 格式。移除后扫描为 0，恢复后正常。与 auth 无关。

---

## 三、Session 25 修改和结果

### 3.1 STA_REC 延迟到 ROC_GRANT 之后

**修改**：sta_state NOTEXIST→NONE 不再发 BSS_INFO 和 STA_REC，只分配 WCID。
BSS_INFO + STA_REC 首次在 mgd_prepare_tx（ROC_GRANT 后）发送。

**目的**：确保 WTBL 创建时 band_idx 正确（之前 band_idx=0xff → WTBL BAND=0）。

**结果**：命令顺序正确了，band_idx=1 传到固件，但 **WTBL BAND 仍为 0**。

### 3.2 实际命令序列（dmesg 确认）

```
T+0.000  ROC_GRANT status=0 ch=161 dbdcband=1
T+0.000  DEV_INFO active=1 omac=0 band=1
T+0.001  BssActivateCtrl bss=0 omac=0 band=1
T+0.002  BSS_INFO PM_DISABLE bss=0
T+0.003  BSS_INFO full band_idx=1 bssid=b4:ba:12:5b:63:c9
T+0.004  STA_REC wcid=1 conn_state=2 enable=1   → ACK 收到
T+0.029  RX_FILTER=0x0b
T+0.030  send auth (try 1/3)
T+0.074  TXFREE stat=1   → WTBL[1] BAND=0
```

与 Windows 对比命令序列匹配：DEV_INFO → BssActivateCtrl → PM_DISABLE → BSS_INFO → STA_REC → auth。

---

## 四、当前剩余问题清单

### 4.1 WTBL BAND=0（核心症状，但可能不是根因）

**现象**：无论 BSS_INFO 和 STA_REC 传什么 band_idx，WTBL DW0 bits[27:26] 始终为 0。

**已尝试**：
- band_idx=0xff 发送 → BAND=0
- band_idx=1 发送（STA_REC 之前/之后） → BAND=0
- BssActivateCtrl 含 band=1 → BAND=0

**疑问**：WTBL BAND 字段到底由什么控制？可能不是 BSS_INFO/STA_REC 的 band_idx，
而是由固件根据信道频率自行决定？或者我们的 WTBL 读取方法（direct BAR0 0x038100）
读的偏移是否正确？

### 4.2 STA_PAUSE0 = 0x03000001

BIT(0) = WCID 0 被 pause，BIT(24)+BIT(25) = 某些队列被 pause。
DMASHDL 简化后从 0xf133ffff 改善到此值，但仍有 pause。
可能表示固件认为某些条件不满足（频段？信道？配置？）。

### 4.3 MIB TX 全零

Band0 和 Band1 的 TX_OK/FAIL/RETRY 全为 0。
帧从未到达 LMAC/射频前端。固件在 PLE→LMAC 之间某处丢弃。

### 4.4 L1 Remap 读 WTBL 返回全零

mt7927_dump_wtbl() 使用 L1 remap 读 LWTBL/UWTBL 始终全零。
但 direct BAR0 读 (0x038100) 显示有效数据。
原因不明，不阻塞但影响诊断。

### 4.5 mac80211 早期 BSS_INFO

mac80211 的 bss_info_changed 回调在 sta_state 之前触发，
发了两次 BSS_INFO(band_idx=255)。这些在 BssActivateCtrl 之前就到了固件。
Windows 从不在 BssActivateCtrl 之前发 BSS_INFO。

---

## 五、已排除的假设（历史记录）

| Session | 假设 | 排除原因 |
|---------|------|---------|
| 3-8 | TX Ring 选择错误 | 试过 Ring 0/2/15，CT/SF mode，全部失败 |
| 9-12 | TXD 字段错误 | Ghidra 汇编级验证全部匹配 Windows |
| 13 | DW7 TXD_LEN bug | 修复后得到第一个 TXFREE，但 stat=1 |
| 14 | DW1 WLAN_IDX=0 | 修复后 WTBL MAC 正确填充 |
| 15-16 | DEV_INFO/BSS_INFO 缺失 | 补上后无改善 |
| 17-18 | ROC 未获得信道 | ROC_GRANT 正常返回 |
| 19 | CID 用 Windows inner 值 | 导致扫描回归，已回退 |
| 20 | DMASHDL/EFUSE 缺失 | 恢复后扫描改善但 auth 无变化 |
| 21-22 | Ring 2 SF mode | DMA 消费成功但固件静默 |
| 23 | DMASHDL 参数错误 | 简化后 STA_PAUSE 改善但 auth 无变化 |
| 24 | TXP 格式错误 | 结构匹配 CT mode scatter-gather |
| 24 | STA_REC conn_state 值 | PORT_SECURE(2) vs CONNECT(1) 无区别 |
| 24 | STA_REC option fire-and-forget | 改 ACK(0x07) 无区别 |
| 25 | WTBL BAND=0 因 STA_REC 时序 | band_idx=1 传入后 WTBL BAND 仍为 0 |

---

## 六、还没有尝试的方向

### 6.1 BssActivateCtrl conn_state

Windows BssActivateCtrl 中 `basic.conn_state = 1`（activate），
然后 full BSS_INFO 中 `basic.conn_state = 0`（connected/ready）。
我们的 BssActivateCtrl 也是 conn_state=1，full BSS_INFO 是 conn_state=0。✅ 匹配。

### 6.2 BSS_INFO BASIC TLV conn_type

Windows: 0x10001 (CONNECTION_INFRA_STA)。我们: 0x10001。✅ 匹配。

### 6.3 BSS_INFO BASIC TLV 其他字段

Windows BssActivateCtrl 中 BASIC TLV 布局：
```
[+0x0b] sco = 0xFF
[+0x10] active = ~(bss->xxx >> 7) & 1     ← 我们写 0，需确认
[+0x11] network_type = bss->xxx            ← 我们写 0
[+0x14] sta_type = rbp[0xa]               ← 我们写 0x00FE
[+0x1d] phy_mode_lo                        ← 我们写 0x31
[+0x20] wlan_idx                           ← 我们写 0
[+0x22] phy_mode_hi                        ← 未设置
```

**可能有差异但需逐字段对比**。特别是 `active` 字段 — Windows 用 `~(flag >> 7) & 1`，
值取决于 BSS 状态，可能不是 0。

### 6.4 BssActivateCtrl 的 conn_type

Windows: `nicUniCmdBssInfoConnType(adapter) → 0x10001`
但注意 — BssActivateCtrl 内部的 BSS_INFO 和 full BSS_INFO 的 conn_type
可能不同。需从 Ghidra 确认 BssActivateCtrl 中的值。

### 6.5 STA_REC BASIC TLV extra_info

我们 `extra=0x3` (EXTRA_INFO_NEW | EXTRA_INFO_UPDATE)。
Windows 的值未确认。如果 Windows 用不同的 extra 组合，
固件可能以不同方式处理 WTBL 创建。

### 6.6 STA_REC 缺少的 TLV

Windows 发 13 个 TLV，我们只发 5 个（BASIC + RA + STATE + PHY + HDR_TRANS）。
缺少：HT_INFO, VHT_INFO, HE_BASIC, HE_6G_CAP, BA_OFFLOAD, UAPSD, EHT_INFO, EHT_MLD, MLD_SETUP。
某个 TLV 可能是固件允许 TX 的前提条件。

### 6.7 BSS_INFO 缺少的 TLV

Windows 的 full BSS_INFO 有 14 个 TLV（通过 dispatch table）+ RLM 3合1。
我们有：BASIC, RLM, PROTECT, IFS_TIME, HE, COLOR, RATE, SAP, P2P, QBSS, SEC,
MBSSID, 0x0C, IOT, MLD, EHT（已补全到约 16 个）。
但字段值可能不对。

### 6.8 WTBL BAND 字段的真实含义

我们假设 DW0 bits[27:26] = BAND，来源是 MT6639 的 `wf_ds_lwtbl.h`。
但这个字段定义可能在 MT7927 固件中不同。或者 BAND=0 可能不代表 2.4GHz，
而是"默认"或"未分配"。**需要从 Windows 运行状态抓包验证实际 WTBL 值**。

### 6.9 TXFREE stat=1 的确切含义

stat=1 在 CONNAC3 中可能不是"失败"。需要从固件代码或文档确认：
- stat=0 = ?
- stat=1 = ?
- stat=2 = ?
TXFREE DW1 byte2=0x05 可能是 tx_count=5（固件尝试 5 次后放弃）。

### 6.10 Ring 0 TX_DONE 中断

我们用 Ring 0 CT mode 提交管理帧，但 TX_DONE 中断掩码是否包含 Ring 0？
INT_MASK=0x2600f010 中 BIT(4)=HOST_TX_DONE_INT_ENA0=1 → Ring 0 TX_DONE 已启用。
但 TXFREE 是通过 RX event ring 返回的，不是通过 TX_DONE 中断。

### 6.11 802.11 auth 帧本身

帧内容是否正确？30 字节 = 24(header) + 6(auth body: algo=0, seq=1, status=0)。
mac80211 构建的帧应该没问题，但可以用 AR9271 monitor mode 验证空中有无信号。

### 6.12 BSS_INFO active 字段

我们全程 `basic.active = 0`（BSS_INFO 日志显示 `[+C]active=0`）。
Windows: `active = ~(bss->0x2e6964 >> 7) & 1`，取决于 BSS 激活状态。
如果 BSS 已激活，active 应该是 1。**我们可能一直发 active=0（未激活），
导致固件认为 BSS 不活跃，拒绝发送帧。**

---

## 七、TXFREE 详细解码

三次 auth 尝试的 TXFREE 对比：

| 字段 | 尝试1 | 尝试2 | 尝试3 | 变化 |
|------|-------|-------|-------|------|
| DW0 | 0x30010014 | 同 | 同 | 不变 |
| DW1 | 0xaf050001 | 0xcf050101 | 0x5f050201 | byte1 递增(0→1→2), byte3 变化 |
| DW2 | 0x90001000 | 同 | 同 | 不变 |
| DW3 | 0x5f000000 | 同 | 同 | 不变 |
| DW4 | 0x3fff8000 | 0x3fff8001 | 0x3fff8002 | token 递增 |

DW1 byte1 (0x00→0x01→0x02) 可能是序列号/PID。
DW1 byte3 变化 (0xaf→0xcf→0x5f) 可能是时间戳或帧计数器。
DW4 低位 (0→1→2) 是 token ID，与我们分配的 token 匹配。

---

## 八、Windows 连接流程 vs 我们的流程（精确对比）

### Windows（从 `win_re_connect_flow_complete.md`）

```
[1] ChipConfig (CMD_ID=0xca, 328B payload)        ← 每次 connect 重发
[2] BssActivateCtrl:
      DEV_INFO (CID=1, band_idx from BSS context)
      BSS_INFO (CID=2, BASIC+MLD, conn_state=1)
[3] PM_DISABLE (BSS_INFO CID=2, tag=0x1B)
[4] Full BSS_INFO (CID=2, 14 TLVs, conn_state=0)
[5] SCAN_CANCEL
[6] ChPrivilege (CID=0x27)                         ← 信道申请
[7] STA_REC (CID=3, 13 TLVs)
[8] Auth TX
```

### 我们的当前流程

```
bss_info_changed:
  [A] BSS_INFO (CID=2, full TLVs, band_idx=255)   ← 无 BssActivateCtrl
  [B] BSS_INFO (CID=2, full TLVs, band_idx=255)   ← BSSID 更新

sta_state NOTEXIST→NONE:
  [C] WCID 分配 + ADM 清零（无 MCU 命令）

mgd_prepare_tx:
  [D] ROC acquire (CID=0x27)
  --- 等待 ROC_GRANT ---
  [E] DEV_INFO (CID=1, band=1)
  [F] BssActivateCtrl (CID=2, BASIC+MLD, band=1, conn_state=1)
  [G] PM_DISABLE (CID=2, tag=0x1B)
  [H] Full BSS_INFO (CID=2, full TLVs, band=1, conn_state=0)
  [I] STA_REC (CID=3, 5 TLVs, wcid=1)
  [J] RX_FILTER
  [K] Auth TX
```

### 差异

| 差异点 | Windows | 我们 | 影响等级 |
|--------|---------|------|---------|
| ChipConfig 重发 | 每次 connect | 只在 init | 🟡 未知 |
| 早期 BSS_INFO(band=255) | 无 | 有 [A][B] | 🔴 可能污染 |
| BssActivateCtrl 时机 | ChPrivilege 之前 | ChPrivilege 之后 | 🟡 可能 |
| BSS_INFO basic.active | 取决于状态(可能=1) | 始终 0 | 🔴 可能关键 |
| STA_REC TLV 数量 | 13 个 | 5 个 | 🟡 可能 |
| ChPrivilege vs ROC | ChPrivilege | ROC | 🟡 功能等价 |
| SCAN_CANCEL | 显式发送 | 无 | 🟢 可能无关 |

---

## 九、下一步优先级建议

1. **验证 BSS_INFO active 字段** — 我们始终写 0，Windows 可能写 1。
   这可能直接告诉固件"BSS 未激活，不要发帧"。
2. **阻止早期 BSS_INFO(band=255)** — bss_info_changed 回调在 band 已知前就发了。
3. **验证 WTBL BAND 是否真的是问题** — 用 AR9271 monitor mode 抓包，
   看 auth 帧是否真的没发出去。
4. **解码 TXFREE stat/count 含义** — 从 mt6639 或 Windows RE 确认字段定义。
5. **补全 STA_REC TLV** — 从 13 个减到 5 个可能导致固件缺少必要配置。
