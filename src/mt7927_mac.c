// SPDX-License-Identifier: GPL-2.0
/*
 * MT7927 (MT6639) MAC layer - TXD/RXD processing
 * Copyright (C) 2026
 *
 * CONNAC3 TXD format for data frames and RXD parsing.
 * Based on mt76/mt7925/mac.c, simplified for standalone driver.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>

#include "mt7927_pci.h"

/* ============================================================================
 * Helper functions
 * ============================================================================ */

/*
 * Map IEEE80211 AC to LMAC queue index.
 * Same mapping as mt76_connac_lmac_mapping().
 */
static u8 mt7927_lmac_mapping(u8 ac)
{
	static const u8 map[] = {
		[IEEE80211_AC_BK] = 0,
		[IEEE80211_AC_BE] = 1,
		[IEEE80211_AC_VI] = 2,
		[IEEE80211_AC_VO] = 3,
	};

	if (ac >= ARRAY_SIZE(map))
		return 1; /* default to BE */
	return map[ac];
}

/*
 * Convert RCPI (0-255) to RSSI in dBm.
 * RCPI = 2 * (dBm + 110), so dBm = (RCPI - 220) / 2.
 */
static inline s8 mt7927_rcpi_to_rssi(u8 rcpi)
{
	if (!rcpi)
		return -128;
	return ((int)rcpi - 220) / 2;
}

/* ============================================================================
 * TXS 解码辅助函数 — 解析 CONNAC3 TXS 错误位
 * ============================================================================ */

/*
 * 解码一条 TXS 记录并打印关键字段:
 *   ack_to   = BIT(16): 帧已发出但未收到 ACK (帧到达空中但 AP 未响应)
 *   rts_to   = BIT(17): RTS 超时 (帧可能未离开芯片)
 *   q_to     = BIT(18): 队列超时 (帧从未发出)
 *   rate     = [13:0]:  实际使用的发送速率
 *   band/wcid/pid/ts: 用于关联到具体帧
 */
static void mt7927_mac_dump_txs(struct mt7927_dev *dev, const __le32 *txs_data,
				const char *src)
{
	u32 txs0 = le32_to_cpu(txs_data[0]);
	u32 txs2 = le32_to_cpu(txs_data[2]);
	u32 txs3 = le32_to_cpu(txs_data[3]);
	u32 txs4 = le32_to_cpu(txs_data[4]);
	u8  ack_err = FIELD_GET(MT_TXS0_ACK_ERROR_MASK, txs0);
	u16 tx_rate = FIELD_GET(MT_TXS0_TX_RATE, txs0);
	u8  band    = FIELD_GET(MT_TXS2_BAND, txs2);
	u16 wcid    = FIELD_GET(MT_TXS2_WCID, txs2);
	u8  pid     = FIELD_GET(MT_TXS3_PID, txs3);
	u32 ts      = FIELD_GET(MT_TXS4_TIMESTAMP, txs4);

	/* 按 CONNAC3 TXS 格式解码关键错误位和上下文 */
	dev_info(&dev->pdev->dev,
		 "%s: ack_err=0x%x ack_to=%u rts_to=%u q_to=%u rate=0x%04x band=%u wcid=%u pid=%u ts=0x%08x\n",
		 src, ack_err,
		 !!(txs0 & MT_TXS0_ACK_TIMEOUT),
		 !!(txs0 & MT_TXS0_RTS_TIMEOUT),
		 !!(txs0 & MT_TXS0_QUEUE_TIMEOUT),
		 tx_rate, band, wcid, pid, ts);
}

/* ============================================================================
 * TX path: TXD construction (CONNAC3 format, 8 DWORDs = 32 bytes)
 * ============================================================================ */

/*
 * Fill TXD fields for 802.3 (hardware encapsulated) frames.
 * Called when IEEE80211_TX_CTL_HW_80211_ENCAP is set.
 */
static void mt7927_mac_write_txwi_8023(struct mt7927_dev *dev, __le32 *txwi,
					struct sk_buff *skb,
					struct mt7927_wcid *wcid)
{
	u8 tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
	u32 val;

	/* TXD[1]: TID + HDR_FORMAT = 802.3 */
	val = FIELD_PREP(MT_TXD1_TID, tid) |
	      FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_802_3);

	if (skb->protocol >= cpu_to_be16(ETH_P_802_3_MIN))
		val |= MT_TXD1_ETH_802_3;

	txwi[1] |= cpu_to_le32(val);

	/* TXD[2]: frame type = data */
	val = FIELD_PREP(MT_TXD2_FRAME_TYPE, IEEE80211_FTYPE_DATA >> 2) |
	      FIELD_PREP(MT_TXD2_SUB_TYPE, 0);
	txwi[2] |= cpu_to_le32(val);
}

/*
 * Fill TXD fields for native 802.11 frames (management, non-encap data).
 */
static void mt7927_mac_write_txwi_80211(struct mt7927_dev *dev, __le32 *txwi,
					 struct sk_buff *skb,
					 struct ieee80211_key_conf *key)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	__le16 fc = hdr->frame_control;
	u8 fc_type = (le16_to_cpu(fc) & IEEE80211_FCTL_FTYPE) >> 2;
	u8 fc_stype = (le16_to_cpu(fc) & IEEE80211_FCTL_STYPE) >> 4;
	u8 mac_hdr_len = ieee80211_get_hdrlen_from_skb(skb);
	u8 tid = 0;
	u32 val;

	if (ieee80211_is_data_qos(fc))
		tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;

	/* TXD[1]: TID + HDR_FORMAT = 802.11 + header length */
	val = FIELD_PREP(MT_TXD1_TID, tid) |
	      FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_802_11) |
	      FIELD_PREP(MT_TXD1_HDR_INFO, mac_hdr_len / 2);

	/* MT6639: 管理帧设 FIXED_RATE — nicTxSetPktLowestFixedRate()
	 * 之前禁用过 (实验 B), 但现在走 CMD ring 路径后应该启用
	 * FIXED_RATE_IDX=0 由主函数设置, 这里只设 DW1 bit31 */
	if (ieee80211_is_mgmt(fc))
		val |= MT_TXD1_FIXED_RATE;

	txwi[1] |= cpu_to_le32(val);

	/* TXD[2]: frame type/subtype + header padding for encryption */
	val = FIELD_PREP(MT_TXD2_FRAME_TYPE, fc_type) |
	      FIELD_PREP(MT_TXD2_SUB_TYPE, fc_stype);

	if (key && (mac_hdr_len % 4))
		val |= FIELD_PREP(MT_TXD2_HDR_PAD, 2);

	txwi[2] |= cpu_to_le32(val);
}

/*
 * mt7927_mac_write_txwi - Build CONNAC3 TXD (8 DWORDs, 32 bytes)
 *
 * @dev: device
 * @txwi: output TXD buffer (must be at least MT_TXD_SIZE bytes)
 * @skb: socket buffer (payload, without TXD prepended)
 * @wcid: wireless client ID (NULL for broadcast)
 * @key: encryption key (NULL if unencrypted)
 * @pid: packet ID for TX status tracking (0 = no tracking)
 * @beacon: true for beacon frames
 */
void mt7927_mac_write_txwi(struct mt7927_dev *dev, __le32 *txwi,
			    struct sk_buff *skb, struct mt7927_wcid *wcid,
			    struct ieee80211_key_conf *key, int pid,
			    bool beacon)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	bool is_8023 = info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP;
	bool is_mgmt = false;
	u8 p_fmt = MT_TX_TYPE_CT; /* Cut-through: TXD+TXP in coherent, payload separate */
	u8 q_idx;
	u16 wlan_idx = wcid ? wcid->idx : 0;
	u8 omac_idx = 0;
	u8 band_idx = 0;
	u32 val;

	/* 检测管理帧 (auth/assoc/probe 等) — 走完全不同的 TX 路径 */
	if (!is_8023 && skb->len >= 2) {
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

		is_mgmt = ieee80211_is_mgmt(hdr->frame_control);
	}

	/* Get VIF info from WCID if available */
	if (wcid && wcid->sta) {
		struct mt7927_sta *msta;

		msta = container_of(wcid, struct mt7927_sta, wcid);
		if (msta->vif) {
			omac_idx = msta->vif->omac_idx;
			band_idx = msta->vif->band_idx;
		}
	}

	/* ====================================================================
	 * All frames (mgmt + data): use CONNAC3 FIELD_PREP macros
	 *
	 * Session 27: removed is_mgmt raw hex path — it had critical DW1 bugs:
	 *   - WLAN_IDX only 8 bits (& 0xFF), but CONNAC3 uses 12 bits GENMASK(11,0)
	 *     → bits[11:8] got polluted by hdr_info → firmware saw WLAN_IDX=0x801
	 *   - HDR_INFO placed at bits[12:8] instead of GENMASK(20,16)
	 *   - HDR_FORMAT=3 (802.11_EXT) instead of 2 (802.11)
	 *   These caused firmware to look up wrong WTBL entry → silent TX drop
	 *
	 * The data path's mt7927_mac_write_txwi_80211() handles mgmt frames
	 * correctly: FIXED_RATE, frame type/subtype, hdr_format, hdr_info.
	 * ==================================================================== */
	if (beacon) {
		q_idx = MT_TX_MCU_PORT_RX_Q0;
		p_fmt = MT_TX_TYPE_FW;
	} else if (!is_8023) {
		/* S44: 管理帧 → ALTX0, 数据帧 → AC 队列
		 * 之前所有 802.11 帧都用 ALTX0, 导致数据帧不走 WTBL 加密 */
		if (is_mgmt)
			q_idx = 0x10; /* MT_LMAC_ALTX0 */
		else
			q_idx = mt7927_lmac_mapping(skb_get_queue_mapping(skb));
	} else {
		q_idx = mt7927_lmac_mapping(skb_get_queue_mapping(skb));
	}

	memset(txwi, 0, MT_TXD_SIZE);

	/* TXD[0]: total length, packet format, queue index */
	val = FIELD_PREP(MT_TXD0_TX_BYTES, skb->len + MT_TXD_SIZE) |
	      FIELD_PREP(MT_TXD0_PKT_FMT, p_fmt) |
	      FIELD_PREP(MT_TXD0_Q_IDX, q_idx);
	txwi[0] = cpu_to_le32(val);

	/* TXD[1]: WLAN_IDX, OWN_MAC, band */
	val = FIELD_PREP(MT_TXD1_WLAN_IDX, wlan_idx) |
	      FIELD_PREP(MT_TXD1_OWN_MAC, omac_idx) |
	      FIELD_PREP(MT_TXD1_TGID, band_idx);
	txwi[1] = cpu_to_le32(val);

	/* Fill format-specific fields in TXD[1] and TXD[2] */
	if (is_8023)
		mt7927_mac_write_txwi_8023(dev, txwi, skb, wcid);
	else
		mt7927_mac_write_txwi_80211(dev, txwi, skb, key);

	/* TXD[2]: REMAINING_LIFE_TIME */
	if (!is_8023)
		txwi[2] |= cpu_to_le32(FIELD_PREP(MT_TXD2_MAX_TX_TIME, 30));

	/* Note: Windows DW2 has 0xA0000000 (BIT(31)+BIT(29)) in raw hex path,
	 * but in CONNAC3 macros these bits = POWER_OFFSET, NOT FIX_RATE.
	 * Setting them causes firmware to not return TXFREE (tested).
	 * FIXED_RATE is in DW1 BIT(31) in CONNAC3 — already set above. */

	/* TXD[3]: TX control flags */
	val = FIELD_PREP(MT_TXD3_REM_TX_COUNT, 30);
	if (key)
		val |= MT_TXD3_PROTECT_FRAME;
	if (info->flags & IEEE80211_TX_CTL_NO_ACK)
		val |= MT_TXD3_NO_ACK;
	if (is_multicast_ether_addr(skb->data))
		val |= MT_TXD3_BCM;
	txwi[3] = cpu_to_le32(val);

	/* TXD[5]: PID and TX status request.
	 * Windows sets 0x600 (TX_STATUS_HOST | TX_STATUS_MCU) for all mgmt frames.
	 * Without these bits, firmware internally drops the frame (TXFREE stat=1).
	 * For data frames, only set TX_STATUS_HOST when pid != 0 (for TXS tracking). */
	val = FIELD_PREP(MT_TXD5_PID, pid);
	if (is_mgmt) {
		val |= MT_TXD5_TX_STATUS_HOST | MT_TXD5_TX_STATUS_MCU;
	} else if (pid) {
		val |= MT_TXD5_TX_STATUS_HOST;
	}
	txwi[5] = cpu_to_le32(val);

	/* TXD[6]: MSDU count, disable MAT, fixed rate.
	 * MSDU_CNT and DIS_MAT are REQUIRED — without them firmware
	 * silently drops the frame (no TXFREE returned, tested). */
	val = FIELD_PREP(MT_TXD6_MSDU_CNT, 1) |
	      MT_TXD6_DIS_MAT;
	if (is_8023)
		val |= MT_TXD6_DAS;
	txwi[6] = cpu_to_le32(val);

	/* Fixed rate for mgmt and any FIXED_RATE frames */
	if (txwi[1] & cpu_to_le32(MT_TXD1_FIXED_RATE)) {
		/* OFDM 6Mbps: rate_idx=11 for both 2.4GHz and 5GHz.
		 * NOTE: Windows DW6=0x004B0000 includes bit 22 (BW in CONNAC3
		 * macros → 40MHz) — don't set it, causes firmware hang. */
		txwi[6] |= cpu_to_le32(FIELD_PREP(MT_TXD6_TX_RATE, 11));
		txwi[3] |= cpu_to_le32(MT_TXD3_BA_DISABLE);
	}

	/* DW7: TXD_LEN — only extend for encrypted frames (PN in DW8-DW9).
	 * TXD_LEN=0: standard 32-byte TXD, TXP starts at offset 32.
	 * TXD_LEN=1: 48-byte TXD (adds DW8-DW11), TXP at offset 48.
	 * Auth frames are unencrypted → TXD_LEN must be 0, else firmware
	 * misparses TXP offset and cannot find payload DMA address.
	 * (memset already zeroed txwi[7]) */

	/* Debug: dump TXD for all frames (S44 diag) */
	if (is_mgmt || key) {
		print_hex_dump(KERN_INFO, "CT-TXD: ", DUMP_PREFIX_OFFSET,
			       16, 4, txwi, MT_TXD_SIZE, false);
	}
}

/*
 * mt7927_mac_write_txwi_mgmt_sf - Build TXD for management frames in SF mode (Ring 2)
 *
 * SF (Store-and-Forward) mode: TXD (32 bytes) + 802.11 frame are contiguous
 * in a single DMA buffer. No TXP scatter-gather needed.
 *
 * TXD values BINARY-VERIFIED via Ghidra RE of Windows XmitWriteTxDv1
 * (FUN_1401a2ca4 in mtkwecx.sys v5705275).
 * See: docs/win_re_dw2_dw6_verified.md (authoritative, assembly-level proof)
 *
 * Key facts from RE (all verified at assembly level):
 *   - DW0 upper = 0x84000000 (PKT_FT=0/SF, SN_EN=1, Q_IDX=1)
 *   - DW1: HDR_FORMAT=0b11(bits[15:14]), TID=7, bit[25]=1 for 802.11
 *   - DW2 = 0xA000000B (fixed rate flags + frame type/subtype)
 *   - DW5 = 0x600|PID (TX_STATUS_2_HOST + TX_STATUS_FMT + PID)
 *   - DW6 = 0x004B0000 (OFDM 6Mbps)
 *   - DW7 = 0 (fixed-rate path clears bit30, frame type in DW2 not DW7)
 *
 * @dev: device
 * @txwi: output TXD buffer (32 bytes)
 * @skb: the 802.11 management frame
 * @wcid: wireless client ID (may be NULL)
 */
void mt7927_mac_write_txwi_mgmt_sf(struct mt7927_dev *dev, __le32 *txwi,
				    struct sk_buff *skb,
				    struct mt7927_wcid *wcid)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	__le16 fc = hdr->frame_control;
	u8 fc_type = (le16_to_cpu(fc) & IEEE80211_FCTL_FTYPE) >> 2;
	u8 fc_stype = (le16_to_cpu(fc) & IEEE80211_FCTL_STYPE) >> 4;
	u8 mac_hdr_len = ieee80211_get_hdrlen_from_skb(skb);
	u16 wlan_idx = wcid ? wcid->idx : 0;
	u8 omac_idx = 0;
	u8 band_idx = 0;
	u32 val;

	if (wcid && wcid->sta) {
		struct mt7927_sta *msta;

		msta = container_of(wcid, struct mt7927_sta, wcid);
		if (msta->vif) {
			omac_idx = msta->vif->omac_idx;
			band_idx = msta->vif->band_idx;
		}
	}

	memset(txwi, 0, MT_TXD_SIZE);

	/* ====================================================================
	 * TXD fields from deep Ghidra RE of XmitWriteTxDv1 (FUN_1401a2ca4)
	 * Source: docs/win_re_txd_dw0_dw1_precise.md (Sections 5, 10, 12)
	 * ==================================================================== */

	/* DW0: Q_IDX=8, PKT_FMT=0 (SF mode), TX_BYTES = MSDU length
	 * Windows RE: queue_class=0x04, Q_IDX = 0x04 << 1 = 8
	 *   FIELD_PREP(GENMASK(31,25), 8) = 0x10000000
	 * Source: docs/re/win_re_full_txd_dma_path.md Section 4 Step 2
	 *
	 * TX_BYTES = MSDU payload length (NOT including 32-byte TXD!)
	 * Previous bug: used (skb->len + MT_TXD_SIZE) = 62 for 30-byte auth frame,
	 * firmware interpreted as 62 bytes of MSDU after TXD → buffer overread → silent drop.
	 * DMA descriptor SD_LEN0 carries the total DMA buffer size (TXD+payload). */
	txwi[0] = cpu_to_le32(FIELD_PREP(MT_TXD0_Q_IDX, 8) |
			      FIELD_PREP(MT_TXD0_TX_BYTES, skb->len));

	/* DW1: CONNAC3 FIELD_PREP — aligned with unified path (mt7927_mac_write_txwi)
	 * Key fixes vs old raw bits:
	 *   - WLAN_IDX: 12-bit GENMASK(11,0), was 8-bit & 0xFF (truncation bug)
	 *   - TGID (band_idx): GENMASK(13,12), was COMPLETELY MISSING → firmware
	 *     didn't know which band to TX on → WTBL BAND=0 for 5GHz frames
	 *   - HDR_FORMAT: 2 (802.11 native), was 3 (extended)
	 *   - HDR_INFO: GENMASK(20,16) = mac_hdr_len/2, was GENMASK(12,8) raw bytes
	 *   - FIXED_RATE: BIT(31), consistent with CONNAC3 */
	val = FIELD_PREP(MT_TXD1_WLAN_IDX, wlan_idx) |
	      FIELD_PREP(MT_TXD1_OWN_MAC, omac_idx) |
	      FIELD_PREP(MT_TXD1_TGID, band_idx) |
	      FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_802_11) |
	      FIELD_PREP(MT_TXD1_HDR_INFO, mac_hdr_len / 2) |
	      MT_TXD1_FIXED_RATE;
	txwi[1] = cpu_to_le32(val);

	/* DW2: FRAME_TYPE + SUB_TYPE + MAX_TX_TIME
	 *
	 * Windows RE: DW2 |= 0xA0000000 (bit31+bit29) — but in CONNAC3,
	 * those bits = POWER_OFFSET, NOT FIXED_RATE/BM!
	 * FIXED_RATE is DW1 BIT(31) in CONNAC3 (already set above).
	 * Setting 0xA0000000 causes firmware to NOT return TXFREE (tested).
	 *
	 * Align with unified CT path (which works): frame_type + MAX_TX_TIME */
	val = FIELD_PREP(MT_TXD2_FRAME_TYPE, fc_type) |
	      FIELD_PREP(MT_TXD2_SUB_TYPE, fc_stype) |
	      FIELD_PREP(MT_TXD2_MAX_TX_TIME, 30);
	txwi[2] = cpu_to_le32(val);

	/* DW3: REM_TX_COUNT=30 */
	val = FIELD_PREP(MT_TXD3_REM_TX_COUNT, 30);
	if (is_multicast_ether_addr(hdr->addr1))
		val |= MT_TXD3_NO_ACK | MT_TXD3_BCM;
	txwi[3] = cpu_to_le32(val);

	/* DW4: zero (no PN) */

	/* DW5: TX status request with PID
	 *
	 * Verified from Ghidra RE (docs/win_re_dw2_dw6_verified.md):
	 *   XmitWriteTxDv1 lines 131-146:
	 *     For mgmt frames with param_3+0x0f != 0 (always true):
	 *       DW5 |= 0x600  (bit10=TX_STATUS_2_HOST, bit9=TX_STATUS_FMT)
	 *       DW5[7:0] = PID byte (1-99 rotating)
	 *
	 * Without TX_STATUS_2_HOST, firmware does NOT report TX completion!
	 * This was the key missing piece — explains "DMA consumes but FW silent". */
	txwi[5] = cpu_to_le32(0x00000600 | (dev->tx_mgmt_pid & 0xFF));
	dev->tx_mgmt_pid++;
	if (dev->tx_mgmt_pid == 0 || dev->tx_mgmt_pid > 99)
		dev->tx_mgmt_pid = 1;

	/* DW6: OFDM 6Mbps fixed rate + MSDU_CNT + DIS_MAT
	 *
	 * Ghidra RE: fixed-rate block = (param_2[6] & 0x7e00ffff) | 0x4b0000
	 *   The mask 0x7e00ffff PRESERVES bits[0:15], meaning MSDU_CNT/DIS_MAT
	 *   were set BEFORE the fixed-rate block and kept.
	 *
	 * MSDU_CNT=1 (GENMASK(9,4)) and DIS_MAT (BIT(3)) are REQUIRED:
	 *   Without them firmware silently drops the frame (no TXFREE returned).
	 *   This was the Root Cause of Ring 2 SF "DMA consumes but no TXFREE". */
	txwi[6] = cpu_to_le32(0x004B0000 |
			      FIELD_PREP(MT_TXD6_MSDU_CNT, 1) |
			      MT_TXD6_DIS_MAT);

	/* DW7: 0x00000000
	 *
	 * Verified from Ghidra RE:
	 *   Fixed-rate block CLEARS bit30: param_2[7] &= 0xbfffffff
	 *   Frame type/subtype go into DW2 (not DW7) for fixed-rate path.
	 *   DW7 = 0 confirmed. */
	txwi[7] = cpu_to_le32(0);

	/* Debug dump */
	dev_info(&dev->pdev->dev,
		 "TX-SF-TXD: fc=0x%04x len=%u wlan=%u omac=%u band=%u DA=%pM\n",
		 le16_to_cpu(fc), skb->len, wlan_idx, omac_idx, band_idx,
		 hdr->addr1);
	print_hex_dump(KERN_INFO, "SF-TXD: ", DUMP_PREFIX_OFFSET,
		       16, 4, txwi, MT_TXD_SIZE, false);
}

/*
 * mt7927_tx_prepare_skb - CT mode: build TXD+TXP in coherent pool, DMA map payload
 *
 * CT (Cut-Through) mode flow:
 *   1. Allocate a token (simple round-robin, check slot is free)
 *   2. DMA map skb->data (payload — do NOT prepend TXD!)
 *   3. Build TXD (32 bytes) + TXP (32 bytes) in txwi_buf[token]
 *   4. TXP points to payload DMA address via scatter-gather
 *   5. Return token and txwi DMA address to caller
 *
 * 来源: mt7925/pci_mac.c mt7925e_tx_prepare_skb()
 */
int mt7927_tx_prepare_skb(struct mt7927_dev *dev, struct sk_buff *skb,
			  struct mt7927_wcid *wcid, int *token_out,
			  dma_addr_t *txwi_dma_out)
{
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_key_conf *key = info->control.hw_key;
	struct mt7927_hw_txp *txp;
	dma_addr_t payload_dma;
	unsigned long flags;
	__le32 *txwi;
	int token;
	u16 i;

	if (!dev->txwi_buf)
		return -EINVAL;

	/* 1. Allocate token — simple round-robin scan */
	spin_lock_irqsave(&dev->tx_token.lock, flags);
	for (i = 0; i < MT7927_TOKEN_SIZE; i++) {
		token = dev->tx_token.next_id;
		dev->tx_token.next_id = (token + 1) % MT7927_TOKEN_SIZE;
		if (!dev->tx_token.skb[token])
			break;
	}
	if (i == MT7927_TOKEN_SIZE) {
		spin_unlock_irqrestore(&dev->tx_token.lock, flags);
		dev_dbg(&dev->pdev->dev, "TX: token pool exhausted\n");
		return -ENOSPC;
	}
	/* Reserve the slot */
	dev->tx_token.skb[token] = skb;
	spin_unlock_irqrestore(&dev->tx_token.lock, flags);

	/* 诊断: 管理帧 dump 802.11 header (前 30 字节) */
	{
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

		if (ieee80211_is_mgmt(hdr->frame_control)) {
			dev_info(&dev->pdev->dev,
				 "TX mgmt payload: fc=0x%04x len=%u\n",
				 le16_to_cpu(hdr->frame_control), skb->len);
			print_hex_dump(KERN_INFO, "TX-frame: ",
				       DUMP_PREFIX_OFFSET,
				       16, 1, skb->data,
				       min_t(int, skb->len, 30), false);
		}
	}

	/* 2. 诊断实验: 用 coherent buffer 替代 dma_map_single
	 * 如果 stat 变化，说明问题在 DMA 地址映射 */
	if (dev->tx_payload_buf && skb->len <= 2048) {
		memcpy(dev->tx_payload_buf, skb->data, skb->len);
		payload_dma = dev->tx_payload_dma;
		/* 标记为 coherent — TXFREE 中不要 dma_unmap */
		dev->tx_token.dma_addr[token] = 0; /* 0 = skip unmap */
		dev->tx_token.dma_len[token] = 0;
	} else {
		payload_dma = dma_map_single(&dev->pdev->dev, skb->data,
					     skb->len, DMA_TO_DEVICE);
		if (dma_mapping_error(&dev->pdev->dev, payload_dma)) {
			spin_lock_irqsave(&dev->tx_token.lock, flags);
			dev->tx_token.skb[token] = NULL;
			spin_unlock_irqrestore(&dev->tx_token.lock, flags);
			return -ENOMEM;
		}
		dev->tx_token.dma_addr[token] = payload_dma;
		dev->tx_token.dma_len[token] = skb->len;
	}

	/* 3. Compute txwi location in coherent pool */
	txwi = (__le32 *)(dev->txwi_buf + token * MT7927_TXWI_SIZE);

	/* 4. Build TXD (first 32 bytes)
	 * For mgmt frames, assign rotating PID (1-99) so TXS can be correlated
	 * to specific frames. CT path previously used pid=0 which was a blind spot.
	 * SF path (Ring 2) already uses dev->tx_mgmt_pid — share the same counter. */
	{
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
		int pid = 0;

		if (skb->len >= 2 && ieee80211_is_mgmt(hdr->frame_control)) {
			pid = dev->tx_mgmt_pid;
			dev->tx_mgmt_pid++;
			if (dev->tx_mgmt_pid == 0 || dev->tx_mgmt_pid > 99)
				dev->tx_mgmt_pid = 1;
		}
		mt7927_mac_write_txwi(dev, txwi, skb, wcid, key, pid, false);
	}

	/* 5. Fill TXP (second 32 bytes) — scatter-gather page table */
	txp = (struct mt7927_hw_txp *)(txwi + MT_TXD_SIZE / 4);
	memset(txp, 0, sizeof(*txp));

	txp->msdu_id[0] = cpu_to_le16(token | MT_MSDU_ID_VALID);

	/* ptr[0].buf0 = payload DMA address, len0 = length | LAST */
	txp->ptr[0].buf0 = cpu_to_le32(lower_32_bits(payload_dma));
	txp->ptr[0].len0 = cpu_to_le16(skb->len | MT_TXP_LEN_LAST);

	/* 诊断: 管理帧 dump TXD + TXP 分开显示, 含字段解析 */
	{
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

		if (ieee80211_is_mgmt(hdr->frame_control)) {
			u32 txd1 = le32_to_cpu(txwi[1]);
			u16 wlan_idx = FIELD_GET(MT_TXD1_WLAN_IDX, txd1);
			u8 own_mac = FIELD_GET(MT_TXD1_OWN_MAC, txd1);

			dev_info(&dev->pdev->dev,
				 "TX mgmt: fc=0x%04x wlan_idx=%u own_mac=%u len=%u token=%u\n",
				 le16_to_cpu(hdr->frame_control),
				 wlan_idx, own_mac, skb->len, token);
			print_hex_dump(KERN_INFO, "mt7927 TXD: ",
				       DUMP_PREFIX_OFFSET,
				       16, 4, txwi, MT_TXD_SIZE, false);
			print_hex_dump(KERN_INFO, "mt7927 TXP: ",
				       DUMP_PREFIX_OFFSET,
				       16, 4, txp, sizeof(*txp), false);
		}
	}

	/* 6. Output token and txwi DMA address */
	*token_out = token;
	*txwi_dma_out = dev->txwi_dma + token * MT7927_TXWI_SIZE;

	return 0;
}

/*
 * mt7927_mac_tx_free - Handle TXFREE (TX completion) event from firmware
 *
 * TXFREE events arrive as PKT_TYPE_TXRX_NOTIFY on the MCU RX ring.
 * Each event contains a list of completed MSDU IDs (tokens).
 * For each token, we DMA unmap the payload and free the skb.
 *
 * TXFREE format (CONNAC3):
 *   DW0: PKT_TYPE | MSDU_CNT | RX_BYTE_CNT
 *   DW1: VER | ...
 *   DW2+: per-MSDU info words (token ID in bits 14:0)
 *
 * 来源: mt76/mt76_connac_mac.c mt76_connac2_mac_write_txwi() → tx_free
 *       mt76/mt7925/mac.c mt7925_mac_tx_free()
 */
void mt7927_mac_tx_free(struct mt7927_dev *dev, struct sk_buff *skb)
{
	__le32 *data = (__le32 *)skb->data;
	u32 hdr0, hdr1;
	u16 msdu_cnt;
	int i;
	unsigned long flags;

	if (skb->len < 8)
		goto out;

	hdr0 = le32_to_cpu(data[0]);
	hdr1 = le32_to_cpu(data[1]);
	msdu_cnt = FIELD_GET(MT_TXFREE0_MSDU_CNT, hdr0);

	dev_info(&dev->pdev->dev,
		 "TXFREE: msdu_cnt=%u ver=%lu len=%u\n",
		 msdu_cnt, (unsigned long)FIELD_GET(MT_TXFREE1_VER, hdr1),
		 skb->len);

	/* Debug: hex dump first 5 DWs (20 bytes) of TXFREE */
	{
		int dbg_dw = min_t(int, skb->len / 4, 5);
		int d;
		for (d = 0; d < dbg_dw; d++)
			dev_info(&dev->pdev->dev,
				 "TXFREE DW%d: 0x%08x\n",
				 d, le32_to_cpu(data[d]));
	}

	/* Parse per-MSDU info words starting at DW2.
	 *
	 * TXFREE word types:
	 *   PAIR (bit31=1): contains WLAN_ID, no MSDU IDs — skip
	 *   HEADER (bit30=1, bit31=0): TX status header with STAT/COUNT — log
	 *   MSDU (bit31=0, bit30=0): packed 2× 15-bit MSDU IDs (low + high)
	 *     Low  = bits 14:0, High = bits 29:15, 0x7FFF = invalid
	 *
	 * 来源: mt76/mt7925/mac.c mt7925_mac_tx_free() */
	for (i = 2; i < (skb->len / 4); i++) {
		u32 info = le32_to_cpu(data[i]);
		int j;

		/* PAIR word (bit 31) contains actual WLAN_ID used for TX */
		if (info & MT_TXFREE_INFO_PAIR) {
			dev_info(&dev->pdev->dev,
				 "TXFREE: pair wlan=%lu\n",
				 (unsigned long)FIELD_GET(
					MT_TXFREE_INFO_WLAN_ID, info));
			continue;
		}

		/* Bug 3 fix: HEADER word (bit 30) — log TX status for debug */
		if (info & MT_TXFREE_INFO_HEADER) {
			u8 stat = FIELD_GET(MT_TXFREE_INFO_STAT, info);
			u8 count = FIELD_GET(MT_TXFREE_INFO_COUNT, info);

			if (stat) {
				/* Hardware diagnostic: read MIB/PLE/PSE registers
				 * to determine if frame actually went out on air.
				 *
				 * Band 0 MIB BAR0: 0x024800 (bus 0x820ed000)
				 * Band 1 MIB BAR0: 0x0a4800 (bus 0x820fd000)
				 * Offsets from CODA bn0_wf_mib_top.h:
				 *   TSCR7 (SU_TX_OK): +0x68C
				 *   TBCR0 (TX_20MHz): +0x6AC
				 *   RSCR0 (RX_FCS_OK): +0x75C (validation)
				 *   BTFCR (per-WCID TX_FAIL): +0x5B0 */
				u32 mib0_tx_ok, mib0_tx20, mib0_rx_ok;
				u32 mib1_tx_ok, mib1_tx20, mib1_rx_ok;
				u32 ple_empty, pse_empty;
				u32 ple_sta0, ple_sta1;

				/* Band 0 MIB */
				mib0_tx_ok  = mt7927_rr(dev, MT_MIB_TSCR7(0));
				mib0_tx20   = mt7927_rr(dev, MT_MIB_TBCR0(0));
				mib0_rx_ok  = mt7927_rr(dev, MT_MIB_RSCR0(0));

				/* Band 1 MIB */
				mib1_tx_ok  = mt7927_rr(dev, MT_MIB_TSCR7(1));
				mib1_tx20   = mt7927_rr(dev, MT_MIB_TBCR0(1));
				mib1_rx_ok  = mt7927_rr(dev, MT_MIB_RSCR0(1));

				/* PLE/PSE queue status */
				/* bus2chip: 0x820c0000→BAR0 0x08000 (PLE) */
				ple_empty = mt7927_rr(dev, 0x08360);  /* PLE_QUEUE_EMPTY */
				pse_empty = mt7927_rr(dev, 0x0c0b0);  /* PSE_QUEUE_EMPTY */

				/* WTBL WCID 1 via direct BAR0 (0x820D8000→0x038000)
				 * L1 remap broken for WTBL — returns all zeros */
				{
					u32 dw[8];
					u8 mac[6];
					int w;

					for (w = 0; w < 8; w++)
						dw[w] = mt7927_rr(dev, 0x038100 + w * 4);

					/* Decode MAC from DW0/DW1 (little-endian words) */
					mac[0] = (dw[1] >> 0) & 0xff;
					mac[1] = (dw[1] >> 8) & 0xff;
					mac[2] = (dw[1] >> 16) & 0xff;
					mac[3] = (dw[1] >> 24) & 0xff;
					mac[4] = (dw[0] >> 0) & 0xff;
					mac[5] = (dw[0] >> 8) & 0xff;

					dev_info(&dev->pdev->dev,
						 "  WTBL[1] MAC=%pM BAND=%u MUAR=%u\n",
						 mac,
						 (dw[0] >> 26) & 0x3,
						 (dw[0] >> 16) & 0x3f);
					dev_info(&dev->pdev->dev,
						 "  WTBL[1] DW0-3: %08x %08x %08x %08x\n",
						 dw[0], dw[1], dw[2], dw[3]);
					dev_info(&dev->pdev->dev,
						 "  WTBL[1] DW4-7: %08x %08x %08x %08x\n",
						 dw[4], dw[5], dw[6], dw[7]);
				}

				/* PLE station info — TX queued packets */
				ple_sta0 = mt7927_rr(dev, 0x08024);  /* PLE_STA(0) */
				ple_sta1 = mt7927_rr(dev, 0x08028);  /* PLE_STA(1) */

				dev_info(&dev->pdev->dev,
					 "TX-FAIL: stat=%u count=%u wlan=%lu\n",
					 stat, count,
					 (unsigned long)FIELD_GET(
						MT_TXFREE_INFO_WLAN_ID, info));
				dev_info(&dev->pdev->dev,
					 "  Band0: TX_OK=%u TX20=%u RX_OK=%u\n",
					 mib0_tx_ok, mib0_tx20, mib0_rx_ok);
				dev_info(&dev->pdev->dev,
					 "  Band1: TX_OK=%u TX20=%u RX_OK=%u\n",
					 mib1_tx_ok, mib1_tx20, mib1_rx_ok);
				dev_info(&dev->pdev->dev,
					 "  PLE_EMPTY=0x%08x PSE_EMPTY=0x%08x\n",
					 ple_empty, pse_empty);
				dev_info(&dev->pdev->dev,
					 "  PLE_STA0=0x%08x PLE_STA1=0x%08x\n",
					 ple_sta0, ple_sta1);

				/* STA_PAUSE diagnostic */
				{
					u32 sta_pause0 = mt7927_rr(dev, 0x083e0);
					u32 sta_pause1 = mt7927_rr(dev, 0x083e4);
					u32 dis_sta0 = mt7927_rr(dev, 0x08390);
					u32 dis_sta1 = mt7927_rr(dev, 0x08394);
					/* Band 1 (5GHz) MIB TX counters (CODA verified) */
					u32 b1_tx20 = mt7927_rr(dev, MT_MIB_TBCR0(1));
					u32 b1_tx40 = mt7927_rr(dev, MT_MIB_TBCR1(1));
					dev_info(&dev->pdev->dev,
						 "  STA_PAUSE0=0x%08x STA_PAUSE1=0x%08x\n",
						 sta_pause0, sta_pause1);
					dev_info(&dev->pdev->dev,
						 "  DIS_STA0=0x%08x DIS_STA1=0x%08x\n",
						 dis_sta0, dis_sta1);
					dev_info(&dev->pdev->dev,
						 "  B1_TX20=%u B1_TX40=%u\n",
						 b1_tx20, b1_tx40);
				}

				/* Ring 4 DIDX + PLE HIF — Session 33
				 * CODA: wf_ple_top.h 寄存器偏移 (bus 0x820c0000 → BAR0 0x08000) */
				{
					u32 r4d, r6d, r7d, r4c;
					u32 freepg, hif_pg, hif_grp;

					r4d = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(4));
					r6d = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));
					r7d = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(7));
					r4c = mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(4));
					/* PLE_FREEPG_CNT: offset 0x3A0 */
					freepg = mt7927_rr(dev, 0x083a0);
					/* PLE_HIF_PG_INFO: offset 0x3A8
					 * bits[27:16]=HIF_SRC_CNT, bits[11:0]=HIF_RSV_CNT */
					hif_pg = mt7927_rr(dev, 0x083a8);
					/* PLE_PG_HIF_GROUP: offset 0x0C
					 * bits[27:16]=HIF_MAX_QUOTA, bits[11:0]=HIF_MIN_QUOTA */
					hif_grp = mt7927_rr(dev, 0x0800c);
					dev_info(&dev->pdev->dev,
						 "  RX DIDX: R4=%u R6=%u R7=%u (R4_CIDX=%u)\n",
						 r4d, r6d, r7d, r4c);
					dev_info(&dev->pdev->dev,
						 "  PLE: FREEPG=0x%08x HIF_PG=0x%08x HIF_GRP=0x%08x\n",
						 freepg, hif_pg, hif_grp);
				}

				/* DMASHDL state: check if BYPASS is still on,
				 * and read group quotas / queue mapping.
				 * These are BAR0-accessible (no L1 remap needed). */
				{
					u32 dmashdl_sw, dmashdl_opt, dmashdl_grp0;
					u32 dmashdl_qmap0, dmashdl_status;
					u32 glo_cfg;

					dmashdl_sw = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
					dmashdl_opt = mt7927_rr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL);
					dmashdl_grp0 = mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(0));
					dmashdl_qmap0 = mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0);
					dmashdl_status = mt7927_rr(dev, MT_HIF_DMASHDL_STATUS_RD);
					glo_cfg = mt7927_rr(dev, MT_WPDMA_GLO_CFG);

					dev_info(&dev->pdev->dev,
						 "  DMASHDL: SW=0x%08x (BYPASS=%d) OPT=0x%08x\n",
						 dmashdl_sw,
						 !!(dmashdl_sw & MT_HIF_DMASHDL_BYPASS_EN),
						 dmashdl_opt);
					dev_info(&dev->pdev->dev,
						 "  DMASHDL: GRP0=0x%08x QMAP0=0x%08x STATUS=0x%08x\n",
						 dmashdl_grp0, dmashdl_qmap0,
						 dmashdl_status);
					dev_info(&dev->pdev->dev,
						 "  GLO_CFG=0x%08x (FWDL_BYPASS=%d TX_EN=%d RX_EN=%d)\n",
						 glo_cfg,
						 !!(glo_cfg & MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL),
						 !!(glo_cfg & MT_WFDMA_GLO_CFG_TX_DMA_EN),
						 !!(glo_cfg & MT_WFDMA_GLO_CFG_RX_DMA_EN));
				}
			}
			continue;
		}

		/* Bug 2 fix: MSDU word packs 2× 15-bit token IDs
		 * j=0: bits 14:0, j=1: bits 29:15 */
		for (j = 0; j < 2; j++) {
			u16 msdu_id = (info >> (15 * j)) & 0x7FFF;
			struct sk_buff *tx_skb;
			dma_addr_t addr;
			u32 len;

			if (msdu_id == 0x7FFF || msdu_id >= MT7927_TOKEN_SIZE)
				continue;

			spin_lock_irqsave(&dev->tx_token.lock, flags);
			tx_skb = dev->tx_token.skb[msdu_id];
			addr = dev->tx_token.dma_addr[msdu_id];
			len = dev->tx_token.dma_len[msdu_id];

			dev->tx_token.skb[msdu_id] = NULL;
			dev->tx_token.dma_addr[msdu_id] = 0;
			dev->tx_token.dma_len[msdu_id] = 0;
			spin_unlock_irqrestore(&dev->tx_token.lock, flags);

			if (tx_skb && addr) {
				dma_unmap_single(&dev->pdev->dev, addr, len,
						 DMA_TO_DEVICE);
				dev_kfree_skb_any(tx_skb);
			}
		}
	}

out:
	dev_kfree_skb(skb);
}

/* ============================================================================
 * RX path: Rate information extraction from P-RXV
 * ============================================================================ */

/*
 * mt7927_mac_fill_rx_rate - Extract rate info from P-RXV (Physical RX Vector)
 *
 * P-RXV is 4 DWORDs within GROUP_3 of the RXD:
 *   rxv[0]: TX_RATE (MCS), NSTS (NSS-1), TXBF, HT_AD_CODE
 *   rxv[1]: (reserved)
 *   rxv[2]: TX_MODE, FRAME_MODE (BW), HT_SHORT_GI, HT_STBC, DCM
 *   rxv[3]: RCPI0-3 (signal strength per antenna)
 */
void mt7927_mac_fill_rx_rate(struct mt7927_dev *dev,
			     struct ieee80211_rx_status *status,
			     struct ieee80211_supported_band *sband,
			     __le32 *rxv)
{
	u32 v0 = le32_to_cpu(rxv[0]);
	u32 v2 = le32_to_cpu(rxv[2]);
	u8 idx, nss, mode, gi, bw, stbc;

	idx = FIELD_GET(MT_PRXV_TX_RATE, v0);
	nss = FIELD_GET(MT_PRXV_NSTS, v0) + 1;
	stbc = FIELD_GET(MT_PRXV_HT_STBC, v2);
	gi = FIELD_GET(MT_PRXV_HT_SHORT_GI, v2);
	mode = FIELD_GET(MT_PRXV_TX_MODE, v2);
	bw = FIELD_GET(MT_PRXV_FRAME_MODE, v2);

	/* STBC uses double the spatial streams */
	if (stbc && nss > 1)
		nss >>= 1;

	switch (mode) {
	case MT_PHY_TYPE_CCK:
	case MT_PHY_TYPE_OFDM:
		/* Legacy: map hardware rate index to sband bitrate index */
		if (sband) {
			int i, offset = 0;

			if (mode == MT_PHY_TYPE_CCK)
				idx &= ~BIT(2); /* strip short preamble */
			else if (sband->band == NL80211_BAND_2GHZ)
				offset = 4; /* skip CCK rates */

			for (i = offset; i < sband->n_bitrates; i++) {
				if ((sband->bitrates[i].hw_value & 0xff) == idx) {
					status->rate_idx = i;
					break;
				}
			}
		} else {
			status->rate_idx = idx;
		}
		return;

	case MT_PHY_TYPE_HT_GF:
	case MT_PHY_TYPE_HT:
		status->encoding = RX_ENC_HT;
		status->rate_idx = idx;
		if (idx > 31)
			return;
		break;

	case MT_PHY_TYPE_VHT:
		status->encoding = RX_ENC_VHT;
		status->rate_idx = idx & GENMASK(3, 0);
		status->nss = nss;
		break;

	case MT_PHY_TYPE_HE_SU:
	case MT_PHY_TYPE_HE_EXT_SU:
	case MT_PHY_TYPE_HE_TB:
	case MT_PHY_TYPE_HE_MU:
		status->encoding = RX_ENC_HE;
		status->rate_idx = idx & GENMASK(3, 0);
		status->nss = nss;
		status->he_gi = gi;
		status->he_dcm = FIELD_GET(MT_PRXV_DCM, v2);
		break;

	case MT_PHY_TYPE_EHT_SU:
	case MT_PHY_TYPE_EHT_TRIG:
	case MT_PHY_TYPE_EHT_MU:
		status->encoding = RX_ENC_EHT;
		status->rate_idx = idx & GENMASK(3, 0);
		status->nss = nss;
		status->eht.gi = gi;
		break;

	default:
		return;
	}

	/* Bandwidth */
	switch (bw) {
	case 1:
		status->bw = RATE_INFO_BW_40;
		break;
	case 2:
		status->bw = RATE_INFO_BW_80;
		break;
	case 3:
		status->bw = RATE_INFO_BW_160;
		break;
	case 4:
		status->bw = RATE_INFO_BW_320;
		break;
	default:
		status->bw = RATE_INFO_BW_20;
		break;
	}

	/* STBC and short GI for pre-HE modes */
	status->enc_flags |= RX_ENC_FLAG_STBC_MASK * stbc;
	if (mode < MT_PHY_TYPE_HE_SU && gi)
		status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
}

/* ============================================================================
 * RX path: RXD parsing
 *
 * CONNAC3 RXD layout:
 *   +0x00  RXD[0-7]  Base header (32 bytes, mandatory)
 *   +0x20  GROUP_4   Frame control/seq/qos (16 bytes, optional)
 *   +0x30  GROUP_1   IV information (16 bytes, optional)
 *   +0x40  GROUP_2   Timestamp/AMPDU (16 bytes, optional)
 *   +0x50  GROUP_3   P-RXV rate info (16 bytes, optional)
 *   +0x60  GROUP_5   C-RXV extended (96 bytes, optional)
 *   +xx    Payload   802.3 or 802.11 frame
 * ============================================================================ */

/*
 * mt7927_mac_fill_rx - Parse RXD and fill ieee80211_rx_status
 *
 * Returns 0 on success, -EINVAL if frame should be dropped.
 */
int mt7927_mac_fill_rx(struct mt7927_dev *dev, struct sk_buff *skb)
{
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct ieee80211_supported_band *sband = NULL;
	__le32 *rxd = (__le32 *)skb->data;
	u32 rxd0, rxd1, rxd2, rxd3, rxd4;
	bool unicast, hdr_trans;
	u16 seq_ctrl = 0;
	u8 chfreq, remove_pad;

	memset(status, 0, sizeof(*status));

	rxd0 = le32_to_cpu(rxd[0]);
	rxd1 = le32_to_cpu(rxd[1]);
	rxd2 = le32_to_cpu(rxd[2]);
	rxd3 = le32_to_cpu(rxd[3]);
	rxd4 = le32_to_cpu(rxd[4]);

	/* --- Error checks --- */

	if (rxd2 & MT_RXD2_NORMAL_AMSDU_ERR) {
		dev_info_ratelimited(&dev->pdev->dev,
			"fill_rx DROP: AMSDU_ERR rxd0=0x%08x rxd1=0x%08x rxd2=0x%08x\n",
			rxd0, rxd1, rxd2);
		return -EINVAL;
	}

	if (rxd2 & MT_RXD2_NORMAL_MAX_LEN_ERROR) {
		dev_info_ratelimited(&dev->pdev->dev,
			"fill_rx DROP: MAX_LEN_ERROR rxd0=0x%08x rxd2=0x%08x\n",
			rxd0, rxd2);
		return -EINVAL;
	}

	hdr_trans = !!(rxd2 & MT_RXD2_NORMAL_HDR_TRANS);

	/* HDR_OFFSET: additional padding between RXD groups and frame data
	 * 来源: mt7925/mac.c line 436, 546 — hdr_gap += 2 * remove_pad
	 * 这个值指示 RXD 末尾到实际帧数据之间的填充字节数 (单位: 2字节) */
	remove_pad = FIELD_GET(MT_RXD2_NORMAL_HDR_OFFSET, rxd2);

	/* Header translation + cipher mismatch → drop */
	if (hdr_trans && (rxd1 & MT_RXD1_NORMAL_CM)) {
		dev_info_ratelimited(&dev->pdev->dev,
			"fill_rx DROP: HDR_TRANS+CM rxd1=0x%08x rxd2=0x%08x\n",
			rxd1, rxd2);
		return -EINVAL;
	}

	/* --- Extract basic fields --- */

	unicast = FIELD_GET(MT_RXD3_NORMAL_ADDR_TYPE, rxd3) == 1;

	/* Channel frequency (channel number from hardware) */
	chfreq = FIELD_GET(MT_RXD3_NORMAL_CH_FREQ, rxd3);
	if (chfreq > 180)
		status->band = NL80211_BAND_6GHZ;
	else if (chfreq > 14)
		status->band = NL80211_BAND_5GHZ;
	else
		status->band = NL80211_BAND_2GHZ;

	status->freq = ieee80211_channel_to_frequency(chfreq, status->band);

	/* --- Error flags (non-fatal) --- */

	if (rxd1 & MT_RXD1_NORMAL_ICV_ERR)
		status->flag |= RX_FLAG_ONLY_MONITOR;

	if (rxd3 & MT_RXD3_NORMAL_FCS_ERR)
		status->flag |= RX_FLAG_FAILED_FCS_CRC;

	if (rxd1 & MT_RXD1_NORMAL_TKIP_MIC_ERR)
		status->flag |= RX_FLAG_MMIC_ERROR;

	/* --- Security: mark decrypted if hw handled it ---
	 * S44: 硬件加密 — 当 SEC_MODE!=0 且无 cipher/ICV 错误时，
	 * 标记帧已由固件解密，mac80211 跳过软件解密 */

	if (FIELD_GET(MT_RXD2_NORMAL_SEC_MODE, rxd2) != 0 &&
	    !(rxd1 & (MT_RXD1_NORMAL_CLM | MT_RXD1_NORMAL_CM))) {
		status->flag |= RX_FLAG_DECRYPTED;
		status->flag |= RX_FLAG_IV_STRIPPED;
		status->flag |= RX_FLAG_MMIC_STRIPPED | RX_FLAG_MIC_STRIPPED;
	}

	/* --- Advance past base header (8 DWORDs = 32 bytes) --- */

	rxd += 8;

	/* --- GROUP_4: Frame control, sequence, QoS (4 DWORDs) --- */

	if (rxd1 & MT_RXD1_NORMAL_GROUP_4) {
		u32 v0 = le32_to_cpu(rxd[0]);
		u32 v2 = le32_to_cpu(rxd[2]);

		(void)FIELD_GET(MT_RXD8_FRAME_CONTROL, v0); /* fc, for future use */
		seq_ctrl = FIELD_GET(MT_RXD10_SEQ_CTRL, v2);

		rxd += 4;
	}

	/* --- GROUP_1: IV information (4 DWORDs) --- */

	if (rxd1 & MT_RXD1_NORMAL_GROUP_1) {
		/* IV extraction skipped for now — needed for fragment reassembly */
		rxd += 4;
	}

	/* --- GROUP_2: Timestamp and AMPDU info (4 DWORDs) --- */

	if (rxd1 & MT_RXD1_NORMAL_GROUP_2) {
		status->flag |= RX_FLAG_MACTIME_START;

		if (!(rxd2 & MT_RXD2_NORMAL_NON_AMPDU))
			status->flag |= RX_FLAG_AMPDU_DETAILS;

		rxd += 4;
	}

	/* --- GROUP_3: P-RXV rate and signal info (4 DWORDs) --- */

	if (rxd1 & MT_RXD1_NORMAL_GROUP_3) {
		u32 v3 = le32_to_cpu(rxd[3]);
		u8 rcpi0 = FIELD_GET(MT_PRXV_RCPI0, v3);
		u8 rcpi1 = FIELD_GET(MT_PRXV_RCPI1, v3);

		/* Signal strength */
		status->chains = dev->phy.antenna_mask;
		status->chain_signal[0] = mt7927_rcpi_to_rssi(rcpi0);
		status->chain_signal[1] = mt7927_rcpi_to_rssi(rcpi1);
		status->signal = max(status->chain_signal[0],
				     status->chain_signal[1]);

		/* Rate info */
		if (dev->hw && dev->hw->wiphy)
			sband = dev->hw->wiphy->bands[status->band];
		mt7927_mac_fill_rx_rate(dev, status, sband, rxd);

		rxd += 4;
	}

	/* --- GROUP_5: C-RXV extended (24 DWORDs = 96 bytes) --- */

	if (rxd1 & MT_RXD1_NORMAL_GROUP_5)
		rxd += 24;

	/* --- Strip RXD + padding from SKB, leaving only the frame payload ---
	 * hdr_gap = RXD headers + 2 * remove_pad (additional padding)
	 * 来源: mt7925/mac.c line 546-554 */
	{
		u16 hdr_gap = (u8 *)rxd - skb->data + 2 * remove_pad;

		if (hdr_gap > skb->len) {
			dev_info_ratelimited(&dev->pdev->dev,
				"fill_rx DROP: hdr_gap=%u > len=%u rxd0=0x%08x rxd1=0x%08x\n",
				hdr_gap, skb->len, rxd0, rxd1);
			return -EINVAL;
		}
		skb_pull(skb, hdr_gap);
	}

	/* --- Handle header translation: 反转 802.3→802.11 ---
	 *
	 * S43 关键修复: 硬件头翻译 (HW_HDR_TRANS) 输出 802.3 帧。
	 * mac80211 的 8023 快速路径需要 sta->fast_rx（仅 AUTHORIZED 后设置），
	 * 导致所有 pre-authorization 数据帧（含 EAPOL）被丢弃。
	 *
	 * 解决: 反转头翻译，构造 802.11 + LLC/SNAP 头，走 mac80211 慢路径。
	 * 慢路径不需要 fast_rx，能正确处理 EAPOL。
	 *
	 * 802.3: [DA(6)][SA(6)][ET(2)][payload]  = 14 + N
	 * 802.11: [FC(2)][Dur(2)][A1(6)][A2(6)][A3(6)][SC(2)][SNAP(6)][ET(2)][payload] = 32 + N
	 * 需扩展 18 字节 (headroom 来自已剥离的 RXD)
	 */
	if (hdr_trans) {
		if (skb->len >= ETH_HLEN && skb_headroom(skb) >= 18) {
			u8 da[ETH_ALEN], sa[ETH_ALEN];
			__be16 etype;
			struct ieee80211_hdr_3addr dot11 = {};
			static const u8 rfc1042[6] = {
				0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00
			};

			/* Save 802.3 fields */
			memcpy(da, skb->data, ETH_ALEN);
			memcpy(sa, skb->data + ETH_ALEN, ETH_ALEN);
			etype = *(__be16 *)(skb->data + 2 * ETH_ALEN);

			/* DA=0 修复 (MUAR 未编程时) */
			if (is_zero_ether_addr(da) && dev->hw && dev->hw->wiphy)
				ether_addr_copy(da, dev->hw->wiphy->perm_addr);

			/* Strip 802.3 header */
			skb_pull(skb, ETH_HLEN);

			/* Build 802.11 header: FromDS data frame
			 * Addr1=DA(RA), Addr2=BSSID=SA(TA), Addr3=SA */
		{
			u16 fc_flags = IEEE80211_FTYPE_DATA |
				       IEEE80211_STYPE_DATA |
				       IEEE80211_FCTL_FROMDS;
			/* S44: 硬件加密修复 — 原始帧是加密的，
			 * FC 必须反映 Protected 状态，否则 mac80211
			 * 会丢弃 "未加密的数据帧" (安全降级检测) */
			if (status->flag & RX_FLAG_DECRYPTED)
				fc_flags |= IEEE80211_FCTL_PROTECTED;
			dot11.frame_control = cpu_to_le16(fc_flags);
		}
			memcpy(dot11.addr1, da, ETH_ALEN);
			memcpy(dot11.addr2, sa, ETH_ALEN);
			memcpy(dot11.addr3, sa, ETH_ALEN);
			dot11.seq_ctrl = cpu_to_le16(seq_ctrl);

			/* Prepend LLC/SNAP (8 bytes: 6 SNAP + 2 EtherType) */
			skb_push(skb, 2);
			memcpy(skb->data, &etype, 2);
			skb_push(skb, 6);
			memcpy(skb->data, rfc1042, 6);

			/* Prepend 802.11 header (24 bytes) */
			skb_push(skb, sizeof(dot11));
			memcpy(skb->data, &dot11, sizeof(dot11));

			/* DON'T set RX_FLAG_8023 — this is now a native 802.11 frame */
		} else {
			/* Insufficient headroom — just set 8023 flag and hope for the best */
			status->flag |= RX_FLAG_8023;
		}
	}

	/* --- AMSDU info --- */

	if (FIELD_GET(MT_RXD4_NORMAL_PAYLOAD_FORMAT, rxd4)) {
		status->flag |= RX_FLAG_AMSDU_MORE;
		if (FIELD_GET(MT_RXD4_NORMAL_PAYLOAD_FORMAT, rxd4) ==
		    MT_RXD4_LAST_AMSDU_FRAME)
			status->flag &= ~RX_FLAG_AMSDU_MORE;
	}

	/* --- Aggregation tracking --- */

	if (unicast)
		status->flag |= RX_FLAG_SKIP_MONITOR;

	return 0;
}

/* ============================================================================
 * RX dispatch: Route received frames by type
 * ============================================================================ */

/*
 * Handle MCU event received on RX ring.
 *
 * In UniCmd event format (CONNAC3), the event layout is:
 *   rxd[0..7]: RXD header (32 bytes)
 *   +0x20: len (2 bytes)
 *   +0x22: pkt_type_id (2 bytes) — not the event ID!
 *   +0x24: eid (1 byte) — THIS is the event ID!
 *   +0x25: seq (1 byte)
 *   +0x26: option (1 byte)
 *   ...
 *   +0x2C: TLV payload
 *
 * mt7925 dispatches on rxd->eid, NOT rxd->pkt_type_id.
 * 来源: mt76/mt7925/mcu.c mt7925_mcu_uni_rx_unsolicited_event()
 *       mt76/mt76_connac_mcu.h enum MCU_UNI_EVENT_xxx
 */
static void mt7927_mcu_rx_event(struct mt7927_dev *dev, struct sk_buff *skb)
{
	struct mt7927_mcu_rxd *rxd;

	if (skb->len < sizeof(*rxd))
		goto out;

	rxd = (struct mt7927_mcu_rxd *)skb->data;

	/* 诊断: dump 所有 MCU 事件前 64 字节 */
	dev_info(&dev->pdev->dev,
		 "mcu-evt: eid=0x%02x len=%u\n", rxd->eid, skb->len);
	print_hex_dump(KERN_INFO, "mcu-evt: ", DUMP_PREFIX_OFFSET,
		       16, 1, skb->data, min_t(int, skb->len, 64), false);

	/* MCU_UNI_EVENT_SCAN_DONE = 0x0e — 匹配 eid 字段
	 * 来源: mt76/mt76_connac_mcu.h, mt7925/mcu.c line 587
	 *
	 * 注意: ieee80211_scan_completed() 不能在 NAPI/tasklet 上下文调用,
	 * 必须通过 scan_work 延迟执行, 否则会触发 WARNING.
	 * 来源: net/mac80211/scan.c __ieee80211_scan_completed() 检查 SDATA_STATE_RUNNING */
	if (rxd->eid == MCU_UNI_EVENT_SCAN_DONE) {
		if (test_and_clear_bit(MT7927_SCANNING, &dev->scan_state)) {
			/* 通过 scan_work 延迟通知 mac80211, 避免从 NAPI 上下文直接调用 */
			schedule_delayed_work(&dev->scan_work, 0);
		}
	}

	/* TX_DONE 事件:
	 * UNI event: eid=0x2D (UNI_EVENT_ID_STATUS_TO_HOST), TLV tag 0 = TX_DONE
	 * Legacy event: eid=0x0f (EVENT_ID_TX_DONE)
	 *
	 * TXS_TO_MCU 在 DW5 中设置后, 固件在管理帧发送完成后发此事件.
	 * UNI_EVENT_TX_DONE TLV layout at skb->data + 48:
	 *   RXD header (48 bytes) then:
	 *   [tag(2)][len(2)] = TLV header at offset 48 (tag=0x0000, len=0x0020)
	 *   [ucPacketSeq(1)][ucStatus(1)][u2SeqNum(2)][ucWlanIdx(1)][ucTxCnt(1)][u2TxRate(2)]...
	 *   at offset 52 (= 48 + 4 = after TLV tag+len)
	 * Status: 0=SUCCESS, 1=TX_FAILED, 3=MPDU_ERROR
	 */
	if (rxd->eid == 0x2D) {
		/* UNI_EVENT_ID_STATUS_TO_HOST — parse TX_DONE TLV */
		if (skb->len >= 48 + 4 + 8) { /* rxd hdr + TLV hdr + min data */
			u8 *tlv = skb->data + 48 + 4; /* skip rxd hdr (48) + TLV tag(2)+len(2) */
			u8 pid_val = tlv[0];   /* ucPacketSeq */
			u8 status  = tlv[1];   /* ucStatus: 0=OK, 1=FAIL, 3=MPDU_ERR */
			u16 sn     = le16_to_cpu(*(__le16 *)(tlv + 2)); /* u2SequenceNumber */
			u8 widx    = tlv[4];   /* ucWlanIndex */
			u8 tx_cnt  = tlv[5];   /* ucTxCount */
			u16 tx_rate = le16_to_cpu(*(__le16 *)(tlv + 6)); /* u2TxRate */

			dev_info(&dev->pdev->dev,
				 "TX_DONE(UNI): PID=%u status=%u(%s) SN=%u WIDX=%u cnt=%u rate=0x%04x\n",
				 pid_val, status,
				 status == 0 ? "OK" : status == 1 ? "TX_FAIL" :
				 status == 3 ? "MPDU_ERR" : "UNKNOWN",
				 sn, widx, tx_cnt, tx_rate);
		}
	}

	if (rxd->eid == 0x0f) {
		/* Legacy EVENT_ID_TX_DONE — parse inline fields */
		/* EVENT_TX_DONE starts at rxd+sizeof(WIFI_EVENT) = after event header */
		if (skb->len >= 48 + 8) { /* hdr + min event body */
			u8 *evt = skb->data + 48;
			u8 pid_val = evt[0];
			u8 status = evt[1];
			u16 sn = le16_to_cpu(*(__le16 *)(evt + 2));
			u8 widx = evt[4];
			u8 tx_cnt = evt[5];

			dev_info(&dev->pdev->dev,
				 "TX_DONE(legacy): PID=%u status=%u SN=%u WIDX=%u cnt=%u\n",
				 pid_val, status, sn, widx, tx_cnt);
		}
	}

	/* ROC_GRANT 事件 (eid=0x27 = MCU_UNI_CMD_ROC)
	 * 固件确认已切换到 ROC 请求的信道, 唤醒 mgd_prepare_tx() 中的等待
	 *
	 * 关键: 从 ROC_GRANT TLV 提取 dbdcband 字段来更新 band_idx!
	 * MT6639 是 DBDC 芯片, 5GHz 和 2.4GHz 使用不同的 radio (band 0/1).
	 * 固件通过 ROC_GRANT 返回实际分配的 band, 驱动必须用它来:
	 *   1. 更新 VIF 的 band_idx
	 *   2. TXD 的 TGID 字段使用正确的 band
	 *   3. BSS_INFO 使用正确的 band_idx
	 * 如果不更新, TXD 会带错误的 TGID → 固件丢弃 TX 帧 (stat=1)
	 *
	 * ROC_GRANT TLV layout (mt7925/mt7925.h mt7925_roc_grant_tlv):
	 *   +0: tag(2) +2: len(2) +4: bss_idx +5: tokenid +6: status
	 *   +7: primarychannel +8: rfsco +9: rfband +10: channelwidth
	 *   +11: centerfreqseg1 +12: centerfreqseg2 +13: reqtype
	 *   +14: dbdcband +15: rsv +16: max_interval(4)
	 *
	 * TLV body starts at sizeof(mt76_connac2_mcu_rxd) = 48 bytes from skb start
	 * (our mt7927_mcu_rxd is 44 bytes, but wire format has 4 extra padding bytes)
	 */
	if (rxd->eid == 0x27) {
		u8 dbdcband = 0xff;
		u8 status = 0;
		u8 primary_ch = 0;

		/* Parse ROC_GRANT TLV if event is large enough
		 * TLV starts at offset 48 (mt76_connac2_mcu_rxd size)
		 * dbdcband is at TLV offset 14 → skb offset 48+14=62 */
		if (skb->len >= 48 + 20) { /* 48 hdr + 20 TLV min size */
			u8 *tlv = skb->data + 48;

			status = tlv[6];
			primary_ch = tlv[7];
			dbdcband = tlv[14];
		}

		dev->roc_grant_band_idx = dbdcband;
		dev_info(&dev->pdev->dev,
			 "mcu-evt: ROC_GRANT status=%u ch=%u dbdcband=%u\n",
			 status, primary_ch, dbdcband);
		dev->roc_active = true;

		/* Clear DROP_OTHER_UC in RFCR immediately after ROC_GRANT.
		 * Firmware restores RFCR (including DROP_OTHER_UC=1) when
		 * granting the channel. Clear it here so unicast frames
		 * can pass through even if MUAR is not programmed.
		 * Use band from ROC_GRANT (dbdcband, 0=2.4GHz, 1=5GHz). */
		{
			u8 band = (dbdcband < 2) ? dbdcband : 0;
			u32 rfcr = mt7927_rr(dev, MT_WF_RFCR(band));
			if (rfcr & MT_WF_RFCR_DROP_OTHER_UC) {
				mt7927_wr(dev, MT_WF_RFCR(band),
					  rfcr & ~MT_WF_RFCR_DROP_OTHER_UC);
				dev_info(&dev->pdev->dev,
					 "ROC_GRANT: cleared RFCR DROP_OTHER_UC band%u "
					 "(0x%08x -> 0x%08x)\n",
					 band, rfcr, rfcr & ~MT_WF_RFCR_DROP_OTHER_UC);
			}
		}

		complete(&dev->roc_complete);
	}

out:
	dev_kfree_skb(skb);
}

/*
 * mt7927_queue_rx_skb - Top-level RX dispatch
 *
 * Called from NAPI poll when a completed RX descriptor is found.
 * Routes the SKB based on PKT_TYPE in RXD[0].
 *
 * @dev: device
 * @q: which RX queue this came from (MT_RXQ_MAIN or MT_RXQ_MCU)
 * @skb: received SKB with data starting at RXD[0]
 */
void mt7927_queue_rx_skb(struct mt7927_dev *dev, enum mt76_rxq_id q,
			 struct sk_buff *skb)
{
	__le32 *rxd = (__le32 *)skb->data;
	u32 rxd0 = le32_to_cpu(rxd[0]);
	u32 type, flag;

	type = FIELD_GET(MT_RXD0_PKT_TYPE, rxd0);
	flag = FIELD_GET(MT_RXD0_PKT_FLAG, rxd0);

	/*
	 * CONNAC3 SW_PKT_TYPE check: some data frames arrive with
	 * non-zero PKT_TYPE but should be treated as normal frames.
	 * 来源: mt7925/mac.c mt7925_queue_rx_skb() lines 1215-1221
	 */
	if (type != PKT_TYPE_NORMAL) {
		u32 sw_type = FIELD_GET(MT_RXD0_SW_PKT_TYPE_MASK, rxd0);

		if ((sw_type & MT_RXD0_SW_PKT_TYPE_MAP) ==
		    MT_RXD0_SW_PKT_TYPE_FRAME)
			type = PKT_TYPE_NORMAL;
	}

	/*
	 * mt7925 特殊处理: PKT_TYPE_RX_EVENT(7) + flag=0x1 表示
	 * 这是通过 MCU ring 路由的普通数据帧 (如扫描期间的 beacon/probe response),
	 * 不是真正的 MCU 事件. 重新归类为 PKT_TYPE_NORMAL_MCU.
	 * 来源: mt7925/mac.c line 1223-1224
	 */
	if (type == PKT_TYPE_RX_EVENT && flag == 0x1)
		type = PKT_TYPE_NORMAL_MCU;

	switch (type) {
	case PKT_TYPE_NORMAL_MCU:
	case PKT_TYPE_NORMAL: {
		/*
		 * 数据帧 (beacon, probe response, data 等):
		 * 解析 RXD 并交给 mac80211. 扫描期间 beacon/probe response
		 * 可能通过 MCU ring (ring 6) 到达, 类型为 PKT_TYPE_NORMAL_MCU(8).
		 * 来源: mt7925/mac.c lines 1240-1246
		 */
		int ret = mt7927_mac_fill_rx(dev, skb);

		if (ret) {
			dev->rx_drop_count++;
			if (dev->rx_drop_count <= 20)
				dev_info(&dev->pdev->dev,
					"RX DROP #%u: fill_rx=%d rxd0=0x%08x rxd1=0x%08x rxd2=0x%08x len=%u\n",
					dev->rx_drop_count, ret, rxd0,
					le32_to_cpu(rxd[1]),
					le32_to_cpu(rxd[2]), skb->len);
			dev_kfree_skb(skb);
			return;
		}
		dev->rx_ok_count++;
		/* 诊断: RX 帧类型记录 */
		{
			u8 *d = skb->data;

			if (skb->len >= 24) {
				u8 fc0 = d[0];
				u8 ftype = (fc0 >> 2) & 0x3;
				u8 subtype = (fc0 >> 4) & 0xf;

				if (ftype == 0 && subtype != 8 && subtype != 4) {
					dev_info(&dev->pdev->dev,
						"RX-MGMT #%u: FC=%02x%02x subtype=%u len=%u freq=%u\n",
						dev->rx_ok_count, fc0, d[1], subtype,
						skb->len,
						IEEE80211_SKB_RXCB(skb)->freq);
				}
				/* EAPOL 检测: data frame + LLC/SNAP + ethertype 0x888e */
				if (ftype == 2 && skb->len >= 32 + 2) {
					/* 802.11 hdr(24) + SNAP(6) + ET(2) */
					u16 et = (d[30] << 8) | d[31];

					if (et == 0x888E) {
						dev_info(&dev->pdev->dev,
							"RX-EAPOL #%u: 802.11 len=%u FC=%02x%02x "
							"A1=%02x:%02x:%02x:%02x:%02x:%02x\n",
							dev->rx_ok_count, skb->len,
							fc0, d[1],
							d[4], d[5], d[6], d[7], d[8], d[9]);
					} else if (dev->rx_ok_count <= 300) {
						dev_info_ratelimited(&dev->pdev->dev,
							"RX-DATA #%u: FC=%02x%02x len=%u\n",
							dev->rx_ok_count, fc0, d[1], skb->len);
					}
				}
			}
		}
		if (dev->hw)
			ieee80211_rx_irqsafe(dev->hw, skb);
		else
			dev_kfree_skb(skb);
		return;
	}

	case PKT_TYPE_RX_EVENT:
		/* 真正的 MCU 事件 (flag != 0x1), 路由到 MCU 事件处理器 */
		mt7927_mcu_rx_event(dev, skb);
		return;

	case PKT_TYPE_TXRX_NOTIFY:
		/* TXFREE — TX completion notification with token IDs */
		mt7927_mac_tx_free(dev, skb);
		return;

	case PKT_TYPE_TXS: {
		/* CONNAC3 TXS: RXD 头 (4 DW) 后跟随若干 TXS 记录, 每条 12 DW.
		 * 遍历所有记录, 用 mt7927_mac_dump_txs() 解码错误位:
		 *   ack_to=1 → 帧已发出但未收到 ACK (AP 侧问题)
		 *   rts_to=1 → RTS 超时 (信道争用失败)
		 *   q_to=1   → 队列超时 (帧从未离开芯片, 固件或 PLE 问题)
		 */
		__le32 *end = (__le32 *)&skb->data[skb->len];
		__le32 *txs;
		int txs_cnt = 0;

		for (txs = rxd + MT_TXS_HDR_SIZE;
		     txs + MT_TXS_SIZE <= end;
		     txs += MT_TXS_SIZE) {
			mt7927_mac_dump_txs(dev, txs, "TXS");
			txs_cnt++;
		}

		if (!txs_cnt) {
			/* 没有完整的 TXS 记录, 打印原始头部用于调试 */
			u32 dw0 = le32_to_cpu(rxd[0]);
			u32 dw1 = skb->len >= 8  ? le32_to_cpu(rxd[1]) : 0;
			u32 dw2 = skb->len >= 12 ? le32_to_cpu(rxd[2]) : 0;

			dev_info(&dev->pdev->dev,
				 "TXS: len=%u 无有效记录 DW0=0x%08x DW1=0x%08x DW2=0x%08x\n",
				 skb->len, dw0, dw1, dw2);
		}

		dev_kfree_skb(skb);
		return;
	}

	default:
		dev_info(&dev->pdev->dev,
			 "rx-unknown: q=%d type=%u flag=0x%x rxd0=0x%08x len=%u\n",
			 q, type, flag, rxd0, skb->len);
		dev_kfree_skb(skb);
		return;
	}
}
