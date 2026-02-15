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

	/* 管理帧需要固定速率发送 (基本速率)
	 * 来源: mt7925/mac.c line 682-684 */
	if (!ieee80211_is_data(fc))
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
	u8 p_fmt = MT_TX_TYPE_CT; /* Cut-through: TXD+TXP in coherent, payload separate */
	u8 q_idx;
	u16 wlan_idx = wcid ? wcid->idx : 0;
	u8 omac_idx = 0;
	u8 band_idx = 0;
	u32 val;

	/* Get VIF info from WCID if available */
	if (wcid && wcid->sta) {
		struct mt7927_sta *msta;

		msta = container_of(wcid, struct mt7927_sta, wcid);
		if (msta->vif) {
			omac_idx = msta->vif->omac_idx;
			band_idx = msta->vif->band_idx;
		}
	}

	/* Determine queue index */
	if (beacon) {
		q_idx = MT_TX_MCU_PORT_RX_Q0;
		p_fmt = MT_TX_TYPE_FW;
	} else if (!is_8023) {
		/* 管理帧 (auth/assoc/probe) 必须走 ALTX0 队列
		 * 来源: mt7925/mac.c line 757-763, qid >= MT_TXQ_PSD → ALTX0 */
		q_idx = 0x10; /* MT_LMAC_ALTX0 */
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

	/* TXD[3]: TX control flags */
	val = FIELD_PREP(MT_TXD3_REM_TX_COUNT, 15);

	if (key)
		val |= MT_TXD3_PROTECT_FRAME;

	if (info->flags & IEEE80211_TX_CTL_NO_ACK)
		val |= MT_TXD3_NO_ACK;

	if (is_multicast_ether_addr(skb->data))
		val |= MT_TXD3_BCM;

	txwi[3] = cpu_to_le32(val);

	/* TXD[5]: PID and TX status request */
	val = FIELD_PREP(MT_TXD5_PID, pid);
	if (pid)
		val |= MT_TXD5_TX_STATUS_HOST;
	txwi[5] = cpu_to_le32(val);

	/* TXD[6]: MSDU count, DAS, disable MAT */
	val = FIELD_PREP(MT_TXD6_MSDU_CNT, 1) |
	      MT_TXD6_DAS |
	      MT_TXD6_DIS_MAT;
	txwi[6] = cpu_to_le32(val);

	/* 使用固定速率时: 设置 TX_RATE + 禁用 BA
	 * 来源: mt7925/mac.c line 817-834
	 * MT792x_BASIC_RATES_TBL = 11 (基址)
	 * 5GHz 需要跳过 4 个 CCK 速率 → index 15 (OFDM 6Mbps)
	 * 2.4GHz 用 index 11 (CCK 1Mbps) */
	if (txwi[1] & cpu_to_le32(MT_TXD1_FIXED_RATE)) {
		u8 rate_idx = 11; /* 2.4GHz: CCK 1Mbps */

		if (dev->hw && dev->hw->conf.chandef.chan &&
		    dev->hw->conf.chandef.chan->band != NL80211_BAND_2GHZ)
			rate_idx = 15; /* 5GHz/6GHz: OFDM 6Mbps */

		txwi[6] |= cpu_to_le32(FIELD_PREP(MT_TXD6_TX_RATE, rate_idx));
		txwi[3] |= cpu_to_le32(MT_TXD3_BA_DISABLE);
	}
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

	/* 2. DMA map payload (skb->data, not prepended with TXD) */
	payload_dma = dma_map_single(&dev->pdev->dev, skb->data, skb->len,
				     DMA_TO_DEVICE);
	if (dma_mapping_error(&dev->pdev->dev, payload_dma)) {
		spin_lock_irqsave(&dev->tx_token.lock, flags);
		dev->tx_token.skb[token] = NULL;
		spin_unlock_irqrestore(&dev->tx_token.lock, flags);
		return -ENOMEM;
	}

	/* Save mapping info for TXFREE cleanup */
	dev->tx_token.dma_addr[token] = payload_dma;
	dev->tx_token.dma_len[token] = skb->len;

	/* 3. Compute txwi location in coherent pool */
	txwi = (__le32 *)(dev->txwi_buf + token * MT7927_TXWI_SIZE);

	/* 4. Build TXD (first 32 bytes) */
	mt7927_mac_write_txwi(dev, txwi, skb, wcid, key, 0, false);

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

			if (stat)
				dev_info(&dev->pdev->dev,
					 "TXFREE: hdr stat=%u count=%u wlan=%lu\n",
					 stat, count,
					 (unsigned long)FIELD_GET(
						MT_TXFREE_INFO_WLAN_ID, info));
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

	if (rxd2 & MT_RXD2_NORMAL_AMSDU_ERR)
		return -EINVAL;

	if (rxd2 & MT_RXD2_NORMAL_MAX_LEN_ERROR)
		return -EINVAL;

	hdr_trans = !!(rxd2 & MT_RXD2_NORMAL_HDR_TRANS);

	/* HDR_OFFSET: additional padding between RXD groups and frame data
	 * 来源: mt7925/mac.c line 436, 546 — hdr_gap += 2 * remove_pad
	 * 这个值指示 RXD 末尾到实际帧数据之间的填充字节数 (单位: 2字节) */
	remove_pad = FIELD_GET(MT_RXD2_NORMAL_HDR_OFFSET, rxd2);

	/* Header translation + cipher mismatch → drop */
	if (hdr_trans && (rxd1 & MT_RXD1_NORMAL_CM))
		return -EINVAL;

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

	/* --- Security: mark decrypted if hw handled it --- */

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

		if (hdr_gap > skb->len)
			return -EINVAL;
		skb_pull(skb, hdr_gap);
	}

	/* --- Handle header translation --- */

	if (hdr_trans)
		status->flag |= RX_FLAG_8023;

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
	case PKT_TYPE_NORMAL:
		/*
		 * 数据帧 (beacon, probe response, data 等):
		 * 解析 RXD 并交给 mac80211. 扫描期间 beacon/probe response
		 * 可能通过 MCU ring (ring 6) 到达, 类型为 PKT_TYPE_NORMAL_MCU(8).
		 * 来源: mt7925/mac.c lines 1240-1246
		 */
		if (mt7927_mac_fill_rx(dev, skb)) {
			dev_kfree_skb(skb);
			return;
		}
		if (dev->hw)
			ieee80211_rx_irqsafe(dev->hw, skb);
		else
			dev_kfree_skb(skb);
		return;

	case PKT_TYPE_RX_EVENT:
		/* 真正的 MCU 事件 (flag != 0x1), 路由到 MCU 事件处理器 */
		mt7927_mcu_rx_event(dev, skb);
		return;

	case PKT_TYPE_TXRX_NOTIFY:
		/* TXFREE — TX completion notification with token IDs */
		mt7927_mac_tx_free(dev, skb);
		return;

	case PKT_TYPE_TXS:
		/* TX status report — skip for now */
		dev_kfree_skb(skb);
		return;

	default:
		dev_dbg(&dev->pdev->dev,
			"rx-unknown: q=%d type=%u flag=0x%x rxd0=0x%08x len=%u\n",
			q, type, flag, rxd0, skb->len);
		dev_kfree_skb(skb);
		return;
	}
}
