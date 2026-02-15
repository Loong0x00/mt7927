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
static inline u32 mt7927_rr(struct mt7927_dev *dev, u32 reg)
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
static inline void mt7927_wr(struct mt7927_dev *dev, u32 reg, u32 val)
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

/* L1 remap 读取 — 用于 0x18xxxxxx 芯片地址
 * 来源: 已验证代码 mt7927_init_dma.c 行 1372-1408 */
static u32 mt7927_rr_l1(struct mt7927_dev *dev, u32 chip_addr)
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
			dma_free_coherent(&dev->pdev->dev, ring->buf_size,
					  ring->buf[i], ring->buf_dma[i]);
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

/* 在 RX ring 6 (MCU 事件) 上轮询响应 */
static int mt7927_mcu_wait_resp(struct mt7927_dev *dev, int timeout_ms)
{
	struct mt7927_ring *ring = &dev->ring_rx6;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	struct mt76_desc *d;
	u32 ctrl;

	do {
		d = &ring->desc[ring->tail];
		ctrl = le32_to_cpu(d->ctrl);

		if (ctrl & MT_DMA_CTL_DMA_DONE) {
			u16 idx = ring->tail;
			u32 *evt = ring->buf[idx];
			u32 sdl0 = FIELD_GET(MT_DMA_CTL_SD_LEN0, ctrl);

			dev_info(&dev->pdev->dev,
				 "mcu-evt: q%u idx=%u ctrl=0x%08x sdl0=%u w0=0x%08x w1=0x%08x w2=0x%08x\n",
				 ring->qid, idx, ctrl, sdl0,
				 evt[0], evt[1], evt[2]);

			/* Hex dump 前 64 字节用于分析 UniCmd 响应 */
			if (sdl0 > 12) {
				int dump_words = (sdl0 + 3) / 4;

				if (dump_words > 16)
					dump_words = 16;
				dev_info(&dev->pdev->dev,
					 "  evt[3..7]: %08x %08x %08x %08x %08x\n",
					 evt[3], evt[4], evt[5], evt[6], evt[7]);
				if (dump_words > 8)
					dev_info(&dev->pdev->dev,
						 "  evt[8..12]: %08x %08x %08x %08x %08x\n",
						 evt[8], evt[9], evt[10], evt[11], evt[12]);
			}

			/* 清除 DMA_DONE, 重置描述符 */
			d->ctrl = cpu_to_le32(
				FIELD_PREP(MT_DMA_CTL_SD_LEN0,
					   ring->buf_size));

			/* 推进 tail */
			ring->tail = (ring->tail + 1) % ring->ndesc;

			/* 归还已处理的 slot 给 DMA — 写 CIDX = tail
			 * tail 指向下一个待处理位置，告诉 DMA 之前的 slot 可重用
			 * 参考: mt76/dma.c mt76_dma_kick_queue() 写 q->head
			 * 不更新 CIDX 会导致 256 事件后 ring 耗尽 */
			mt7927_wr(dev,
				  MT_WPDMA_RX_RING_CIDX(ring->qid),
				  ring->tail);

			/* 清除 INT_STA 中对应 RX ring 的中断状态位
			 * WFDMA 使用 W1C (Write-1-to-Clear) 语义
			 * 参考: mt76/mt792x_dma.c mt792x_irq_tasklet()
			 * 如果不清除，WFDMA 可能不再触发后续 DMA 写入 */
			{
				u32 int_sta = mt7927_rr(dev,
							MT_WFDMA_HOST_INT_STA);
				if (int_sta)
					mt7927_wr(dev, MT_WFDMA_HOST_INT_STA,
						  int_sta);
			}

			return 0;
		}
		usleep_range(1000, 2000);
	} while (time_before(jiffies, timeout));

	dev_warn(&dev->pdev->dev,
		 "mcu-evt 超时: q%u tail=%u cidx=0x%x didx=0x%x\n",
		 ring->qid, ring->tail,
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

	dev_info(&dev->pdev->dev,
		 "mcu-cmd: cid=0x%02x len=%zu plen=%zu seq=%u\n",
		 cid, len, plen, txd->seq);

	/* 通过 TX ring 15 发送 */
	ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
	if (!ret)
		ret = mt7927_mcu_wait_resp(dev, 200);

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

	/* 通过 TX ring 15 发送 */
	ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
	if (!ret && (option & UNI_CMD_OPT_ACK)) {
		/* 暂时禁用 RX6 中断, 防止 NAPI 和轮询同时消费 RX ring 6
		 * 不禁用会导致 race: NAPI 消费事件后推进 tail,
		 * 轮询看到 tail 位置没有 DMA_DONE 就超时 */
		mt7927_wr(dev, MT_WFDMA_INT_ENA_CLR, BIT(14));  /* RX6 int */
		ret = mt7927_mcu_wait_resp(dev, 200);
		mt7927_wr(dev, MT_WFDMA_INT_ENA_SET, BIT(14));  /* restore */
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

		/* 诊断: 读回 EXT_CTRL 验证配置 */
		dev_info(&dev->pdev->dev,
			 "EXT_CTRL: RX4=0x%08x RX6=0x%08x RX7=0x%08x TX15=0x%08x TX16=0x%08x\n",
			 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4)),
			 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6)),
			 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7)),
			 mt7927_rr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15)),
			 mt7927_rr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16)));

		/* GLO_CFG: 启用 TX_DMA_EN | RX_DMA_EN + Windows 关键位 (步骤 4)
		 * 来源: Windows — glo_cfg |= 0x5 + 额外关键位
		 * 注意: MT6639 用 0x5, MT7925 用 0x4000005 */
		val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
		val |= MT_WFDMA_GLO_CFG_TX_DMA_EN |
		       MT_WFDMA_GLO_CFG_RX_DMA_EN;
		/* FWDL 阶段需要 bypass DMASHDL */
		val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
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

/* Forward declaration — 定义在 post_fw_init 之后的 section 7h */
static int mt7927_mcu_set_eeprom(struct mt7927_dev *dev);

/* ------- 7g. PostFwDownloadInit -------
 * 来源: Windows AsicConnac3xPostFwDownloadInit (FUN_1401c9510)
 * register_playbook.md 行 240-266 */
static int mt7927_post_fw_init(struct mt7927_dev *dev)
{
	u32 val;
	int ret;

	dev_info(&dev->pdev->dev, "===== PostFwDownloadInit 开始 =====\n");

	/* Step 1: DMASHDL 启用 — MCU 命令前唯一寄存器写入
	 * 来源: Windows — BAR0+0xd6060 |= 0x10101
	 * 确定性: 100% */
	val = mt7927_rr(dev, MT_DMASHDL_ENABLE);
	val |= MT_DMASHDL_ENABLE_VAL;
	mt7927_wr(dev, MT_DMASHDL_ENABLE, val);
	dev_info(&dev->pdev->dev,
		 "DMASHDL 启用: 0x%08x\n",
		 mt7927_rr(dev, MT_DMASHDL_ENABLE));

	/* 重新启用 WpdmaConfig (固件启动后) */
	mt7927_wpdma_config(dev, true);

	/* 关闭 FWDL bypass (固件已启动, 使用正常 DMASHDL 路由)
	 * 注意: 必须在 wpdma_config 之后, 因为 wpdma_config 会设置 BIT(9) */
	val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
	val &= ~MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
	mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

	dev_info(&dev->pdev->dev,
		 "PostFwInit: GLO_CFG=0x%08x (bypass cleared)\n",
		 mt7927_rr(dev, MT_WPDMA_GLO_CFG));

	/* 重新设置中断掩码 */
	mt7927_config_int_mask(dev, true);

	/* Step 2: NIC_CAPABILITY — UniCmd 格式!
	 * 来源: Windows RE ghidra_post_fw_init.md
	 *   class=0x8a (NIC capability), target=0xed (routing info, 不在 TXD 中)
	 *   payload=NULL, len=0 — 纯查询, 无 payload!
	 * ⚠️ 之前的 cid=0x0E (CHIP_CONFIG) 是错误的! Windows 用 class=0x8a */
	dev_info(&dev->pdev->dev, "发送 NIC_CAPABILITY (UniCmd class=0x8a)\n");
	{
		/* NIC_CAP 无 payload — Windows RE 确认:
		 * mcu_dispatch(ctx, class=0x8a, target=0xed, payload=NULL, len=0)
		 * option=0x07: BIT(1)=UNI + BIT(0)=ACK + BIT(2)=need_response */
		ret = mt7927_mcu_send_unicmd(dev, UNI_CMD_ID_NIC_CAP,
					      UNI_CMD_OPT_SET_ACK,
					      NULL, 0);
		if (ret && ret != -ETIMEDOUT)
			dev_warn(&dev->pdev->dev,
				 "NIC_CAP 失败: %d (继续)\n", ret);
	}

	/* Step 3: Config 命令 (class=0x02) — 已跳过!
	 * 来源: Windows RE — Data: {1, 0, 0x70000}
	 * 诊断发现: 此命令 (CID=0x02=BSS_INFO_UPDATE) 发送后 MCU 不再响应
	 * 可能原因: payload 格式不匹配 BSS_INFO_UPDATE 的 TLV 格式
	 * 导致固件 MCU 事件通道中断 */
	dev_info(&dev->pdev->dev, "跳过 Config (class=0x02) — 诊断发现它破坏 MCU 通道\n");

	/* Step 4: Config 命令 (class=0xc0) — 暂时保留, 验证是否也有问题
	 * 来源: Windows RE — Data: {0x820cc800, 0x3c200} */
	dev_info(&dev->pdev->dev, "发送 Config (class=0xc0)\n");
	{
		__le32 cfg_data[2] = {
			cpu_to_le32(0x820cc800),
			cpu_to_le32(0x3c200),
		};

		ret = mt7927_mcu_send_unicmd(dev, 0x00c0,
					      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
					      cfg_data, sizeof(cfg_data));
		if (ret)
			dev_warn(&dev->pdev->dev,
				 "Config 0xc0 失败: %d (继续)\n", ret);
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
		/* mt7925 参考: "KeepFullPwr 0" = 禁用深度睡眠 (初始化默认)
		 * mt7925_mcu_set_deep_sleep(dev, false) → "KeepFullPwr 1" */
		const char *cfg_str = "KeepFullPwr 0";
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
	/* Step 10: EFUSE_CTRL / EEPROM 模式设置
	 * 参考: mt76/mt7925/init.c line 110 — mt7925_mcu_set_eeprom(dev)
	 * 在 PostFwDownloadInit 之后、mac80211 注册之前 */
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
 * 参考: mt76/mt7925/mcu.c mt7925_mcu_set_channel_domain()
 * 在 mac80211 .start() 回调中发送
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
	hdr.alpha2[0] = '0';
	hdr.alpha2[1] = '0';  /* world domain */
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
 * 参考: mt76/mt7925/mcu.c mt7925_mcu_set_rts_thresh()
 * 在 mac80211 .start() 回调中发送
 */
static int mt7927_mcu_set_rts_thresh(struct mt7927_dev *dev)
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
		.len_thresh = cpu_to_le32(0x92b),  /* 2347 */
		.pkt_thresh = cpu_to_le32(0x02),
	};
	int ret;

	dev_info(&dev->pdev->dev,
		 "BAND_CONFIG RTS_THRESH: thresh=0x%x pkt=0x%x\n",
		 0x92b, 0x02);

	ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_BAND_CONFIG,
				      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
				      &req, sizeof(req));
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "BAND_CONFIG RTS 失败: %d\n", ret);

	return ret;
}

/* EFUSE_CTRL / EEPROM (CID=0x2d, tag=0x02)
 * 参考: mt76/mt7925/mcu.c mt7925_mcu_set_eeprom()
 * 在 post_fw_init 末尾发送
 */
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

	dev_info(&dev->pdev->dev,
		 "EFUSE_CTRL: mode=%d format=%d\n",
		 req.buffer_mode, req.format);

	/* mt7925 使用 send_and_get (wait_resp=true) 等待响应,
	 * 但我们的 MCU 事件接收路径在 PostFwDownloadInit 之后不可靠,
	 * 先用 fire-and-forget (0x06) 避免超时阻塞初始化
	 * TODO: 修复 MCU 事件接收后改回 0x07 */
	ret = mt7927_mcu_send_unicmd(dev, MT_MCU_CLASS_EFUSE_CTRL,
				      UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET,
				      &req, sizeof(req));
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "EFUSE_CTRL 失败: %d\n", ret);

	return ret;
}

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
		 "DMASHDL=0x%08x fw_sync=0x%08x ROMCODE=0x%08x\n",
		 mt7927_rr(dev, MT_DMASHDL_ENABLE),
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
	struct {
		u8 rsv[4];
		struct dev_info_active_tlv tlv;
	} __packed req = {};

	req.tlv.tag = cpu_to_le16(UNI_DEV_INFO_ACTIVE);
	req.tlv.len = cpu_to_le16(sizeof(req.tlv));
	req.tlv.active = enable;
	req.tlv.band_idx = mvif->band_idx;
	req.tlv.omac_idx = mvif->omac_idx;
	memcpy(req.tlv.omac_addr, vif->addr, ETH_ALEN);

	dev_info(&dev->pdev->dev,
		 "mcu: DEV_INFO active=%d omac=%d addr=%pM\n",
		 enable, mvif->omac_idx, vif->addr);

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_DEV_INFO, &req,
					   sizeof(req));
}

/* BSS_INFO_UPDATE (CID=0x02) — 配置 BSS */
static int mt7927_mcu_add_bss_info(struct mt7927_dev *dev,
				   struct ieee80211_vif *vif, bool enable)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct {
		struct bss_req_hdr hdr;
		struct mt76_connac_bss_basic_tlv basic;
	} __packed req = {};

	req.hdr.bss_idx = mvif->bss_idx;
	req.basic.tag = cpu_to_le16(UNI_BSS_INFO_BASIC);
	req.basic.len = cpu_to_le16(sizeof(req.basic));
	req.basic.active = enable;
	req.basic.omac_idx = mvif->omac_idx;
	req.basic.hw_bss_idx = mvif->bss_idx;
	req.basic.band_idx = mvif->band_idx;
	/* STA 模式: conn_type = CONNECTION_INFRA_STA (0x10001)
	 * 来源: mt76_connac_mcu.c line 1206 */
	req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_STA);
	req.basic.conn_state = enable ? 1 : 0;
	req.basic.wmm_idx = mvif->wmm_idx;
	req.basic.bmc_tx_wlan_idx = cpu_to_le16(mvif->sta.wcid.idx);
	req.basic.sta_idx = cpu_to_le16(mvif->sta.wcid.idx);

	if (vif->cfg.assoc && vif->bss_conf.bssid) {
		memcpy(req.basic.bssid, vif->bss_conf.bssid, ETH_ALEN);
		req.basic.bcn_interval =
			cpu_to_le16(vif->bss_conf.beacon_int);
		req.basic.dtim_period = vif->bss_conf.dtim_period;
	}

	dev_info(&dev->pdev->dev,
		 "mcu: BSS_INFO bss=%d active=%d bssid=%pM\n",
		 mvif->bss_idx, enable, req.basic.bssid);

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_BSS_INFO, &req,
					   sizeof(req));
}

/* STA_REC_UPDATE (CID=0x03) — 添加/更新 STA 记录 */
static int mt7927_mcu_sta_update(struct mt7927_dev *dev,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 bool enable, u8 conn_state)
{
	struct mt7927_vif *mvif = (struct mt7927_vif *)vif->drv_priv;
	struct mt7927_sta *msta;
	struct {
		struct sta_req_hdr hdr;
		struct sta_rec_basic basic;
		struct sta_rec_hdr_trans hdr_trans;
	} __packed req = {};

	if (sta)
		msta = (struct mt7927_sta *)sta->drv_priv;
	else
		msta = &mvif->sta;

	/* STA 请求头 */
	req.hdr.bss_idx = mvif->bss_idx;
	req.hdr.wlan_idx_lo = msta->wcid.idx & 0xff;
	req.hdr.wlan_idx_hi = (msta->wcid.idx >> 8) & 0xff;
	req.hdr.tlv_num = cpu_to_le16(2);
	req.hdr.is_tlv_append = 1;

	/* STA_REC_BASIC (tag=0) */
	req.basic.tag = cpu_to_le16(STA_REC_BASIC);
	req.basic.len = cpu_to_le16(sizeof(req.basic));
	req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_STA);
	req.basic.conn_state = conn_state;
	req.basic.extra_info = cpu_to_le16(EXTRA_INFO_VER | EXTRA_INFO_NEW);
	if (sta) {
		req.basic.qos = sta->wme;
		req.basic.aid = cpu_to_le16(sta->aid);
		memcpy(req.basic.peer_addr, sta->addr, ETH_ALEN);
	}

	/* STA_REC_HDR_TRANS (tag=0x2B) */
	req.hdr_trans.tag = cpu_to_le16(STA_REC_HDR_TRANS);
	req.hdr_trans.len = cpu_to_le16(sizeof(req.hdr_trans));
	req.hdr_trans.from_ds = 1;
	req.hdr_trans.to_ds = 1;

	dev_info(&dev->pdev->dev,
		 "mcu: STA_REC wcid=%d state=%d enable=%d\n",
		 msta->wcid.idx, conn_state, enable);

	return mt7927_mcu_send_unicmd_set(dev, MCU_UNI_CMD_STA_REC, &req,
					   sizeof(req));
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

/* --- TX: 通过 TX ring 0 发送数据帧 --- */
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

	/* 构建 TXD 并 prepend 到 skb */
	ret = mt7927_tx_prepare_skb(dev, skb, wcid);
	if (ret) {
		dev_kfree_skb(skb);
		return;
	}

	/* 入队到 TX ring 0 */
	if (!dev->ring_tx0.desc) {
		dev_kfree_skb(skb);
		return;
	}

	ret = mt7927_tx_queue_skb(dev, &dev->ring_tx0, skb);
	if (ret) {
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

	/* 1. SET_DOMAIN_INFO — 信道域信息 (最关键!)
	 * 固件不知道合法信道列表就不会扫描 */
	ret = mt7927_mcu_set_channel_domain(dev);
	if (ret)
		dev_warn(&dev->pdev->dev,
			 "mac80211 start: set_channel_domain 失败: %d (继续)\n",
			 ret);

	/* 2. BAND_CONFIG / RTS_THRESHOLD
	 * 参考: mt7925/main.c line 325 */
	ret = mt7927_mcu_set_rts_thresh(dev);
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
	mvif->band_idx = 0;
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

	if (changed & BSS_CHANGED_ASSOC) {
		mt7927_mcu_add_bss_info(dev, vif, vif->cfg.assoc);
		dev_info(&dev->pdev->dev,
			 "mac80211: bss_info assoc=%d\n", vif->cfg.assoc);
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

	return mt7927_mcu_hw_scan(dev, vif, req);
}

static void mt7927_cancel_hw_scan(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif)
{
	struct mt7927_dev *dev = mt7927_hw_dev(hw);

	mt7927_mcu_cancel_hw_scan(dev, vif);
	cancel_delayed_work_sync(&dev->scan_work);
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

	dev_info(&dev->pdev->dev,
		 "mac80211: sta_state %d->%d addr=%pM\n",
		 old_state, new_state, sta->addr);

	/* NOTEXIST → NONE: 分配 WCID */
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
				      CONN_STATE_PORT_SECURE);
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
	/* 注: HAS_RATE_CONTROL 暂不设置 — 让 mac80211 用 minstrel_ht,
	 * 等数据路径完善后再切换到固件 rate control */
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
	init_waitqueue_head(&dev->mcu_wait);
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

	/* 释放 TX ring 0 (数据) */
	mt7927_ring_free(dev, &dev->ring_tx0);

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
