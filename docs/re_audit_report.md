# Windows RE 文档审计报告

**审计日期**: 2026-02-22
**审计范围**: docs/ 目录下所有 Windows 逆向工程相关文档 (60+ 文件)
**审计方法**: 逐文件与已知事实交叉验证，文档间互相比对

---

## 一、已知证伪事实清单 (审计基准)

以下事实已通过实际测试或后续逆向确认，作为审计基准:

| # | 事实 | 来源 |
|---|------|------|
| F1 | CID 应使用 mt7925/outer 值，不是 Windows inner CID | Session 20 bisect |
| F2 | BSS_INFO packed_field 应为 0x00080015，不是 MT6639 的 0x10001 | Session 18 |
| F3 | mt7925 TXD 宏不可用于管理帧 (Q_IDX 7bit vs 5bit 等) | Session 22 |
| F4 | Ring 2 SF mode DMA 消费成功但固件崩溃 (PLE crash) | Session 22-23 |
| F5 | WTBL BAND 字段问题至今未解决 | Session 23 |
| F6 | STA_REC timing 不是根因 (多种时序都试过) | Session 1-23 |
| F7 | DMASHDL 应简化为 Windows 风格 `QMAP0 |= 0x10101` | Session 23 |
| F8 | conn_state=0 vs 1 无差异 | 测试确认 |
| F9 | add_interface 阶段的 BSS_INFO(band=255) 可能污染固件状态 | Session 24 |
| F10 | Windows auth 帧走 Ring 2 SF mode，不是 Ring 0 或 Ring 3 | Session 17 RE 确认 |

---

## 二、逐文件审计结果

---

## win_re_tx_mgmt_path.md

### 问题 1: "Auth 帧使用 Ring 3" — 已证伪
- 位置: Section 4 "TX Ring 选择", Section 7 "结论"
- 内容: "Auth 帧使用 Ring 3 (参数 3 硬编码)"，"1. Auth 帧使用 Ring 3"
- 问题: 参数 `3` 是 FUN_14009a46c 的序号生成器参数，不是 ring index。后续文档 `win_re_full_txd_dma_path.md` 明确更正: "FUN_14009a46c(param_1, 3, 1) — Sequence number generator (NOT ring selector!)"。管理帧实际使用 **Ring 2** (N6PciTxSendPkt param_3=2 硬编码)。
- 参考: `win_re_full_txd_dma_path.md` Section 2, `win_re_ring2_analysis.md` Section 3.2

### 问题 2: "建议测试 TX Ring 3" / "DW7 TXD_LENGTH=1" — 错误建议
- 位置: Section 7 "下一步实验建议"
- 内容: "测试 TX Ring 3" + "设置 DW7 TXD_LENGTH=1"
- 问题: Ring 3 不存在于 Windows ring 布局中。DW7=0 已被 `win_re_dw2_dw6_verified.md` 汇编级验证为正确值。TXD_LEN bit 30 被 Windows 显式清除。
- 参考: `win_re_dw2_dw6_verified.md` Section 5

### 问题 3: TXD 字段推测大量错误
- 位置: Section 5 "TXD 格式推断"
- 内容: "TXD_LEN_1_PAGE 标志 (DW7[31:30]=1) 在 MT6639 中始终设置" / "LONG_FORMAT 可能需要"
- 问题: 这些是未经验证的推测。后续汇编验证确认 DW7=0，无 TXD_LEN 扩展。
- 参考: `win_re_dw2_dw6_verified.md`

---

## win_re_connection_flow.md

### 问题 1: 连接命令序列不完整
- 位置: Section "Connection Command Sequence"
- 内容: 流程图显示 WdiTaskConnect → DEV_INFO → ChReqPrivilege → BSS_INFO → STA_REC → Auth
- 问题: 缺少关键步骤。后续 `win_re_connect_flow.md` 发现 **PM_DISABLE** 命令 (BSS_INFO tag=0x1B) 在 BSS_INFO 之前必须发送。`win_re_connect_flow_complete.md` 进一步发现 **CHIP_CONFIG** 也在连接时重发。
- 参考: `win_re_connect_flow.md` Section 二

### 问题 2: "0xa00577 可能是 Q_IDX + flags"
- 位置: Section "③ Auth 帧 TX 路径"
- 内容: "`0xa00577` 可能是 `(Q_IDX << 20) | flags`"
- 问题: 后续分析确认 0xa00577/0x1600b71 是调试标签 (debug tag)，不是 ring/queue 参数。`win_re_full_txd_dma_path.md` 明确: "Parameter 0x1600b71 or 0xa00577 is just a debug tag, NOT size"。
- 参考: `win_re_full_txd_dma_path.md` Section 3

### 问题 3: "切换到 nicUniCmdChReqPrivilege 替换 ROC API" — 推测性建议
- 位置: Section "建议的修复方向"
- 内容: "最高优先级: 切换到 nicUniCmdChReqPrivilege — 替换 ROC API"
- 问题: nicUniCmdChReqPrivilege 和 ROC acquire 走的是同一个 CH_PRIVILEGE CID=0x27，是同一功能的不同封装。替换 API 不能解决问题。
- 参考: `win_re_cid_mapping.md` entry [12]

---

## win_re_txd_dw0_dw1_precise.md

### 问题 1: DW2/DW6 值完全错误
- 位置: 被 `win_re_dw2_dw6_verified.md` 引用对比
- 内容: "DW2 = 0x00000000 (unless forced)", "DW6 = 0x00000000 (Windows DW6=0)"
- 问题: Auth 帧永远使用 fixed rate (MlmeHardTransmit 中 local_7c=1 无条件设置)。正确值: DW2=0xA000000B, DW6=0x004B0000。
- 参考: `win_re_dw2_dw6_verified.md` "Doc2 error" section

### 问题 2: DW5=0 结论错误
- 位置: 同上
- 内容: "DW5 = 0x00000000"
- 问题: Auth 帧的 DW5 = 0x600 | PID。这是因为 param_3+0x0f (seq_num) 永远非零，且 FRAME_TYPE=0 + SUB_TYPE=0xB 满足 else-if 条件。
- 参考: `win_re_dw2_dw6_verified.md` Section 3

---

## win_re_full_txd_dma_path.md

### 问题 1: DW5=0 结论错误
- 位置: Section 4 "TXD Construction" / Section 10 对比表
- 内容: DW5 列为 0x00000000
- 问题: 同上，正确值为 0x600 | PID。这份文档的 DW2 和 DW6 是正确的，但 DW5 错了。
- 参考: `win_re_dw2_dw6_verified.md` "Doc1 error"

---

## win_re_cid_mapping.md

### 问题 1: 暗示使用 inner CID — 与实践冲突
- 位置: Section 12 "Implications for Driver"
- 内容: "BSS_INFO CID in header: Correct. inner_CID=0x02 goes in the UniCmd header" + "BSS_INFO_PROTECT: Currently sent with outer=0x4c → handler uses inner=0x4a (NOT 0x02). Our driver currently sends this with the wrong inner CID — need to verify."
- 问题: 文档本身正确描述了 dispatch table，但隐含的建议是"应该使用 inner CID"。Session 20 的 bisect 明确证明: 使用 inner CID 会导致 scan 完全失败。当前 mt7925 的 CID 值 (与 outer tag 对应) 才是工作的值。inner CID 只是 dispatch table 内部的路由标签，不等于 UniCmd header 应使用的值。
- 参考: CLAUDE.md "CID 重要教训" section

### 问题 2: Entry 9 标注为 "DEV_INFO" — 不完整
- 位置: Section 2 table, entry [9]
- 内容: `0x11 → 0x01 → DEV_INFO ✓`
- 问题: 后续 `win_re_connect_flow_complete.md` 发现 entry 9 (0x140143540) 实际是 `nicUniCmdBssActivateCtrl`，是 DEV_INFO + BSS_INFO(BASIC+MLD) 的**组合命令**，不是单纯的 DEV_INFO。
- 参考: `win_re_connect_flow_complete.md` Section 三

### 问题 3: BAND_CONFIG 的两个 CID 值冲突
- 位置: Section 3 confirmed mappings
- 内容: Entry [27]: outer=0x0a, inner=0x08, 标注 "BSS_INFO_HE (or ext type 8)"; Entry [52]: outer=0x93, inner=0x49, 标注 "BAND_CONFIG"
- 问题: 与 `win_re_connect_flow_complete.md` 的重新分析冲突。后者将 entry [27] 标注为 "BAND_CONFIG"。两份文档对同一 entry 给出不同命名。
- 参考: 对比 `win_re_cid_mapping.md` entry 27 vs `win_re_connect_flow_complete.md` entry 27

---

## win_re_bss_info_tlv_dispatch.md

### 问题 1: CID=0x26 标注错误
- 位置: 文件标题和 Section 1
- 内容: "BSS_INFO (CID=0x26) 命令的 14 个 TLV handler"
- 问题: BSS_INFO 的 inner CID 是 0x02，不是 0x26。0x26 不出现在 dispatch table 中。可能是与 MT6639 Android 的枚举值混淆。
- 参考: `win_re_cid_mapping.md` 明确 BSS_INFO inner CID = 0x02

### 问题 2: nicUniCmdSetBssInfo 地址与其他文档不一致
- 位置: Section 1
- 内容: "nicUniCmdSetBssInfo: VA=0x14013e320"
- 问题: `win_re_bss_info_tlvs.md` 和 `win_re_bss_info_all_tlvs.md` 将此函数定位在 0x1401444a0。两者不一致 — 前者可能分析了 v5603998 版本而非 v5705275。
- 参考: `win_re_bss_info_all_tlvs.md` 明确 "nicUniCmdSetBssInfo (0x1401444a0)"

### 问题 3: Dispatch Table 地址与其他文档冲突
- 位置: Section 1
- 内容: "Dispatch Table: VA=0x14023fac8"
- 问题: `win_re_bss_info_all_tlvs.md` 和 `win_re_full_connect_cmd_sequence.md` 使用 0x1402505b0。两份文档分析不同版本的二进制 (v5603998 vs v5705275)，但未明确标注这一差异。
- 参考: `win_re_bss_info_all_tlvs.md` Section 1

### 问题 4: TLV tag 编号与后续分析不一致
- 位置: Section 2 mapping table
- 内容: RATE=tag 0x000b, SEC=tag 0x0010, QBSS=tag 0x000f, SAP=tag 0x000d
- 问题: 这些 tag 值与 `win_re_bss_info_all_tlvs.md` 的分析一致，但与 `win_re_conn_type_verify.md` 中 "Finding 3: BSS_INFO TLV Tags may be Wrong" 的说法冲突。后者声称 Windows 使用的 tag 与 MT6639 Android 不同，但实际上两份文档的 tag 值是一致的。
- 参考: `win_re_conn_type_verify.md` Finding 3

---

## win_re_wtbl_starec_analysis.md

### 问题 1: "conn_type=0x10002 可能是关键差异" — 推测性结论被当作高优先级
- 位置: Section "WTBL[1] DW0=0 的可能原因", "🔴 高优先级"
- 内容: "Windows 发 0x10001 (INFRA_STA)，我们发 0x10002 (INFRA_AP)...建议: 改为 0x10001 测试"
- 问题: 这里混淆了两个不同的 conn_type 概念。STA_REC 的 conn_type 不是 BSS_INFO packed_field。`win_re_conn_type_verify.md` 后续确认 BSS_INFO packed_field 是硬编码 0x00080015。此外，STA_REC conn_type 的 0x10001 vs 0x10002 区别在 `win_re_sta_rec_all_tlvs.md` 中被明确: 0x10001 是 INFRA STA (客户端连接 AP)，与我们的使用场景一致。声称改 conn_type 能解决 WTBL 问题是推测。
- 参考: `win_re_conn_type_verify.md`, `win_re_sta_rec_all_tlvs.md` STA_REC_BASIC

### 问题 2: "BSS Activate 从未调用 — 极可能导致 TX 失败" — 过度推测
- 位置: Section "总结对比表"
- 内容: "BSS Activate ✅ 调用 / ❌ 从未调用 / 极可能导致 TX 失败"
- 问题: 我们的驱动在 add_interface 中确实发送了 DEV_INFO + BSS_INFO。BssActivateCtrl 只是 Windows 的组合封装。实际缺失的可能是**重发时序**，而不是命令本身。
- 参考: `win_re_connect_flow_complete.md` Section 三

---

## win_re_conn_type_verify.md

### 问题 1: "Finding 1: BSS_INFO_BASIC TLV format is WRONG" — 混淆 dispatch handler 与 wire format
- 位置: "Critical Findings" Section
- 内容: "Windows firmware expects a compact 12-byte payload...Our driver sends at MT6639 struct's conn_type offset: 0x00010001...This is a payload format mismatch."
- 问题: 此分析的 handler 0x140144db0 是 CID dispatch table 中 outer_tag=0x05 的 handler，它处理的是 nicUniCmdAllocEntry 中间步骤的 TLV 构建。实际发给固件的 BSS_INFO_BASIC TLV 是通过 nicUniCmdBssInfoTagBasic (0x14014c610) 构建的，使用了完全不同的结构 (32 字节)。0x140144db0 只是 dispatch 入口，不是 wire format 生成器。`win_re_bss_info_all_tlvs.md` 中 Entry[0] BASIC 的 alloc size 是 0x20 (32 bytes)。
- 参考: `win_re_bss_info_all_tlvs.md` Entry[0], `win_re_bss_info_tlvs.md` nicUniCmdBssInfoTagBasic

### 问题 2: "Finding 2: BSS_INFO TLV Tags may be Wrong" — 错误推论
- 位置: "Finding 2" section
- 内容: "Windows firmware uses different tag numbers than MT6639 Android source"，列出 tag=0x05 (BASIC), 0x17 (RLM), 0x12 (PROTECT) 等
- 问题: 这里列出的 tag 值是 dispatch table 的 **outer_tag**，不是 TLV 的 tag 字段。实际 BSS_INFO TLV tag 在 `win_re_bss_info_all_tlvs.md` 中由 handler 内部设置: BASIC tag=0x0000, RLM tag=0x0002, PROTECT tag=0x0003 等。这些与 MT6639 Android 的 TLV tag 是兼容的。
- 参考: `win_re_bss_info_all_tlvs.md` Section 4

---

## win_re_mpdu_config_analysis.md

### 问题 1: "P0: RATE TLV missing" — 推测为根因但未证实
- 位置: Executive Summary
- 内容: "P0: RATE TLV missing u2OperationalRateSet → Firmware may lack valid rate table"
- 问题: 推测性结论。后续 Session 21 测试中，即使添加了 RATE TLV，auth TX 仍然失败 (Session 20 "Test 5-7 BASIC TLV 字段逐个测试均无改善")。缺少 RATE TLV 不是 MPDU_ERR 的根因。
- 参考: CLAUDE.md "Test 5-7" notes

### 问题 2: 结构体对比中的 __rsv1 "Bug" — 实际无影响
- 位置: Section 2
- 内容: "Offset 4-5 (__rsv1[2]) is u2OperationalRateSet — it's always 0! BUG"
- 问题: 即使 OperationalRateSet 为 0，auth 帧仍然以 OFDM 6Mbps 发送 (TXD 中已固定速率)。RATE TLV 中的 operational rates 不影响 fixed-rate 管理帧的发送。这不是 "Bug"。
- 参考: Ring 0 测试中帧确实发射了 30 次 (MIB TX20=30)

---

## win_re_scan_domain_band_payloads.md

### 问题 1: Payload 大小计算错误 — 已被后续文档更正
- 位置: "快速参考" table
- 内容: SET_DOMAIN payload=0x34 bytes, BAND_CONFIG payload=0x44 bytes (alloc - 0x18 header)
- 问题: `win_re_payload_formats_detailed.md` 开头明确更正: "之前分析错误地将 alloc_size - 0x18 作为 firmware payload 大小。正确: SET_DOMAIN size_param=0x4c=76 bytes, BAND_CONFIG size_param=0x5c=92 bytes"。正确的 payload 大小是 `size_param` 参数本身。
- 参考: `win_re_payload_formats_detailed.md` "关键更正" section

---

## win_re_sta_rec_tlvs.md

### 问题 1: "BSS Activate 从未调用 — 极可能导致 TX 失败" — 重复的推测性结论
- 位置: Section 11 "nicUniCmdBssActivateCtrl"
- 内容: "我们目前没有调用此函数。这可能是 auth 帧失败的根本原因之一"
- 问题: 同 win_re_wtbl_starec_analysis.md 问题 2。我们发送了独立的 DEV_INFO + BSS_INFO，功能等价。推测这是 "根本原因" 是不准确的。
- 参考: `win_re_connect_flow_complete.md`

### 问题 2: STA_REC TLV 数量 "缺 8 个" — 数字可能误导
- 位置: Section "总结对比表"
- 内容: "TLV 总数: 13 个 vs 5 个 = 缺 8 个"
- 问题: 缺少 TLV 不等于需要全部添加。HE_6G_CAP、EHT_INFO、EHT_MLD、MLD_SETUP、T2LM 是 WiFi 7 特性，auth 帧不需要。实际关键缺失可能只有 HT_INFO 和 VHT_INFO (如果 AP 要求)。但即使全部添加，auth 仍失败 (Sessions 15-23)。
- 参考: CLAUDE.md "已尝试的方法" MCU 命令部分

---

## txd_starec_analysis.md

### 问题 1: "conn_state=DISCONNECT 是致命 Bug" — 后续测试未证实
- 位置: Section 3 "Bug #1 (严重度: 致命)"
- 内容: "conn_state = DISCONNECT 而非 PORT_SECURE...固件因为目标 STA 处于 DISCONNECT 状态，拒绝发射帧 → TXFREE stat=1!"
- 问题: 这个分析基于 mt7925 参考代码推导，不是从 Windows RE 确认。`win_re_conn_type_verify.md` 确认 Windows 发送 conn_state=1 (CONNECTED)。我们已修改为 conn_state=1，但 auth 仍然失败 (F8: conn_state=0 vs 1 无差异)。
- 参考: `win_re_conn_type_verify.md` Finding 3, MEMORY.md F8

### 问题 2: DW6=0x000f001c 解码中 TX_RATE 位域错误
- 位置: Section 1 "DW6: 0x000f001c"
- 内容: "TX_RATE bits[21:16] = 15 = 速率表索引15=OFDM 6Mbps"
- 问题: 这是旧版 TXD，后续 `win_re_dw2_dw6_verified.md` 确认正确的 DW6=0x004B0000。TX_RATE 编码为 0x4B = OFDM mode 1 + MCS 0x0B = 6Mbps。0x000f001c 中的 bits[21:16]=0xf 不是正确的 CONNAC3 rate 编码。
- 参考: `win_re_dw2_dw6_verified.md` Section 4

---

## win_tx_path_analysis.md

### 问题 1: "Windows RE 文档仅覆盖初始化阶段" — 在撰写时正确，后续已过时
- 位置: Section 一 "核心结论"
- 内容: "现有 Windows 逆向文档仅覆盖了初始化阶段...连接流程从未被逆向分析"
- 问题: 这在 Session 15 写作时正确，但随后 Session 17-24 大量补充了连接流程逆向 (win_re_connection_flow.md, win_re_full_txd_dma_path.md 等)。文档本身未更新以反映这些进展。
- 参考: 后续所有 win_re_*.md 文档

### 问题 2: Ring 布局表错误
- 位置: Section "Windows TX Ring 布局"
- 内容: Ring 0=数据/管理(AC0/BE), Ring 1=AC1/BK, Ring 2=AC2/VI, Ring 3=AC3/VO
- 问题: 完全错误。Windows 实际布局: Ring 0=数据, Ring 1=数据, **Ring 2=管理帧(SF mode)**, Ring 15=MCU, Ring 16=MCU。没有 Ring 3=VO。
- 参考: `win_re_ring2_analysis.md` Section 6

---

## win_auth_flow_analysis.md

### 问题 1: "conn_state=DISCONNECT" 被标为根因 — 同 txd_starec_analysis.md
- 位置: Section 三 逐项差异对比
- 内容: 标注 "致命 Bug: conn_state = DISCONNECT"
- 问题: 同上。已改为 conn_state=1 但未解决问题。
- 参考: F8

### 问题 2: MT6639 conn_type 推荐值不可靠
- 位置: Section 二
- 内容: "MT6639 Android: conn_type=CONNECTION_INFRA_AP(0x10002)"
- 问题: `win_re_sta_rec_all_tlvs.md` 明确 Windows STA_REC conn_type 对于客户端连接应为 0x10001 (INFRA STA)。MT6639 Android 使用 0x10002 是因为在它的枚举中 AP 和 STA 的编码与 Windows 不同。
- 参考: `win_re_sta_rec_all_tlvs.md` STA_REC_BASIC conn_type mapping

---

## win_re_connect_flow.md (Session 24)

### 问题 1: "STATE_INFO 在 auth 前 wire state=0 (DISCONNECT)" — 推测
- 位置: Section 一 Executive Summary 表格
- 内容: "STATE_INFO 在 auth 前是多少? wire state=0 (DISCONNECT)"
- 问题: 这与 `win_re_conn_type_verify.md` 确认的 conn_state=1 (CONNECTED) 冲突。两份文档对 Windows 在 auth 前发送的 STA_REC state 值给出不同结论。conn_type_verify 是汇编级验证，更可信。
- 参考: `win_re_conn_type_verify.md` Task 2

### 问题 2: "BSS_INFO 14 TLV 发送时序错误" — 需要更多验证
- 位置: Section 一
- 内容: "我们在 auth 前发送了完整 14-TLV BSS_INFO...这是导致固件行为异常的直接原因"
- 问题: 声称 14-TLV BSS_INFO 是 "直接原因" 是推测。Windows 也在 MtCmdSetBssInfo 中发送 14 TLV BSS_INFO (win_re_full_connect_cmd_sequence.md Step 4)，只是时序在 JoinProc 之前而非之后。
- 参考: `win_re_full_connect_cmd_sequence.md`

---

## codex_rx_path_analysis.md

### 问题 1: PLE STATION_PAUSE 分析正确但因果推导有误
- 位置: Section 一
- 内容: "Windows 从不写 PLE_STATION_PAUSE0...PLE pause 是固件响应 BSS_INFO/STA_REC MCU 命令的结果"
- 问题: 寄存器访问分析本身正确。但隐含的因果推导 "BSS_INFO/STA_REC 命令错误 → PLE pause" 是推测，未排除其他可能原因 (如 DMASHDL 配置错误，已在 Session 23 部分修复)。
- 参考: MEMORY.md Session 23 DMASHDL 关键发现

---

## win_re_hif_ctrl_investigation.md

### 问题 1: "0xd6060 |= 0x10101 未确认" — 已修复但文档未更新
- 位置: Section 1.2 差异分析
- 内容: "我们的代码中未搜索到对 0xd6060 的写入。这可能是关键缺失。"
- 问题: 这在 Session 17 撰写时正确，但 Session 20 已添加此操作到 mt7927_post_fw_init()。文档未更新。
- 参考: `win_re_ring2_analysis.md` Section 7.3 确认已实现

### 问题 2: "AsicConnac3xDownloadBufferBin 可能是可选的"
- 位置: Section 1.2
- 内容: "仅当 flag==1 时执行, 可能是可选的"
- 问题: 正确的推测，但未后续验证。此额外二进制下载步骤至今未实现，可能是一个遗漏。

---

## win_re_full_connect_cmd_sequence.md

### 问题 1: "BSS_INFO 不在 MlmeCntlWaitJoinProc 中发送" — 与后续分析冲突
- 位置: Section 1 关键发现 #6
- 内容: "BSS_INFO 不在 MlmeCntlWaitJoinProc 中发送 — 可能在 MlmeCntlOidConnectProc 或通过 BssActivateCtrl"
- 问题: `win_re_connect_flow.md` (Session 24) 明确确认 BSS_INFO full (14 TLV) 在 MlmeCntlOidConnectProc 中通过 MtCmdSetBssInfo 发送 (Step 4)。这不是 "可能"，而是确认的事实。

### 问题 2: STA_REC inner CID 标注不一致
- 位置: Section 2.2 和 3.1
- 内容: "Legacy 0x13 → UniCmd CID=3 (STA_REC)"
- 问题: `win_re_cid_mapping.md` 中 STA_REC 的 inner CID 是 0x25，不是 0x03。0x03 是 SET_DOMAIN / WFDMA_CFG 的 inner CID。这里混淆了 dispatch entry 索引和 inner CID。Legacy tag 0x13 对应 dispatch entry [22]，但 entry [22] 的 inner CID 可能与 wire CID 不同。
- 参考: `win_re_cid_mapping.md` Section 10

---

## win_re_connect_flow_complete.md

### 问题 1: dispatch table entry 命名与 win_re_cid_mapping.md 冲突
- 位置: Section 2.3 完整条目列表
- 内容: Entry [9] = "BssActivateCtrl", Entry [13] = "BSS_INFO_HE_SUB", Entry [15] = "BSS_INFO (full)"
- 问题: `win_re_cid_mapping.md` 对同样的 entries 给出不同名称: Entry [9] = "DEV_INFO", Entry [13] = "BSS_INFO_BASIC", Entry [15] = "BSS_INFO_RATE"。两份文档的 handler 地址都是正确的，但功能命名互相矛盾。
- 参考: `win_re_cid_mapping.md` Section 2

---

## win_re_ring2_analysis.md

### 问题 1: "Ring 2 不是固件崩溃的根因" — 与实际现象不一致
- 位置: Executive Summary
- 内容: "Ring 2 不是固件崩溃的根因...当前问题是 Ring 0 CT mode 发送 auth 帧产生 MPDU_ERR (status=3, count=30)，PHY 确实发射了 30 次但 AP 不回 ACK"
- 问题: 这份文档是 Session 21 的分析结论。但 Session 22-23 回到 Ring 2 后，固件直接崩溃 (PLE 耗尽)，而不是 MPDU_ERR。Ring 2 的固件崩溃可能有独立原因。文档的 "Ring 2 DMA 层: 基本正确" 结论与固件崩溃的事实矛盾。
- 参考: MEMORY.md "Ring 2 SF TX: DMA 消费成功 → 500ms 后 PLE_EMPTY=0x00000000 → Ring 15 挂死"

---

## win_re_payload_formats_detailed.md

整体可信。明确更正了 `win_re_scan_domain_band_payloads.md` 的错误。无重大问题。

---

## win_re_dw2_dw6_verified.md

整体可信。汇编级验证，是 TXD 字段最权威的参考。明确指出了 Doc1 和 Doc2 的错误。无问题。

---

## win_re_bss_info_all_tlvs.md (Session 24)

整体可信。直接反汇编分析，给出了所有 14 个 TLV 的精确结构。与 `win_re_bss_info_tlv_dispatch.md` 中的 tag 值一致。

---

## win_re_sta_rec_all_tlvs.md (Session 24)

整体可信。直接反汇编分析所有 13 个 STA_REC TLV。conn_type=0x10001 (INFRA STA) 的映射与 `win_re_conn_type_verify.md` 一致。

---

## win_re_wfdma_glo_cfg.md

整体可信。Packed prefetch 值已被多份文档交叉确认。

---

## win_re_efuse_ctrl_analysis.md

整体可信。EFUSE_CTRL 已在 Session 20 恢复并确认工作。

---

## win_re_class02_and_postinit.md

整体可信。CLASS_02 command (CID=0x0b, payload={1,0,0x70000}) 的分析与 CLAUDE.md 一致。

---

## references/ghidra_post_fw_init.md

### 问题 1: 基于 v5603998 版本，地址与 v5705275 不同
- 位置: 标题
- 内容: 所有地址基于 v5603998 二进制
- 问题: 后续大部分分析基于 v5705275。函数地址可能不同 (如 AsicConnac3xPostFwDownloadInit = 0x1401c9510 在两个版本中可能不同)。但功能映射应该一致。使用此文档时需注意版本差异。
- 参考: 所有 win_re_* 文档使用 v5705275

---

## txp_format_research_session24.md

整体正确标注了 "UNVERIFIED" 状态。诚实地说明了缺乏 Windows RE 验证。

---

## 三、文档间主要冲突总结

### 冲突 1: Ring 选择
| 文档 | 声称 |
|------|------|
| `win_re_tx_mgmt_path.md` | Ring 3 |
| `win_tx_path_analysis.md` | Ring 0 或 Ring 15 |
| `win_re_full_txd_dma_path.md` | **Ring 2** (正确) |
| `win_re_ring2_analysis.md` | **Ring 2** (正确) |

**结论**: Ring 2 已被汇编级确认 (N6PciTxSendPkt param_3=2)。

### 冲突 2: TXD DW2 值
| 文档 | DW2 值 |
|------|--------|
| `win_re_txd_dw0_dw1_precise.md` | 0x00000000 |
| `win_re_full_txd_dma_path.md` | 0xA000000B (正确) |
| `win_re_dw2_dw6_verified.md` | **0xA000000B** (汇编验证) |

**结论**: 0xA000000B 正确。

### 冲突 3: TXD DW5 值
| 文档 | DW5 值 |
|------|--------|
| `win_re_txd_dw0_dw1_precise.md` | 0x00000000 |
| `win_re_full_txd_dma_path.md` | 0x00000000 |
| `win_re_dw2_dw6_verified.md` | **0x600 | PID** (汇编验证) |

**结论**: 0x600|PID 正确。两份早期文档都错了。

### 冲突 4: STA_REC conn_state 在 auth 前的值
| 文档 | 声称 |
|------|------|
| `win_re_connect_flow.md` (codex) | state=0 (DISCONNECT) |
| `win_re_conn_type_verify.md` | state=1 (CONNECTED) |

**结论**: conn_type_verify 是汇编级验证，更可信。但两者分析的函数层级不同 — 一个看 MtCmdSendStaRecUpdate 的输入构建，一个看 dispatch handler 的输出。需要进一步澄清。

### 冲突 5: BSS_INFO_BASIC payload 格式
| 文档 | 声称 |
|------|------|
| `win_re_conn_type_verify.md` | 12 字节紧凑 payload，packed=0x00080015 |
| `win_re_bss_info_all_tlvs.md` | 32 字节 (alloc 0x20)，含 conn_type 等 |
| `win_re_bss_info_tlvs.md` | 32 字节 (nicUniCmdBssInfoTagBasic) |

**结论**: conn_type_verify 分析的是 dispatch table handler (入口函数)，而 bss_info_tlvs/all_tlvs 分析的是 TLV builder 函数。两者是不同层级的函数。实际发给固件的 TLV 由 builder 生成 (32 bytes)。dispatch handler 负责 outer→inner 路由和初始分配。两者都正确但描述不同层级。

### 冲突 6: nicUniCmdSetBssInfo 函数地址
| 文档 | 地址 |
|------|------|
| `win_re_bss_info_tlv_dispatch.md` | 0x14013e320 (v5603998?) |
| `win_re_bss_info_all_tlvs.md` | 0x1401444a0 (v5705275) |
| `win_re_bss_info_tlvs.md` | 0x1401444a0 (v5705275, 但是这是不同的函数) |

**结论**: 来自不同二进制版本。v5705275 的地址可信。

### 冲突 7: CID dispatch entry 命名
| 文档 | Entry [9] (outer=0x11) | Entry [15] (outer=0x12) |
|------|------------------------|-------------------------|
| `win_re_cid_mapping.md` | DEV_INFO | BSS_INFO_RATE |
| `win_re_connect_flow_complete.md` | BssActivateCtrl | BSS_INFO (full) |

**结论**: connect_flow_complete 更深入分析了 handler 功能，其命名更准确。cid_mapping 的命名基于浅层推测。

---

## 四、整体可信度评级

### 高可信度 (可直接参考)
- `win_re_dw2_dw6_verified.md` — 汇编级验证，TXD DW2/5/6/7 最权威
- `win_re_bss_info_all_tlvs.md` — Session 24 直接反汇编，BSS_INFO 结构最完整
- `win_re_sta_rec_all_tlvs.md` — Session 24 直接反汇编，STA_REC 结构最完整
- `win_re_ring2_analysis.md` — Ring 2 操作的全面综合分析 (但 "Ring 2 不是根因" 结论需更新)
- `win_re_payload_formats_detailed.md` — 汇编级验证 SET_DOMAIN/BAND_CONFIG payload
- `win_re_connect_flow_complete.md` — BssActivateCtrl 深度分析
- `win_re_wfdma_glo_cfg.md` — 寄存器配置直接 RE
- `win_re_dma_descriptor_format.md` — DMA 描述符汇编级验证
- `references/ghidra_post_fw_init.md` — PostFwDownloadInit 权威分析 (注意 v5603998 版本)

### 中可信度 (需结合其他文档使用)
- `win_re_cid_mapping.md` — dispatch table 数据正确，但命名和 "Implications" 推测部分需谨慎
- `win_re_full_txd_dma_path.md` — 整体框架正确 (Ring 2 确认)，但 DW5=0 错误
- `win_re_conn_type_verify.md` — 汇编分析本身正确，但 "Findings" 推导有误 (混淆层级)
- `win_re_connection_flow.md` — 流程框架正确，但细节推测多 (0xa00577, Ring 3)
- `win_re_connect_flow.md` — PM_DISABLE 发现有价值，但 STATE_INFO=0 结论与其他文档冲突
- `win_re_hif_ctrl_investigation.md` — HIF_CTRL 分析正确，但 "缺失步骤" 列表已部分过时
- `win_re_full_connect_cmd_sequence.md` — 命令序列框架正确，STA_REC CID 标注有误
- `win_re_bss_info_tlvs.md` — Ghidra 反编译原始输出，可作为参考但需人工解读
- `win_re_sta_rec_tlvs.md` — 同上
- `win_re_wtbl_starec_analysis.md` — conn_type 分析有价值，但推测性结论过多
- `txp_format_research_session24.md` — 诚实标注 "UNVERIFIED"，可参考

### 低可信度 (大量结论已证伪或过时)
- `win_re_txd_dw0_dw1_precise.md` — DW2/5/6 三个值都错误
- `win_re_tx_mgmt_path.md` — Ring 3 结论完全错误，TXD 推测大量错误
- `win_tx_path_analysis.md` — Ring 布局表完全错误，基于过时信息
- `win_auth_flow_analysis.md` — "致命 Bug" conn_state 结论已证伪
- `txd_starec_analysis.md` — DW6 值错误，conn_state "致命 Bug" 已证伪
- `win_re_mpdu_config_analysis.md` — P0 优先级结论未被验证证实
- `win_re_bss_info_tlv_dispatch.md` — 基于旧版本二进制，CID=0x26 标注错误
- `win_re_scan_domain_band_payloads.md` — payload 大小计算错误 (已被更正)

### 建议废弃 (内容已被更好的文档完全替代)
- `win_re_txd_dw0_dw1_precise.md` → 被 `win_re_dw2_dw6_verified.md` 替代
- `win_re_tx_mgmt_path.md` → 被 `win_re_full_txd_dma_path.md` + `win_re_ring2_analysis.md` 替代
- `win_tx_path_analysis.md` → 被 `win_re_full_txd_dma_path.md` 替代
- `win_re_scan_domain_band_payloads.md` → 被 `win_re_payload_formats_detailed.md` 替代
- `win_re_bss_info_tlv_dispatch.md` → 被 `win_re_bss_info_all_tlvs.md` 替代

---

## 五、关键教训

1. **推测性结论被反复当作 "致命 Bug" 或 "P0 优先级"**。至少 5 份文档将未验证的推测标记为最高优先级修复项，但实际测试后全部未能解决问题。今后应明确区分 "汇编验证的事实" 和 "推测性假设"。

2. **同一字段在不同文档中有不同值但未交叉标注**。例如 DW5 在 3 份文档中有 2 个不同值，直到第 4 份文档才明确指出前面的错误。建议每个文档开头列出 "本文更正/推翻的前文结论"。

3. **两个二进制版本 (v5603998 vs v5705275) 的分析混在一起**，未清晰标注。这导致函数地址和 dispatch table 地址冲突。

4. **CID / outer_tag / inner_CID / TLV tag 四个概念经常混淆**。dispatch table 的 outer_tag != UniCmd header 中使用的 CID != TLV payload 中的 tag 字段。多份文档在这些概念间混用，导致错误推导。

5. **"缺少 X 命令/TLV 是根因" 的推测模式**反复出现但从未成功。RATE TLV、PROTECT TLV、BSS Activate、conn_type、conn_state、STA_REC TLV 数量 — 这些都被标记为 "极可能根因" 但全部未能解决 auth TX。真正的根因可能在更基础的层面 (DMASHDL 配置、PLE 初始化、Ring 2 EXT_CTRL 等)。

---

*报告生成于 2026-02-22，基于对 60+ 文档的交叉审计。*
