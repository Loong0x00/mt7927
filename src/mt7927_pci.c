// SPDX-License-Identifier: GPL-2.0
/*
 * mt7927_pci.c - MT7927/MT6639 PCIe WiFi 驱动
 *
 * 100% 基于 Windows 驱动逆向工程 (mtkwecx.sys v5603998 + v5705275)
 * 初始化序列严格按 Windows vtable 顺序:
 *   1. MT6639PreFirmwareDownloadInit
 *   2. InitTxRxRing (rings 4, 6, 7 + TX 15)
 *   3. WpdmaConfig (prefetch + GLO_CFG)
 *   4. ConfigIntMask (0x2600f000)
 *   5. FWDL (搬运已验证代码)
 *   6. PostFwDownloadInit (DMASHDL + NIC_CAP)
 *
 * MT7927 = MT6639 移动芯片的 PCIe 封装版本，不是 MT76 家族。
 * PCI ID: 14c3:6639
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/bitfield.h>
#include <linux/byteorder/generic.h>
#include <linux/etherdevice.h>
#include <net/cfg80211.h>

#include "mt7927_pci.h"

/* ===== 模块参数 ===== */
static int skip_fwdl;
module_param(skip_fwdl, int, 0644);
MODULE_PARM_DESC(skip_fwdl, "跳过固件下载 (调试用)");

/* 固件路径 */
#define MT7927_FW_PATCH  "mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin"
#define MT7927_FW_RAM    "mediatek/WIFI_RAM_CODE_MT6639_2_1.bin"
MODULE_FIRMWARE(MT7927_FW_PATCH);
MODULE_FIRMWARE(MT7927_FW_RAM);

/* =====================================================================
 * mac80211 频段和速率定义
 * 参考: mt76/mac80211.c
 * ===================================================================== */

#define MT7927_CCK_RATE(_idx, _rate) {					\
	.bitrate = _rate,						\
	.flags = IEEE80211_RATE_SHORT_PREAMBLE,				\
	.hw_value = (MT_PHY_TYPE_CCK << 8) | (_idx),			\
	.hw_value_short = (MT_PHY_TYPE_CCK << 8) | (4 + (_idx)),	\
}

#define MT7927_OFDM_RATE(_idx, _rate) {					\
	.bitrate = _rate,						\
	.hw_value = (MT_PHY_TYPE_OFDM << 8) | (_idx),			\
	.hw_value_short = (MT_PHY_TYPE_OFDM << 8) | (_idx),		\
}

/* 2.4GHz 信道 (1-14) */
static struct ieee80211_channel mt7927_2ghz_channels[] = {
	CHAN2G(1, 2412),  CHAN2G(2, 2417),  CHAN2G(3, 2422),
	CHAN2G(4, 2427),  CHAN2G(5, 2432),  CHAN2G(6, 2437),
	CHAN2G(7, 2442),  CHAN2G(8, 2447),  CHAN2G(9, 2452),
	CHAN2G(10, 2457), CHAN2G(11, 2462), CHAN2G(12, 2467),
	CHAN2G(13, 2472), CHAN2G(14, 2484),
};

/* 5GHz 信道 (36-177) */
static struct ieee80211_channel mt7927_5ghz_channels[] = {
	CHAN5G(36, 5180),  CHAN5G(40, 5200),  CHAN5G(44, 5220),  CHAN5G(48, 5240),
	CHAN5G(52, 5260),  CHAN5G(56, 5280),  CHAN5G(60, 5300),  CHAN5G(64, 5320),
	CHAN5G(100, 5500), CHAN5G(104, 5520), CHAN5G(108, 5540), CHAN5G(112, 5560),
	CHAN5G(116, 5580), CHAN5G(120, 5600), CHAN5G(124, 5620), CHAN5G(128, 5640),
	CHAN5G(132, 5660), CHAN5G(136, 5680), CHAN5G(140, 5700), CHAN5G(144, 5720),
	CHAN5G(149, 5745), CHAN5G(153, 5765), CHAN5G(157, 5785), CHAN5G(161, 5805),
	CHAN5G(165, 5825), CHAN5G(169, 5845), CHAN5G(173, 5865), CHAN5G(177, 5885),
};

/* 基础速率表: CCK + OFDM */
static struct ieee80211_rate mt7927_rates[] = {
	MT7927_CCK_RATE(0, 10),
	MT7927_CCK_RATE(1, 20),
	MT7927_CCK_RATE(2, 55),
	MT7927_CCK_RATE(3, 110),
	MT7927_OFDM_RATE(11, 60),
	MT7927_OFDM_RATE(15, 90),
	MT7927_OFDM_RATE(10, 120),
	MT7927_OFDM_RATE(14, 180),
	MT7927_OFDM_RATE(9,  240),
	MT7927_OFDM_RATE(13, 360),
	MT7927_OFDM_RATE(8,  480),
	MT7927_OFDM_RATE(12, 540),
};

/* 2.4GHz 频段: HT (802.11n) 2x2 */
static struct ieee80211_supported_band mt7927_band_2ghz = {
	.channels = mt7927_2ghz_channels,
	.n_channels = ARRAY_SIZE(mt7927_2ghz_channels),
	.bitrates = mt7927_rates,
	.n_bitrates = ARRAY_SIZE(mt7927_rates),
	.ht_cap = {
		.ht_supported = true,
		.cap = IEEE80211_HT_CAP_SGI_20 |
		       IEEE80211_HT_CAP_SGI_40 |
		       IEEE80211_HT_CAP_TX_STBC |
		       (1 << IEEE80211_HT_CAP_RX_STBC_SHIFT) |
		       IEEE80211_HT_CAP_LDPC_CODING |
		       IEEE80211_HT_CAP_MAX_AMSDU,
		.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
		.ampdu_density = IEEE80211_HT_MPDU_DENSITY_4,
		.mcs = {
			.rx_mask = { 0xff, 0xff },
			.tx_params = IEEE80211_HT_MCS_TX_DEFINED,
		},
	},
};

/* 5GHz 频段: HT + VHT (802.11ac) 2x2, 160MHz */
static struct ieee80211_supported_band mt7927_band_5ghz = {
	.channels = mt7927_5ghz_channels,
	.n_channels = ARRAY_SIZE(mt7927_5ghz_channels),
	.bitrates = mt7927_rates + 4,	/* OFDM only */
	.n_bitrates = ARRAY_SIZE(mt7927_rates) - 4,
	.ht_cap = {
		.ht_supported = true,
		.cap = IEEE80211_HT_CAP_SGI_20 |
		       IEEE80211_HT_CAP_SGI_40 |
		       IEEE80211_HT_CAP_TX_STBC |
		       (1 << IEEE80211_HT_CAP_RX_STBC_SHIFT) |
		       IEEE80211_HT_CAP_LDPC_CODING |
		       IEEE80211_HT_CAP_MAX_AMSDU,
		.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
		.ampdu_density = IEEE80211_HT_MPDU_DENSITY_4,
		.mcs = {
			.rx_mask = { 0xff, 0xff },
			.tx_params = IEEE80211_HT_MCS_TX_DEFINED,
		},
	},
	.vht_cap = {
		.vht_supported = true,
		.cap = IEEE80211_VHT_CAP_SHORT_GI_80 |
		       IEEE80211_VHT_CAP_SHORT_GI_160 |
		       IEEE80211_VHT_CAP_TXSTBC |
		       IEEE80211_VHT_CAP_RXSTBC_1 |
		       IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
		       IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK |
		       IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE |
		       IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
		       (3 << IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT),
		.vht_mcs = {
			.rx_mcs_map = cpu_to_le16(0xfffa),
			.tx_mcs_map = cpu_to_le16(0xfffa),
		},
	},
};

/* =====================================================================
 * 1. Helper Functions — 寄存器 I/O
 * 来源: 已验证的现有驱动 mt7927_init_dma.c 行 431-562
 * ===================================================================== */

/* 安全读取 BAR0 寄存器 */
u32 mt7927_rr(struct mt7927_dev *dev, u32 reg)
{
	if (unlikely(reg + sizeof(u32) > dev->bar0_len)) {
		dev_warn(&dev->pdev->dev,
			 "mmio rr 越界: reg=0x%08x bar0_len=0x%llx\n",
			 reg, (u64)dev->bar0_len);
		return 0xffffffff;
	}
	return ioread32(dev->bar0 + reg);
}

/* 安全写入 BAR0 寄存器 */
void mt7927_wr(struct mt7927_dev *dev, u32 reg, u32 val)
{
	if (unlikely(reg + sizeof(u32) > dev->bar0_len)) {
		dev_warn(&dev->pdev->dev,
			 "mmio wr 越界: reg=0x%08x val=0x%08x bar0_len=0x%llx\n",
			 reg, val, (u64)dev->bar0_len);
		return;
	}
	iowrite32(val, dev->bar0 + reg);
}

/* 读-改-写 */
static inline void mt7927_rmw(struct mt7927_dev *dev, u32 reg,
			      u32 clr, u32 set)
{
	u32 val = mt7927_rr(dev, reg);

	val &= ~clr;
	val |= set;
	mt7927_wr(dev, reg, val);
}

/* 读-改-写 单个位域 */
static inline void mt7927_rmw_field(struct mt7927_dev *dev, u32 reg,
				    u32 mask, u32 val)
{
	u32 cur = mt7927_rr(dev, reg);

	cur &= ~mask;
	cur |= FIELD_PREP(mask, val);
	mt7927_wr(dev, reg, cur);
}

/* 轮询寄存器直到 (reg & mask) == val, 超时 us 微秒 */
static bool mt7927_poll(struct mt7927_dev *dev, u32 reg,
			u32 mask, u32 val, int timeout_us)
{
	int i;

	for (i = 0; i < timeout_us; i += 10) {
		if ((mt7927_rr(dev, reg) & mask) == val)
			return true;
		udelay(10);
	}
	return false;
}

/* L1 remap 读取 — 用于 0x18xxxxxx 芯片地址
 * 来源: 已验证代码 mt7927_init_dma.c 行 1372-1408 */
u32 mt7927_rr_l1(struct mt7927_dev *dev, u32 chip_addr)
{
	u32 base = (chip_addr >> 16) & 0xFFFF;
	u32 offset = chip_addr & 0xFFFF;
	u32 old_l1, val;

	old_l1 = ioread32(dev->bar0 + MT_HIF_REMAP_L1);
	iowrite32(FIELD_PREP(MT_HIF_REMAP_L1_MASK, base),
		  dev->bar0 + MT_HIF_REMAP_L1);
	ioread32(dev->bar0 + MT_HIF_REMAP_L1);  /* 回读确保写入生效 */

	val = ioread32(dev->bar0 + MT_HIF_REMAP_BASE_L1 + offset);

	iowrite32(old_l1, dev->bar0 + MT_HIF_REMAP_L1);  /* 恢复 */
	return val;
}

/* L1 remap 写入 */
static void mt7927_wr_l1(struct mt7927_dev *dev, u32 chip_addr, u32 val)
{
	u32 base = (chip_addr >> 16) & 0xFFFF;
	u32 offset = chip_addr & 0xFFFF;
	u32 old_l1;

	old_l1 = ioread32(dev->bar0 + MT_HIF_REMAP_L1);
	iowrite32(FIELD_PREP(MT_HIF_REMAP_L1_MASK, base),
		  dev->bar0 + MT_HIF_REMAP_L1);
	ioread32(dev->bar0 + MT_HIF_REMAP_L1);

	iowrite32(val, dev->bar0 + MT_HIF_REMAP_BASE_L1 + offset);

	iowrite32(old_l1, dev->bar0 + MT_HIF_REMAP_L1);
}

/* =====================================================================
 * 2. DMA Ring Management
 * 来源: 已验证代码 mt7927_init_dma.c 行 678-780
 * ===================================================================== */

/* 分配 TX ring (描述符 + HW 寄存器写入)
 * 来源: Windows MT6639InitTxRxRing — TX ring 写入序列 */
static int mt7927_tx_ring_alloc(struct mt7927_dev *dev,
				struct mt7927_ring *ring,
				u16 qid, u16 ndesc)
{
	u16 i;

	ring->qid = qid;
	ring->ndesc = ndesc;
	ring->head = 0;
	ring->buf = NULL;
	ring->buf_dma = NULL;
	ring->buf_size = 0;

	ring->desc = dma_alloc_coherent(&dev->pdev->dev,
					ndesc * sizeof(struct mt76_desc),
					&ring->desc_dma, GFP_KERNEL);
	if (!ring->desc)
		return -ENOMEM;

	memset(ring->desc, 0, ndesc * sizeof(struct mt76_desc));

	/* TX 描述符初始化: DMA_DONE=1 (空闲状态) */
	for (i = 0; i < ndesc; i++)
		ring->desc[i].ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);

	/* 写 WFDMA HW 寄存器
	 * 来源: Windows MT6639InitTxRxRing — 写入顺序 BASE→CNT→CIDX→DIDX */
	mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(qid),
		  lower_32_bits(ring->desc_dma));
	mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(qid), ndesc);
	mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(qid), 0);  /* TX CIDX=0 */
	mt7927_wr(dev, MT_WPDMA_TX_RING_DIDX(qid), 0);

	return 0;
}

/* 分配 RX ring (描述符 + buffer + HW 寄存器写入)
 * 来源: Windows MT6639InitTxRxRing — RX ring 写入序列
 * 注意: Windows 设置 RX CIDX = ndesc-1 (预设消费者索引) */
static int mt7927_rx_ring_alloc(struct mt7927_dev *dev,
				struct mt7927_ring *ring,
				u16 qid, u16 ndesc, u32 buf_size)
{
	u16 i;

	ring->qid = qid;
	ring->ndesc = ndesc;
	ring->head = ndesc - 1;  /* Windows: CIDX = ndesc-1 */
	ring->tail = 0;
	ring->buf_size = buf_size;

	ring->desc = dma_alloc_coherent(&dev->pdev->dev,
					ndesc * sizeof(struct mt76_desc),
					&ring->desc_dma, GFP_KERNEL);
	if (!ring->desc)
		return -ENOMEM;

	ring->buf = kcalloc(ndesc, sizeof(*ring->buf), GFP_KERNEL);
	ring->buf_dma = kcalloc(ndesc, sizeof(*ring->buf_dma), GFP_KERNEL);
	if (!ring->buf || !ring->buf_dma)
		goto err;

	memset(ring->desc, 0, ndesc * sizeof(struct mt76_desc));

	for (i = 0; i < ndesc; i++) {
		ring->buf[i] = dma_alloc_coherent(&dev->pdev->dev, buf_size,
						  &ring->buf_dma[i],
						  GFP_KERNEL);
		if (!ring->buf[i])
			goto err;

		ring->desc[i].buf0 =
			cpu_to_le32(lower_32_bits(ring->buf_dma[i]));
		ring->desc[i].buf1 = cpu_to_le32(0);
		ring->desc[i].info = cpu_to_le32(0);
		/* Windows RX 描述符初始化: 清 BIT(31), 设 buffer 大小
		 * 来源: register_playbook.md 行 123-129 */
		ring->desc[i].ctrl =
			cpu_to_le32(FIELD_PREP(MT_DMA_CTL_SD_LEN0, buf_size));
	}

	/* 写 WFDMA HW 寄存器
	 * 来源: Windows MT6639InitTxRxRing — RX 写入顺序 */
	mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(qid),
		  lower_32_bits(ring->desc_dma));
	mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(qid), ndesc);
	mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(qid), 0);
	/* Windows: CIDX = ring_count - 1 (所有 buffer 可用) */
	mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(qid), ring->head);

	return 0;

err:
	/* 清理已分配的资源 */
	if (ring->buf && ring->buf_dma) {
		for (i = 0; i < ndesc; i++) {
			if (!ring->buf[i])
				continue;
			dma_free_coherent(&dev->pdev->dev, buf_size,
					  ring->buf[i], ring->buf_dma[i]);
		}
	}
	kfree(ring->buf);
	kfree(ring->buf_dma);
	ring->buf = NULL;
	ring->buf_dma = NULL;

	if (ring->desc) {
		dma_free_coherent(&dev->pdev->dev,
				  ndesc * sizeof(struct mt76_desc),
				  ring->desc, ring->desc_dma);
		ring->desc = NULL;
	}
	return -ENOMEM;
}

/* 释放 ring 资源 */
static void mt7927_ring_free(struct mt7927_dev *dev,
			     struct mt7927_ring *ring)
{
	u16 i;

	if (!ring->desc)
		return;

	if (ring->buf && ring->buf_dma) {
		for (i = 0; i < ring->ndesc; i++) {
			if (!ring->buf[i])
				continue;
			if (ring->buf_size > 0) {
				/* RX ring: buf[i] is coherent DMA buffer */
				dma_free_coherent(&dev->pdev->dev,
						  ring->buf_size,
						  ring->buf[i],
						  ring->buf_dma[i]);
			} else {
				/* TX ring: buf[i] is skb pointer */
				struct sk_buff *skb = ring->buf[i];

				if (ring->buf_dma[i])
					dma_unmap_single(&dev->pdev->dev,
							 ring->buf_dma[i],
							 skb->len,
							 DMA_TO_DEVICE);
				dev_kfree_skb_any(skb);
			}
		}
	}
	kfree(ring->buf);
	kfree(ring->buf_dma);
	ring->buf = NULL;
	ring->buf_dma = NULL;

	dma_free_coherent(&dev->pdev->dev,
			  ring->ndesc * sizeof(struct mt76_desc),
			  ring->desc, ring->desc_dma);
	ring->desc = NULL;
}

/* 重新编程 ring 的 HW 寄存器 (CLR_OWN 后必须调用)
 * 来源: CLR_OWN 副作用 — ROM 清零所有 HOST ring BASE */
static void mt7927_ring_reprogram_tx(struct mt7927_dev *dev,
				     struct mt7927_ring *ring)
{
	mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(ring->qid),
		  lower_32_bits(ring->desc_dma));
	mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(ring->qid), ring->ndesc);
	mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid), 0);
	ring->head = 0;
}

static void mt7927_ring_reprogram_rx(struct mt7927_dev *dev,
				     struct mt7927_ring *ring)
{
	mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(ring->qid),
		  lower_32_bits(ring->desc_dma));
	mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(ring->qid), ring->ndesc);
	mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(ring->qid), 0);
	mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid), ring->head);
	ring->tail = 0;
}

/* 重新配置所有 per-ring EXT_CTRL (CLR_OWN 后必须调用)
 * CLR_OWN 会清零 EXT_CTRL 寄存器和 packed prefetch，需要恢复预取配置
 * 来源: 旧驱动 Mode 53 验证的工作值 + 团队修复 */
static void mt7927_reprogram_prefetch(struct mt7927_dev *dev)
{
	u32 val;

	/* 重写 Windows packed prefetch 寄存器 (CLR_OWN 会清零)
	 * 来源: 团队修复 Task #3 */
	mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG0, MT_WFDMA_PREFETCH_VAL0);  /* 0xd70f0 */
	mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG1, MT_WFDMA_PREFETCH_VAL1);  /* 0xd70f4 */
	mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG2, MT_WFDMA_PREFETCH_VAL2);  /* 0xd70f8 */
	mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG3, MT_WFDMA_PREFETCH_VAL3);  /* 0xd70fc */

	/* 重写 GLO_CFG_EXT1 BIT(28) */
	val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
	val |= MT_WPDMA_GLO_CFG_EXT1_WIN;  /* BIT(28) */
	mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, val);

	/* 重写 per-ring 预取配置 */
	mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
	mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0080, 0x8));
	mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0100, 0x4));
	mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x0140, 0x4));
	mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0180, 0x10));
	mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(0), PREFETCH(0x0280, 0x4));
	mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(2), PREFETCH(0x02c0, 0x4));
}

/* =====================================================================
 * 3. DMA 控制函数
 * ===================================================================== */

/* 禁用 DMA — 来源: vendor asicConnac3xWfdmaControl(FALSE)
 * 已验证代码 mt7927_init_dma.c 行 625-676 */
static void mt7927_dma_disable(struct mt7927_dev *dev)
{
	u32 val;
	int i;

	/* 清除 DMA_EN, chain_en, omit 位 */
	val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
	val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN |
		 MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
		 MT_GLO_CFG_OMIT_TX_INFO | MT_GLO_CFG_OMIT_RX_INFO_PFET2);
	mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
	wmb();

	/* 等待 DMA idle */
	for (i = 0; i < 100; i++) {
		val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
		if (!(val & (MT_WFDMA_GLO_CFG_TX_DMA_BUSY |
			     MT_WFDMA_GLO_CFG_RX_DMA_BUSY)))
			break;
		usleep_range(500, 1000);
	}

	/* WFDMA 逻辑复位 */
	val = mt7927_rr(dev, MT_WFDMA0_RST);
	val &= ~(MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST);
	mt7927_wr(dev, MT_WFDMA0_RST, val);
	val |= MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST;
	mt7927_wr(dev, MT_WFDMA0_RST, val);
	usleep_range(100, 200);
}

/* 清理所有 DMA rings */
static void mt7927_dma_cleanup(struct mt7927_dev *dev)
{
	mt7927_dma_disable(dev);
	mt7927_ring_free(dev, &dev->ring_wm);
	mt7927_ring_free(dev, &dev->ring_fwdl);
	mt7927_ring_free(dev, &dev->ring_rx4);
	mt7927_ring_free(dev, &dev->ring_rx6);
	mt7927_ring_free(dev, &dev->ring_rx7);
}

/* =====================================================================
 * 4. TX ring 提交 (kick) + MCU 事件轮询
 * 来源: 已验证代码 mt7927_init_dma.c 行 871-920, 797-868
 * ===================================================================== */

/* 向 TX ring 提交一个描述符并等待 WFDMA 消费 */
static int mt7927_kick_ring_buf(struct mt7927_dev *dev,
				struct mt7927_ring *ring,
				dma_addr_t dma, u32 len, bool last_sec)
{
	struct mt76_desc *d;
	u16 idx = ring->head;
	u32 ctrl;
	int i;

	d = &ring->desc[idx];
	memset(d, 0, sizeof(*d));

	ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, len);
	if (last_sec)
		ctrl |= MT_DMA_CTL_LAST_SEC0;

	d->buf0 = cpu_to_le32(lower_32_bits(dma));
	d->ctrl = cpu_to_le32(ctrl);
	wmb();

	ring->head = (idx + 1) % ring->ndesc;
	mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid), ring->head);
	wmb();

	/* 通过 HOST2MCU 软件中断通知 MCU */
	mt7927_wr(dev, MT_HOST2MCU_SW_INT_SET, BIT(0));

	/* 等待 WFDMA 消费 (DIDX 追上 CIDX) */
	for (i = 0; i < 100; i++) {
		u32 hw_didx = mt7927_rr(dev,
					MT_WPDMA_TX_RING_DIDX(ring->qid));
		if (hw_didx == ring->head)
			return 0;
		usleep_range(500, 1000);
	}

	dev_warn(&dev->pdev->dev,
		 "ring%u 未被消费: cidx=0x%x didx=0x%x\n",
		 ring->qid,
		 mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid)),
		 mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid)));
	return -ETIMEDOUT;
}

/* 消费 RX ring 6 中一个 DMA_DONE 的 slot, 推进 tail + CIDX + 清 INT_STA */
static void mt7927_mcu_consume_slot(struct mt7927_dev *dev,
				    struct mt7927_ring *ring)
{
	struct mt76_desc *d = &ring->desc[ring->tail];

	/* 清除 DMA_DONE, 重置描述符 */
	d->ctrl = cpu_to_le32(
		FIELD_PREP(MT_DMA_CTL_SD_LEN0, ring->buf_size));

	/* 推进 tail */
	ring->tail = (ring->tail + 1) % ring->ndesc;

	/* 归还已处理的 slot 给 DMA — 写 CIDX = tail */
	mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid), ring->tail);

	/* 清除 INT_STA (W1C) */
	{
		u32 int_sta = mt7927_rr(dev, MT_WFDMA_HOST_INT_STA);

		if (int_sta)
			mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, int_sta);
	}
}

/* 在 RX ring 6 (MCU 事件) 上轮询响应, 匹配 CID + seq
 *
 * UniEvent 在 DMA buf 中的布局 (little-endian):
 *   evt[0..7]: RXD (32 bytes)
 *   evt[8]:    len(16) | pkt_type_id(16)    — offset 0x20
 *   evt[9]:    eid(8) | seq(8) | option(8) | rsv(8) — offset 0x24
 *   evt[10]:   ext_eid(8) | rsv1[2](16) | s2d(8)   — offset 0x28
 *
 * 匹配规则: evt_seq == mcu_wait_seq
 * ext_eid 对应发送的 CID 低 8 位 (作为额外验证) */
static int mt7927_mcu_wait_resp(struct mt7927_dev *dev, int timeout_ms)
{
	struct mt7927_ring *ring = &dev->ring_rx6;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	u8 wait_seq = dev->mcu_wait_seq;
	u16 wait_cid = dev->mcu_wait_cid;

	do {
		struct mt76_desc *d = &ring->desc[ring->tail];
		u32 ctrl = le32_to_cpu(d->ctrl);

		if (ctrl & MT_DMA_CTL_DMA_DONE) {
			u16 idx = ring->tail;
			u32 *evt = ring->buf[idx];
			u32 sdl0 = FIELD_GET(MT_DMA_CTL_SD_LEN0, ctrl);
			u8 evt_eid, evt_seq, evt_ext_eid;

			/* 解析 UniEvent 头字段 */
			evt_eid = evt[9] & 0xff;
			evt_seq = (evt[9] >> 8) & 0xff;
			evt_ext_eid = evt[10] & 0xff;

			dev_info(&dev->pdev->dev,
				 "mcu-evt: q%u idx=%u sdl0=%u eid=0x%02x ext_eid=0x%02x seq=%u (wait: cid=0x%04x seq=%u)\n",
				 ring->qid, idx, sdl0,
				 evt_eid, evt_ext_eid, evt_seq,
				 wait_cid, wait_seq);

			/* 检查是否匹配等待的命令响应 */
			if (evt_seq == wait_seq) {
				/* 命令响应 — 匹配成功 */
				dev_info(&dev->pdev->dev,
					 "mcu-resp: matched cid=0x%04x seq=%u ext_eid=0x%02x\n",
					 wait_cid, evt_seq, evt_ext_eid);
				mt7927_mcu_consume_slot(dev, ring);
				return 0;
			}

			/* 异步事件 — seq 不匹配, 跳过 */
			dev_info(&dev->pdev->dev,
				 "mcu-async: skipping eid=0x%02x ext_eid=0x%02x seq=%u (wanted seq=%u)\n",
				 evt_eid, evt_ext_eid, evt_seq, wait_seq);
			mt7927_mcu_consume_slot(dev, ring);
			continue;
		}
		usleep_range(1000, 2000);
	} while (time_before(jiffies, timeout));

	dev_warn(&dev->pdev->dev,
		 "mcu-evt 超时: cid=0x%04x seq=%u q%u tail=%u cidx=0x%x didx=0x%x\n",
		 wait_cid, wait_seq, ring->qid, ring->tail,
		 mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid)),
		 mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(ring->qid)));
	return -ETIMEDOUT;
}

/* =====================================================================
 * 5. MCU 命令发送
 * 来源: Windows Ghidra RE — TXD 格式 + MtCmdEnqueueFWCmd
 * 已验证代码 mt7927_init_dma.c 行 922-983
 * ===================================================================== */

/* 发送 MCU 命令 (Legacy 路径, 0x40 字节头部)
 * TXD 格式严格匹配 Windows:
 *   TXD[0] = total_len | 0x41000000 (Q_IDX=0x20, PKT_FMT=2)
 *   TXD[1] = flags | 0x4000 (HDR_FORMAT_V3=1, 无 BIT(31))
 * 来源: register_playbook.md 行 279-288 */
static int mt7927_mcu_send_cmd(struct mt7927_dev *dev, u8 cid,
			       const void *payload, size_t plen)
{
	struct mt76_connac2_mcu_txd *txd;
	dma_addr_t dma;
	void *buf;
	size_t len = sizeof(*txd) + plen;
	u32 val;
	int ret;

	buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memset(buf, 0, len);
	txd = buf;
	if (plen > 0)
		memcpy((u8 *)buf + sizeof(*txd), payload, plen);

	/* TXD[0]: Q_IDX=0x20 (MCU 命令端口), PKT_FMT=2
	 * Windows: TXD[0] = total_len | 0x41000000 */
	val = FIELD_PREP(MT_TXD0_TX_BYTES, len) |
	      FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) |
	      FIELD_PREP(MT_TXD0_Q_IDX, MT_TX_MCU_PORT_RX_Q0);
	txd->txd[0] = cpu_to_le32(val);

	/* TXD[1]: HDR_FORMAT_V3=1, 永不设置 BIT(31) LONG_FORMAT
	 * Windows: TXD[1] = flags | 0x4000 */
	val = FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
	txd->txd[1] = cpu_to_le32(val);

	/* MCU 命令头部字段 */
	txd->len = cpu_to_le16(len - sizeof(txd->txd));
	txd->pq_id = cpu_to_le16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU,
					    MT_TX_MCU_PORT_RX_Q0));
	txd->cid = cid;
	txd->pkt_type = MCU_PKT_ID;  /* 0xa0 */
	txd->seq = ++dev->mcu_seq & 0xf;
	if (!txd->seq)
		txd->seq = ++dev->mcu_seq & 0xf;
	txd->s2d_index = 0;  /* MCU_S2D_H2N */

	/* 设置等待序列号, mcu_wait_resp 需要匹配 */
	dev->mcu_wait_seq = txd->seq;
	dev->mcu_wait_cid = cid;

	dev_info(&dev->pdev->dev,
		 "mcu-cmd: cid=0x%02x len=%zu plen=%zu seq=%u\n",
		 cid, len, plen, txd->seq);

	/* 暂时禁用 RX6 中断, 防止 NAPI 和轮询同时消费 RX ring 6 */
	mt7927_wr(dev, MT_WFDMA_INT_ENA_CLR, BIT(14));
	ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
	if (!ret)
		ret = mt7927_mcu_wait_resp(dev, 200);
	mt7927_wr(dev, MT_WFDMA_INT_ENA_SET, BIT(14));

	dma_free_coherent(&dev->pdev->dev, len, buf, dma);
	return ret;
}

/* 发送 FWDL scatter 数据 (通过 FWDL ring 16) */
static int mt7927_mcu_send_scatter(struct mt7927_dev *dev,
				   const u8 *data, u32 len)
{
	dma_addr_t dma;
	void *buf;
	int ret;

	buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, len);
	ret = mt7927_kick_ring_buf(dev, &dev->ring_fwdl, dma, len, true);

	dma_free_coherent(&dev->pdev->dev, len, buf, dma);
	return ret;
}

/* 分块发送固件数据 */
static int mt7927_mcu_send_firmware_chunks(struct mt7927_dev *dev,
					   const u8 *data, u32 len,
					   u32 max_len)
{
	while (len > 0) {
		u32 cur = min(len, max_len);
		int ret;

		ret = mt7927_mcu_send_scatter(dev, data, cur);
		if (ret)
			return ret;

		data += cur;
		len -= cur;
	}
	return 0;
}

/* 发送 UniCmd (CONNAC3 固件启动后命令格式)
 * 来源: mt76/mt76_connac_mcu.c + mt7925/mcu.c + src/docs/unicmd_format_analysis.md */
static int mt7927_mcu_send_unicmd(struct mt7927_dev *dev, u16 cmd_id,
				   u8 option, const void *payload, size_t plen)
{
	struct mt7927_mcu_uni_txd *txd;
	dma_addr_t dma;
	void *buf;
	size_t len = sizeof(*txd) + plen;
	u32 val;
	int ret;

	buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memset(buf, 0, len);
	txd = buf;
	if (plen > 0 && payload)
		memcpy((u8 *)buf + sizeof(*txd), payload, plen);

	/* TXD[0]: Q_IDX=0x20, PKT_FMT=2, 和 legacy 相同 */
	val = FIELD_PREP(MT_TXD0_TX_BYTES, len) |
	      FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) |
	      FIELD_PREP(MT_TXD0_Q_IDX, MT_TX_MCU_PORT_RX_Q0);
	txd->txd[0] = cpu_to_le32(val);

	/* TXD[1]: HDR_FORMAT_V3=1, 无 BIT(31), 和 legacy 相同 */
	val = FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
	txd->txd[1] = cpu_to_le32(val);

	/* UniCmd header
	 * len = 整个 UniCmd 包长度 (0x20-末尾), 即 16字节内部头 + payload
	 * 来源: Windows RE +0x20 = header_size - 0x20 + payload_len
	 *        mt76: txd->len = cpu_to_le16(skb->len) 其中 skb->len = 内部头+payload */
	txd->len = cpu_to_le16(plen + 16);
	txd->cid = cpu_to_le16(cmd_id);
	txd->pkt_type = MCU_PKT_ID;  /* 0xa0 */
	txd->seq = ++dev->mcu_seq & 0xf;
	if (!txd->seq)
		txd->seq = ++dev->mcu_seq & 0xf;
	txd->s2d_index = 0;  /* MCU_S2D_H2N */
	txd->option = option;

	dev_info(&dev->pdev->dev,
		 "unicmd: cid=0x%04x opt=0x%02x len=%zu plen=%zu seq=%u\n",
		 cmd_id, option, len, plen, txd->seq);

	/* 需要等响应时, 必须阻止 NAPI 消费 RX6 上的 MCU response。
	 * 问题: NAPI completion 会重新启用 BIT(14), 所以单靠 disable+sync 不够。
	 * 解决: napi_disable 完全阻止 NAPI poll 运行, 确保 polling 独占 RX6。
	 * probe 阶段 NAPI 未初始化 (state=0), napi_disable 安全退化为 no-op 级别。 */
	if (option & UNI_CMD_OPT_ACK) {
		dev->mcu_wait_cid = cmd_id;
		dev->mcu_wait_seq = txd->seq;
		if (dev->napi_running)
			napi_disable(&dev->napi_rx_mcu);
		mt7927_wr(dev, MT_WFDMA_INT_ENA_CLR, BIT(14));
	}

	/* 通过 TX ring 15 发送 */
	ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
	if (!ret && (option & UNI_CMD_OPT_ACK))
		ret = mt7927_mcu_wait_resp(dev, 2000);

	if (option & UNI_CMD_OPT_ACK) {
		mt7927_wr(dev, MT_WFDMA_INT_ENA_SET, BIT(14));
		if (dev->napi_running)
			napi_enable(&dev->napi_rx_mcu);
	}

	dma_free_coherent(&dev->pdev->dev, len, buf, dma);
	return ret;
}

/* =====================================================================
 * 6. FWDL 辅助函数
 * 来源: 已验证代码 mt7927_init_dma.c 行 1066-1307
 * ===================================================================== */

/* 补丁下载模式计算 */
static u32 mt7927_patch_dl_mode(u32 sec_info)
{
	u32 mode = DL_MODE_NEED_RSP;

	if (sec_info == PATCH_SEC_NOT_SUPPORT)
		return mode;

	switch (FIELD_GET(PATCH_SEC_ENC_TYPE_MASK, sec_info)) {
	case PATCH_SEC_ENC_TYPE_PLAIN:
		break;
	case PATCH_SEC_ENC_TYPE_AES:
		mode |= DL_MODE_ENCRYPT;
		mode |= FIELD_PREP(DL_MODE_KEY_IDX,
				   sec_info & PATCH_SEC_ENC_AES_KEY_MASK) &
			DL_MODE_KEY_IDX;
		mode |= DL_MODE_RESET_SEC_IV;
		break;
	case PATCH_SEC_ENC_TYPE_SCRAMBLE:
		mode |= DL_MODE_ENCRYPT;
		mode |= DL_CONFIG_ENCRY_MODE_SEL;
		mode |= DL_MODE_RESET_SEC_IV;
		break;
	}
	return mode;
}

/* Patch 信号量控制 */
static int mt7927_mcu_patch_sem_ctrl(struct mt7927_dev *dev, bool get)
{
	struct {
		__le32 op;
	} req = {
		.op = cpu_to_le32(get ? PATCH_SEM_GET : PATCH_SEM_RELEASE),
	};

	return mt7927_mcu_send_cmd(dev, MCU_CMD_PATCH_SEM_CONTROL,
				   &req, sizeof(req));
}

/* 初始化下载 (地址+长度+模式) */
static int mt7927_mcu_init_download(struct mt7927_dev *dev,
				    u32 addr, u32 len, u32 mode)
{
	struct {
		__le32 addr;
		__le32 len;
		__le32 mode;
	} req = {
		.addr = cpu_to_le32(addr),
		.len = cpu_to_le32(len),
		.mode = cpu_to_le32(mode),
	};
	/* MT6639 使用 0x900000 作为补丁地址 */
	u8 cmd = (addr == MCU_PATCH_ADDRESS_MT6639) ?
		 MCU_CMD_PATCH_START_REQ : MCU_CMD_TARGET_ADDRESS_LEN_REQ;

	return mt7927_mcu_send_cmd(dev, cmd, &req, sizeof(req));
}

/* 补丁完成信号 */
static int mt7927_mcu_start_patch(struct mt7927_dev *dev)
{
	struct {
		u8 check_crc;
		u8 rsv[3];
	} req = { 0 };

	return mt7927_mcu_send_cmd(dev, MCU_CMD_PATCH_FINISH_REQ,
				   &req, sizeof(req));
}

/* 固件启动命令 */
static int mt7927_mcu_start_firmware(struct mt7927_dev *dev,
				     u32 addr, u32 option)
{
	struct {
		__le32 option;
		__le32 addr;
	} req = {
		.option = cpu_to_le32(option),
		.addr = cpu_to_le32(addr),
	};

	return mt7927_mcu_send_cmd(dev, MCU_CMD_FW_START_REQ,
				   &req, sizeof(req));
}

/* RAM 下载模式计算 — 关键: 必须设置 DL_MODE_ENCRYPT | DL_MODE_RESET_SEC_IV */
static u32 mt7927_ram_dl_mode(u8 feature_set)
{
	u32 mode = DL_MODE_NEED_RSP;

	if (feature_set & FW_FEATURE_NON_DL)
		return mode;

	if (feature_set & FW_FEATURE_SET_ENCRYPT) {
		mode |= DL_MODE_ENCRYPT;
		mode |= DL_MODE_RESET_SEC_IV;
	}
	if (feature_set & FW_FEATURE_ENCRY_MODE)
		mode |= DL_CONFIG_ENCRY_MODE_SEL;
	mode |= FIELD_PREP(DL_MODE_KEY_IDX,
			   FIELD_GET(FW_FEATURE_SET_KEY_IDX, feature_set));

	return mode;
}

/* =====================================================================
 * 7. Windows Init Sequence — 按 vtable 顺序
 * ===================================================================== */

/* ------- 7a. MT6639 MCU 初始化 (PreFirmwareDownloadInit 前置步骤) -------
 * 来源: 已验证代码 mt7927_init_dma.c 行 1422-1626
 * 流程: CONNINFRA 唤醒 → cbinfra remap → EMI 睡眠保护
 *       → WF 复位 → MCU ownership → 轮询 ROMCODE → MCIF remap */
static int mt7927_mcu_init_mt6639(struct mt7927_dev *dev)
{
	u32 val;
	int i;

	dev_info(&dev->pdev->dev, "MT6639 MCU 初始化: 开始\n");

	/* 检查 MCU 当前状态 */
	val = mt7927_rr(dev, MT_ROMCODE_INDEX);
	dev_info(&dev->pdev->dev,
		 "ROMCODE_INDEX 复位前 = 0x%08x\n", val);

	/* Step 1: CONNINFRA 唤醒
	 * 来源: vendor mt6639.c, 必须在 cbinfra 访问前执行 */
	mt7927_wr(dev, MT_WAKEPU_TOP, 0x1);
	usleep_range(1000, 2000);

	/* Step 2: cbinfra PCIe remap
	 * 来源: vendor set_cbinfra_remap */
	mt7927_wr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF,
		  MT_PCIE_REMAP_WF_VALUE);
	mt7927_wr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF_BT,
		  MT_PCIE_REMAP_WF_BT_VALUE);

	/* Step 2b: EMI 睡眠保护 (L1 remap)
	 * 来源: upstream mt7925/pci.c — 在 WFSYS 复位前设置 */
	val = mt7927_rr_l1(dev, MT_HW_EMI_CTL);
	val |= MT_HW_EMI_CTL_SLPPROT_EN;
	mt7927_wr_l1(dev, MT_HW_EMI_CTL, val);

	/* Step 3: WF 子系统复位 (vendor 方法 — CB_INFRA_RGU)
	 * 来源: vendor mt6639_mcu_reset */
	val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
	val |= MT_WF_SUBSYS_RST_BIT;
	mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
	msleep(1);

	val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
	val &= ~MT_WF_SUBSYS_RST_BIT;
	mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
	msleep(5);

	/* Step 4: MCU ownership 设置
	 * 来源: cb_infra_slp_ctrl.h, chip 0x70025034 */
	mt7927_wr(dev, MT_CB_INFRA_MCU_OWN_SET, BIT(0));

	/* Step 5: 轮询 ROMCODE_INDEX = 0x1D1E (MCU 空闲)
	 * 来源: Windows PreFirmwareDownloadInit — 最多 500 次/1ms */
	for (i = 0; i < 1000; i++) {
		val = mt7927_rr(dev, MT_ROMCODE_INDEX);
		if (val == MT_MCU_IDLE_VALUE) {
			dev_info(&dev->pdev->dev,
				 "MCU 空闲, 用时 %d ms\n", i);
			goto mcu_ready;
		}
		usleep_range(1000, 2000);
	}
	dev_err(&dev->pdev->dev,
		"MCU 空闲超时, ROMCODE_INDEX = 0x%08x\n", val);
	mt7927_wr(dev, MT_WAKEPU_TOP, 0x0);
	return -ETIMEDOUT;

mcu_ready:
	/* Step 6: MCIF remap — MCU 写入 host PCIe 内存的关键配置
	 * 来源: vendor mt6639.c PCIE2AP_REMAP_WF_1_BA */
	mt7927_wr(dev, MT_MCIF_REMAP_WF_1_BA, MT_MCIF_REMAP_VAL);

	/* 清除 CONNINFRA 唤醒 */
	mt7927_wr(dev, MT_WAKEPU_TOP, 0x0);

	dev_info(&dev->pdev->dev, "MT6639 MCU 初始化: 完成\n");
	return 0;
}

/* ------- 7b. SET_OWN / CLR_OWN 序列 -------
 * 来源: register_playbook.md 行 211-236
/* ------- 7c. InitTxRxRing — 严格匹配 Windows -------
 * 来源: Windows MT6639InitTxRxRing (FUN_1401e4580)
 * Windows 分配: TX 0,1,2,3 + RX 4,6,7
 * 我们分配: TX 15(MCU), 16(FWDL) + RX 4,6,7 */
static int mt7927_init_tx_rx_ring(struct mt7927_dev *dev)
{
	int ret;

	dev_info(&dev->pdev->dev, "InitTxRxRing: 分配 rings\n");

	/* TX Ring 15 — MCU 命令 */
	ret = mt7927_tx_ring_alloc(dev, &dev->ring_wm,
				   MT_TXQ_MCU_WM_RING,
				   MT_TXQ_MCU_WM_RING_SIZE);
	if (ret) {
		dev_err(&dev->pdev->dev, "TX ring 15 分配失败\n");
		return ret;
	}

	/* TX Ring 16 — FWDL */
	ret = mt7927_tx_ring_alloc(dev, &dev->ring_fwdl,
				   MT_TXQ_FWDL_RING,
				   MT_TXQ_FWDL_RING_SIZE);
	if (ret) {
		dev_err(&dev->pdev->dev, "TX ring 16 分配失败\n");
		return ret;
	}

	/* RX Ring 4 — 数据接收
	 * 来源: Windows RX[0] = HW ring 4, offset 0x40 */
	ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx4,
				   MT_RXQ_DATA,
				   MT_RX_RING_SIZE, MT_RX_BUF_SIZE);
	if (ret) {
		dev_err(&dev->pdev->dev, "RX ring 4 分配失败\n");
		return ret;
	}

	/* RX Ring 6 — MCU 事件接收
	 * 来源: Windows RX[1] = HW ring 6, offset 0x60
	 * !! 不是 ring 5 (offset 0x50) !! */
	ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx6,
				   MT_RXQ_MCU_EVENT,
				   MT_RX_RING_SIZE, MT_RX_BUF_SIZE);
	if (ret) {
		dev_err(&dev->pdev->dev, "RX ring 6 分配失败\n");
		return ret;
	}

	/* RX Ring 7 — 辅助数据接收
	 * 来源: Windows RX[2] = HW ring 7, offset 0x70 */
	ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx7,
				   MT_RXQ_DATA2,
				   MT_RX_RING_SIZE, MT_RX_BUF_SIZE);
	if (ret) {
		dev_err(&dev->pdev->dev, "RX ring 7 分配失败\n");
		return ret;
	}

	/* 同步 TX CIDX 到 HW DIDX (Windows 行为) */
	{
		u32 didx;

		didx = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(
				     dev->ring_wm.qid)) & 0xfff;
		dev->ring_wm.head = didx % dev->ring_wm.ndesc;
		mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid),
			  dev->ring_wm.head);

		didx = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(
				     dev->ring_fwdl.qid)) & 0xfff;
		dev->ring_fwdl.head = didx % dev->ring_fwdl.ndesc;
		mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid),
			  dev->ring_fwdl.head);
	}

	dev_info(&dev->pdev->dev,
		 "InitTxRxRing: TX15=0x%llx TX16=0x%llx RX4=0x%llx RX6=0x%llx RX7=0x%llx\n",
		 (u64)dev->ring_wm.desc_dma,
		 (u64)dev->ring_fwdl.desc_dma,
		 (u64)dev->ring_rx4.desc_dma,
		 (u64)dev->ring_rx6.desc_dma,
		 (u64)dev->ring_rx7.desc_dma);
	return 0;
}

/* ------- 7d. WpdmaConfig — Windows 预取 + GLO_CFG -------
 * 来源: Windows MT6639WpdmaConfig (FUN_1401e5be0)
 * register_playbook.md 行 134-170 */
static void mt7927_wpdma_config(struct mt7927_dev *dev, bool enable)
{
	u32 val;

	if (enable) {
		/* 预取配置 (Windows WpdmaConfig 步骤 3a-3e)
		 * 来源: register_playbook.md 行 145-148
		 * 确定性: 100% — Ghidra RE 直接提取的值 */

		/* 触发预取重置 (读回写) */
		val = mt7927_rr(dev, MT_WFDMA_PREFETCH_CTRL);
		mt7927_wr(dev, MT_WFDMA_PREFETCH_CTRL, val);

		/* Windows: 4 个 packed 预取配置寄存器 */
		mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG0,
			  MT_WFDMA_PREFETCH_VAL0); /* 0xd70f0 = 0x660077 */
		mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG1,
			  MT_WFDMA_PREFETCH_VAL1); /* 0xd70f4 = 0x1100 */
		mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG2,
			  MT_WFDMA_PREFETCH_VAL2); /* 0xd70f8 = 0x30004f */
		mt7927_wr(dev, MT_WFDMA_PREFETCH_CFG3,
			  MT_WFDMA_PREFETCH_VAL3); /* 0xd70fc = 0x542200 */

		/* Per-ring 预取配置 — 告诉 WFDMA 每个 ring 的预取缓冲区位置和深度
		 * 这些和 packed prefetch (0xd70f0) 是互补的，都需要配置
		 * 值参考旧驱动（已验证工作）的布局
		 * 来源: mt7927_init_dma.c Mode 53 */
		mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
		mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0080, 0x8));
		mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0100, 0x4));
		mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x0140, 0x4));
		mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0180, 0x10));
		/* TX ring 0 (数据帧) — 放在 TX15 之后 (0x0180 + 0x10*16 = 0x0280)
		 * depth=0x4 与 mt7925 一致 (mt7925/pci.c line 232)
		 * 缺少此配置导致 auth 帧无法通过 DMA 发出 */
		mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(0), PREFETCH(0x0280, 0x4));
		/* TX ring 2 (管理帧 SF) — Session 21: 添加 per-ring EXT_CTRL!
		 * MT6639 soc5_0.c:562 写 0x01c00004, 我们之前跳过了
		 * Ring 2 缺少 prefetch buffer 可能导致 WFDMA 路由失败
		 * base=0x02c0 (TX0 之后: 0x0280 + 4*16 = 0x02c0), depth=4 */
		mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(2), PREFETCH(0x02c0, 0x4));

		/* GLO_CFG: 启用 TX_DMA_EN | RX_DMA_EN + Windows 关键位 (步骤 4)
		 * 来源: Windows — glo_cfg |= 0x5 + 额外关键位
		 * 注意: MT6639 用 0x5, MT7925 用 0x4000005 */
		val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
		val |= MT_WFDMA_GLO_CFG_TX_DMA_EN |
		       MT_WFDMA_GLO_CFG_RX_DMA_EN;
		/* Windows 关键位 (来自团队修复) */
		val |= MT_GLO_CFG_TX_WB_DDONE;                  /* BIT(6) */
		val |= MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN;   /* BIT(15) - 预取链使能 */
		val |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;          /* BIT(20) */
		val |= MT_GLO_CFG_OMIT_RX_INFO_PFET2;           /* BIT(21) */
		val |= MT_GLO_CFG_ADDR_EXT_EN;                  /* BIT(26) */
		val |= MT_GLO_CFG_OMIT_TX_INFO;                 /* BIT(28) */
		val |= MT_GLO_CFG_CLK_GATE_DIS;                 /* BIT(30) */
		mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
	}

	/* GLO_CFG_EXT1 BIT(28) — 无条件设置 (步骤 5-6)
	 * 来源: Windows MT6639WpdmaConfig — 0xd42b4 |= 0x10000000
	 * 确定性: 100% */
	val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
	val |= MT_WPDMA_GLO_CFG_EXT1_WIN;  /* BIT(28) */
	mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, val);

	dev_info(&dev->pdev->dev,
		 "WpdmaConfig(%s): GLO_CFG=0x%08x EXT1=0x%08x\n",
		 enable ? "enable" : "disable",
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG),
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1));
}

/* ------- 7e. ConfigIntMask — Windows 中断掩码 -------
 * 来源: Windows MT6639ConfigIntMask (FUN_1401e43e0)
 * register_playbook.md 行 174-196 */
static void mt7927_config_int_mask(struct mt7927_dev *dev, bool enable)
{
	/* Windows: INT_ENA = 0x2600f000
	 * BIT(29) = MCU2HOST_SW_INT
	 * BIT(25) = 未确定
	 * BIT(15:12) = RX ring 4/5/6/7 完成中断 */
	if (enable)
		mt7927_wr(dev, MT_WFDMA_INT_ENA_SET,
			  MT_WFDMA_INT_MASK_WIN);  /* 0xd4228 */
	else
		mt7927_wr(dev, MT_WFDMA_INT_ENA_CLR,
			  MT_WFDMA_INT_MASK_WIN);  /* 0xd422c */

	/* MCU2HOST_SW_INT_ENA 设置 (团队修复次要项)
	 * 来源: 旧驱动验证的工作配置 */
	if (enable)
		mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, BIT(0));

	/* MT6639/MT7927 芯片特定中断: 0x74030188 BIT(16)
	 * 来源: register_playbook.md 行 165-168
	 * 注意: BAR0 映射需确认, 先尝试 0x010188 */
	{
		u32 chip_int = mt7927_rr(dev, MT_CONN_INFRA_30188);

		if (chip_int != 0xffffffff) {
			if (enable)
				chip_int |= MT_CONN_INFRA_30188_BIT16;
			else
				chip_int &= ~MT_CONN_INFRA_30188_BIT16;
			mt7927_wr(dev, MT_CONN_INFRA_30188, chip_int);
		}
	}

	dev_info(&dev->pdev->dev,
		 "ConfigIntMask(%s): INT_ENA=0x%08x\n",
		 enable ? "enable" : "disable",
		 mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA));
}

/* ------- 7f. FWDL 主函数 -------
 * 来源: 已验证代码 mt7927_init_dma.c 行 1145-1307
 * 流程: SET_OWN/CLR_OWN → patch_sem → init_dl → scatter → finish
 *       → RAM: init_dl → scatter → FW_START → poll fw_sync */
static int mt7927_load_patch(struct mt7927_dev *dev)
{
	const struct mt76_connac2_patch_hdr *hdr;
	const struct firmware *fw;
	int i, ret;
	u32 max_len = 0x800;  /* 2048 字节 (团队修复次要项) */

	ret = request_firmware(&fw, MT7927_FW_PATCH, &dev->pdev->dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "补丁固件加载失败: %d\n", ret);
		return ret;
	}

	if (!fw || fw->size < sizeof(*hdr)) {
		ret = -EINVAL;
		goto out;
	}

	hdr = (const void *)fw->data;
	dev_info(&dev->pdev->dev, "PATCH build %.16s\n", hdr->build_date);

	ret = mt7927_mcu_patch_sem_ctrl(dev, true);
	if (ret)
		goto out;

	for (i = 0; i < be32_to_cpu(hdr->desc.n_region); i++) {
		const struct mt76_connac2_patch_sec *sec;
		u32 addr, len, mode, sec_info;
		const u8 *dl;

		sec = (const void *)(fw->data + sizeof(*hdr) +
				     i * sizeof(*sec));
		if ((be32_to_cpu(sec->type) & PATCH_SEC_TYPE_MASK) !=
		    PATCH_SEC_TYPE_INFO) {
			ret = -EINVAL;
			break;
		}

		addr = be32_to_cpu(sec->info.addr);
		len = be32_to_cpu(sec->info.len);
		dl = fw->data + be32_to_cpu(sec->offs);
		sec_info = be32_to_cpu(sec->info.sec_key_idx);
		mode = mt7927_patch_dl_mode(sec_info);

		ret = mt7927_mcu_init_download(dev, addr, len, mode);
		if (ret)
			break;

		ret = mt7927_mcu_send_firmware_chunks(dev, dl, len, max_len);
		if (ret)
			break;
	}

	if (!ret)
		ret = mt7927_mcu_start_patch(dev);

	mt7927_mcu_patch_sem_ctrl(dev, false);
out:
	release_firmware(fw);
	return ret;
}

static int mt7927_load_ram(struct mt7927_dev *dev)
{
	const struct mt76_connac2_fw_trailer *tr;
	const struct firmware *fw;
	u32 override = 0, option = 0;
	int ret, i, offset = 0;
	u32 max_len = MT_FWDL_MAX_LEN;

	ret = request_firmware(&fw, MT7927_FW_RAM, &dev->pdev->dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "RAM 固件加载失败: %d\n", ret);
		return ret;
	}

	if (!fw || fw->size < sizeof(*tr)) {
		ret = -EINVAL;
		goto out;
	}

	tr = (const void *)(fw->data + fw->size - sizeof(*tr));
	dev_info(&dev->pdev->dev, "RAM fw %.10s build %.15s\n",
		 tr->fw_ver, tr->build_date);

	for (i = 0; i < tr->n_region; i++) {
		const struct mt76_connac2_fw_region *region;
		u32 len, addr, mode;

		region = (const void *)((const u8 *)tr -
			 (tr->n_region - i) * sizeof(*region));
		len = le32_to_cpu(region->len);
		addr = le32_to_cpu(region->addr);
		mode = mt7927_ram_dl_mode(region->feature_set);

		dev_info(&dev->pdev->dev,
			 "RAM region %d: addr=0x%08x len=0x%x feature=0x%02x mode=0x%08x\n",
			 i, addr, len, region->feature_set, mode);

		if (region->feature_set & FW_FEATURE_NON_DL)
			goto next;

		if (region->feature_set & FW_FEATURE_OVERRIDE_ADDR)
			override = addr;

		ret = mt7927_mcu_init_download(dev, addr, len, mode);
		if (ret)
			goto out;

		ret = mt7927_mcu_send_firmware_chunks(dev, fw->data + offset,
						     len, max_len);
		if (ret)
			goto out;
next:
		offset += len;
	}

	if (override)
		option |= FW_START_OVERRIDE;

	ret = mt7927_mcu_start_firmware(dev, override, option);
out:
	release_firmware(fw);
	return ret;
}

/* 完整 FWDL 流程 */
static int mt7927_fw_download(struct mt7927_dev *dev)
{
	u32 fw_sync;
	int ret, i;

	dev_info(&dev->pdev->dev, "===== FWDL 开始 =====\n");

	/* 注意: SET_OWN/CLR_OWN 已在 probe 阶段完成 (团队修复 Task #4)
	 * rings 已经在 CLR_OWN 后配置好，这里不再重复 */

	/* DMASHDL bypass */
	mt7927_rmw(dev, MT_HIF_DMASHDL_SW_CONTROL,
		   0, MT_HIF_DMASHDL_BYPASS_EN);

	/* NEED_REINIT 标志 */
	mt7927_rmw(dev, MT_MCU_WPDMA0_DUMMY_CR, 0, MT_WFDMA_NEED_REINIT);

	wmb();
	msleep(10);

	/* 补丁下载 */
	ret = mt7927_load_patch(dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "补丁下载失败: %d\n", ret);
		return ret;
	}

	/* RAM 下载 + FW_START */
	ret = mt7927_load_ram(dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "RAM 下载失败: %d\n", ret);
		return ret;
	}

	/* 轮询 fw_sync=0x3 — 固件完全启动
	 * 来源: Windows — poll 0x7c0600f0 for 0x3 */
	for (i = 0; i < 500; i++) {
		fw_sync = mt7927_rr(dev, MT_CONN_ON_MISC);
		if ((fw_sync & 0x3) == 0x3) {
			dev_info(&dev->pdev->dev,
				 "fw_sync=0x%x, 固件启动成功 (%d ms)\n",
				 fw_sync, i);
			dev->fw_loaded = true;
			dev->fw_sync = fw_sync;
			break;
		}
		usleep_range(1000, 2000);
	}

	if (!dev->fw_loaded) {
		dev_err(&dev->pdev->dev,
			"fw_sync 超时: 0x%08x (期望 0x3)\n", fw_sync);
		return -ETIMEDOUT;
	}

	/* 注意: 不做 post-FWDL DMA 重置/reprogram!
	 * Windows 在 FWDL 后只做 DMASHDL enable, 不重置 DMA。
	 * 在新的初始化流程中, CLR_OWN 在 probe 阶段完成,
	 * FWDL 不触发 CLR_OWN, 所以 rings 不会被清零。
	 * 重置 DMA 指针会破坏固件已缓存的 DMA 状态! */

	dev_info(&dev->pdev->dev, "===== FWDL 完成 =====\n");
	return 0;
}

/* EFUSE_CTRL / EEPROM (CID=0x2d, tag=0x02)
 * 来源: mt7925 — 在 5d87f81 中验证工作 (scan 61 BSS)
 * 删除后 scan 回归, 恢复之 */
static int mt7927_mcu_set_eeprom(struct mt7927_dev *dev)
{
	struct {
		u8 _rsv[4];
		__le16 tag;
		__le16 len;
		u8 buffer_mode;
		u8 format;
		__le16 buf_len;
	} __packed req = {
		.tag = cpu_to_le16(0x02),  /* UNI_EFUSE_BUFFER_MODE */
		.len = cpu_to_le16(sizeof(req) - 4),
		.buffer_mode = 0,  /* EE_MODE_EFUSE */
		.format = 1,       /* EE_FORMAT_WHOLE */
	};
	int ret;

	dev_info(&dev->pdev->dev, "EFUSE_CTRL: mode=%d format=%d\n",
		 req.buffer_mode, req.format);

	ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_EFUSE_CTRL,
				      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
				      &req, sizeof(req));
	if (ret)
		dev_warn(&dev->pdev->dev, "EFUSE_CTRL 失败: %d\n", ret);

	return ret;
}

/* ------- 7f-2. DMASHDL 完整初始化 -------
 * 参考: mt6639/chips/mt6639/hal_dmashdl_mt6639.c mt6639DmashdlInit()
 * 配置 DMA scheduler 队列映射、组配额、refill 控制
 *
 * 关键发现: BYPASS=1 (FWDL 阶段设置) 从未清除，导致固件无法正确路由
 * TX 包到 LMAC。MIB TX 计数器全零 = 帧从未到达射频。
 *
 * MT6639 PCIe 配置:
 *   - 3 个活跃组 (0,1,2): max=0xfff, min=0x10
 *   - Queue 16 (ALTX/管理帧) → Group 0
 *   - HIF_GUP_ACT_MAP=0x8007: rings 0,1,2,15 启用调度
 */
static void mt7927_dmashdl_init(struct mt7927_dev *dev)
{
	int i;

	/* 1. Packet max page size: PLE=0x1, PSE=0x18
	 * PLE_PACKET_MAX_SIZE[11:0], PSE_PACKET_MAX_SIZE[27:16] */
	mt7927_wr(dev, MT_HIF_DMASHDL_PKT_MAX_SIZE,
		  (0x18 << 16) | 0x1);

	/* 2. Group quotas: 16 groups
	 * GROUP_CONTROL: MAX_QUOTA[27:16], MIN_QUOTA[11:0]
	 * Group 0-2: max=0xfff, min=0x10 (active for data/mgmt traffic)
	 * Groups 3-14: max=0, min=0 (unused)
	 * Group 15: max=0x30, min=0 (reserved) */
	mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(0), 0x0fff0010);
	mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(1), 0x0fff0010);
	mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(2), 0x0fff0010);
	for (i = 3; i < 15; i++)
		mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i), 0);
	mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(15), 0x00300000);

	/* 3. Queue-to-group mapping (32 queues, 4 bits each)
	 * MT6639 PCIe layout — critical: Q16(ALTX)→Grp0 for mgmt frames
	 *
	 * QMAP0 (Q0-7):  Q0-2→G0, Q3→G2, Q4-6→G1, Q7→G2 = 0x21112000
	 * QMAP1 (Q8-15): all→G0                            = 0x00000000
	 * QMAP2 (Q16-23): Q16-18→G0, Q19-23→G1             = 0x11111000
	 * QMAP3 (Q24-31): Q24-26→G0, Q27-31→G1             = 0x11111000 */
	mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP0, 0x21112000);
	mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP1, 0x00000000);
	mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP2, 0x11111000);
	mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP3, 0x11111000);

	/* 4. Refill control: enable for groups 0,1,2; disable for 3-15
	 * REFILL_DISABLE bit at BIT(16+N) — set=disabled, clear=enabled */
	mt7927_rmw(dev, MT_HIF_DMASHDL_REFILL_CONTROL,
		   GENMASK(31, 16), 0xfff80000);

	/* 5. Priority-to-group: direct mapping (priority N → group N) */
	mt7927_wr(dev, MT_HIF_DMASHDL_SCHED_SET0, 0x76543210);
	mt7927_wr(dev, MT_HIF_DMASHDL_SCHED_SET1, 0xfedcba98);

	/* 6. Slot arbiter: disabled (GROUP_SEQUENCE_ORDER_TYPE[16]=0) */
	mt7927_rmw(dev, MT_HIF_DMASHDL_PAGE_SETTING, BIT(16), 0);

	/* 7. Optional control: HIF_ACK_CNT_TH=4, HIF_GUP_ACT_MAP=0x8007
	 * MT6639 PCIe 参考 (hal_dmashdl_mt6639.h):
	 *   MT6639_DMASHDL_HIF_GUP_ACT_MAP = 0x8007
	 * GUP_ACT_MAP [15:0]: BIT(0)|BIT(1)|BIT(2)|BIT(15) = rings 0,1,2 + ring 15
	 * Ring 15 CMD 流量 (Q_IDX=0x20 MCU TX port) 通过 DMASHDL MCU path 路由
	 * OPTIONAL_CONTROL = ACK_TH(4) << 16 | GUP_MAP(0x8007) = 0x00048007 */
	mt7927_wr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL, 0x00048007);

	/* 8. DMASHDL BYPASS 保持 ON (固件自行设置, 不可清除)
	 * 固件启动后 SW=0x10000000 即 BYPASS_EN=1, 这是固件的主动设置。
	 * 若清除 BYPASS_EN: ring 15 Q_IDX=0x20(=32) 超出 DMASHDL 队列映射范围(0-31),
	 * 所有 MCU 命令全部超时 (ring15 未被消费), PostFwDownloadInit 完全失败。
	 * Windows 同样使用 BYPASS=1 (Session 15 验证)。
	 * 结论: BYPASS=1 是 MT7927/MT6639 PCIe 的正确配置, 不影响数据 TX。 */

	dev_info(&dev->pdev->dev,
		 "DMASHDL init: SW=0x%08x GRP0=0x%08x QMAP0=0x%08x OPT=0x%08x\n",
		 mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL),
		 mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(0)),
		 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0),
		 mt7927_rr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL));
}

/* ------- 7g. PostFwDownloadInit -------
 * 来源: Windows AsicConnac3xPostFwDownloadInit (FUN_1401c9510)
 * register_playbook.md 行 240-266 */
static int mt7927_post_fw_init(struct mt7927_dev *dev)
{
	u32 val;
	int ret;

	dev_info(&dev->pdev->dev, "===== PostFwDownloadInit 开始 =====\n");

	/* Step 0+1: DMASHDL full init (mt6639 格式) + Windows enable bits
	 *
	 * Session 20 bisect: full init 必需 (跳过 → scan 0 BSS)
	 * Session 21 发现: dmashdl_init() hard-write QUEUE_MAP0=0x21112000
	 *   覆盖了 Windows 的 |= 0x10101 enable bits!
	 *   BIT(0)+BIT(8)+BIT(16) 控制 HOST→FW 数据通道使能
	 *   缺失导致 Ring 2 mgmt 帧 "DMA 消费成功但固件静默"
	 * 修复: 先 full init，再 OR enable bits (Windows 顺序)
	 */
	mt7927_dmashdl_init(dev);

	/* Windows: readl(0xd6060) |= 0x10101 — DMASHDL init 之后! */
	{
		u32 dmashdl_qm0 = mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0);

		dmashdl_qm0 |= 0x10101;  /* BIT(0) | BIT(8) | BIT(16) */
		mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP0, dmashdl_qm0);

		dev_info(&dev->pdev->dev,
			 "DMASHDL QUEUE_MAP0: 0x%08x (enable bits applied)\n",
			 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0));
	}

	/* 重新启用 WpdmaConfig (固件启动后) */
	mt7927_wpdma_config(dev, true);

	/* 关闭 FWDL bypass (固件已启动, 使用正常 DMASHDL 路由) */
	val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
	val &= ~MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
	mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

	dev_info(&dev->pdev->dev,
		 "PostFwInit: GLO_CFG=0x%08x (bypass cleared)\n",
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG));

	/* 重新设置中断掩码 */
	mt7927_config_int_mask(dev, true);

	/* Step 2: NIC_CAPABILITY — UniCmd 格式!
	 * 来源: Windows RE dispatch table (0x1402507e0 entry)
	 *   outer_tag=0x8a (dispatch key), inner_CID=0x0e (写入 UniCmd header)
	 *   payload=NULL, len=0 — 纯查询, 无 payload!
	 * ⚠️ 历史错误: 之前用 0x8a 作为 header CID, 实际上 0x8a 是 dispatch 路由 key
	 *   Windows 写入 header 的 CID = 0x0e (现已修正为 UNI_CMD_ID_NIC_CAP=0x0e) */
	dev_info(&dev->pdev->dev, "发送 NIC_CAPABILITY (UniCmd inner_CID=0x0e)\n");
	{
		/* NIC_CAP 无 payload — Windows RE 确认:
		 * nicUniCmdNicCapability: mov dl, 0x0e → call nicUniCmdAllocEntry
		 * option=0x07: BIT(1)=UNI + BIT(0)=ACK + BIT(2)=need_response */
		ret = mt7927_mcu_send_unicmd(dev, UNI_CMD_ID_NIC_CAP,
					      UNI_CMD_OPT_SET_ACK,
					      NULL, 0);
		if (ret && ret != -ETIMEDOUT)
			dev_warn(&dev->pdev->dev,
				 "NIC_CAP 失败: %d (继续)\n", ret);
	}

	/* Step 3: Config 命令 (CID=0x02, option=0x06 fire-and-forget)
	 * Windows RE (win_re_class02_and_postinit.md): outer=0x02, 12字节 payload
	 * 之前跳过原因: 用了 option=0x07 (等待响应), 固件不回复导致 MCU 通道阻塞
	 * 正确做法: option=0x06 fire-and-forget, 固件处理后不发响应
	 * Payload: {u16=0x0001, u8=0, u8=0, u32=0x00070000, u32=0} */
	dev_info(&dev->pdev->dev, "发送 Config 命令 (CID=0x02, fire-and-forget)\n");
	{
		struct {
			__le16 field0;	/* = 0x0001 */
			u8     field2;	/* = 0x00  */
			u8     pad;	/* = 0x00  */
			__le32 field4;	/* = 0x00070000 */
			__le32 field8;	/* = 0x00000000 */
		} __packed config_payload = {
			.field0 = cpu_to_le16(0x0001),
			.field4 = cpu_to_le32(0x00070000),
		};

		ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_CONFIG,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      &config_payload, sizeof(config_payload));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "Config 命令失败: %d (继续)\n", ret);
	}

	/* Step 4: WFDMA_CFG 命令 (inner CID=0x0d, outer tag=0xc0)
	 * 来源: Windows dispatch table — outer 0xc0 → inner 0x0d
	 * Data: {0x820cc800, 0x3c200} */
	dev_info(&dev->pdev->dev, "发送 WFDMA_CFG (CID=0x0d)\n");
	{
		__le32 cfg_data[2] = {
			cpu_to_le32(0x820cc800),
			cpu_to_le32(0x3c200),
		};

		ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_WFDMA_CFG,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      cfg_data, sizeof(cfg_data));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "WFDMA_CFG 0x0d 失败: %d (继续)\n", ret);
	}
	/* Step 5: DBDC 设置 (class=0x28, MT6639/MT7927 only)
	 * 来源: Windows RE — MtCmdUpdateDBDCSetting (FUN_1400d3c40)
	 *   payload = 36 字节 (0x24), 初始调用参数: dbdc_en=1, rfBand=0
	 * 参考: mt6639/nic_uni_cmd_event.c nicUniCmdSetMbmc()
	 *        mt76/mt7925/mcu.c mt7925_mcu_set_dbdc()
	 * UniCmd TLV 格式:
	 *   UNI_CMD_MBMC header (4 字节 reserved)
	 *   + UNI_CMD_MBMC_SETTING TLV (tag=0, len=8, mbmc_en=1, rfBand=0)
	 * SET 命令, option=0x06, fire-and-forget */
	dev_info(&dev->pdev->dev, "发送 DBDC 设置 (class=0x28, MT6639/MT7927)\n");
	{
		struct {
			u8 rsv[4];              /* UNI_CMD_MBMC header */
			__le16 tag;             /* UNI_CMD_MBMC_TAG_SETTING = 0 */
			__le16 len;             /* sizeof(this TLV) = 8 */
			u8 mbmc_en;             /* 1 = 启用 DBDC */
			u8 rf_band;             /* 0 = unused */
			u8 pad[2];
		} __packed dbdc_payload;

		memset(&dbdc_payload, 0, sizeof(dbdc_payload));
		dbdc_payload.tag = cpu_to_le16(0);  /* UNI_CMD_MBMC_TAG_SETTING */
		dbdc_payload.len = cpu_to_le16(8);  /* TLV 长度: tag(2)+len(2)+data(4) */
		dbdc_payload.mbmc_en = 1;           /* 启用 DBDC */
		dbdc_payload.rf_band = 0;           /* unused */

		ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_DBDC,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      &dbdc_payload, sizeof(dbdc_payload));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "DBDC 0x28 失败: %d (继续)\n", ret);
	}
	/* Step 6: 1ms 延迟 (Windows: KeStallExecutionProcessor(10)*100) */
	usleep_range(1000, 2000);

	/* Step 7: SetPassiveToActiveScan (class=0xca)
	 * 来源: Ghidra RE — AsicConnac3xSetPassiveToActiveScan
	 *   class=0xca, target=0xed, 字符串 "PassiveToActiveScan"
	 * 参考: mt7925/mcu.c mt7925_mcu_chip_config()
	 *   payload = {rsv[4], TLV(tag=2, len), mt76_connac_config}
	 *   mt76_connac_config: {id, type, resp_type, data_size, resv, data[320]}
	 * SET 命令, option=0x06, fire-and-forget */
	dev_info(&dev->pdev->dev, "发送 ScanConfig (class=0xca, PassiveToActiveScan)\n");
	{
		struct {
			u8 rsv[4];              /* UniCmd CHIP_CONFIG header */
			__le16 tag;             /* UNI_CHIP_CONFIG_CHIP_CFG = 2 */
			__le16 len;             /* TLV 长度 */
			__le16 id;              /* config id */
			u8 type;                /* config type */
			u8 resp_type;           /* response type */
			__le16 data_size;       /* string length */
			__le16 data_resv;       /* reserved */
			u8 data[320];           /* config string */
		} __packed scan_cfg;
		const char *scan_str = "PassiveToActiveScan";
		u16 str_len = strlen(scan_str) + 1;

		memset(&scan_cfg, 0, sizeof(scan_cfg));
		scan_cfg.tag = cpu_to_le16(2);  /* UNI_CHIP_CONFIG_CHIP_CFG */
		scan_cfg.len = cpu_to_le16(sizeof(scan_cfg) - 4);
		scan_cfg.data_size = cpu_to_le16(str_len);
		memcpy(scan_cfg.data, scan_str, str_len);

		ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_SCAN_CFG,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      &scan_cfg, sizeof(scan_cfg));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "ScanConfig 失败: %d (继续)\n", ret);
	}
	/* Step 8: SetFWChipConfig (class=0xca)
	 * 来源: Ghidra RE — AsicConnac3xSetFWChipConfig
	 *   class=0xca, target=0xed, 配置字符串
	 *   格式: 同 PassiveToActiveScan — UniCmd CHIP_CONFIG TLV (tag=2)
	 *   mt7925 参考: mt7925_mcu_chip_config() 发送 "KeepFullPwr 0" 等字符串
	 * ⚠️ Windows RE 未提供此初始化阶段的确切字符串内容
	 *   Ghidra 只说 "String config + 0x148 byte payload"
	 *   mt7925 初始化时发送 "KeepFullPwr 0" 禁用深度睡眠 — 此处复用
	 * SET 命令, option=0x06, fire-and-forget */
	dev_info(&dev->pdev->dev, "发送 ChipConfig (class=0xca, KeepFullPwr)\n");
	{
		struct {
			u8 rsv[4];              /* UniCmd CHIP_CONFIG header */
			__le16 tag;             /* UNI_CHIP_CONFIG_CHIP_CFG = 2 */
			__le16 len;             /* TLV 长度 */
			__le16 id;              /* config id */
			u8 type;                /* config type */
			u8 resp_type;           /* response type */
			__le16 data_size;       /* string length */
			__le16 data_resv;       /* reserved */
			u8 data[320];           /* config string */
		} __packed chip_cfg;
		/* mt7925: set_deep_sleep(false) → "KeepFullPwr 1" = 保持全功率 = 禁用深睡眠
		 * "KeepFullPwr 0" = 允许深度睡眠 (错误! 会导致固件不发帧) */
		const char *cfg_str = "KeepFullPwr 1";
		u16 str_len = strlen(cfg_str) + 1;

		memset(&chip_cfg, 0, sizeof(chip_cfg));
		chip_cfg.tag = cpu_to_le16(2);  /* UNI_CHIP_CONFIG_CHIP_CFG */
		chip_cfg.len = cpu_to_le16(sizeof(chip_cfg) - 4);
		chip_cfg.data_size = cpu_to_le16(str_len);
		memcpy(chip_cfg.data, cfg_str, str_len);

		ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_SCAN_CFG,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      &chip_cfg, sizeof(chip_cfg));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "ChipConfig 失败: %d (继续)\n", ret);
	}
	/* Step 9: SetLogLevelConfig (class=0xca)
	 * 来源: Ghidra RE — AsicConnac3xSetLogLevelConfig
	 *   class=0xca, target=0xed, 字符串 "EvtDrvnLogCatLvl"
	 *   格式: 同 PassiveToActiveScan — UniCmd CHIP_CONFIG TLV (tag=2)
	 * Windows RE 确认字符串名称 "EvtDrvnLogCatLvl" + 格式字符串
	 * mt6639 参考: wlanDbgSetLogLevelImpl() 设置 FW 日志等级
	 * 默认: "EvtDrvnLogCatLvl 0" (ENUM_WIFI_LOG_LEVEL_DEFAULT=0)
	 * SET 命令, option=0x06, fire-and-forget */
	dev_info(&dev->pdev->dev, "发送 LogConfig (class=0xca, EvtDrvnLogCatLvl)\n");
	{
		struct {
			u8 rsv[4];              /* UniCmd CHIP_CONFIG header */
			__le16 tag;             /* UNI_CHIP_CONFIG_CHIP_CFG = 2 */
			__le16 len;             /* TLV 长度 */
			__le16 id;              /* config id */
			u8 type;                /* config type */
			u8 resp_type;           /* response type */
			__le16 data_size;       /* string length */
			__le16 data_resv;       /* reserved */
			u8 data[320];           /* config string */
		} __packed log_cfg;
		/* Ghidra RE 字符串: "EvtDrvnLogCatLvl"
		 * 格式推测: "EvtDrvnLogCatLvl 0" (默认日志等级)
		 * TODO: 如果固件不认识此格式, 可能需要进一步 RE 确认参数 */
		const char *log_str = "EvtDrvnLogCatLvl 0";
		u16 str_len = strlen(log_str) + 1;

		memset(&log_cfg, 0, sizeof(log_cfg));
		log_cfg.tag = cpu_to_le16(2);  /* UNI_CHIP_CONFIG_CHIP_CFG */
		log_cfg.len = cpu_to_le16(sizeof(log_cfg) - 4);
		log_cfg.data_size = cpu_to_le16(str_len);
		memcpy(log_cfg.data, log_str, str_len);

		ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_SCAN_CFG,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      &log_cfg, sizeof(log_cfg));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "LogConfig 失败: %d (继续)\n", ret);
	}
	/* Step 10: EFUSE_CTRL — 恢复 (CID=0x2d, mt7925 格式)
	 * 5d87f81 中存在且 scan 工作 (61 BSS), 删除后 scan 回归。
	 * 注: 用旧 CID=0x2d (mt7925) 有效, 新 CID=0x05 (Windows inner) 会破坏 RF。 */
	mt7927_mcu_set_eeprom(dev);

	/* 诊断: 检查所有 HOST RX 环的 DIDX 变化 */
	dev_info(&dev->pdev->dev,
		 "PostFwInit RX status: RX4 DIDX=%u RX6 DIDX=%u RX7 DIDX=%u\n",
		 mt7927_rr(dev, MT_WFDMA_HOST_RX_RING4_DIDX),
		 mt7927_rr(dev, MT_WFDMA_HOST_RX_RING6_DIDX),
		 mt7927_rr(dev, MT_WFDMA_HOST_RX_RING7_DIDX));
	dev_info(&dev->pdev->dev,
		 "PostFwInit MCU: MCU_RX0 BASE=0x%08x INT_STA=0x%08x MCU_CMD=0x%08x\n",
		 mt7927_rr(dev, 0x02500),  /* MCU_RX0 BASE */
		 mt7927_rr(dev, MT_WFDMA_HOST_INT_STA),
		 mt7927_rr(dev, MT_MCU_CMD_REG));

	dev_info(&dev->pdev->dev, "===== PostFwDownloadInit 完成 =====\n");
	return 0;
}

/* =====================================================================
 * 7h. 扫描前置 MCU 命令 — 固件不收到这些命令会静默丢弃 scan 请求
 * ===================================================================== */

/* SET_DOMAIN_INFO (CID=0x15) — 告诉固件合法信道列表
 * 来源: mt7925 TLV 格式 — 在 5d87f81 中验证工作 (61 BSS)
 * 注意: Windows RE inner CID=0x03 + 76B 固定格式在 784be1d 中测试失败 (0 BSS)
 * 保留 mt7925 TLV 格式直到进一步 RE 确认
 * payload 格式:
 *   hdr: alpha2[4], bw_2g, bw_5g, bw_6g, pad
 *   TLV(tag=2): n_2ch, n_5ch, n_6ch, pad
 *   信道列表: {hw_value(le16), pad(le16), flags(le32)} × N
 */
static int mt7927_mcu_set_channel_domain(struct mt7927_dev *dev)
{
	struct {
		u8 alpha2[4];
		u8 bw_2g;
		u8 bw_5g;
		u8 bw_6g;
		u8 pad;
	} __packed hdr;
	struct {
		__le16 tag;
		__le16 len;
		u8 n_2ch;
		u8 n_5ch;
		u8 n_6ch;
		u8 pad;
	} __packed n_ch;
	struct {
		__le16 hw_value;
		__le16 pad;
		__le32 flags;
	} __packed chan_entry;

	int n_2ch = ARRAY_SIZE(mt7927_2ghz_channels);
	int n_5ch = ARRAY_SIZE(mt7927_5ghz_channels);
	int total_ch = n_2ch + n_5ch;
	size_t buf_len = sizeof(hdr) + sizeof(n_ch) +
			 total_ch * sizeof(chan_entry);
	u8 *buf, *ptr;
	int i, ret;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* 填充 header */
	memset(&hdr, 0, sizeof(hdr));
	hdr.alpha2[0] = 'C';
	hdr.alpha2[1] = 'N';  /* China domain — 匹配实际使用环境 */
	hdr.bw_2g = 0;        /* BW_20_40M */
	hdr.bw_5g = 3;        /* BW_20_40_80_160M */
	hdr.bw_6g = 3;

	/* 填充 TLV header */
	memset(&n_ch, 0, sizeof(n_ch));
	n_ch.tag = cpu_to_le16(2);  /* UNI_CMD_CHANNEL_DOMAIN_SET_DOMAIN_INFO = 2 */
	n_ch.len = cpu_to_le16(sizeof(n_ch) +
			       total_ch * sizeof(chan_entry));
	n_ch.n_2ch = n_2ch;
	n_ch.n_5ch = n_5ch;
	n_ch.n_6ch = 0;

	/* 组装 buffer */
	ptr = buf;
	memcpy(ptr, &hdr, sizeof(hdr));
	ptr += sizeof(hdr);
	memcpy(ptr, &n_ch, sizeof(n_ch));
	ptr += sizeof(n_ch);

	/* 2.4GHz channels */
	for (i = 0; i < n_2ch; i++) {
		memset(&chan_entry, 0, sizeof(chan_entry));
		chan_entry.hw_value = cpu_to_le16(
			mt7927_2ghz_channels[i].hw_value);
		chan_entry.flags = cpu_to_le32(
			mt7927_2ghz_channels[i].flags);
		memcpy(ptr, &chan_entry, sizeof(chan_entry));
		ptr += sizeof(chan_entry);
	}

	/* 5GHz channels */
	for (i = 0; i < n_5ch; i++) {
		memset(&chan_entry, 0, sizeof(chan_entry));
		chan_entry.hw_value = cpu_to_le16(
			mt7927_5ghz_channels[i].hw_value);
		chan_entry.flags = cpu_to_le32(
			mt7927_5ghz_channels[i].flags);
		memcpy(ptr, &chan_entry, sizeof(chan_entry));
		ptr += sizeof(chan_entry);
	}

	dev_info(&dev->pdev->dev,
		 "SET_DOMAIN_INFO: n_2ch=%d n_5ch=%d total_len=%zu\n",
		 n_2ch, n_5ch, buf_len);

	ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_SET_DOMAIN,
				      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
				      buf, buf_len);
	kfree(buf);

	if (ret)
		dev_warn(&dev->pdev->dev,
			 "SET_DOMAIN_INFO 失败: %d\n", ret);

	return ret;
}

/* BAND_CONFIG / RTS_THRESHOLD (CID=0x08, tag=0x08)
 * 来源: mt7925 TLV 格式 — 在 5d87f81 中验证工作
 * 注意: Windows RE inner CID=0x49 + 92B 固定格式在 784be1d 中测试失败
 * 保留 mt7925 TLV 格式直到进一步 RE 确认
 */
static int mt7927_mcu_set_rts_thresh(struct mt7927_dev *dev, u32 val)
{
	struct {
		u8 band_idx;
		u8 _rsv[3];
		__le16 tag;
		__le16 len;
		__le32 len_thresh;
		__le32 pkt_thresh;
	} __packed req = {
		.band_idx = 0,
		.tag = cpu_to_le16(0x08),  /* UNI_BAND_CONFIG_RTS_THRESHOLD */
		.len = cpu_to_le16(sizeof(req) - 4),
		.len_thresh = cpu_to_le32(val),
		.pkt_thresh = cpu_to_le32(0x02),
	};
	int ret;

	dev_info(&dev->pdev->dev,
		 "BAND_CONFIG RTS_THRESH: thresh=0x%x pkt=0x%x\n",
		 val, 0x02);

	ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_BAND_CONFIG,
				      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
				      &req, sizeof(req));
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "BAND_CONFIG RTS 失败: %d\n", ret);

	return ret;
}

/* 注: EFUSE_CTRL 使用 mt7925 CID=0x2d + TLV 格式，已恢复 (在上方定义)。
 * Windows inner CID=0x05 会破坏 RF 配置，不要使用。 */

/* =====================================================================
 * 8. 诊断 dump 函数
 * ===================================================================== */

static void mt7927_dump_status(struct mt7927_dev *dev)
{
	int r;

	dev_info(&dev->pdev->dev,
		 "===== 关键寄存器状态 =====\n");
	dev_info(&dev->pdev->dev,
		 "GLO_CFG=0x%08x EXT0=0x%08x EXT1=0x%08x\n",
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG),
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT0),
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1));
	dev_info(&dev->pdev->dev,
		 "INT_STA=0x%08x INT_ENA=0x%08x MCU_CMD=0x%08x\n",
		 mt7927_rr(dev, MT_WFDMA_HOST_INT_STA),
		 mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA),
		 mt7927_rr(dev, MT_MCU_CMD_REG));
	dev_info(&dev->pdev->dev,
		 "DMASHDL_SW=0x%08x fw_sync=0x%08x ROMCODE=0x%08x\n",
		 mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL),
		 mt7927_rr(dev, MT_CONN_ON_MISC),
		 mt7927_rr(dev, MT_ROMCODE_INDEX));

	/* TX ring 15 状态 */
	dev_info(&dev->pdev->dev,
		 "TX15: BASE=0x%08x CNT=%u CIDX=%u DIDX=%u\n",
		 mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(15)),
		 mt7927_rr(dev, MT_WPDMA_TX_RING_CNT(15)),
		 mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(15)),
		 mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(15)));

	/* 所有 RX ring 状态 */
	for (r = 0; r <= 7; r++) {
		u32 base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(r));

		if (base == 0)
			continue;
		dev_info(&dev->pdev->dev,
			 "RX%d: BASE=0x%08x CNT=%u CIDX=%u DIDX=%u\n",
			 r, base,
			 mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(r)),
			 mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(r)),
			 mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(r)));
	}

	/* MCU DMA0 RX rings (host→MCU 方向) */
	{
		int i;

		for (i = 0; i < 4; i++) {
			u32 mbase = ioread32(dev->bar0 + 0x02500 + (i << 4));

			dev_info(&dev->pdev->dev,
				 "MCU_RX%d: BASE=0x%08x CNT=%u CIDX=%u DIDX=%u\n",
				 i, mbase,
				 ioread32(dev->bar0 + 0x02504 + (i << 4)),
				 ioread32(dev->bar0 + 0x02508 + (i << 4)),
				 ioread32(dev->bar0 + 0x0250c + (i << 4)));
		}
	}

	/* 预取配置 */
	dev_info(&dev->pdev->dev,
		 "Prefetch: CFG0=0x%08x CFG1=0x%08x CFG2=0x%08x CFG3=0x%08x\n",
		 mt7927_rr(dev, MT_WFDMA_PREFETCH_CFG0),
		 mt7927_rr(dev, MT_WFDMA_PREFETCH_CFG1),
		 mt7927_rr(dev, MT_WFDMA_PREFETCH_CFG2),
		 mt7927_rr(dev, MT_WFDMA_PREFETCH_CFG3));
}

/* =====================================================================
 * 9a. 扫描/连接 MCU 命令 + 工作队列
 * ===================================================================== */

/* 发送 UniCmd SET 命令 (fire-and-forget, 不等待响应)
 * 封装 mt7927_mcu_send_unicmd 用于 SET 命令 */
static int mt7927_mcu_send_unicmd_set(struct mt7927_dev *dev, u16 cmd_id,
				       const void *payload, size_t plen)
{
	/* 使用 option=0x06 (UNI+SET, 不等 ACK) 用于一般 SET 命令
	 * DEV_INFO/BSS_INFO 用 0x07 会导致超时 — 固件可能不对这些命令发 ACK
	 * scan 命令单独在调用处使用 0x07 */
	return mt7927_mcu_send_unicmd(dev, cmd_id, 0x06, payload, plen);
}

/* --- MAC 硬件初始化 --- */

/* WTBL UPDATE — 清除 ADM 计数器
 * 参考: mt7925/mac.c mt7925_mac_wtbl_update() */
static bool mt7927_mac_wtbl_update(struct mt7927_dev *dev, int idx, u32 mask)
{
	mt7927_wr(dev, MT_WTBL_UPDATE,
		  FIELD_PREP(MT_WTBL_UPDATE_WLAN_IDX, idx) | mask);

	/* 等待 BUSY 清除 */
	return mt7927_poll(dev, MT_WTBL_UPDATE, MT_WTBL_UPDATE_BUSY, 0, 5000);
}

/* Per-band MAC 初始化
 * 参考: mt792x_mac.c mt792x_mac_init_band() */
static void mt7927_mac_init_band(struct mt7927_dev *dev, u8 band)
{
	u32 val, mask;

	/* TMAC TX 定时控制 */
	val = mt7927_rr(dev, MT_TMAC_CTCR0(band));
	val &= ~MT_TMAC_CTCR0_INS_DDLMT_REFTIME;
	val |= FIELD_PREP(MT_TMAC_CTCR0_INS_DDLMT_REFTIME, 0x3f);
	val |= MT_TMAC_CTCR0_INS_DDLMT_VHT_SMPDU_EN |
	       MT_TMAC_CTCR0_INS_DDLMT_EN;
	mt7927_wr(dev, MT_TMAC_CTCR0(band), val);

	/* RMAC MIB RX 时间 */
	mt7927_rmw(dev, MT_WF_RMAC_MIB_TIME0(band),
		   0, MT_WF_RMAC_MIB_RXTIME_EN);
	mt7927_rmw(dev, MT_WF_RMAC_MIB_AIRTIME0(band),
		   0, MT_WF_RMAC_MIB_RXTIME_EN);

	/* MIB TX/RX duration */
	mt7927_rmw(dev, MT_MIB_SCR1(band), 0, MT_MIB_TXDUR_EN);
	mt7927_rmw(dev, MT_MIB_SCR1(band), 0, MT_MIB_RXDUR_EN);

	/* DMA per-band: max RX len + disable RXD G5 */
	val = mt7927_rr(dev, MT_DMA_DCR0(band));
	val &= ~(MT_DMA_DCR0_MAX_RX_LEN | MT_DMA_DCR0_RXD_G5_EN);
	val |= FIELD_PREP(MT_DMA_DCR0_MAX_RX_LEN, 1536);
	mt7927_wr(dev, MT_DMA_DCR0(band), val);

	/* WTBLOFF RCPI 模式 */
	mask = MT_WTBLOFF_TOP_RSCR_RCPI_MODE | MT_WTBLOFF_TOP_RSCR_RCPI_PARAM;
	val = FIELD_PREP(MT_WTBLOFF_TOP_RSCR_RCPI_MODE, 0) |
	      FIELD_PREP(MT_WTBLOFF_TOP_RSCR_RCPI_PARAM, 0x3);
	mt7927_rmw(dev, MT_WTBLOFF_TOP_RSCR(band), mask, val);
}

/* --- 速率表初始化 --- */

/* 编程 WTBL 固定速率表的单个条目
 * 参考: mt7925/mac.c mt7925_mac_set_fixed_rate_table() */
static void mt7927_mac_set_fixed_rate_table(struct mt7927_dev *dev,
					    u8 tbl_idx, u16 rate_idx)
{
	u32 ctrl = MT_WTBL_ITCR_WR | MT_WTBL_ITCR_EXEC | tbl_idx;

	mt7927_wr(dev, MT_WTBL_ITDR0, rate_idx);
	mt7927_wr(dev, MT_WTBL_ITDR1, MT_WTBL_SPE_IDX_SEL);
	mt7927_wr(dev, MT_WTBL_ITCR, ctrl);
}

/* 基本速率 hw_value (从 mt76 mac80211.c mt76_rates[] 复制)
 * 格式: (MT_PHY_TYPE_xxx << 8) | rate_idx
 *   MT_PHY_TYPE_CCK = 0, MT_PHY_TYPE_OFDM = 1 */
static const u16 mt7927_basic_rate_hw_values[] = {
	0x0000,  /* CCK 1Mbps   */
	0x0001,  /* CCK 2Mbps   */
	0x0002,  /* CCK 5.5Mbps */
	0x0003,  /* CCK 11Mbps  */
	0x010b,  /* OFDM 6Mbps  */
	0x010f,  /* OFDM 9Mbps  */
	0x010a,  /* OFDM 12Mbps */
	0x010e,  /* OFDM 18Mbps */
	0x0109,  /* OFDM 24Mbps */
	0x010d,  /* OFDM 36Mbps */
	0x0108,  /* OFDM 48Mbps */
	0x010c,  /* OFDM 54Mbps */
};

/* 初始化基本速率表 (idx 11-22)
 * 参考: mt7925/init.c mt7925_mac_init_basic_rates()
 * TXD 中 FIXED_RATE=1 + TX_RATE=15 使用的就是速率表索引 15 (OFDM 6Mbps) */
static void mt7927_mac_init_basic_rates(struct mt7927_dev *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt7927_basic_rate_hw_values); i++) {
		u16 hw_val = mt7927_basic_rate_hw_values[i];
		/* 转换 hw_value 为 WTBL 速率格式:
		 * MT_TX_RATE_MODE = GENMASK(9,6), MT_TX_RATE_IDX = GENMASK(5,0) */
		u16 rate = ((hw_val >> 8) << 6) | (hw_val & 0x3f);
		u16 idx = 11 + i;  /* MT792x_BASIC_RATES_TBL = 11 */

		mt7927_mac_set_fixed_rate_table(dev, idx, rate);
	}
	/* 验证: 读回 rate table index 15 (OFDM 6Mbps) 确认写入生效 */
	{
		u32 ctrl = MT_WTBL_ITCR_EXEC | 15; /* READ index 15 */

		mt7927_wr(dev, MT_WTBL_ITCR, ctrl);
		udelay(10);
		dev_info(&dev->pdev->dev,
			 "rate: programmed %d basic rates (idx 11-22), "
			 "verify idx15 readback: ITDR0=0x%08x (expect OFDM 6M=0x4b)\n",
			 (int)ARRAY_SIZE(mt7927_basic_rate_hw_values),
			 mt7927_rr(dev, MT_WTBL_ITDR0));
	}
}

/* MAC 全局初始化
 * 参考: mt7925/init.c mt7925_mac_init() */
static void mt7927_mac_init(struct mt7927_dev *dev)
{
	int i;

	/* 1. MDP (MAC Data Path) — 配置 RX 路径 */
	mt7927_rmw_field(dev, MT_MDP_DCR1, MT_MDP_DCR1_MAX_RX_LEN, 1536);
	mt7927_rmw(dev, MT_MDP_DCR0, 0, MT_MDP_DCR0_DAMSDU_EN);

	/* 2. 清除所有 WTBL 条目的 ADM 计数器
	 * 不清除 → 固件可能读到陈旧的 admission 计数 → 影响 TX 调度 */
	for (i = 0; i < MT7927_WTBL_SIZE; i++)
		mt7927_mac_wtbl_update(dev, i, MT_WTBL_UPDATE_ADM_COUNT_CLEAR);

	/* 3. Per-band 初始化 (TMAC/RMAC/MIB/DMA/WTBLOFF) */
	for (i = 0; i < 2; i++)
		mt7927_mac_init_band(dev, i);

	/* 4. 基本速率表 */
	mt7927_mac_init_basic_rates(dev);

	dev_info(&dev->pdev->dev,
		 "mac_init: MDP + WTBL(%d) + band(0,1) + rate_table done\n",
		 MT7927_WTBL_SIZE);
}

/* --- 扫描 MCU 命令 --- */

/* 发送 HW scan 请求 (UniCmd CID=0x16) */
static int mt7927_mcu_hw_scan(struct mt7927_dev *dev,
			      struct ieee80211_vif *vif,
			      struct ieee80211_scan_request *scan_req)
{
	struct cfg80211_scan_request *sreq = &scan_req->req;
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	u8 *buf, *pos;
	size_t buf_len;
	struct scan_hdr_tlv *hdr;
	struct scan_req_tlv *req;
	struct scan_ssid_tlv *ssid;
	struct scan_bssid_tlv *bssid;
	struct scan_chan_info_tlv *chan;
	int i, ret;

	buf_len = sizeof(*hdr) + sizeof(*req) + sizeof(*ssid) +
		  sizeof(*bssid) + sizeof(*chan);
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	pos = buf;

	/* 固定头 */
	hdr = (struct scan_hdr_tlv *)pos;
	hdr->seq_num = ++dev->scan_seq_num;
	hdr->bss_idx = mvif->bss_idx;
	pos += sizeof(*hdr);

	/* UNI_SCAN_REQ (tag=1) */
	req = (struct scan_req_tlv *)pos;
	req->tag = cpu_to_le16(UNI_SCAN_REQ);
	req->len = cpu_to_le16(sizeof(*req));
	req->scan_type = sreq->n_ssids ? 1 : 0;
	req->probe_req_num = sreq->n_ssids ? 2 : 0;
	req->scan_func = SCAN_FUNC_SPLIT_SCAN;
	pos += sizeof(*req);

	/* UNI_SCAN_SSID (tag=10) */
	ssid = (struct scan_ssid_tlv *)pos;
	ssid->tag = cpu_to_le16(UNI_SCAN_SSID);
	ssid->len = cpu_to_le16(sizeof(*ssid));
	if (sreq->n_ssids) {
		ssid->ssid_type = BIT(2);
		ssid->ssids_num = min_t(u8, sreq->n_ssids,
					MT7927_SCAN_MAX_SSIDS);
		for (i = 0; i < ssid->ssids_num; i++) {
			ssid->ssids[i].ssid_len =
				cpu_to_le32(sreq->ssids[i].ssid_len);
			memcpy(ssid->ssids[i].ssid, sreq->ssids[i].ssid,
			       sreq->ssids[i].ssid_len);
		}
	} else {
		ssid->ssid_type = BIT(0);
	}
	pos += sizeof(*ssid);

	/* UNI_SCAN_BSSID (tag=11) */
	bssid = (struct scan_bssid_tlv *)pos;
	bssid->tag = cpu_to_le16(UNI_SCAN_BSSID);
	bssid->len = cpu_to_le16(sizeof(*bssid));
	eth_broadcast_addr(bssid->bssid);
	bssid->match_ssid_ind = 10;
	bssid->match_short_ssid_ind = 10;
	pos += sizeof(*bssid);

	/* UNI_SCAN_CHANNEL (tag=12) */
	chan = (struct scan_chan_info_tlv *)pos;
	chan->tag = cpu_to_le16(UNI_SCAN_CHANNEL);
	chan->len = cpu_to_le16(sizeof(*chan));
	if (sreq->n_channels) {
		chan->channel_type = 4;
		chan->channels_num = min_t(u8, sreq->n_channels,
					  MT7927_SCAN_MAX_CHANNELS);
		for (i = 0; i < chan->channels_num; i++) {
			struct ieee80211_channel *c = sreq->channels[i];

			switch (c->band) {
			case NL80211_BAND_2GHZ:
				chan->channels[i].band = 1;
				break;
			case NL80211_BAND_5GHZ:
				chan->channels[i].band = 2;
				break;
			case NL80211_BAND_6GHZ:
				chan->channels[i].band = 3;
				break;
			default:
				chan->channels[i].band = 1;
			}
			chan->channels[i].channel_num = c->hw_value;
		}
	}

	set_bit(MT7927_SCANNING, &dev->scan_state);

	dev_info(&dev->pdev->dev,
		 "scan: type=%d ssids=%d channels=%d\n",
		 req->scan_type, ssid->ssids_num, chan->channels_num);

	/* ⚠️ scan 命令必须使用 option=0x07 (ACK+UNI+SET), 不是 0x06!
	 * mt7925 在 mt7925_mcu_fill_message() 中 non-query 命令使用
	 * MCU_CMD_UNI_EXT_ACK = 0x07, 并且 wait_resp=true.
	 * 缺少 BIT(0) ACK 位, 固件会静默忽略 scan 命令. */
	ret = mt7927_mcu_send_unicmd(dev, MCU_UNI_CMD_SCAN_REQ,
				     UNI_CMD_OPT_SET_ACK, buf, buf_len);
	if (ret)
		clear_bit(MT7927_SCANNING, &dev->scan_state);

	kfree(buf);
	return ret;
}

/* 取消 HW scan */
static int mt7927_mcu_cancel_hw_scan(struct mt7927_dev *dev,
				     struct ieee80211_vif *vif)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct {
		struct scan_hdr_tlv hdr;
		struct {
			__le16 tag;
			__le16 len;
		} cancel;
	} __packed req = {};

	req.hdr.seq_num = dev->scan_seq_num;
	req.hdr.bss_idx = mvif->bss_idx;
	req.cancel.tag = cpu_to_le16(UNI_SCAN_CANCEL);
	req.cancel.len = cpu_to_le16(4);

	clear_bit(MT7927_SCANNING, &dev->scan_state);

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_SCAN_REQ,
					   &req, sizeof(req));
}

/* scan 工作队列 — scan_done 事件触发后延迟通知 mac80211
 * 来源: mt7925/main.c mt7925_scan_work()
 *
 * 注意: ieee80211_scan_completed() 不能从 NAPI/tasklet 上下文调用,
 * 因为 mac80211 内部需要获取 mutex 和调度其他工作.
 * mt7925 也通过 work queue 来调用. */
static void mt7927_scan_work(struct work_struct *work)
{
	struct mt7927_dev *dev = container_of(work, struct mt7927_dev,
					      scan_work.work);
	struct cfg80211_scan_info info = {
		.aborted = false,
	};

	/* scan_done 事件已清除 MT7927_SCANNING 标志,
	 * 这里直接通知 mac80211 扫描完成.
	 * 如果 scanning 标志仍然设置 (超时路径), 也清除它. */
	if (!test_bit(MT7927_SCANNING, &dev->scan_state)) {
		if (dev->hw && dev->hw_init_done)
			ieee80211_scan_completed(dev->hw, &info);
	}
}

/* --- 连接 MCU 命令 --- */

/* DEV_INFO_UPDATE (CID=0x01) — 激活虚拟接口 */
static int mt7927_mcu_uni_add_dev(struct mt7927_dev *dev,
				  struct ieee80211_vif *vif, bool enable)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	/* MT6639: UNI_CMD_DEVINFO header + DEVINFO_ACTIVE TLV
	 * header 包含 OwnMacIdx + DbdcIdx, TLV 只有 active + mac
	 * 之前 rsv[4] 全零: DbdcIdx=0 (只用 band0), MAC 偏移 2 字节!
	 * 已修复: header 设正确值, TLV 匹配 MT6639 的 12 字节格式 */
	struct {
		struct dev_info_hdr hdr;
		struct dev_info_active_tlv tlv;
	} __packed req = {};

	req.hdr.omac_idx = mvif->omac_idx;
	req.hdr.band_idx = 0xff; /* ENUM_BAND_AUTO — 固件自动选择 DBDC band */

	req.tlv.tag = cpu_to_le16(UNI_DEV_INFO_ACTIVE);
	req.tlv.len = cpu_to_le16(sizeof(req.tlv));
	req.tlv.active = enable;
	memcpy(req.tlv.omac_addr, vif->addr, ETH_ALEN);

	dev_info(&dev->pdev->dev,
		 "mcu: DEV_INFO active=%d omac=%d band=AUTO addr=%pM (size=%zu)\n",
		 enable, mvif->omac_idx, vif->addr, sizeof(req));

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_DEV_INFO, &req,
					   sizeof(req));
}

/* BSS_INFO PM_DISABLE — 在 BSS activate 前禁用省电模式
 * Windows RE: nicUniCmdSetBssInfo 序列开头发送 tag=0x1B (PM)
 * payload: 4字节BSS头 + 4字节TLV (tag + len, 无额外数据)
 * len字段=0x0004 表示只有 TLV header 本身 (4 bytes) */
static int mt7927_mcu_bss_pm_disable(struct mt7927_dev *dev, u8 bss_idx)
{
	struct {
		u8  bss_idx;
		u8  pad[3];
		__le16 tag;
		__le16 len;
	} __packed req = {};

	req.bss_idx = bss_idx;
	req.tag = cpu_to_le16(UNI_BSS_INFO_PM);
	req.len = cpu_to_le16(4);  /* TLV header only */

	dev_info(&dev->pdev->dev, "mcu: BSS_INFO PM_DISABLE bss=%d\n", bss_idx);
	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_BSS_INFO,
					  &req, sizeof(req));
}

/* BSS_INFO_UPDATE (CID=0x02) — 配置 BSS */
static int mt7927_mcu_add_bss_info(struct mt7927_dev *dev,
				   struct ieee80211_vif *vif, bool enable)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct ieee80211_channel *chan;
	struct {
		struct bss_req_hdr hdr;
		struct mt76_connac_bss_basic_tlv basic; /* tag=0 */
		struct bss_ra_tlv ra;                   /* tag=1 */
		struct bss_rlm_tlv rlm;                 /* tag=2 */
		struct bss_protect_tlv protect;         /* tag=3 */
		struct bss_color_tlv color;             /* tag=4 */
		struct bss_he_tlv he;                   /* tag=5 */
		struct bss_rate_tlv rate;               /* tag=0xB */
		struct bss_sap_tlv sap;                 /* tag=0xD */
		struct bss_p2p_tlv p2p;                 /* tag=0xE */
		struct bss_qbss_tlv qbss;               /* tag=0xF */
		struct bss_sec_tlv sec;                 /* tag=0x10 */
		struct bss_ifs_time_tlv ifs_time;       /* tag=0x17 */
		struct bss_iot_tlv iot;                 /* tag=0x18 */
		struct bss_mld_tlv mld;                 /* tag=0x1A */
	} __packed req = {};

	/* Windows RE: PM_DISABLE (tag=0x1B) 在完整 BSS_INFO 之前发送
	 * 告诉固件禁用省电模式，允许立即发送管理帧 */
	if (enable)
		mt7927_mcu_bss_pm_disable(dev, mvif->bss_idx);

	/* 优先从 vif BSS conf 获取信道 (AP 的实际信道),
	 * 回退到 hw->conf (可能是扫描/默认信道).
	 * mt7925: mt7925_mcu_bss_basic_tlv() 用 chandef->chan */
	if (vif->bss_conf.chanreq.oper.chan)
		chan = vif->bss_conf.chanreq.oper.chan;
	else
		chan = dev->hw->conf.chandef.chan;

	req.hdr.bss_idx = mvif->bss_idx;

	/* === BSS_BASIC TLV (tag=0) === */
	req.basic.tag = cpu_to_le16(UNI_BSS_INFO_BASIC);
	req.basic.len = cpu_to_le16(sizeof(req.basic));
	req.basic.active = enable;
	req.basic.omac_idx = mvif->omac_idx;
	req.basic.hw_bss_idx = mvif->bss_idx;
	req.basic.band_idx = mvif->band_idx;
	/* TEST-A: revert to old CONNECTION_INFRA_STA (5d87f81 value, scan worked) */
	req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_STA);
	/* TEST-1: conn_state 枚举翻转 (CONNECTED=0, DISCONNECTED=1)
	 * BSS_INFO 枚举: MEDIA_STATE_CONNECTED=0, MEDIA_STATE_DISCONNECTED=1
	 * 之前 enable ? 1 : 0 实际上是 enable → DISCONNECTED, 可能让固件认为未连接 */
	req.basic.conn_state = enable ? 0 : 1;
	req.basic.wmm_idx = mvif->wmm_idx;
	req.basic.bmc_tx_wlan_idx = cpu_to_le16(mvif->sta.wcid.idx);
	/* Windows/MT6639: StaRecIdxOfAP=0xFFFE (NOT_FOUND) during initial BSS activation.
	 * This field gets updated after STA_REC is created with the actual wlan_idx.
	 * Using wcid.idx=0 was wrong — firmware uses NOT_FOUND for initial activate. */
	req.basic.sta_idx = cpu_to_le16(0xFFFE);

	if (vif->bss_conf.bssid)
		memcpy(req.basic.bssid, vif->bss_conf.bssid, ETH_ALEN);
	if (vif->bss_conf.beacon_int) {
		req.basic.bcn_interval =
			cpu_to_le16(vif->bss_conf.beacon_int);
		req.basic.dtim_period = vif->bss_conf.dtim_period;
	}

	/* PHY mode — 告诉固件此 BSS 的能力 (OFDM/HT/VHT/HE)
	 * 参考: mt7925/mcu.c mt7925_mcu_bss_basic_tlv() line 2503
	 * 缺失此字段, 固件不知道该用什么调制方式 */
	if (chan) {
		if (chan->band == NL80211_BAND_5GHZ) {
			/* 5GHz: 11a + HT + VHT */
			req.basic.phymode = 0x31; /* PHY_MODE_A|AN|AC */
			req.basic.nonht_basic_phy =
				cpu_to_le16(3); /* PHY_TYPE_OFDM_INDEX */
		} else if (chan->band == NL80211_BAND_6GHZ) {
			req.basic.phymode = 0xb1; /* A|AN|AC|AX_5G */
			req.basic.nonht_basic_phy =
				cpu_to_le16(3); /* PHY_TYPE_OFDM_INDEX */
		} else {
			/* 2.4GHz: 11b/g + HT */
			req.basic.phymode = 0x0f; /* PHY_MODE_B|G|GN */
			req.basic.nonht_basic_phy =
				cpu_to_le16(1); /* PHY_TYPE_ERP_INDEX */
		}
	}

	/* === BSS_RLM TLV (tag=2) — 告诉固件信道/频段/带宽 === */
	req.rlm.tag = cpu_to_le16(UNI_BSS_INFO_RLM);
	req.rlm.len = cpu_to_le16(sizeof(req.rlm));
	if (chan) {
		req.rlm.control_channel = chan->hw_value;
		req.rlm.center_chan = chan->hw_value; /* 20MHz: center=control */
		req.rlm.bw = 0;  /* CMD_CBW_20MHZ */
		req.rlm.tx_streams = 2;  /* MT6639: 2T2R */
		req.rlm.rx_streams = 2;
		req.rlm.ht_op_info = 4;  /* HT 40MHz allowed */
		switch (chan->band) {
		case NL80211_BAND_5GHZ:
			req.rlm.band = 2;  /* CMD_BAND_5G */
			break;
		case NL80211_BAND_6GHZ:
			req.rlm.band = 3;  /* CMD_BAND_6G */
			break;
		default:
			req.rlm.band = 1;  /* CMD_BAND_24G */
			break;
		}
	}

	/* === BSS_PROTECT TLV (tag=3) — 保护模式 ===
	 * 初始 auth 阶段无保护, protect_mode=0
	 * MT6639: nic_uni_cmd_event.c nicUniCmdBssInfoProtect() */
	req.protect.tag = cpu_to_le16(UNI_BSS_INFO_PROTECT);
	req.protect.len = cpu_to_le16(sizeof(req.protect));
	req.protect.protect_mode = 0;

	/* === BSS_IFS_TIME TLV (tag=0x17) — 帧间距时间 ===
	 * 5GHz 必须用 short slot (9μs), 否则 DIFS=56μs (应为 34μs)
	 * MT6639: nic_uni_cmd_event.c nicUniCmdBssInfoIfsTime() */
	req.ifs_time.tag = cpu_to_le16(UNI_BSS_INFO_IFS_TIME);
	req.ifs_time.len = cpu_to_le16(sizeof(req.ifs_time));
	req.ifs_time.slot_valid = 1;
	/* Windows RE: 只设slot_valid和slot_time，sifs_valid=0 */
	if (chan && chan->band != NL80211_BAND_2GHZ) {
		req.ifs_time.slot_time = cpu_to_le16(9);    /* 5GHz short slot */
	} else {
		req.ifs_time.slot_time = cpu_to_le16(20);   /* 2.4GHz legacy */
	}

	/* === BSS_RATE TLV (tag=0x0B) — 速率集合 ===
	 * 告诉固件此 BSS 可用的速率, 缺少时固件可能无法发送管理帧
	 * 来源: mt6639/nic/nic_uni_cmd_event.c line 1694 */
	req.rate.tag = cpu_to_le16(UNI_BSS_INFO_RATE);
	req.rate.len = cpu_to_le16(sizeof(req.rate));
	if (chan) {
		if (chan->band == NL80211_BAND_2GHZ) {
			req.rate.operational_rate = cpu_to_le16(ERP_RATE_SET);
			req.rate.basic_rate = cpu_to_le16(HR_DSSS_ERP_BASIC_RATE);
			req.rate.short_preamble = 1;
		} else {
			req.rate.operational_rate = cpu_to_le16(OFDM_RATE_SET);
			req.rate.basic_rate = cpu_to_le16(OFDM_BASIC_RATE);
			req.rate.short_preamble = 0;
		}
	}

	/* === BSS_RA TLV (tag=1) — 速率适配配置 ===
	 * 来源: MT6639 UNI_CMD_BSSINFO_RA */
	req.ra.tag = cpu_to_le16(UNI_BSS_INFO_RA);
	req.ra.len = cpu_to_le16(sizeof(req.ra));
	if (chan && chan->band == NL80211_BAND_2GHZ)
		req.ra.short_preamble = 1;

	/* === BSS_COLOR TLV (tag=4) — HE BSS Color ===
	 * 初始认证阶段禁用 BSS Color */
	req.color.tag = cpu_to_le16(UNI_BSS_INFO_BSS_COLOR);
	req.color.len = cpu_to_le16(sizeof(req.color));
	req.color.enable = 0;
	req.color.bss_color = 0;

	/* === BSS_HE TLV (tag=5) — HE 能力配置 ===
	 * CONNAC3 固件需要此 TLV 以处理 HE BSS
	 * au2MaxNssMcs=0: 初始阶段不设特定 NSS/MCS 限制 */
	req.he.tag = cpu_to_le16(UNI_BSS_INFO_HE);
	req.he.len = cpu_to_le16(sizeof(req.he));
	req.he.txop_duration_rts_threshold = 0;
	req.he.default_pe_duration = 0;
	req.he.er_su_disable = 0;

	/* === BSS_SAP TLV (tag=0x0D) — Soft AP 配置 ===
	 * STA 模式: 全填 0 (无 SSID 广播) */
	req.sap.tag = cpu_to_le16(UNI_BSS_INFO_SAP);
	req.sap.len = cpu_to_le16(sizeof(req.sap));

	/* === BSS_P2P TLV (tag=0x0E) — P2P 配置 ===
	 * STA 模式: private_data=0 */
	req.p2p.tag = cpu_to_le16(UNI_BSS_INFO_P2P);
	req.p2p.len = cpu_to_le16(sizeof(req.p2p));

	/* === BSS_QBSS TLV (tag=0x0F) — QoS BSS 配置 ===
	 * WPA2 连接通常支持 QoS，设 is_qbss=1 */
	req.qbss.tag = cpu_to_le16(UNI_BSS_INFO_QBSS);
	req.qbss.len = cpu_to_le16(sizeof(req.qbss));
	req.qbss.is_qbss = 1;

	/* === BSS_SEC TLV (tag=0x10) — 安全配置 ===
	 * Open System Auth 阶段: auth_mode=0, enc=0
	 * 实际加密配置在 CONNECT 状态时更新 */
	req.sec.tag = cpu_to_le16(UNI_BSS_INFO_SEC);
	req.sec.len = cpu_to_le16(sizeof(req.sec));
	req.sec.auth_mode = 0;   /* AUTH_MODE_OPEN */
	req.sec.enc_status = 0;  /* ENCRYPT_DISABLED */
	req.sec.cipher_suit = 0; /* no cipher */

	/* === BSS_STA_IOT TLV (tag=0x18) — IoT AP 兼容 ===
	 * iot_ap_bmp=0: 不启用特殊 IoT workaround */
	req.iot.tag = cpu_to_le16(UNI_BSS_INFO_STA_IOT);
	req.iot.len = cpu_to_le16(sizeof(req.iot));

	/* === BSS_MLD TLV (tag=0x1A) — CONNAC3/WiFi 7 必需 ===
	 * MT6639 在 BSS_ACTIVATE_CTRL 和 SET_BSS_INFO 中都发送此 TLV
	 * 即使非 MLO 连接也需要: GroupMldId=0xff, OmRemapIdx=0xff
	 * 来源: mt6639/nic/nic_uni_cmd_event.c nicUniCmdBssActivateCtrl() */
	req.mld.tag = cpu_to_le16(UNI_BSS_INFO_MLD);
	req.mld.len = cpu_to_le16(sizeof(req.mld));
	req.mld.group_mld_id = 0xff;	/* MLD_GROUP_NONE */
	req.mld.own_mld_id = mvif->bss_idx;
	memcpy(req.mld.own_mld_addr, vif->addr, ETH_ALEN);
	req.mld.om_remap_idx = 0xff;	/* OM_REMAP_IDX_NONE */

	dev_info(&dev->pdev->dev,
		 "mcu: BSS_INFO bss=%d active=%d conn_state=%u phymode=0x%02x bssid=%pM ch=%u band=%u band_idx=%u mld_id=%u\n",
		 mvif->bss_idx, enable, req.basic.conn_state,
		 req.basic.phymode, req.basic.bssid,
		 req.rlm.control_channel, req.rlm.band, req.basic.band_idx,
		 req.mld.own_mld_id);

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_BSS_INFO, &req,
					   sizeof(req));
}

/* STA_REC_UPDATE (CID=0x25) — 添加/更新 STA 记录 [Windows inner CID=0x25, outer_tag=0xb1]
 *
 * mt7925 在 STA_REC 中不包含 STA_REC_WTBL TLV!
 * 固件根据 STA_REC_BASIC 的 EXTRA_INFO_NEW 标志自动创建 WTBL 条目.
 * 包含 STA_REC_WTBL 可能导致 MT6639 固件解析错误.
 *
 * 来源: mt76/mt7925/mcu.c mt7925_mcu_sta_cmd()
 *       docs/mt7925_sta_state_flow.md
 */
static int mt7927_mcu_sta_update(struct mt7927_dev *dev,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 bool enable, u8 conn_state)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct mt7927_sta *msta;
	/* MT6639 发 10 个 TLV, 我们发 7 个:
	 *   BASIC(0) + RA(1) + STATE(7) + HT(9) + VHT(0xA) + PHY(0x15) + HDR_TRANS(0x2B) + HE_BASIC(0x19)
	 * 新增 HE_BASIC — MT6639 和 Windows 均发送此 TLV，固件需要以配置 WTBL HE 能力 */
	struct {
		struct sta_req_hdr hdr;
		struct sta_rec_basic basic;
		struct sta_rec_ra_info ra;
		struct sta_rec_state state;
		struct sta_rec_ht_info ht;
		struct sta_rec_vht_info vht;
		struct sta_rec_phy phy;
		struct sta_rec_hdr_trans hdr_trans;
		struct sta_rec_he_basic he;
	} __packed req = {};

	if (sta)
		msta = (struct mt7927_sta *)sta->drv_priv;
	else
		msta = &mvif->sta;

	/* STA 请求头 */
	req.hdr.bss_idx = mvif->bss_idx;
	req.hdr.wlan_idx_lo = msta->wcid.idx & 0xff;
	req.hdr.wlan_idx_hi = (msta->wcid.idx >> 8) & 0xff;
	req.hdr.muar_idx = mvif->omac_idx;
	req.hdr.is_tlv_append = 1;
	req.hdr.tlv_num = cpu_to_le16(8); /* BASIC+RA+STATE+HT+VHT+PHY+HDR_TRANS+HE_BASIC */

	/* STA_REC_BASIC (tag=0)
	 * 来源: mt76/mt76_connac_mcu.c mt76_connac_mcu_sta_basic_tlv()
	 * 新建 STA: PORT_SECURE + EXTRA_INFO_NEW → 固件自动创建 WTBL
	 * 更新 STA: PORT_SECURE (无 NEW) → 更新已有条目
	 * 删除 STA: DISCONNECT, 无 NEW */
	req.basic.tag = cpu_to_le16(STA_REC_BASIC);
	req.basic.len = cpu_to_le16(sizeof(req.basic));
	/* MT6639 ext_cmd + gl_hook_api: STA_REC always uses CONNECTION_INFRA_STA
	 * (0x10001) regardless of peer type. UniCmd path differs but ext_cmd
	 * path is what the firmware expects for WTBL creation. */
	if (vif->type == NL80211_IFTYPE_STATION)
		req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_STA);
	else
		req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_AP);

	if (enable) {
		req.basic.conn_state = conn_state;
		req.basic.extra_info = cpu_to_le16(EXTRA_INFO_VER);
		/* EXTRA_INFO_NEW: MT6639 分析表明 enable=true 时始终设置
		 * u2ExtraInfo = 0x03 (VER | NEW), 不论 conn_state 值
		 * 固件每次都重新创建/更新 WTBL 条目 */
		req.basic.extra_info |= cpu_to_le16(EXTRA_INFO_NEW);
	} else {
		req.basic.conn_state = CONN_STATE_DISCONNECT;
		req.basic.extra_info = cpu_to_le16(EXTRA_INFO_VER);
	}

	if (sta) {
		req.basic.qos = sta->wme;
		req.basic.aid = cpu_to_le16(sta->aid);
		memcpy(req.basic.peer_addr, sta->addr, ETH_ALEN);
	}

	/* STA_REC_RA (tag=1) — 支持的速率
	 * 来源: mt7925/mcu.c mt7925_mcu_sta_rate_ctrl_tlv()
	 * 固件需要知道 STA 的速率能力来初始化内部速率表 */
	req.ra.tag = cpu_to_le16(STA_REC_RA);
	req.ra.len = cpu_to_le16(sizeof(req.ra));
	if (sta && dev->hw && dev->hw->conf.chandef.chan) {
		enum nl80211_band band = dev->hw->conf.chandef.chan->band;
		u16 supp = sta->deflink.supp_rates[band];

		if (band == NL80211_BAND_2GHZ)
			req.ra.legacy = cpu_to_le16(
				FIELD_PREP(RA_LEGACY_OFDM, supp >> 4) |
				FIELD_PREP(RA_LEGACY_CCK, supp & 0xf));
		else
			req.ra.legacy = cpu_to_le16(
				FIELD_PREP(RA_LEGACY_OFDM, supp));

		if (sta->deflink.ht_cap.ht_supported)
			memcpy(req.ra.rx_mcs_bitmask,
			       sta->deflink.ht_cap.mcs.rx_mask,
			       sizeof(req.ra.rx_mcs_bitmask));
	} else {
		/* fallback: 5GHz all OFDM rates */
		req.ra.legacy = cpu_to_le16(FIELD_PREP(RA_LEGACY_OFDM, 0xFF));
	}

	/* STA_REC_STATE (tag=7 = STATE_CHANGED for MT6639)
	 * 来源: MT6639 cnmStaRecChangeState(), tag=0x07
	 * ⚠️ MT6639 布局: flags(4) + state(1) + opmode(1) + action(1) + pad(1) = 12 bytes
	 * 之前 tag=2 是 RA_COMMON_INFO, 固件从未收到正确的 STATE 通知! */
	req.state.tag = cpu_to_le16(STA_REC_STATE);
	req.state.len = cpu_to_le16(sizeof(req.state));
	req.state.flags = 0;
	/* STA_STATE_3 (value=2) 清除 PLE STATION_PAUSE → TX scheduler 可以分发帧
	 * STA_STATE_1 (value=0) 设置 STATION_PAUSE → 断开时暂停 TX
	 * 根因: 之前用 state=0 导致 STATION_PAUSE0 bit1=1, auth帧永远卡在 PLE 无法到达射频
	 * MT6639 cnmStaRecChangeState(STATE_3) 触发固件清除 STATION_PAUSE */
	req.state.state = enable ? 2 : 0;

	/* STA_REC_HT_INFO (tag=0x09) — HT 能力
	 * MT6639: nicUniCmdStaRecTagHtInfo() — 仅当 ht_cap 非零时发送
	 * 固件需要 HT 能力来创建正确的 WTBL 条目 */
	req.ht.tag = cpu_to_le16(STA_REC_HT);
	req.ht.len = cpu_to_le16(sizeof(req.ht));
	if (sta && sta->deflink.ht_cap.ht_supported)
		req.ht.ht_cap = cpu_to_le16(sta->deflink.ht_cap.cap);

	/* STA_REC_VHT_INFO (tag=0x0A) — VHT 能力
	 * MT6639: nicUniCmdStaRecTagVhtInfo() — 仅当 vht_cap 非零时发送
	 * 5GHz 必需, 2.4GHz 可选 */
	req.vht.tag = cpu_to_le16(STA_REC_VHT);
	req.vht.len = cpu_to_le16(sizeof(req.vht));
	if (sta && sta->deflink.vht_cap.vht_supported) {
		req.vht.vht_cap = cpu_to_le32(sta->deflink.vht_cap.cap);
		req.vht.vht_rx_mcs_map = cpu_to_le16(
			sta->deflink.vht_cap.vht_mcs.rx_mcs_map);
		req.vht.vht_tx_mcs_map = cpu_to_le16(
			sta->deflink.vht_cap.vht_mcs.tx_mcs_map);
	}

	/* STA_REC_PHY (tag=0x15) — PHY type + basic rate set
	 * MT6639 固件使用自己的 PHY 枚举:
	 *   HR_DSSS=BIT(0), ERP=BIT(1), OFDM=BIT(2), HT=BIT(3), VHT=BIT(4)
	 * 来源: mt6639-analyst 确认的 MT6639 固件值 */
	req.phy.tag = cpu_to_le16(STA_REC_PHY);
	req.phy.len = cpu_to_le16(sizeof(req.phy));
	if (dev->hw && dev->hw->conf.chandef.chan) {
		enum nl80211_band band = dev->hw->conf.chandef.chan->band;

		if (band == NL80211_BAND_5GHZ) {
			/* 5GHz: OFDM|HT|VHT = BIT(2)|BIT(3)|BIT(4) = 0x1C */
			req.phy.phy_type = 0x1C;
			/* OFDM mandatory rates: 6/12/24 Mbps */
			req.phy.basic_rate = cpu_to_le16(0x0150);
		} else {
			/* 2.4GHz: HR_DSSS|ERP|OFDM|HT = 0x0F */
			req.phy.phy_type = 0x0F;
			/* CCK basic rates: 1/2/5.5/11 Mbps */
			req.phy.basic_rate = cpu_to_le16(0x000F);
		}
	} else {
		/* fallback: 5GHz */
		req.phy.phy_type = 0x1C;
		req.phy.basic_rate = cpu_to_le16(0x0150);
	}

	/* STA_REC_HDR_TRANS (tag=0x2B) */
	req.hdr_trans.tag = cpu_to_le16(STA_REC_HDR_TRANS);
	req.hdr_trans.len = cpu_to_le16(sizeof(req.hdr_trans));
	req.hdr_trans.from_ds = 0;
	req.hdr_trans.to_ds = 1;  /* STA mode: to_ds=1, from_ds=0 */
	req.hdr_trans.dis_rx_hdr_tran = 1;  /* disable RX header translation */

	/* STA_REC_HE_BASIC (tag=0x19) — HE 基础能力
	 * MT6639 和 Windows 均发送此 TLV，固件需要以配置 HE WTBL 条目
	 * MT6639 hardcoded pkt_ext=2 for mobile chips */
	req.he.tag = cpu_to_le16(STA_REC_HE_BASIC);
	req.he.len = cpu_to_le16(sizeof(req.he));
	if (sta && sta->deflink.he_cap.has_he) {
		memcpy(req.he.mac_cap,
		       sta->deflink.he_cap.he_cap_elem.mac_cap_info,
		       sizeof(req.he.mac_cap));
		memcpy(req.he.phy_cap,
		       sta->deflink.he_cap.he_cap_elem.phy_cap_info,
		       sizeof(req.he.phy_cap));
		req.he.rx_mcs_80 =
			sta->deflink.he_cap.he_mcs_nss_supp.rx_mcs_80;
		req.he.rx_mcs_160 =
			sta->deflink.he_cap.he_mcs_nss_supp.rx_mcs_160;
		req.he.rx_mcs_80p80 =
			sta->deflink.he_cap.he_mcs_nss_supp.rx_mcs_80p80;
	}
	req.he.pkt_ext = 2; /* MT6639 hardcoded for mobile */

	dev_info(&dev->pdev->dev,
		 "mcu: STA_REC wcid=%d conn_state=%d state=%d enable=%d len=%zu\n",
		 msta->wcid.idx, req.basic.conn_state, req.state.state,
		 enable, sizeof(req));

	return mt7927_mcu_send_unicmd(dev, MCU_UNI_CMD_STA_REC,
				      UNI_CMD_OPT_SET_ACK, &req, sizeof(req));
}

/* 密钥安装 (STA_REC_UPDATE + STA_REC_KEY_V3) */
static int mt7927_mcu_add_key(struct mt7927_dev *dev,
			      struct ieee80211_vif *vif,
			      struct ieee80211_sta *sta,
			      struct ieee80211_key_conf *key,
			      enum set_key_cmd cmd)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct mt7927_sta *msta;
	struct {
		struct sta_req_hdr hdr;
		struct sta_rec_sec_uni sec;
	} __packed req = {};
	u8 cipher;

	if (sta)
		msta = (struct mt7927_sta *)sta->drv_priv;
	else
		msta = &mvif->sta;

	/* Linux cipher → CONNAC3 cipher */
	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
		cipher = CONNAC3_CIPHER_AES_CCMP;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		cipher = CONNAC3_CIPHER_TKIP;
		break;
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
		cipher = CONNAC3_CIPHER_GCMP;
		break;
	case WLAN_CIPHER_SUITE_AES_CMAC:
		cipher = CONNAC3_CIPHER_BIP_CMAC_128;
		break;
	default:
		return -EOPNOTSUPP;
	}

	req.hdr.bss_idx = mvif->bss_idx;
	req.hdr.wlan_idx_lo = msta->wcid.idx & 0xff;
	req.hdr.wlan_idx_hi = (msta->wcid.idx >> 8) & 0xff;
	req.hdr.tlv_num = cpu_to_le16(1);
	req.hdr.is_tlv_append = 1;

	req.sec.tag = cpu_to_le16(STA_REC_KEY_V3);
	req.sec.len = cpu_to_le16(sizeof(req.sec));
	req.sec.add = (cmd == SET_KEY);
	req.sec.tx_key = 1;
	req.sec.key_type = 1; /* pairwise */
	req.sec.bss_idx = mvif->bss_idx;
	req.sec.cipher_id = cipher;
	req.sec.key_id = key->keyidx;
	req.sec.key_len = key->keylen;
	req.sec.wlan_idx = msta->wcid.idx;
	if (sta)
		memcpy(req.sec.peer_addr, sta->addr, ETH_ALEN);
	memcpy(req.sec.key, key->key, min_t(u8, key->keylen, 32));

	dev_info(&dev->pdev->dev,
		 "mcu: KEY %s cipher=%d keyidx=%d keylen=%d\n",
		 cmd == SET_KEY ? "SET" : "DEL", cipher,
		 key->keyidx, key->keylen);

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_STA_REC, &req,
					   sizeof(req));
}

/* =====================================================================
 * 9. mac80211 回调函数 + 注册
 * ===================================================================== */

/* 从 ieee80211_hw 获取 mt7927_dev 指针 */
static inline struct mt7927_dev *mt7927_hw_dev(struct ieee80211_hw *hw)
{
	return *(struct mt7927_dev **)hw->priv;
}

/*
 * mt7927_mgmt_tx_worker — 管理帧通过 TX Ring 0 (CT mode) 发送 [诊断实验]
 *
 * 实验目的: Ring 2 SF mode 固件完全静默。历史上 Ring 0 CT mode 返回 TXFREE stat=1，
 * 现在 BSS_INFO/DMASHDL 更完整后，换回 CT mode 看新的错误码。
 *
 * CT mode: TXD+TXP 在 coherent pool，payload DMA map 分开，firmware 读 TXP 找 payload。
 * TXD PKT_FMT=1 (CT), Q_IDX=0x10 (ALTX0/mgmt)
 */
static void mt7927_mgmt_tx_worker(struct work_struct *work)
{
	struct mt7927_dev *dev = container_of(work, struct mt7927_dev,
					      mgmt_tx_work);
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&dev->mgmt_tx_queue)) != NULL) {
		u16 wcid_idx = skb->priority;
		struct mt7927_wcid *wcid = NULL;
		struct ieee80211_hdr *hdr;
		int ret;

		if (wcid_idx < MT7927_WTBL_SIZE)
			wcid = dev->wcid[wcid_idx];

		hdr = (struct ieee80211_hdr *)skb->data;
		dev_info(&dev->pdev->dev,
			 "TX mgmt via Ring 0 CT: fc=0x%04x len=%u wcid=%d DA=%pM\n",
			 le16_to_cpu(hdr->frame_control),
			 skb->len,
			 wcid ? wcid->idx : -1,
			 hdr->addr1);

		/* Check if ring_tx0 is initialized */
		if (!dev->ring_tx0.desc) {
			dev_err(&dev->pdev->dev,
				"TX mgmt: ring_tx0 not initialized!\n");
			dev_kfree_skb_any(skb);
			continue;
		}

		/* Enqueue to Ring 0 via CT mode:
		 * mt7927_tx_queue_skb() calls mt7927_tx_prepare_skb() which builds
		 * TXD (CT mode, PKT_FMT=1, Q_IDX=0x10) + TXP in coherent pool,
		 * DMA maps payload separately, writes DMA descriptor */
		ret = mt7927_tx_queue_skb(dev, &dev->ring_tx0, skb, wcid);
		if (ret) {
			dev_err(&dev->pdev->dev,
				"TX mgmt: enqueue to ring_tx0 failed ret=%d\n", ret);
			dev_kfree_skb_any(skb);
			continue;
		}

		/* Kick Ring 0 — write CIDX to trigger DMA */
		mt7927_tx_kick(dev, &dev->ring_tx0);

		dev_info(&dev->pdev->dev,
			 "TX mgmt: submitted to ring_tx0 CT OK (head=%u tail=%u)\n",
			 dev->ring_tx0.head, dev->ring_tx0.tail);

		/* DIAG: PLE/PSE + MIB TX 诊断
		 * 等 500ms 给固件足够时间处理+发送，再读寄存器 */
		{
			u32 ple_empty, pse_empty, ple_free;
			/* MIB TX 计数器 (band 1 = 5GHz, auth 使用 band=1)
			 * MT6639: BN1_WF_MIB_TOP_BASE=0x820fd000 → BAR0+0x0a4800
			 * TBCR0 (TX_20MHZ_CNT):  MIB_BASE+0x6AC
			 * BTBCR (TX_BYTE_CNT):   MIB_BASE+0x450 (per WLAN_IDX)
			 * TRDR0 (TX_OK_MPDU):    MIB_BASE+0x1AC */
			u32 mib_tbcr0_b0, mib_tbcr0_b1;
			u32 mib_btbcr_b1, mib_trdr0_b1;

			msleep(500);

			/* PLE_QUEUE_EMPTY: BAR0+0x080b0 (PLE_BASE+0xb0)
			 * PSE_QUEUE_EMPTY: BAR0+0x0c0b0 (PSE_BASE+0xb0)
			 * PLE_FREEPG_CNT:  BAR0+0x08100 (PLE_BASE+0x100) */
			ple_empty = mt7927_rr(dev, 0x080b0);
			pse_empty = mt7927_rr(dev, 0x0c0b0);
			ple_free  = mt7927_rr(dev, 0x08100);

			/* MIB TX counters: confirm if auth frame reached PHY */
			mib_tbcr0_b0 = mt7927_rr(dev, 0x024800 + 0x6ac); /* band0 TX_20MHz_CNT */
			mib_tbcr0_b1 = mt7927_rr(dev, 0x0a4800 + 0x6ac); /* band1 TX_20MHz_CNT */
			mib_btbcr_b1 = mt7927_rr(dev, 0x0a4800 + 0x450); /* band1 TX_BYTE_CNT[0] */
			mib_trdr0_b1 = mt7927_rr(dev, 0x0a4800 + 0x1ac); /* band1 TX_OK_MPDU */

			dev_info(&dev->pdev->dev,
				 "POST-TX DIAG: PLE_EMPTY=0x%08x PSE_EMPTY=0x%08x PLE_FREE=0x%08x\n",
				 ple_empty, pse_empty, ple_free);
			dev_info(&dev->pdev->dev,
				 "POST-TX MIB: B0_TX20=%u B1_TX20=%u B1_TXBYTE=0x%x B1_TXOK=%u\n",
				 mib_tbcr0_b0, mib_tbcr0_b1, mib_btbcr_b1, mib_trdr0_b1);

			/* DMASHDL 诊断寄存器 (BAR0 base=0xd6000) */
			{
				u32 dmashdl_err   = mt7927_rr(dev, 0xd60dc); /* ERROR_FLAG_CTRL */
				u32 dmashdl_sta   = mt7927_rr(dev, 0xd6100); /* STATUS_RD: free+ffa */
				u32 dmashdl_gp0   = mt7927_rr(dev, 0xd6140); /* STATUS_RD_GP0 */
				u32 dmashdl_pkt0  = mt7927_rr(dev, 0xd6180); /* RD_GROUP_PKT_CNT0 */
				u32 dmashdl_ctrl  = mt7927_rr(dev, 0xd6018); /* CONTROL_SIGNAL */
				u32 dmashdl_dbg0  = mt7927_rr(dev, 0xd60c0); /* DEBUG_PORT00 */
				u32 dmashdl_dbg1  = mt7927_rr(dev, 0xd60c4); /* DEBUG_PORT01 */
				u32 glo_ext0      = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT0);

				dev_info(&dev->pdev->dev,
					 "DMASHDL: ERR=0x%08x STA=0x%08x GP0=0x%08x PKT0=0x%08x\n",
					 dmashdl_err, dmashdl_sta, dmashdl_gp0, dmashdl_pkt0);
				dev_info(&dev->pdev->dev,
					 "DMASHDL: CTRL=0x%08x DBG0=0x%08x DBG1=0x%08x EXT0=0x%08x\n",
					 dmashdl_ctrl, dmashdl_dbg0, dmashdl_dbg1, glo_ext0);
			}
		}

		dev_kfree_skb_any(skb);
	}
}

/* --- DIAG: WTBL 硬件条目读取 (用于诊断 auth TX 失败) ---
 * 来源: mt6639/chips/common/dbg_wtbl_connac3x.c halWtblReadRaw()
 * CONNAC3 间接访问: 先写 WDUCR 选择 GROUP，再按地址读 DW */
static void mt7927_dump_wtbl(struct mt7927_dev *dev, u16 wlan_idx)
{
	u32 lwtbl[MT_LWTBL_LEN_IN_DW];
	u32 uwtbl[MT_UWTBL_LEN_IN_DW];
	u32 group;
	int i;

	dev_info(&dev->pdev->dev,
		 "=== WTBL DUMP wlan_idx=%u ===\n", wlan_idx);

	/* --- 读取 LWTBL (36 DW) --- */
	group = (wlan_idx >> 7) & 0x1F;
	mt7927_wr_l1(dev, MT_WF_WTBLON_WDUCR, group);

	for (i = 0; i < MT_LWTBL_LEN_IN_DW; i++)
		lwtbl[i] = mt7927_rr_l1(dev, MT_LWTBL_IDX2ADDR(wlan_idx, i));

	/* 打印 LWTBL 原始数据 (每行 4 DW) */
	for (i = 0; i < MT_LWTBL_LEN_IN_DW; i += 4) {
		int remain = MT_LWTBL_LEN_IN_DW - i;

		if (remain >= 4)
			dev_info(&dev->pdev->dev,
				 "LWTBL[%u] DW%02d-%02d: %08x %08x %08x %08x\n",
				 wlan_idx, i, i + 3,
				 lwtbl[i], lwtbl[i + 1],
				 lwtbl[i + 2], lwtbl[i + 3]);
		else
			dev_info(&dev->pdev->dev,
				 "LWTBL[%u] DW%02d-%02d: %08x %08x %08x %08x\n",
				 wlan_idx, i, i + remain - 1,
				 lwtbl[i],
				 remain > 1 ? lwtbl[i + 1] : 0,
				 remain > 2 ? lwtbl[i + 2] : 0,
				 remain > 3 ? lwtbl[i + 3] : 0);
	}

	/* 解析 LWTBL 关键字段 */
	{
		u8 mac[6];
		u16 aid;
		u32 dw0 = lwtbl[0], dw1 = lwtbl[1], dw2 = lwtbl[2];

		/* MAC 地址: DW0[15:0]=addr[47:32], DW1[31:0]=addr[31:0] */
		mac[0] = (dw1 >> 24) & 0xff;
		mac[1] = (dw1 >> 16) & 0xff;
		mac[2] = (dw1 >> 8) & 0xff;
		mac[3] = dw1 & 0xff;
		mac[4] = (dw0 >> 8) & 0xff;
		mac[5] = dw0 & 0xff;

		aid = dw2 & 0xfff;

		dev_info(&dev->pdev->dev,
			 "LWTBL[%u] PARSED: MAC=%pM AID=%u MUAR=%u BAND=%u\n",
			 wlan_idx, mac,
			 aid,
			 (dw0 >> 16) & 0x3f,   /* MUAR index */
			 (dw0 >> 26) & 0x3);   /* BAND */
		dev_info(&dev->pdev->dev,
			 "LWTBL[%u] FLAGS: QoS=%u HT=%u VHT=%u HE=%u EHT=%u\n",
			 wlan_idx,
			 !!(dw2 & MT_LWTBL_DW2_QOS),
			 !!(dw2 & MT_LWTBL_DW2_HT),
			 !!(dw2 & MT_LWTBL_DW2_VHT),
			 !!(dw2 & MT_LWTBL_DW2_HE),
			 !!(dw2 & MT_LWTBL_DW2_EHT));
	}

	/* --- 读取 UWTBL (10 DW) --- */
	group = (wlan_idx >> 7) & 0x3F;
	mt7927_wr_l1(dev, MT_WF_UWTBL_WDUCR, group); /* TARGET=0 for UWTBL */

	for (i = 0; i < MT_UWTBL_LEN_IN_DW; i++)
		uwtbl[i] = mt7927_rr_l1(dev, MT_UWTBL_IDX2ADDR(wlan_idx, i));

	/* 打印 UWTBL 原始数据 */
	for (i = 0; i < MT_UWTBL_LEN_IN_DW; i += 4) {
		int remain = MT_UWTBL_LEN_IN_DW - i;

		if (remain >= 4)
			dev_info(&dev->pdev->dev,
				 "UWTBL[%u] DW%02d-%02d: %08x %08x %08x %08x\n",
				 wlan_idx, i, i + 3,
				 uwtbl[i], uwtbl[i + 1],
				 uwtbl[i + 2], uwtbl[i + 3]);
		else
			dev_info(&dev->pdev->dev,
				 "UWTBL[%u] DW%02d-%02d: %08x %08x\n",
				 wlan_idx, i, i + remain - 1,
				 uwtbl[i],
				 remain > 1 ? uwtbl[i + 1] : 0);
	}

	dev_info(&dev->pdev->dev,
		 "=== WTBL DUMP wlan_idx=%u END ===\n", wlan_idx);
}

/* --- TX: mac80211 发帧回调 --- */
static void mt7927_mac80211_tx(struct ieee80211_hw *hw,
			       struct ieee80211_tx_control *control,
			       struct sk_buff *skb)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct mt7927_wcid *wcid = NULL;
	int ret;

	if (control && control->sta) {
		struct mt7927_sta *msta;

		msta = (struct mt7927_sta *)control->sta->drv_priv;
		wcid = &msta->wcid;
	}

	/* 管理帧 → CMD ring 15 (PKT_FMT=2, Q_IDX=0, inline TXD+frame)
	 * 数据帧 → data ring 0 (PKT_FMT=0 CT mode, TXD+TXP scatter-gather)
	 *
	 * MT6639: 管理帧走 TC4 → ring 15, PKT_FMT=2(CMD), Q_IDX=0(MCU_Q0)
	 * 之前测 ring 15 失败是因为 PKT_FMT 和 Q_IDX 都错了 */
	if (skb->len >= 24) {
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

		if (ieee80211_is_mgmt(hdr->frame_control)) {
			dev_info(&dev->pdev->dev,
				 "TX mgmt->ring2: fc=0x%04x DA=%pM len=%u wcid=%d\n",
				 le16_to_cpu(hdr->frame_control),
				 hdr->addr1, skb->len,
				 wcid ? wcid->idx : -1);

			/* Store wcid index in skb->priority for worker */
			skb->priority = wcid ? wcid->idx : 0;
			skb_queue_tail(&dev->mgmt_tx_queue, skb);
			schedule_work(&dev->mgmt_tx_work);
			return;
		}

		dev_info(&dev->pdev->dev,
			 "TX data→ring0: fc=0x%04x DA=%pM len=%u wcid=%d\n",
			 le16_to_cpu(hdr->frame_control),
			 hdr->addr1, skb->len,
			 wcid ? wcid->idx : -1);
	}
	if (!dev->ring_tx0.desc) {
		dev_err(&dev->pdev->dev, "TX: ring_tx0 not initialized!\n");
		dev_kfree_skb(skb);
		return;
	}

	ret = mt7927_tx_queue_skb(dev, &dev->ring_tx0, skb, wcid);
	if (ret) {
		dev_err(&dev->pdev->dev, "TX: queue_skb failed: %d\n", ret);
		dev_kfree_skb(skb);
		return;
	}

	mt7927_tx_kick(dev, &dev->ring_tx0);
}

/* --- start/stop --- */
static int mt7927_mac80211_start(struct ieee80211_hw *hw)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	int ret;

	dev_info(&dev->pdev->dev, "mac80211: start\n");

	/* === DIAG: 测试 MCU 通道是否还活着 === */
	dev_info(&dev->pdev->dev, "DIAG: 测试 MCU 通道...\n");
	ret = mt7927_mcu_send_unicmd(dev, UNI_CMD_ID_NIC_CAP,
				     UNI_CMD_OPT_SET_ACK, NULL, 0);
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "DIAG: MCU 通道已断! NIC_CAP 无响应 ret=%d\n", ret);
	else
		dev_info(&dev->pdev->dev,
			 "DIAG: MCU 通道正常, NIC_CAP 成功\n");

	/* 发送固件必需的扫描前置命令
	 * 参考: mt76/mt7925/main.c mt7925_start() lines 314-329
	 * 缺少这些命令，固件会静默丢弃 scan 请求！ */

	/* 1. SET_DOMAIN_INFO — 信道域信息 (最关键!) */
	ret = mt7927_mcu_set_channel_domain(dev);
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "mac80211 start: set_channel_domain 失败: %d (继续)\n",
			 ret);

	/* 2. BAND_CONFIG / RTS_THRESHOLD */
	ret = mt7927_mcu_set_rts_thresh(dev, 0x92b);
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "mac80211 start: set_rts_thresh 失败: %d (继续)\n",
			 ret);

	return 0;
}

static void mt7927_mac80211_stop(struct ieee80211_hw *hw, bool suspend)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);

	dev_info(&dev->pdev->dev, "mac80211: stop (suspend=%d)\n", suspend);
}

/* --- interface management --- */
static int mt7927_add_interface(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	int idx, ret;

	for (idx = 0; idx < 4; idx++) {
		if (!(dev->vif_mask & BIT_ULL(idx)))
			break;
	}
	if (idx >= 4)
		return -ENOSPC;

	mvif->bss_idx = idx;
	mvif->omac_idx = idx;
	mvif->wmm_idx = idx;
	mvif->band_idx = 0xff;	/* BAND_AUTO — 固件自动选择, ROC_GRANT 会更新 */
	mvif->vif = vif;
	mvif->sta.wcid.idx = idx;
	mvif->sta.vif = mvif;
	dev->vif_mask |= BIT_ULL(idx);
	dev->omac_mask |= BIT_ULL(idx);

	/* 按 mt7925 顺序发送 MCU 命令:
	 * 1. DEV_INFO_UPDATE — 激活 OMAC
	 * 2. BSS_INFO_UPDATE — 创建 BSS, 固件分配资源
	 * 缺少 BSS_INFO 会导致 scan 等后续命令被固件静默忽略 */
	ret = mt7927_mcu_uni_add_dev(dev, vif, true);
	if (ret)
		goto err;

	ret = mt7927_mcu_add_bss_info(dev, vif, true);
	if (ret)
		goto err;

	dev_info(&dev->pdev->dev,
		 "mac80211: add_interface bss_idx=%d type=%d\n",
		 idx, vif->type);
	return 0;

err:
	dev->vif_mask &= ~BIT_ULL(idx);
	dev->omac_mask &= ~BIT_ULL(idx);
	return ret;
}

static void mt7927_remove_interface(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;

	/* 停用接口 — mt7925 顺序: BSS_INFO(off) → DEV_INFO(off) */
	mt7927_mcu_add_bss_info(dev, vif, false);
	mt7927_mcu_uni_add_dev(dev, vif, false);

	dev->vif_mask &= ~BIT_ULL(mvif->bss_idx);
	dev->omac_mask &= ~BIT_ULL(mvif->omac_idx);

	dev_info(&dev->pdev->dev,
		 "mac80211: remove_interface bss_idx=%d\n", mvif->bss_idx);
}

/* --- config --- */
static int mt7927_config(struct ieee80211_hw *hw, int radio_idx, u32 changed)
{
	/* TODO: 处理 IEEE80211_CONF_CHANGE_CHANNEL 等 */
	return 0;
}

/* --- RX filter --- */
static void mt7927_configure_filter(struct ieee80211_hw *hw,
				    unsigned int changed_flags,
				    unsigned int *total_flags,
				    u64 multicast)
{
	*total_flags &= (FIF_ALLMULTI | FIF_BCN_PRBRESP_PROMISC);
}

/* --- BSS info --- */
static void mt7927_bss_info_changed(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_bss_conf *info,
				    u64 changed)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);

	/* BSS_CHANGED_BSSID: mac80211 设置了目标 AP 的 BSSID (auth 之前)
	 * 必须更新固件的 BSS_INFO，否则固件不知道目标 BSS，auth 帧无法发出 */
	if (changed & BSS_CHANGED_BSSID) {
		mt7927_mcu_add_bss_info(dev, vif, true);
		dev_info(&dev->pdev->dev,
			 "mac80211: bss_info BSSID changed to %pM\n",
			 vif->bss_conf.bssid);
	}

	if (changed & BSS_CHANGED_ASSOC) {
		mt7927_mcu_add_bss_info(dev, vif, vif->cfg.assoc);
		dev_info(&dev->pdev->dev,
			 "mac80211: bss_info assoc=%d\n", vif->cfg.assoc);
	}

	if (changed & BSS_CHANGED_BEACON_INT) {
		mt7927_mcu_add_bss_info(dev, vif, true);
		dev_info(&dev->pdev->dev,
			 "mac80211: beacon_int=%d\n",
			 vif->bss_conf.beacon_int);
	}
}

/* --- HW scan --- */
static int mt7927_hw_scan(struct ieee80211_hw *hw,
			  struct ieee80211_vif *vif,
			  struct ieee80211_scan_request *req)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);

	if (test_bit(MT7927_SCANNING, &dev->scan_state))
		return -EBUSY;

	/* 如果 ROC 活跃中 (auth/assoc 进行中), 推迟扫描
	 * 否则扫描命令会中断 ROC, 导致 auth 帧被固件丢弃 */
	if (dev->roc_active)
		return -EBUSY;

	return mt7927_mcu_hw_scan(dev, vif, req);
}

static void mt7927_cancel_hw_scan(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct cfg80211_scan_info info = { .aborted = true };

	mt7927_mcu_cancel_hw_scan(dev, vif);
	cancel_delayed_work_sync(&dev->scan_work);

	/* 通知 mac80211 扫描已终止，清除 local->scan_req + SCAN_HW_SCANNING。
	 * 不调此函数 → mac80211 认为 scan 仍在进行 → 后续所有 scan 返回 -EBUSY */
	if (dev->hw && dev->hw_init_done)
		ieee80211_scan_completed(dev->hw, &info);
}

/* --- STA state machine --- */
static int mt7927_sta_state(struct ieee80211_hw *hw,
			    struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta,
			    enum ieee80211_sta_state old_state,
			    enum ieee80211_sta_state new_state)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct mt7927_sta *msta = (struct mt7927_sta *)sta->drv_priv;
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	int ret;

	dev_info(&dev->pdev->dev,
		 "mac80211: sta_state %d->%d addr=%pM\n",
		 old_state, new_state, sta->addr);

	/* NOTEXIST → NONE: 分配 WCID + 通知固件 */
	if (old_state == IEEE80211_STA_NOTEXIST &&
	    new_state == IEEE80211_STA_NONE) {
		int idx;

		/* 查找空闲 WCID (1-19, 0 留给 vif 自身) */
		for (idx = 1; idx < MT7927_WTBL_SIZE; idx++) {
			if (!dev->wcid[idx])
				break;
		}
		if (idx >= MT7927_WTBL_SIZE)
			return -ENOSPC;

		msta->wcid.idx = idx;
		msta->wcid.sta = sta;
		msta->vif = mvif;
		dev->wcid[idx] = &msta->wcid;

		/* 清除 WTBL ADM 计数器 — mt7925 在每次分配 WCID 后都做这步
		 * 不清除可能导致固件读到陈旧的 admission count 影响 TX 调度
		 * 来源: mt7925/main.c mt7925_mac_sta_add() */
		mt7927_mac_wtbl_update(dev, idx,
				       MT_WTBL_UPDATE_ADM_COUNT_CLEAR);

		/* BSS_INFO 必须在 STA_REC 之前发送
		 * mt7925 注释: "should update bss info before STA add"
		 * 固件需要先知道 BSS 上下文才能正确创建 WTBL 条目 */
		mt7927_mcu_add_bss_info(dev, vif, true);

		/* 告诉固件创建 WTBL 条目
		 * MT6639 UniCmd 转换硬编码 ucConnectionState = STATE_CONNECTED(1)
		 * 不论实际状态如何, STA_REC_BASIC 总是发 conn_state=1
		 * 来源: mt6639/nic/nic_uni_cmd_event.c nicUniCmdStaRecTagBasic() */
		ret = mt7927_mcu_sta_update(dev, vif, sta, true,
					    CONN_STATE_CONNECT);
		if (ret) {
			dev->wcid[idx] = NULL;
			return ret;
		}
		return 0;
	}

	/* NONE → NOTEXIST: 释放 WCID */
	if (old_state == IEEE80211_STA_NONE &&
	    new_state == IEEE80211_STA_NOTEXIST) {
		if (msta->wcid.idx < MT7927_WTBL_SIZE)
			dev->wcid[msta->wcid.idx] = NULL;
		return 0;
	}

	/* AUTH → ASSOC: 发送 BSS_INFO + STA_REC */
	if (old_state == IEEE80211_STA_AUTH &&
	    new_state == IEEE80211_STA_ASSOC) {
		mt7927_mcu_add_bss_info(dev, vif, true);
		mt7927_mcu_sta_update(dev, vif, sta, true,
				      CONN_STATE_CONNECT);
		return 0;
	}

	/* ASSOC → AUTHORIZED: 端口授权 */
	if (old_state == IEEE80211_STA_ASSOC &&
	    new_state == IEEE80211_STA_AUTHORIZED) {
		mt7927_mcu_sta_update(dev, vif, sta, true,
				      CONN_STATE_CONNECT);
		return 0;
	}

	/* ASSOC → AUTH: 断开 */
	if (old_state == IEEE80211_STA_ASSOC &&
	    new_state == IEEE80211_STA_AUTH) {
		mt7927_mcu_sta_update(dev, vif, sta, false,
				      CONN_STATE_DISCONNECT);
		mt7927_mcu_add_bss_info(dev, vif, false);
		return 0;
	}

	return 0;
}

/* --- key management --- */
static int mt7927_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			  struct ieee80211_vif *vif,
			  struct ieee80211_sta *sta,
			  struct ieee80211_key_conf *key)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	int ret;

	/* 仅支持常见加密算法 */
	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_TKIP:
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
	case WLAN_CIPHER_SUITE_AES_CMAC:
		break;
	default:
		return -EOPNOTSUPP;
	}

	ret = mt7927_mcu_add_key(dev, vif, sta, key, cmd);
	if (ret)
		return ret;

	if (cmd == SET_KEY)
		key->flags |= IEEE80211_KEY_FLAG_GENERATE_IV;

	return 0;
}

static int mt7927_set_rts_threshold(struct ieee80211_hw *hw, int radio_idx,
				    u32 val)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);

	return mt7927_mcu_set_rts_thresh(dev, val);
}

/* =====================================================================
 * UNI_CHANNEL_SWITCH — 完整配置 TX+RX 射频链路
 * ROC 只配置 RX 监听，不配置 TX 射频。固件需要 CHANNEL_SWITCH 才能发帧。
 * 参考: mt7996/mcu.c mt7996_mcu_set_chan_info()
 * ===================================================================== */
static int mt7927_mcu_set_chan_info(struct mt7927_dev *dev,
				    struct ieee80211_channel *chan,
				    u16 tag)
{
	struct {
		u8 rsv[4];		/* UniCmd body 通用头 */

		__le16 tag;		/* +0 */
		__le16 len;		/* +2 */
		u8 control_ch;		/* +4: chan->hw_value */
		u8 center_ch;		/* +5: = control_ch (20MHz) */
		u8 bw;			/* +6: 0=20MHz */
		u8 tx_path_num;		/* +7: tx chain count */
		u8 rx_path;		/* +8: rx chain count or mask */
		u8 switch_reason;	/* +9: CH_SWITCH_NORMAL=0 */
		u8 band_idx;		/* +10: 0 */
		u8 center_ch2;		/* +11: 0 (80+80 only) */
		__le16 cac_case;	/* +12: 0 */
		u8 channel_band;	/* +14: 1=2.4G, 2=5G, 3=6G */
		u8 rsv0;		/* +15 */
		__le32 outband_freq;	/* +16: 0 */
		u8 txpower_drop;	/* +20: 0 */
		u8 ap_bw;		/* +21: 0 */
		u8 ap_center_ch;	/* +22: 0 */
		u8 rsv1[53];		/* +23: padding to match mt7996 */
	} __packed req = {0};

	req.tag = cpu_to_le16(tag);
	req.len = cpu_to_le16(sizeof(req) - 4);
	req.control_ch = chan->hw_value;
	req.center_ch = chan->hw_value;	/* 20MHz: center = control */
	req.bw = 0;			/* CMD_CBW_20MHZ */
	req.tx_path_num = 2;		/* MT6639: 2T2R */
	req.switch_reason = CH_SWITCH_NORMAL;
	/* MT6639 DBDC: band 0 = 2.4GHz, band 1 = 5GHz */
	req.band_idx = (chan->band == NL80211_BAND_5GHZ) ? 1 : 0;

	/* rx_path: CHANNEL_SWITCH 用 count, RX_PATH 用 bitmask
	 * 参考 mt7996: if (tag == UNI_CHANNEL_SWITCH) req.rx_path = hweight8() */
	if (tag == UNI_CHANNEL_SWITCH)
		req.rx_path = 2;	/* 2 RX chains */
	else
		req.rx_path = 0x3;	/* bitmask: chain 0 + chain 1 */

	/* band 映射: 1=2.4G, 2=5G, 3=6G (CMD_BAND_*, 与 ROC 一致)
	 * MT7927/MT6639 固件要求 CMD_BAND_* 编码, 不是 mt7996 的 0-indexed */
	switch (chan->band) {
	case NL80211_BAND_5GHZ:
		req.channel_band = 2;
		break;
	case NL80211_BAND_6GHZ:
		req.channel_band = 3;
		break;
	default:
		req.channel_band = 1;
		break;
	}

	dev_info(&dev->pdev->dev,
		 "CHAN_INFO: tag=%u ch=%u band=%u bw=%u tx=%u rx=0x%x reason=%u\n",
		 tag, req.control_ch, req.channel_band, req.bw,
		 req.tx_path_num, req.rx_path, req.switch_reason);

	/* mt7996 用 wait_resp=true — 必须等固件完成信道切换再继续 */
	return mt7927_mcu_send_unicmd(dev, UNI_CMD_ID_CHANNEL_SWITCH,
				       UNI_CMD_OPT_SET_ACK, &req, sizeof(req));
}

/* ROC (Remain On Channel) — 让固件切到目标 AP 频道 */
static void mt7927_mgd_prepare_tx(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_prep_tx_info *info)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct ieee80211_channel *chan;
	u16 duration = info->duration ? info->duration : 1000; /* 默认 1 秒 */
	/* ROC TLV 结构 — 匹配 mt7925/mcu.h UNI_ROC_ACQUIRE 布局 */
	struct {
		u8 rsv[4];              /* UniCmd body 通用头 */
		__le16 tag;             /* +0: UNI_ROC_ACQUIRE = 0 */
		__le16 len;             /* +2 */
		u8 bss_idx;             /* +4 */
		u8 tokenid;             /* +5 */
		u8 control_channel;     /* +6: chan->hw_value */
		u8 sco;                 /* +7: 0 */
		u8 band;                /* +8: 0=2.4G, 1=5G */
		u8 bw;                  /* +9: 0=20MHz */
		u8 center_chan;          /* +10: = control_channel */
		u8 center_chan2;         /* +11: 0 */
		u8 bw_from_ap;          /* +12: 0 */
		u8 center_chan_from_ap;  /* +13: 0 */
		u8 center_chan2_from_ap; /* +14: 0 */
		u8 reqtype;             /* +15: 0 = JOIN */
		__le32 maxinterval;     /* +16: duration (ms) */
		u8 dbdcband;            /* +20: 0xfe (BAND_ALL) */
		u8 pad[3];              /* +21: padding */
	} __packed req = {0};

	/* 获取频道 — 优先从 vif BSS conf 获取 */
	if (vif->bss_conf.chanreq.oper.chan)
		chan = vif->bss_conf.chanreq.oper.chan;
	else
		chan = hw->conf.chandef.chan;
	if (!chan) {
		dev_warn(&dev->pdev->dev, "mgd_prepare_tx: no channel!\n");
		return;
	}

	req.tag = cpu_to_le16(0);  /* UNI_ROC_ACQUIRE */
	req.len = cpu_to_le16(sizeof(req) - 4);
	req.bss_idx = mvif->bss_idx;
	req.control_channel = chan->hw_value;
	req.bw = 0;  /* CMD_CBW_20MHZ */
	req.bw_from_ap = 0;  /* CMD_CBW_20MHZ */
	req.center_chan = chan->hw_value;
	req.center_chan_from_ap = chan->hw_value;
	req.reqtype = 0;  /* JOIN */
	req.dbdcband = 0xff;  /* MT6639 用 ENUM_BAND_AUTO=0xFF */
	req.maxinterval = cpu_to_le32(duration);

	/* band 映射: mt7925 用 1=2.4G, 2=5G, 3=6G */
	switch (chan->band) {
	case NL80211_BAND_5GHZ:
		req.band = 2;
		break;
	default:
		req.band = 1;  /* 2.4GHz */
		break;
	}

	/* 移除了 CHANNEL_SWITCH — STA 模式不需要, 可能和 ROC 冲突
	 * mt7925 STA 模式从不使用 CHANNEL_SWITCH */

	/* 如果 ROC 已活跃, 复用已有的 ROC 会话 — 不要 abort + re-acquire!
	 * 测试发现 MT6639 固件在 abort 后不会为新 acquire 发 ROC_GRANT.
	 * mac80211 auth 重试时频道相同, 直接复用是安全的. */
	if (dev->roc_active) {
		dev_info(&dev->pdev->dev,
			 "mgd_prepare_tx: ROC already active (band_idx=%u), reusing\n",
			 mvif->band_idx);
		return;
	}

	dev_info(&dev->pdev->dev,
		 "mgd_prepare_tx: ROC acquire bss=%u ch=%u band=%u dur=%u\n",
		 mvif->bss_idx, chan->hw_value, req.band, duration);

	/* 重置 completion, 发送 ROC, 等待 ROC_GRANT 事件
	 * mt7925 在 mgd_prepare_tx() 中等待最多 4 秒 */
	reinit_completion(&dev->roc_complete);

	mt7927_mcu_send_unicmd(dev, 0x27, /* MCU_UNI_CMD_ROC */
			       UNI_CMD_OPT_SET_ACK, /* 0x07 — 需要响应 */
			       &req, sizeof(req));

	/* 等待固件 ROC_GRANT 事件确认信道切换完成
	 * 不等待的后果: mac80211 立即发送 auth 帧, 但固件还没切信道 → 丢弃 */
	if (!wait_for_completion_timeout(&dev->roc_complete, 4 * HZ)) {
		/* Fix 5: 超时后发 abort 清理固件 ROC 状态
		 * 不清理的话固件认为 ROC 仍活跃, 后续 ROC 请求全部超时
		 * mt7925 也在超时时 abort: mt7925_set_roc() line 523 */
		struct {
			u8 rsv[4];
			__le16 tag;
			__le16 len;
			u8 bss_idx;
			u8 tokenid;
			u8 dbdcband;
			u8 rsv2[5];
		} __packed timeout_abort = {0};

		timeout_abort.tag = cpu_to_le16(1);  /* UNI_ROC_ABORT */
		timeout_abort.len = cpu_to_le16(sizeof(timeout_abort) - 4);
		timeout_abort.bss_idx = mvif->bss_idx;
		timeout_abort.dbdcband = 0xff;

		dev_warn(&dev->pdev->dev,
			 "mgd_prepare_tx: ROC grant timeout (4s)! sending abort\n");
		mt7927_mcu_send_unicmd(dev, 0x27,
				       UNI_CMD_OPT_SET_ACK,
				       &timeout_abort, sizeof(timeout_abort));
		dev->roc_active = false;
	} else {
		/* ROC_GRANT 返回了固件分配的 dbdcband — 更新 VIF 的 band_idx
		 * MT6639 是 DBDC 芯片: band 0 = 2.4GHz, band 1 = 5GHz
		 * TXD 的 TGID 字段取自 msta->vif->band_idx, 如果 band_idx 错误
		 * 固件会丢弃帧 (TXFREE stat=1)
		 *
		 * mt7925 参考: mt7925/mcu.c mt7925_mcu_roc_iter() line 313 */
		mvif->band_idx = dev->roc_grant_band_idx;
		dev_info(&dev->pdev->dev,
			 "mgd_prepare_tx: ROC grant received, band_idx=%u\n",
			 mvif->band_idx);

		/* 重发 DEV_INFO + BSS_INFO 使固件上下文使用正确的 band_idx
		 * add_interface 时 band_idx=0xff (AUTO), 现在更新为实际值
		 * DEV_INFO 配置 OMAC 的 band 归属, 必须在 BSS_INFO 之前 */
		mt7927_mcu_uni_add_dev(dev, vif, true);
		mt7927_mcu_add_bss_info(dev, vif, true);

		/* 重发 STA_REC — STA_REC 在 sta_state(NOTEXIST→NONE) 时发送,
		 * 那时 band_idx=0xff, WTBL 条目创建时可能关联了错误的 band.
		 * ROC_GRANT 后 band_idx 已更新, 重发 STA_REC 让固件更新 WTBL.
		 * AP STA 分配在 wcid[1], 通过 container_of 反查 ieee80211_sta */
		if (dev->wcid[1]) {
			struct mt7927_sta *msta = container_of(dev->wcid[1],
							       struct mt7927_sta, wcid);
			struct ieee80211_sta *sta = container_of((void *)msta,
								 struct ieee80211_sta,
								 drv_priv);
			dev_info(&dev->pdev->dev,
				 "mgd_prepare_tx: resend STA_REC after ROC_GRANT (band_idx=%u)\n",
				 mvif->band_idx);
			mt7927_mcu_sta_update(dev, vif, sta, true,
					      CONN_STATE_CONNECT);
		}

		/* mt7925 在 mgd_prepare_tx 后不发 CHANNEL_SWITCH
		 * ROC 已经让固件切到目标频道, CHANNEL_SWITCH 可能干扰 ROC 状态
		 * 移除: 之前的 "实验 A" 没有效果 */

		/* 诊断: dump WTBL WLAN_IDX=1 内容 */
		{
			int w;

			for (w = 0; w < 8; w++) {
				u32 ctrl = MT_WTBL_ITCR_EXEC |
					   (1 << 16) | /* WLAN_IDX=1 at bits[29:16] */
					   w;          /* DW index at bits[4:0] */
				mt7927_wr(dev, MT_WTBL_ITCR, ctrl);
				udelay(10);
				dev_info(&dev->pdev->dev,
					 "WTBL[1] DW%d: 0x%08x\n",
					 w, mt7927_rr(dev, MT_WTBL_ITDR0));
			}
		}
	}
}

static void mt7927_mgd_complete_tx(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif,
				   struct ieee80211_prep_tx_info *info)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct {
		u8 rsv[4];
		__le16 tag;     /* UNI_ROC_ABORT = 1 */
		__le16 len;
		u8 bss_idx;
		u8 tokenid;
		u8 dbdcband;    /* 0xff = auto */
		u8 rsv2[5];
	} __packed req = {0};

	/* Fix 4: 只在 ROC 活跃时发 abort
	 * mt7925 用 test_and_clear_bit(MT76_STATE_ROC)
	 * 如果 ROC 未活跃 (超时/已 abort), 不要向固件发无效的 abort */
	if (!dev->roc_active)
		return;

	req.tag = cpu_to_le16(1);  /* UNI_ROC_ABORT */
	req.len = cpu_to_le16(sizeof(req) - 4);
	req.bss_idx = mvif->bss_idx;
	req.dbdcband = 0xff;

	dev->roc_active = false;
	dev_info(&dev->pdev->dev, "mgd_complete_tx: ROC abort\n");

	mt7927_mcu_send_unicmd(dev, 0x27, /* MCU_UNI_CMD_ROC */
			       UNI_CMD_OPT_SET_ACK, /* 0x07 — 等待固件 ACK */
			       &req, sizeof(req));
}

/* ieee80211_ops 回调集合 */
static const struct ieee80211_ops mt7927_ops = {
	.tx			= mt7927_mac80211_tx,
	.start			= mt7927_mac80211_start,
	.stop			= mt7927_mac80211_stop,
	.add_interface		= mt7927_add_interface,
	.remove_interface	= mt7927_remove_interface,
	.config			= mt7927_config,
	.configure_filter	= mt7927_configure_filter,
	.bss_info_changed	= mt7927_bss_info_changed,
	.hw_scan		= mt7927_hw_scan,
	.cancel_hw_scan		= mt7927_cancel_hw_scan,
	.sta_state		= mt7927_sta_state,
	.set_key		= mt7927_set_key,
	.set_rts_threshold	= mt7927_set_rts_threshold,
	.mgd_prepare_tx		= mt7927_mgd_prepare_tx,
	.mgd_complete_tx	= mt7927_mgd_complete_tx,
	.wake_tx_queue		= ieee80211_handle_wake_tx_queue,
	.add_chanctx		= ieee80211_emulate_add_chanctx,
	.remove_chanctx		= ieee80211_emulate_remove_chanctx,
	.change_chanctx		= ieee80211_emulate_change_chanctx,
	.switch_vif_chanctx	= ieee80211_emulate_switch_vif_chanctx,
};

/* 注册 mac80211 设备 */
static int mt7927_register_device(struct mt7927_dev *dev)
{
	struct ieee80211_hw *hw;
	int ret;

	hw = ieee80211_alloc_hw(sizeof(struct mt7927_dev *), &mt7927_ops);
	if (!hw)
		return -ENOMEM;

	*(struct mt7927_dev **)hw->priv = dev;
	dev->hw = hw;
	dev->phy.hw = hw;
	dev->phy.dev = dev;

	SET_IEEE80211_DEV(hw, &dev->pdev->dev);

	/* 私有数据大小 (mac80211 为每个 vif/sta 分配) */
	hw->vif_data_size = sizeof(struct mt7927_vif);
	hw->sta_data_size = sizeof(struct mt7927_sta);

	/* wiphy 参数 */
	hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION);
	hw->queues = 4;
	hw->max_rates = 1;
	hw->max_report_rates = 7;

	/* 硬件特性标志 */
	ieee80211_hw_set(hw, HAS_RATE_CONTROL);
	ieee80211_hw_set(hw, REPORTS_TX_ACK_STATUS);
	ieee80211_hw_set(hw, SIGNAL_DBM);
	ieee80211_hw_set(hw, SINGLE_SCAN_ON_ALL_BANDS);
	ieee80211_hw_set(hw, SUPPORTS_HT_CCK_RATES);
	ieee80211_hw_set(hw, CONNECTION_MONITOR);

	/* 注册频段 */
	hw->wiphy->bands[NL80211_BAND_2GHZ] = &mt7927_band_2ghz;
	hw->wiphy->bands[NL80211_BAND_5GHZ] = &mt7927_band_5ghz;

	/* 芯片能力 */
	dev->phy.cap.has_2ghz = true;
	dev->phy.cap.has_5ghz = true;
	dev->phy.antenna_mask = 0x03;	/* 2x2 MIMO */
	dev->phy.chainmask = 0x03;

	/* MAC 地址 (初期使用随机本地地址, 后续从 NIC_CAP 获取) */
	eth_random_addr(hw->wiphy->perm_addr);

	/* 监管域: 驱动自管理，避免内核世界域给 5GHz 加 NO_IR/PASSIVE-SCAN
	 * 没有这个标志，所有 5GHz 信道都是 PASSIVE-SCAN，无法发送 auth 帧!
	 * 来源: mt76/mac80211.c → REGULATORY_WIPHY_SELF_MANAGED */
	hw->wiphy->regulatory_flags |= REGULATORY_WIPHY_SELF_MANAGED;

	/* 扫描支持 */
	hw->wiphy->max_scan_ssids = MT7927_SCAN_MAX_SSIDS;
	hw->wiphy->max_scan_ie_len = MT7927_SCAN_IE_LEN;

	/* 注册到 mac80211 */
	ret = ieee80211_register_hw(hw);
	if (ret) {
		dev_err(&dev->pdev->dev,
			"ieee80211_register_hw 失败: %d\n", ret);
		ieee80211_free_hw(hw);
		dev->hw = NULL;
		return ret;
	}

	dev->hw_init_done = true;
	dev_info(&dev->pdev->dev,
		 "mac80211: 注册成功, MAC=%pM\n", hw->wiphy->perm_addr);
	return 0;
}

/* 注销 mac80211 设备 */
static void mt7927_unregister_device(struct mt7927_dev *dev)
{
	if (!dev->hw)
		return;

	if (dev->hw_init_done) {
		ieee80211_unregister_hw(dev->hw);
		dev->hw_init_done = false;
	}
	ieee80211_free_hw(dev->hw);
	dev->hw = NULL;
}

/* =====================================================================
 * 10. PCI Probe / Remove
 * ===================================================================== */

static int mt7927_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *id)
{
	struct mt7927_dev *dev;
	int ret, i;

	dev_info(&pdev->dev,
		 "MT7927 PCIe 驱动 probe: vendor=0x%04x device=0x%04x\n",
		 pdev->vendor, pdev->device);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->pdev = pdev;
	pci_set_drvdata(pdev, dev);
	dev->tx_mgmt_pid = 1;	/* PID=0 无效 (Windows RE: 有效范围 1-99) */

	/* PCI 初始化 */
	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_enable_device 失败: %d\n", ret);
		goto err_free;
	}

	pci_set_master(pdev);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "DMA mask 设置失败: %d\n", ret);
		goto err_disable;
	}

	ret = pci_request_regions(pdev, "mt7927");
	if (ret) {
		dev_err(&pdev->dev, "pci_request_regions 失败: %d\n", ret);
		goto err_disable;
	}

	dev->bar0 = pci_iomap(pdev, 0, 0);
	if (!dev->bar0) {
		dev_err(&pdev->dev, "BAR0 映射失败\n");
		ret = -ENOMEM;
		goto err_release;
	}
	dev->bar0_len = pci_resource_len(pdev, 0);
	dev_info(&pdev->dev, "BAR0 映射成功: len=0x%llx\n",
		 (u64)dev->bar0_len);

	/* ===== Windows 初始化序列 (严格按 vtable 顺序) ===== */

	/* 阶段 0: SET_OWN (在 MCU init 前, 新增 - 来自团队修复 Task #4)
	 * 来源: 旧驱动验证的工作序列 */
	dev_info(&pdev->dev, "probe: SET_OWN\n");
	mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_SET_OWN);
	usleep_range(2000, 3000);

	/* 阶段 1: MT6639 MCU 初始化 (PreFirmwareDownloadInit 前置) */
	ret = mt7927_mcu_init_mt6639(dev);
	if (ret) {
		dev_err(&pdev->dev, "MCU 初始化失败: %d\n", ret);
		goto err_unmap;
	}

	/* 阶段 1.5: CLR_OWN (在 MCU init 后, 新增 - 来自团队修复 Task #4)
	 * 触发 ROM 初始化 WFDMA，后续 ring 分配将在 ROM 已配置的状态上工作
	 * 来源: 旧驱动的裸 CLR_OWN (drv_own) 验证序列 */
	dev_info(&pdev->dev, "probe: CLR_OWN (ROM 初始化 WFDMA)\n");
	for (i = 0; i < 10; i++) {
		mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN);
		usleep_range(1000, 2000);
		if (!(mt7927_rr(dev, MT_CONN_ON_LPCTL) & PCIE_LPCR_HOST_OWN_SYNC))
			break;
	}
	dev_info(&pdev->dev, "probe CLR_OWN 完成 (尝试 %d 次)\n", i + 1);

	/* PCIe 睡眠禁用
	 * 来源: Windows AsicConnac3xSetCbInfraPcieSlpCfg */
	{
		u32 slp = mt7927_rr(dev, MT_CB_INFRA_SLP_CTRL);

		if (slp != MT_CB_INFRA_SLP_CTRL_VAL)
			mt7927_wr(dev, MT_CB_INFRA_SLP_CTRL,
				  MT_CB_INFRA_SLP_CTRL_VAL);
	}

	/* DMA 初始化: 清除中断 + 禁用 + 复位 */
	/* 清除中断状态 (团队修复次要项) */
	mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, 0xffffffff);

	mt7927_dma_disable(dev);
	mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
	mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
	wmb();
	msleep(10);

	/* 阶段 2: InitTxRxRing */
	ret = mt7927_init_tx_rx_ring(dev);
	if (ret) {
		dev_err(&pdev->dev, "Ring 初始化失败: %d\n", ret);
		goto err_dma;
	}

	/* 阶段 3: WpdmaConfig */
	mt7927_wpdma_config(dev, true);
	/* FWDL bypass BIT(9): 仅在固件下载阶段单独设置
	 * Windows: BIT(9) 只在 FWDL 前设置, 不通过 wpdma_config 设置 */
	mt7927_rmw(dev, MT_WPDMA_GLO_CFG, 0, MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL);

	/* 阶段 4: ConfigIntMask */
	mt7927_config_int_mask(dev, true);

	/* 阶段 5: FWDL */
	if (skip_fwdl) {
		dev_info(&pdev->dev, "跳过 FWDL (skip_fwdl=1)\n");
	} else {
		ret = mt7927_fw_download(dev);
		if (ret) {
			dev_err(&pdev->dev, "FWDL 失败: %d\n", ret);
			goto err_dma;
		}

		/* 阶段 6: PostFwDownloadInit */
		ret = mt7927_post_fw_init(dev);
		if (ret)
			dev_warn(&pdev->dev,
				 "PostFwDownloadInit 失败: %d (继续)\n", ret);
	}

	/* 诊断 A: PostFwDownloadInit 刚完成, 测试 MCU 通信
	 * 此时环境和 PostFwDownloadInit 中的 NIC_CAP 完全一样
	 * 如果失败 → PostFwDownloadInit 本身破坏了某些状态 */
	dev_info(&pdev->dev,
		 "诊断A: PostFwDownloadInit 刚完成, 重新发送 NIC_CAP\n");
	{
		int test_ret;
		u32 tx15_cidx_before, tx15_didx_before, rx6_didx_before;
		u32 tx15_cidx_after, tx15_didx_after, rx6_didx_after;

		tx15_cidx_before = mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(15));
		tx15_didx_before = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(15));
		rx6_didx_before = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));

		test_ret = mt7927_mcu_send_unicmd(dev,
			UNI_CMD_ID_NIC_CAP, UNI_CMD_OPT_SET_ACK,
			NULL, 0);

		tx15_cidx_after = mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(15));
		tx15_didx_after = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(15));
		rx6_didx_after = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));

		dev_info(&pdev->dev,
			 "诊断A: NIC_CAP %s (ret=%d)\n"
			 "  TX15: CIDX %u→%u DIDX %u→%u  RX6: DIDX %u→%u  tail=%u\n",
			 test_ret ? "失败" : "成功", test_ret,
			 tx15_cidx_before, tx15_cidx_after,
			 tx15_didx_before, tx15_didx_after,
			 rx6_didx_before, rx6_didx_after,
			 dev->ring_rx6.tail);
		/* 检查所有 RX ring DIDX — 找出 MCU 响应去了哪里 */
		{
			int i;
			for (i = 0; i < 8; i++) {
				u32 base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(i));
				u32 cnt = mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(i));
				u32 cidx = mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(i));
				u32 didx = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(i));
				if (base || cnt || cidx || didx)
					dev_info(&pdev->dev,
						 "诊断A: RX%d BASE=0x%08x CNT=%u CIDX=%u DIDX=%u\n",
						 i, base, cnt, cidx, didx);
			}
		}
	}

	/* 诊断 dump */
	mt7927_dump_status(dev);

	/* 阶段 7a: 初始化工作队列 */
	INIT_DELAYED_WORK(&dev->scan_work, mt7927_scan_work);
	INIT_WORK(&dev->mgmt_tx_work, mt7927_mgmt_tx_worker);
	skb_queue_head_init(&dev->mgmt_tx_queue);
	init_waitqueue_head(&dev->mcu_wait);
	init_completion(&dev->roc_complete);
	skb_queue_head_init(&dev->phy.scan_event_list);

	/* 阶段 7b: 分配 NAPI dummy netdev + 初始化 NAPI */
	dev->napi_dev = alloc_netdev_dummy(0);
	if (!dev->napi_dev) {
		dev_err(&pdev->dev, "NAPI dummy netdev 分配失败\n");
		ret = -ENOMEM;
		goto err_dma;
	}

	netif_napi_add(dev->napi_dev, &dev->napi_rx_data,
		       mt7927_poll_rx_data);
	netif_napi_add(dev->napi_dev, &dev->napi_rx_mcu,
		       mt7927_poll_rx_mcu);
	netif_napi_add_tx(dev->napi_dev, &dev->tx_napi,
			  mt7927_poll_tx);

	napi_enable(&dev->napi_rx_data);
	napi_enable(&dev->napi_rx_mcu);
	napi_enable(&dev->tx_napi);
	dev->napi_running = true;

	/* 阶段 7c: 初始化中断 tasklet */
	tasklet_init(&dev->irq_tasklet, mt7927_irq_tasklet,
		     (unsigned long)dev);

	/* 阶段 7d: 注册 IRQ
	 * 顺序: 先关 WFDMA 中断 → 开 PCIe MAC 中断转发 → 注册 IRQ →
	 *        开 WFDMA 中断 + MCU2HOST 软件中断
	 * 来源: mt76/mt7925/pci.c probe 顺序 */
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_INTX);
	if (ret < 0) {
		dev_err(&pdev->dev, "IRQ 向量分配失败: %d\n", ret);
		goto err_napi;
	}

	/* 先关闭 WFDMA 中断, 防止注册前触发 */
	mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA, 0);

	/* 开启 PCIe MAC 层中断转发 — 没有这步中断无法到达 CPU!
	 * 来源: mt7925/pci.c line 407 */
	mt7927_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0xff);

	ret = request_irq(pci_irq_vector(pdev, 0), mt7927_irq_handler,
			  IRQF_SHARED, "mt7927", dev);
	if (ret) {
		dev_err(&pdev->dev, "IRQ 注册失败: %d\n", ret);
		goto err_irq_vec;
	}

	/* 在开启中断前，清除 PostFwDownloadInit 遗留的中断状态
	 * INT_STA=0x06004000 (NIC_CAP + TX done) 会导致虚假 IRQ
	 * MCU_CMD=0x00008000 未 ACK 可能阻塞固件事件队列 */
	{
		u32 int_sta = mt7927_rr(dev, MT_WFDMA_HOST_INT_STA);
		u32 mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);

		mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, int_sta);
		if (mcu_cmd)
			mt7927_wr(dev, MT_MCU_CMD_REG, mcu_cmd);

		dev_info(&pdev->dev,
			 "清除遗留中断: INT_STA=0x%08x MCU_CMD=0x%08x\n",
			 int_sta, mcu_cmd);
	}

	/* 开启 WFDMA 中断 (所有 RX/TX/MCU) */
	mt7927_config_int_mask(dev, true);

	/* 开启 MCU→HOST 软件中断
	 * 来源: mt7925/pci.c line 555 */
	mt7927_rmw(dev, MT_MCU2HOST_SW_INT_ENA, 0,
		   MT_MCU_CMD_WAKE_RX_PCIE);

	dev_info(&pdev->dev,
		 "IRQ 初始化: INT_ENA=0x%08x PCIE_MAC_INT=0x%08x MCU_SW_INT_ENA=0x%08x\n",
		 mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA),
		 mt7927_rr(dev, MT_PCIE_MAC_INT_ENABLE),
		 mt7927_rr(dev, MT_MCU2HOST_SW_INT_ENA));

	/* 阶段 7e: 初始化数据 TX ring 0 */
	ret = mt7927_dma_init_data_rings(dev);
	if (ret) {
		dev_warn(&pdev->dev,
			 "TX ring 0 初始化失败: %d (继续)\n", ret);
		/* 非致命: 扫描仍可工作，只是数据路径不可用 */
	}

	/* 诊断B: IRQ/NAPI/TX0 都初始化后, 禁用中断再测 MCU
	 * 如果诊断A成功但诊断B失败 → 某个初始化步骤破坏了 MCU */
	dev_info(&pdev->dev, "诊断B: 全初始化后, 禁用中断重发 NIC_CAP\n");
	{
		int test_ret;
		u32 tx15_cidx_before, tx15_didx_before, rx6_didx_before;
		u32 tx15_cidx_after, tx15_didx_after, rx6_didx_after;

		/* 禁用中断, 模拟 PostFwDownloadInit 的环境 */
		mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA, 0);

		tx15_cidx_before = mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(15));
		tx15_didx_before = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(15));
		rx6_didx_before = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));

		test_ret = mt7927_mcu_send_unicmd(dev,
			UNI_CMD_ID_NIC_CAP, UNI_CMD_OPT_SET_ACK,
			NULL, 0);

		tx15_cidx_after = mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(15));
		tx15_didx_after = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(15));
		rx6_didx_after = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));

		dev_info(&pdev->dev,
			 "诊断B: NIC_CAP %s (ret=%d)\n"
			 "  TX15: CIDX %u→%u DIDX %u→%u  RX6: DIDX %u→%u  tail=%u\n",
			 test_ret ? "失败" : "成功", test_ret,
			 tx15_cidx_before, tx15_cidx_after,
			 tx15_didx_before, tx15_didx_after,
			 rx6_didx_before, rx6_didx_after,
			 dev->ring_rx6.tail);

		/* 恢复中断 */
		mt7927_config_int_mask(dev, true);
	}

	/* 阶段 7f: MAC 硬件初始化 (MDP + WTBL + Band + rate table)
	 * 参考: mt7925/init.c mt7925_mac_init()
	 * 包含: MDP DCR0/DCR1, WTBL ADM counter clear, TMAC/RMAC/MIB/DMA band init,
	 * 基本速率表 (idx 11-22). 缺失任何一步可能导致 TX 失败. */
	mt7927_mac_init(dev);

	/* 阶段 8: mac80211 注册 (让 wlan0 出现) */
	ret = mt7927_register_device(dev);
	if (ret) {
		dev_err(&pdev->dev, "mac80211 注册失败: %d\n", ret);
		goto err_irq;
	}

	dev_info(&pdev->dev,
		 "========================================\n");
	dev_info(&pdev->dev,
		 "MT7927 初始化完成 (fw_loaded=%d)\n", dev->fw_loaded);
	dev_info(&pdev->dev,
		 "========================================\n");

	return 0;

err_irq:
	free_irq(pci_irq_vector(pdev, 0), dev);
err_irq_vec:
	pci_free_irq_vectors(pdev);
err_napi:
	napi_disable(&dev->napi_rx_data);
	napi_disable(&dev->napi_rx_mcu);
	napi_disable(&dev->tx_napi);
	netif_napi_del(&dev->napi_rx_data);
	netif_napi_del(&dev->napi_rx_mcu);
	netif_napi_del(&dev->tx_napi);
	free_netdev(dev->napi_dev);
err_dma:
	mt7927_dma_cleanup(dev);
err_unmap:
	pci_iounmap(pdev, dev->bar0);
err_release:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free:
	kfree(dev);
	return ret;
}

static void mt7927_pci_remove(struct pci_dev *pdev)
{
	struct mt7927_dev *dev = pci_get_drvdata(pdev);

	dev_info(&pdev->dev, "MT7927 驱动卸载\n");

	/* 取消工作队列 */
	cancel_work_sync(&dev->mgmt_tx_work);
	skb_queue_purge(&dev->mgmt_tx_queue);
	cancel_delayed_work_sync(&dev->scan_work);

	mt7927_unregister_device(dev);
	mt7927_config_int_mask(dev, false);
	mt7927_wr(dev, MT_PCIE_MAC_INT_ENABLE, 0);

	/* 释放 IRQ */
	free_irq(pci_irq_vector(pdev, 0), dev);
	pci_free_irq_vectors(pdev);

	/* 停止 NAPI */
	napi_disable(&dev->napi_rx_data);
	napi_disable(&dev->napi_rx_mcu);
	napi_disable(&dev->tx_napi);
	netif_napi_del(&dev->napi_rx_data);
	netif_napi_del(&dev->napi_rx_mcu);
	netif_napi_del(&dev->tx_napi);
	tasklet_kill(&dev->irq_tasklet);

	if (dev->napi_dev)
		free_netdev(dev->napi_dev);

	/* 释放 TX ring 0 (数据) + TX ring 2 (管理帧 SF mode) */
	mt7927_ring_free(dev, &dev->ring_tx0);
	mt7927_ring_free(dev, &dev->ring_tx2);

	mt7927_dma_cleanup(dev);

	if (dev->bar0)
		pci_iounmap(pdev, dev->bar0);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	kfree(dev);
}

/* ===== PCI 设备 ID 表 ===== */
static const struct pci_device_id mt7927_pci_ids[] = {
	{ PCI_DEVICE(MT7927_PCI_VENDOR_ID, MT7927_PCI_DEVICE_ID) },
	{ PCI_DEVICE(MT7927_PCI_VENDOR_ID, MT7927_PCI_DEVICE_ID_6639) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, mt7927_pci_ids);

static struct pci_driver mt7927_pci_driver = {
	.name     = "mt7927",
	.id_table = mt7927_pci_ids,
	.probe    = mt7927_pci_probe,
	.remove   = mt7927_pci_remove,
};

module_pci_driver(mt7927_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MT7927 Linux Driver Project");
MODULE_DESCRIPTION("MT7927/MT6639 PCIe WiFi driver (Windows RE based)");
MODULE_FIRMWARE(MT7927_FW_PATCH);
MODULE_FIRMWARE(MT7927_FW_RAM);
