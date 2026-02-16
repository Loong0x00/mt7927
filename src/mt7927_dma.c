// SPDX-License-Identifier: GPL-2.0
/*
 * MT7927 DMA engine — interrupt handling, NAPI, TX/RX data path
 *
 * Implements:
 *   - IRQ handler + tasklet (bottom half)
 *   - NAPI poll functions for RX data (ring 4), RX MCU (ring 6), TX completion
 *   - TX enqueue, kick, completion
 *   - RX buffer refill
 *   - Data ring initialization (TX ring 0)
 *
 * Design notes:
 *   - RX buffers are coherent DMA memory (pre-allocated in mt7927_rx_ring_alloc),
 *     so "refill" is just resetting the descriptor — no skb allocation per packet
 *   - TX data uses dma_map_single() for per-packet skb mapping
 *   - Interrupt bit mapping derived from Windows ConfigIntMask (0x2600f000)
 *
 * Copyright (C) 2026
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/bitfield.h>
#include <net/mac80211.h>

#include "mt7927_pci.h"

/* ============================================================================
 * Register I/O helpers
 *
 * mt7927_rr() / mt7927_wr() are static inline in mt7927_pci.c and not
 * accessible from other compilation units. Define local equivalents that
 * access dev->bar0 directly. The bounds check is omitted here since all
 * callers use well-known register offsets.
 * ============================================================================ */

static inline u32 dma_rr(struct mt7927_dev *dev, u32 reg)
{
	return ioread32(dev->bar0 + reg);
}

static inline void dma_wr(struct mt7927_dev *dev, u32 reg, u32 val)
{
	iowrite32(val, dev->bar0 + reg);
}

static inline void dma_rmw(struct mt7927_dev *dev, u32 reg, u32 clr, u32 set)
{
	u32 val = dma_rr(dev, reg);

	val &= ~clr;
	val |= set;
	dma_wr(dev, reg, val);
}

/* ============================================================================
 * Interrupt bit definitions for MT6639/MT7927
 *
 * Derived from Windows ConfigIntMask value: 0x2600f000
 * The WFDMA HOST DMA0 INT_STA register (BAR0+0xd4200) bit layout for MT6639:
 *   BIT(12): RX ring 4 done (WiFi data frames)
 *   BIT(13): RX ring 5 done (unused by Windows)
 *   BIT(14): RX ring 6 done (MCU events)
 *   BIT(15): RX ring 7 done (auxiliary)
 *   BIT(25): TX ring 15 done (MCU WM command)
 *   BIT(29): MCU2HOST software interrupt
 *
 * Note: HOST_RX_DONE_INT_ENA(n)=BIT(n) in mt7927_pci.h is a generic
 * definition from mt76/mt7925. MT6639 may use a different layout where
 * RX ring 4-7 map to bits 12-15. The exact mapping will be verified
 * with hardware testing.
 * ============================================================================ */

#define MT_INT_RX_DONE_DATA	BIT(12)   /* RX ring 4 — WiFi data */
#define MT_INT_RX_DONE_MCU	BIT(14)   /* RX ring 6 — MCU events */
#define MT_INT_RX_DONE_AUX	BIT(15)   /* RX ring 7 — auxiliary */
#define MT_INT_RX_DONE_ALL	(MT_INT_RX_DONE_DATA | MT_INT_RX_DONE_MCU | \
				 MT_INT_RX_DONE_AUX)

/* TX done — reuse header definitions */
#define MT_INT_TX_DONE_DATA	HOST_TX_DONE_INT_ENA0   /* BIT(4) — TX ring 0 data */
#define MT_INT_TX_DONE_WM	HOST_TX_DONE_INT_ENA15  /* BIT(25) */
#define MT_INT_TX_DONE_FWDL	HOST_TX_DONE_INT_ENA16  /* BIT(26) */
#define MT_INT_TX_DONE_ALL	(MT_INT_TX_DONE_DATA | MT_INT_TX_DONE_WM | MT_INT_TX_DONE_FWDL)

/* ============================================================================
 * RX Processing Helpers
 * ============================================================================ */

/*
 * Process one RX descriptor from the given ring.
 *
 * RX buffers are pre-allocated coherent DMA memory. The flow is:
 *   1. Check DMA_DONE at ring->tail
 *   2. Extract length from descriptor ctrl field
 *   3. Allocate skb, copy data from coherent buffer
 *   4. Reset descriptor for DMA reuse
 *   5. Advance ring->tail
 *   6. Update HW CIDX register
 *
 * Returns the received skb, or NULL if no data available.
 */
static struct sk_buff *mt7927_rx_process_one(struct mt7927_dev *dev,
					     struct mt7927_ring *ring)
{
	struct mt76_desc *desc;
	struct sk_buff *skb = NULL;
	u32 ctrl, len;
	u16 idx;
	void *buf;

	desc = &ring->desc[ring->tail];
	ctrl = le32_to_cpu(READ_ONCE(desc->ctrl));

	/* Check if DMA has completed writing to this descriptor */
	if (!(ctrl & MT_DMA_CTL_DMA_DONE))
		return NULL;

	len = FIELD_GET(MT_DMA_CTL_SD_LEN0, ctrl);
	buf = (ring->buf) ? ring->buf[ring->tail] : NULL;

	if (len > 0 && len <= ring->buf_size && buf) {
		skb = dev_alloc_skb(len);
		if (skb) {
			skb_put(skb, len);
			memcpy(skb->data, buf, len);
		}
	}

	/* Reset descriptor: clear DMA_DONE, restore buffer size */
	desc->ctrl = cpu_to_le32(
		FIELD_PREP(MT_DMA_CTL_SD_LEN0, ring->buf_size));

	/* Advance tail — CIDX update is deferred to NAPI poll completion
	 * to batch-release processed slots back to DMA.
	 * Per-slot CIDX writes cause issues if done before NAPI completes. */
	idx = ring->tail;
	ring->tail = (ring->tail + 1) % ring->ndesc;

	return skb;
}

/*
 * mt7927_rx_refill — Reset RX descriptors so DMA can reuse them
 *
 * Since RX buffers are coherent DMA memory (not per-packet skbs),
 * "refilling" means resetting descriptors with DMA_DONE set but
 * that we may have missed processing (error recovery path).
 */
void mt7927_rx_refill(struct mt7927_dev *dev, struct mt7927_ring *ring)
{
	u16 i;

	for (i = 0; i < ring->ndesc; i++) {
		struct mt76_desc *desc = &ring->desc[i];
		u32 ctrl = le32_to_cpu(desc->ctrl);

		if (!(ctrl & MT_DMA_CTL_DMA_DONE))
			continue;

		if (!ring->buf || !ring->buf[i])
			continue;

		/* Reset: clear DMA_DONE, restore buffer size */
		desc->ctrl = cpu_to_le32(
			FIELD_PREP(MT_DMA_CTL_SD_LEN0, ring->buf_size));
	}
}

/* ============================================================================
 * Interrupt Handling — Top Half
 * ============================================================================ */

/*
 * mt7927_irq_handler — Top-half IRQ handler
 *
 * Reads INT_STA to confirm this is our interrupt, disables all interrupts
 * to avoid storms, and schedules the tasklet for bottom-half processing.
 */
irqreturn_t mt7927_irq_handler(int irq, void *dev_instance)
{
	struct mt7927_dev *dev = dev_instance;
	u32 intr;

	/* Quick check: is this our interrupt? */
	intr = dma_rr(dev, MT_WFDMA_HOST_INT_STA);
	if (!intr)
		return IRQ_NONE;

	/* Disable all interrupts immediately to prevent storm */
	dma_wr(dev, MT_WFDMA_HOST_INT_ENA, 0);

	/* Debug: log interrupt bits (rate-limited via printk_ratelimited) */
	dev_info_ratelimited(&dev->pdev->dev,
			     "IRQ: intr=0x%08x\n", intr);

	/* Save interrupt status for tasklet */
	dev->int_mask = intr;

	/* Schedule bottom-half processing */
	tasklet_schedule(&dev->irq_tasklet);

	return IRQ_HANDLED;
}

/* ============================================================================
 * Interrupt Handling — Bottom Half (Tasklet)
 * ============================================================================ */

/*
 * mt7927_irq_tasklet — Bottom-half interrupt processing
 *
 * Reads the saved interrupt status from the top half and dispatches
 * to appropriate NAPI handlers. Re-enables interrupts when done.
 */
void mt7927_irq_tasklet(unsigned long data)
{
	struct mt7927_dev *dev = (struct mt7927_dev *)data;
	u32 intr = dev->int_mask;

	/* Clear all pending interrupts */
	dma_wr(dev, MT_WFDMA_HOST_INT_STA, intr);

	/* RX ring 4: WiFi data frames */
	if (intr & MT_INT_RX_DONE_DATA)
		napi_schedule(&dev->napi_rx_data);

	/* RX ring 6: MCU events / UniCmd responses */
	if (intr & MT_INT_RX_DONE_MCU)
		napi_schedule(&dev->napi_rx_mcu);

	/* TX completion */
	if (intr & MT_INT_TX_DONE_ALL)
		napi_schedule(&dev->tx_napi);

	/* MCU2HOST software interrupt — ACK and schedule NAPI
	 * 来源: mt792x_dma.c mt792x_irq_tasklet() lines 48-57
	 * 必须读取并写回 MT_MCU_CMD 寄存器来 ACK，
	 * 否则固件认为事件未被处理，不会发送后续事件
	 */
	if (intr & MCU2HOST_SW_INT_STA) {
		u32 intr_sw;

		intr_sw = dma_rr(dev, MT_MCU_CMD_REG);
		/* ACK: 写回清除 */
		dma_wr(dev, MT_MCU_CMD_REG, intr_sw);

		dev_info_ratelimited(&dev->pdev->dev,
				     "MCU_CMD: intr_sw=0x%08x\n", intr_sw);

		if (intr_sw & MT_MCU_CMD_WAKE_RX_PCIE) {
			/* 固件通知有 RX 数据 — 调度数据 NAPI */
			napi_schedule(&dev->napi_rx_data);
			/* 也调度 MCU NAPI，因为 MCU 事件也可能通过此路径到达 */
			napi_schedule(&dev->napi_rx_mcu);
		}

		wake_up(&dev->mcu_wait);
	}

	/* Re-enable interrupts */
	dma_wr(dev, MT_WFDMA_HOST_INT_ENA, MT_WFDMA_INT_MASK_WIN);
}

/* ============================================================================
 * NAPI Poll — RX Data (Ring 4)
 * ============================================================================ */

/*
 * mt7927_poll_rx_data — NAPI poll for WiFi data frames on RX ring 4
 *
 * Processes up to @budget descriptors from ring 4. For each completed
 * descriptor, copies data into a new skb and dispatches it via
 * mt7927_queue_rx_skb() for mac80211 processing.
 */
int mt7927_poll_rx_data(struct napi_struct *napi, int budget)
{
	struct mt7927_dev *dev = container_of(napi, struct mt7927_dev,
					      napi_rx_data);
	struct mt7927_ring *ring = &dev->ring_rx4;
	int done = 0;

	while (done < budget) {
		struct sk_buff *skb;

		skb = mt7927_rx_process_one(dev, ring);
		if (!skb)
			break;

		/* Dispatch to mac80211 RX processing (implemented in mac.c) */
		mt7927_queue_rx_skb(dev, MT_RXQ_MAIN, skb);
		done++;
	}

	/* Release processed slots back to DMA */
	if (done > 0)
		dma_wr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid), ring->tail);

	if (done < budget) {
		napi_complete_done(napi, done);
		/* Re-enable RX ring 4 interrupt */
		dma_rmw(dev, MT_WFDMA_HOST_INT_ENA, 0, MT_INT_RX_DONE_DATA);
	}

	return done;
}

/* ============================================================================
 * NAPI Poll — RX MCU Events (Ring 6)
 * ============================================================================ */

/*
 * mt7927_poll_rx_mcu — NAPI poll for MCU events on RX ring 6
 *
 * Processes MCU event responses (UniCmd replies, scan events, etc.).
 * Similar to poll_rx_data but dispatches to MT_RXQ_MCU handler.
 */
int mt7927_poll_rx_mcu(struct napi_struct *napi, int budget)
{
	struct mt7927_dev *dev = container_of(napi, struct mt7927_dev,
					      napi_rx_mcu);
	struct mt7927_ring *ring = &dev->ring_rx6;
	int done = 0;

	while (done < budget) {
		struct sk_buff *skb;

		skb = mt7927_rx_process_one(dev, ring);
		if (!skb)
			break;

		dev_info(&dev->pdev->dev,
			 "rx_mcu: got skb len=%u tail=%u\n",
			 skb->len, ring->tail);

		/* Dispatch to MCU event handler (implemented in mac.c) */
		mt7927_queue_rx_skb(dev, MT_RXQ_MCU, skb);
		done++;
	}

	/* Release processed slots back to DMA by updating CIDX.
	 * CIDX = ring->tail means "all slots up to tail are available for DMA".
	 * Without this, after 256 events the ring is exhausted and DMA stalls.
	 * 参考: mt76/dma.c mt76_dma_kick_queue() writes q->head to CIDX */
	if (done > 0)
		dma_wr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid), ring->tail);

	if (done < budget) {
		napi_complete_done(napi, done);
		/* Re-enable RX ring 6 interrupt */
		dma_rmw(dev, MT_WFDMA_HOST_INT_ENA, 0, MT_INT_RX_DONE_MCU);
	}

	return done;
}

/* ============================================================================
 * NAPI Poll — TX Completion
 * ============================================================================ */

/*
 * mt7927_poll_tx — NAPI poll for TX completion
 *
 * Processes completed TX descriptors, frees transmitted skbs.
 * Currently a minimal implementation for MCU TX rings (15/16).
 * Data TX ring 0 completion will be handled here once data path is active.
 */
int mt7927_poll_tx(struct napi_struct *napi, int budget)
{
	struct mt7927_dev *dev = container_of(napi, struct mt7927_dev,
					      tx_napi);

	/* Process data TX ring completions if data ring is active */
	if (dev->ring_tx0.desc)
		mt7927_tx_complete(dev, &dev->ring_tx0);

	napi_complete(napi);

	/* Re-enable TX done interrupts */
	dma_rmw(dev, MT_WFDMA_HOST_INT_ENA, 0, MT_INT_TX_DONE_ALL);

	return 0;
}

/* ============================================================================
 * TX Data Path
 * ============================================================================ */

/*
 * mt7927_tx_queue_skb — Enqueue a data skb into a TX ring (CT mode)
 *
 * CT mode: the DMA descriptor points to TXD+TXP in coherent memory.
 * The actual frame payload is pointed to by TXP scatter-gather entries.
 * Token-based: skb cleanup happens in TXFREE event, not TX completion.
 *
 * The caller must call mt7927_tx_prepare_skb() first to build TXD+TXP
 * and obtain the token and txwi_dma address.
 *
 * Returns 0 on success, negative errno on failure.
 */
int mt7927_tx_queue_skb(struct mt7927_dev *dev, struct mt7927_ring *ring,
			struct sk_buff *skb, struct mt7927_wcid *wcid)
{
	struct mt76_desc *desc;
	dma_addr_t txwi_dma;
	int token;
	u32 ctrl;
	u16 idx;
	int ret;

	/* Check for ring full: head has caught up with tail */
	if (((ring->head + 1) % ring->ndesc) == ring->tail) {
		dev_dbg(&dev->pdev->dev,
			"TX ring %d full (head=%u tail=%u)\n",
			ring->qid, ring->head, ring->tail);
		return -ENOSPC;
	}

	/* Prepare TXD+TXP in coherent pool and DMA map payload */
	ret = mt7927_tx_prepare_skb(dev, skb, wcid, &token, &txwi_dma);
	if (ret)
		return ret;

	idx = ring->head;
	desc = &ring->desc[idx];

	/* Fill TX descriptor: buf0 = TXD+TXP coherent DMA address */
	desc->buf0 = cpu_to_le32(lower_32_bits(txwi_dma));
	desc->buf1 = cpu_to_le32(0);
	desc->info = cpu_to_le32(0);

	/* ctrl: length of TXD+TXP (64 bytes) + LAST_SEC0
	 * DMA_DONE=0 means "pending for DMA" */
	ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, MT7927_TXWI_SIZE) |
	       MT_DMA_CTL_LAST_SEC0;
	desc->ctrl = cpu_to_le32(ctrl);

	/* Store token (as void*) for ring slot tracking.
	 * txwi_dma stored for diagnostic purposes. */
	ring->buf[idx] = (void *)(unsigned long)token;
	ring->buf_dma[idx] = txwi_dma;

	/* Advance producer index */
	ring->head = (ring->head + 1) % ring->ndesc;

	return 0;
}

/*
 * mt7927_tx_kick — Submit queued TX descriptors to hardware
 *
 * After one or more descriptors have been enqueued via mt7927_tx_queue_skb(),
 * this function writes the updated CPU index (CIDX) to the hardware register
 * to trigger DMA transmission.
 */
void mt7927_tx_kick(struct mt7927_dev *dev, struct mt7927_ring *ring)
{
	/* Ensure all descriptor writes are visible before poking HW */
	wmb();
	dma_wr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid), ring->head);

	/* Diagnostic: read back ring state after kick */
	{
		u32 base = dma_rr(dev, MT_WPDMA_TX_RING_BASE(ring->qid));
		u32 cnt  = dma_rr(dev, MT_WPDMA_TX_RING_CNT(ring->qid));
		u32 cidx = dma_rr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid));
		u32 didx = dma_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid));

		dev_info(&dev->pdev->dev,
			 "TX%d kick: BASE=0x%08x CNT=%u CIDX=%u DIDX=%u\n",
			 ring->qid, base, cnt, cidx, didx);
	}
}

/*
 * mt7927_tx_complete — Process completed TX descriptors (CT mode)
 *
 * In CT mode, the actual skb cleanup (DMA unmap + kfree) is done
 * by the TXFREE event handler (mt7927_mac_tx_free). This function
 * only advances the ring tail to free descriptor slots.
 *
 * Scans from ring->tail up to the hardware's DMA index (DIDX).
 */
void mt7927_tx_complete(struct mt7927_dev *dev, struct mt7927_ring *ring)
{
	u32 dma_idx;

	if (!ring->desc || !ring->buf)
		return;

	/* Read hardware DMA index — DMA has consumed up to this point */
	dma_idx = dma_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid));

	while (ring->tail != dma_idx) {
		struct mt76_desc *desc = &ring->desc[ring->tail];

		/* Clear the ring slot (skb freed by TXFREE event) */
		ring->buf[ring->tail] = NULL;
		ring->buf_dma[ring->tail] = 0;

		/* Reset descriptor */
		desc->buf0 = 0;
		desc->ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);
		desc->buf1 = 0;
		desc->info = 0;

		ring->tail = (ring->tail + 1) % ring->ndesc;
	}
}

/* ============================================================================
 * Data Ring Initialization
 * ============================================================================ */

/*
 * mt7927_dma_init_data_rings — Initialize TX ring 0 for data transmission
 *
 * TX ring 0 is used for WiFi data frames (802.11 or 802.3 after header
 * translation). This function allocates the descriptor ring and the
 * buf/buf_dma tracking arrays needed for TX completion.
 *
 * Note: RX rings 4, 6, 7 are already allocated in mt7927_init_tx_rx_rings()
 * during probe. This function only adds the data TX ring.
 *
 * Since mt7927_tx_ring_alloc() is static in pci.c, the allocation logic
 * is inlined here.
 */
int mt7927_dma_init_data_rings(struct mt7927_dev *dev)
{
	struct mt7927_ring *ring = &dev->ring_tx0;
	u16 ndesc = MT_TXQ_DATA_RING_SIZE;  /* 256 */
	u16 qid = MT_TXQ_DATA_RING;         /* 0 */
	u16 i;

	ring->qid = qid;
	ring->ndesc = ndesc;
	ring->head = 0;
	ring->tail = 0;
	ring->buf_size = 0;

	/* Allocate TXD+TXP coherent DMA buffer pool (CT mode)
	 * Each token gets MT7927_TXWI_SIZE (64) bytes for TXD+TXP */
	dev->txwi_buf = dma_alloc_coherent(&dev->pdev->dev,
					   MT7927_TOKEN_SIZE * MT7927_TXWI_SIZE,
					   &dev->txwi_dma, GFP_KERNEL);
	if (!dev->txwi_buf)
		return -ENOMEM;
	memset(dev->txwi_buf, 0, MT7927_TOKEN_SIZE * MT7927_TXWI_SIZE);
	spin_lock_init(&dev->tx_token.lock);
	dev->tx_token.next_id = 0;

	dev_info(&dev->pdev->dev,
		 "TXWI pool: %u tokens × %lu bytes = %lu bytes, dma=0x%pad\n",
		 MT7927_TOKEN_SIZE, (unsigned long)MT7927_TXWI_SIZE,
		 (unsigned long)(MT7927_TOKEN_SIZE * MT7927_TXWI_SIZE),
		 &dev->txwi_dma);

	/* 诊断: 分配 coherent payload buffer (替代 dma_map_single 测试) */
	dev->tx_payload_buf = dma_alloc_coherent(&dev->pdev->dev, 2048,
						 &dev->tx_payload_dma,
						 GFP_KERNEL);
	if (!dev->tx_payload_buf) {
		dma_free_coherent(&dev->pdev->dev,
				  MT7927_TOKEN_SIZE * MT7927_TXWI_SIZE,
				  dev->txwi_buf, dev->txwi_dma);
		return -ENOMEM;
	}
	dev_info(&dev->pdev->dev,
		 "TX payload coherent buf: dma=0x%pad\n",
		 &dev->tx_payload_dma);

	/* Allocate DMA-coherent descriptor ring */
	ring->desc = dma_alloc_coherent(&dev->pdev->dev,
					ndesc * sizeof(struct mt76_desc),
					&ring->desc_dma, GFP_KERNEL);
	if (!ring->desc)
		goto err_txwi;

	/* Allocate tracking arrays for TX token and DMA addresses */
	ring->buf = kcalloc(ndesc, sizeof(*ring->buf), GFP_KERNEL);
	ring->buf_dma = kcalloc(ndesc, sizeof(*ring->buf_dma), GFP_KERNEL);
	if (!ring->buf || !ring->buf_dma)
		goto err;

	/* Initialize TX descriptors: DMA_DONE=1 (idle/available) */
	for (i = 0; i < ndesc; i++)
		ring->desc[i].ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);

	/* Program WFDMA hardware registers: BASE → CNT → CIDX → DIDX */
	dma_wr(dev, MT_WPDMA_TX_RING_BASE(qid),
	       lower_32_bits(ring->desc_dma));
	dma_wr(dev, MT_WPDMA_TX_RING_CNT(qid), ndesc);
	dma_wr(dev, MT_WPDMA_TX_RING_CIDX(qid), 0);
	dma_wr(dev, MT_WPDMA_TX_RING_DIDX(qid), 0);

	dev_info(&dev->pdev->dev,
		 "TX ring %d: ndesc=%u desc_dma=0x%pad\n",
		 qid, ndesc, &ring->desc_dma);

	return 0;

err:
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
err_txwi:
	if (dev->txwi_buf) {
		dma_free_coherent(&dev->pdev->dev,
				  MT7927_TOKEN_SIZE * MT7927_TXWI_SIZE,
				  dev->txwi_buf, dev->txwi_dma);
		dev->txwi_buf = NULL;
	}
	return -ENOMEM;
}
