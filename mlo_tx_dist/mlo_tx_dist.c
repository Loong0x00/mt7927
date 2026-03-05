/*
 * mlo_tx_dist.c - MLO TX link distribution via kprobe
 *
 * Hooks mt7925_mac_write_txwi to distribute TX frames across MLO links.
 * Instead of always using the primary link WCID, alternates between
 * available links for data frames.
 *
 * Load: sudo insmod mlo_tx_dist.ko
 * Unload: sudo rmmod mlo_tx_dist
 */

#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/atomic.h>
#include <linux/netdevice.h>
#include <net/mac80211.h>

/* From mt792x.h - we need the structure layout */
struct mt76_wcid_proxy {
	/* mt76_wcid is the first member of mt792x_link_sta */
	/* We only need to access specific offsets */
	u8 pad[0]; /* placeholder - actual struct is opaque */
};

/*
 * mt7925_mac_write_txwi signature:
 * void mt7925_mac_write_txwi(struct mt76_dev *dev, __le32 *txwi,
 *     struct sk_buff *skb, struct mt76_wcid *wcid,
 *     struct ieee80211_key_conf *key, int pid,
 *     enum mt76_txq_id qid, u32 changed)
 *
 * We use a kprobe on this function. In the pre-handler we can modify
 * the registers:
 *   RDI = dev, RSI = txwi, RDX = skb, RCX = wcid,
 *   R8 = key, R9 = pid, stack[0] = qid, stack[1] = changed
 *
 * Strategy: read wcid->link_id to determine current link,
 * then look up alternative link WCID and swap RCX.
 */

static atomic_t tx_counter = ATOMIC_INIT(0);
static unsigned int distribute_ratio = 2; /* 1:N ratio, higher = more on primary */
module_param(distribute_ratio, uint, 0644);
MODULE_PARM_DESC(distribute_ratio, "TX distribution ratio (1 in N frames to secondary, default 2)");

static unsigned long total_primary = 0;
static unsigned long total_secondary = 0;
module_param(total_primary, ulong, 0444);
module_param(total_secondary, ulong, 0444);

/*
 * Unfortunately, kprobe pre_handler can modify registers but we need
 * access to mt792x internal structures (mt792x_link_sta, mt792x_sta)
 * which require the actual headers. A kprobe alone can't easily do the
 * WCID swap because we need to traverse:
 *   wcid -> container_of(mt792x_link_sta) -> sta -> link[other] -> wcid
 *
 * Alternative approach: use fprobe/ftrace to hook at function entry,
 * OR directly patch the function by modifying the module's code.
 *
 * Actually, the cleanest approach for runtime injection is to build
 * this module against the DKMS headers so we CAN access the structures.
 */

/*
 * We include the actual mt76 headers by building against the DKMS source.
 * See Makefile for include paths.
 */
#include "mt76.h"
#include "mt792x.h"

static unsigned long total_calls = 0;
static unsigned long bail_no_wcid = 0;
static unsigned long bail_changed = 0;
static unsigned long bail_no_sta = 0;
static unsigned long bail_no_mld = 0;
static unsigned long bail_single = 0;
module_param(total_calls, ulong, 0444);
module_param(bail_no_wcid, ulong, 0444);
module_param(bail_changed, ulong, 0444);
module_param(bail_no_sta, ulong, 0444);
module_param(bail_no_mld, ulong, 0444);
module_param(bail_single, ulong, 0444);

static int pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct mt76_wcid *wcid = (struct mt76_wcid *)regs->cx;
	struct sk_buff *skb = (struct sk_buff *)regs->dx;
	struct ieee80211_tx_info *info;
	struct ieee80211_vif *vif;
	struct mt792x_link_sta *mlink, *alt_link;
	struct mt792x_sta *msta;
	unsigned long valid;
	int cnt, target, i;

	total_calls++;

	if (!wcid || !skb) {
		bail_no_wcid++;
		return 0;
	}

	/* Must be a station WCID (not broadcast/management) */
	if (!wcid->sta) {
		bail_no_sta++;
		return 0;
	}

	/* Check if this is an MLD vif */
	info = IEEE80211_SKB_CB(skb);
	vif = info->control.vif;
	if (!vif || !ieee80211_vif_is_mld(vif)) {
		bail_no_mld++;
		return 0;
	}

	mlink = container_of(wcid, struct mt792x_link_sta, wcid);
	msta = mlink->sta;
	if (!msta)
		return 0;

	valid = msta->valid_links;
	cnt = hweight16(msta->valid_links);
	if (cnt <= 1) {
		bail_single++;
		/* Debug: check vif valid_links vs sta valid_links */
		if (bail_single <= 3)
			pr_info("mlo_tx_dist: bail_single sta_valid=0x%x vif_valid=0x%x vif_active=0x%x wcid_idx=%u link_id=%u\n",
				msta->valid_links, vif->valid_links,
				vif->active_links, wcid->idx, wcid->link_id);
		return 0;
	}

	/* Round-robin: every Nth frame goes to secondary */
	target = atomic_inc_return(&tx_counter) % distribute_ratio;
	if (target != 0) {
		total_primary++;
		return 0; /* keep primary link */
	}

	/* Find the OTHER link's WCID */
	rcu_read_lock();
	for_each_set_bit(i, &valid, IEEE80211_MLD_MAX_NUM_LINKS) {
		alt_link = rcu_dereference(msta->link[i]);
		if (alt_link && alt_link != mlink && alt_link->wcid.idx) {
			/* Swap WCID in RCX register */
			regs->cx = (unsigned long)&alt_link->wcid;
			total_secondary++;
			rcu_read_unlock();
			return 0;
		}
	}
	rcu_read_unlock();

	total_primary++;
	return 0;
}

static struct kprobe kp = {
	.symbol_name = "mt7925_mac_write_txwi",
	.pre_handler = pre_handler,
};

static int __init mlo_tx_dist_init(void)
{
	int ret;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("mlo_tx_dist: register_kprobe failed: %d\n", ret);
		return ret;
	}

	pr_info("mlo_tx_dist: hooked mt7925_mac_write_txwi at %px\n", kp.addr);
	pr_info("mlo_tx_dist: distribute_ratio=%u (1 in %u to secondary)\n",
		distribute_ratio, distribute_ratio);
	return 0;
}

static void __exit mlo_tx_dist_exit(void)
{
	unregister_kprobe(&kp);
	pr_info("mlo_tx_dist: unhooked. primary=%lu secondary=%lu\n",
		total_primary, total_secondary);
}

module_init(mlo_tx_dist_init);
module_exit(mlo_tx_dist_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MT7927 Project");
MODULE_DESCRIPTION("MLO TX link distribution via kprobe on mt7925");
