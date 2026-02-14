/*
 * MT7927 (7927/6639) experimental PCIe bring-up with connac2-style
 * MCU firmware download flow adapted from mt76/mt7925.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/bitfield.h>
#include <linux/byteorder/generic.h>

#define MT7927_VENDOR_ID 0x14c3
#define MT7927_DEVICE_ID 0x7927
#define MT7927_DEVICE_ID_6639 0x6639

/* WFDMA/WPDMA */
#define MT_WFDMA0_BASE              0xd4000
#define MT_WFDMA0(_ofs)             (MT_WFDMA0_BASE + (_ofs))
#define MT_WFDMA_EXT_CSR_BASE       0xd7000
#define MT_WFDMA_EXT_CSR(_ofs)      (MT_WFDMA_EXT_CSR_BASE + (_ofs))

#define MT_WPDMA_GLO_CFG            MT_WFDMA0(0x0208)
#define MT_WPDMA_RST_DTX_PTR        MT_WFDMA0(0x020c)
#define MT_WPDMA_RST_DRX_PTR        MT_WFDMA0(0x0280)
#define MT_WPDMA_TX_RING_BASE(_n)   MT_WFDMA0(0x0300 + ((_n) << 4))
#define MT_WPDMA_TX_RING_CNT(_n)    MT_WFDMA0(0x0304 + ((_n) << 4))
#define MT_WPDMA_TX_RING_CIDX(_n)   MT_WFDMA0(0x0308 + ((_n) << 4))
#define MT_WPDMA_TX_RING_DIDX(_n)   MT_WFDMA0(0x030c + ((_n) << 4))
#define MT_WPDMA_RX_RING_BASE(_n)   MT_WFDMA0(0x0500 + ((_n) << 4))
#define MT_WPDMA_RX_RING_CNT(_n)    MT_WFDMA0(0x0504 + ((_n) << 4))
#define MT_WPDMA_RX_RING_CIDX(_n)   MT_WFDMA0(0x0508 + ((_n) << 4))
#define MT_WPDMA_RX_RING_DIDX(_n)   MT_WFDMA0(0x050c + ((_n) << 4))
#define MT_WPDMA_GLO_CFG_EXT0       MT_WFDMA0(0x02b0)
#define MT_WPDMA_GLO_CFG_EXT1       MT_WFDMA0(0x02b4)
#define MT_WFDMA_TX_RING_EXT_CTRL(_n)  MT_WFDMA0(0x0600 + ((_n) << 2))
#define MT_WFDMA_TX_RING15_EXT_CTRL MT_WFDMA_TX_RING_EXT_CTRL(15)
#define MT_WFDMA_TX_RING16_EXT_CTRL MT_WFDMA_TX_RING_EXT_CTRL(16)
#define MT_WFDMA_TX_RING17_EXT_CTRL MT_WFDMA_TX_RING_EXT_CTRL(17)
#define MT_WFDMA_RX_RING_EXT_CTRL(_n) MT_WFDMA0(0x0680 + ((_n) << 2))
#define MT_WFDMA_RX_RING0_EXT_CTRL  MT_WFDMA_RX_RING_EXT_CTRL(0)
#define MT_WFDMA_INT_RX_PRI         MT_WFDMA0(0x0298)
#define MT_WFDMA_INT_TX_PRI         MT_WFDMA0(0x029c)
#define MT_WFDMA_PRI_DLY_INT_CFG0   MT_WFDMA0(0x02f0)

#define MT_WFDMA_HOST_INT_STA       MT_WFDMA0(0x0200)
#define MT_WFDMA_HOST_INT_ENA       MT_WFDMA0(0x0204)
#define MT_MCU_CMD_REG              MT_WFDMA0(0x01f0)
#define MT_MCU2HOST_SW_INT_ENA      MT_WFDMA0(0x01f4)
#define MT_WFDMA_HOST_CONFIG        MT_WFDMA_EXT_CSR(0x0030)
#define MT_WFDMA_MSI_INT_CFG0       MT_WFDMA_EXT_CSR(0x00f0)
#define MT_WFDMA_MSI_INT_CFG1       MT_WFDMA_EXT_CSR(0x00f4)
#define MT_WFDMA_MSI_INT_CFG2       MT_WFDMA_EXT_CSR(0x00f8)
#define MT_WFDMA_MSI_INT_CFG3       MT_WFDMA_EXT_CSR(0x00fc)
#define MT_WFDMA_HOST_INT_DIS       MT_WFDMA0(0x022c)
#define HOST_RX_DONE_INT_ENA0       BIT(0)
#define HOST_RX_DONE_INT_ENA1       BIT(1)
#define HOST_RX_DONE_INT_ENA(_n)    BIT(_n)   /* RX done for ring n (0-7) */
#define HOST_TX_DONE_INT_ENA15      BIT(25)
#define HOST_TX_DONE_INT_ENA16      BIT(26)
#define HOST_TX_DONE_INT_ENA17      BIT(27)

/* MT6639 MCU init registers (from chips/mt6639 and coda headers) */

/* CB_INFRA_RGU - WF subsystem reset (coda/mt6639/cb_infra_rgu.h) */
/* chip addr 0x70028000, bus2chip: {0x70020000, 0x1f0000, 0x10000} */
#define MT_CB_INFRA_RGU_WF_SUBSYS_RST      0x1f8600  /* BAR0 offset */
#define MT_WF_SUBSYS_RST_BIT               BIT(4)

/* WF_TOP_CFG_ON - MCU status (coda/mt6639/wf_top_cfg_on.h) */
/* chip addr 0x81021000, bus2chip: {0x81020000, 0xc0000, 0x10000} */
#define MT_ROMCODE_INDEX                    0xc1604   /* BAR0 offset */
#define MT_MCU_IDLE_VALUE                   0x1D1E

/* CONN_HOST_CSR_TOP - CONNINFRA wakeup (coda/mt6639/conn_host_csr_top.h) */
/* chip addr 0x7c060000, bus2chip: {0x7c060000, 0xe0000, 0x10000} */
#define MT_WAKEPU_TOP                       0xe01A0   /* BAR0 offset for WAKEPU_TOP */
#define MT_WAKEPU_WF                        0xe01A4   /* BAR0 offset for WAKEPU_WF */

/* CB_INFRA_MISC0 - PCIe address remap (coda/mt6639/cb_infra_misc0.h) */
/* chip addr 0x70026000, bus2chip: {0x70020000, 0x1f0000, 0x10000} */
#define MT_CB_INFRA_MISC0_PCIE_REMAP_WF    0x1f6554  /* BAR0 offset */
#define MT_CB_INFRA_MISC0_PCIE_REMAP_WF_BT 0x1f6558  /* BAR0 offset */
#define MT_PCIE_REMAP_WF_VALUE              0x74037001
#define MT_PCIE_REMAP_WF_BT_VALUE           0x70007000

/* CONN_SEMAPHORE (chip addr from conn_semaphore.h, mapped via CBTOP area) */
/* 暂时使用 bus2chip {0x70000000, 0x1e0000, 0x9000} 区域 */

/* HOST2MCU software interrupt set */
#define MT_HOST2MCU_SW_INT_SET              MT_WFDMA0(0x0108)

/* From MT6639 reverse: PreFirmwareDownloadInit / ConfigIntMask path */
#define MT_CONN_MISC_CFG            0x000d1000 /* 0x7c021000 */

/* MCIF interrupt remap - allows MCU DMA writes to reach host PCIe memory
 * From conn_bus_cr_von.h (found in Downloads/vendor-mediatek...):
 *   CONN_BUS_CR_VON_BASE = 0x7C021000 (= 0x18021000 + 0x64000000)
 *   PCIE2AP_REMAP_WF_1_BA = base + 0x34 = chip 0x7C021034 → BAR0 0xd1034
 * vendor mt6639.c line 3223: writes 0x18051803 to PCIE2AP_REMAP_WF_1_BA
 * Without this, MCU cannot DMA event responses back to host RX rings.
 */
#define MT_CONN_BUS_CR_VON_BASE     0x000d1000 /* chip 0x7C021000, BAR0 offset */
#define MT_MCIF_REMAP_WF_1_BA       0x000d1034 /* PCIE2AP_REMAP_WF_1_BA, BAR0 offset */
#define MT_MCIF_REMAP_VAL           0x18051803 /* vendor mt6639.c value */

/* CB_INFRA_SLP_CTRL - MCU ownership (from cb_infra_slp_ctrl.h found in Downloads)
 * CB_INFRA_SLP_CTRL_BASE = 0x70025000, bus2chip {0x70020000, 0x1f0000, 0x10000}
 * chip 0x70025030 → BAR0 0x1f5030 (MCU_OWN status)
 * chip 0x70025034 → BAR0 0x1f5034 (MCU_OWN set)
 */
#define MT_CB_INFRA_MCU_OWN         0x1f5030   /* MCU_OWN status, BAR0 offset */
#define MT_CB_INFRA_MCU_OWN_SET     0x1f5034   /* MCU_OWN set, BAR0 offset */
#define MT_CONN_CFG_10200           0x00010200
#define MT_CONN_CFG_10204           0x00010204
#define MT_CONN_CFG_10208           0x00010208
#define MT_CONN_CFG_10020           0x00010020
#define MT_CONN_INFRA_30188         0x00010188 /* 0x74030188 -> 0x010188 via mt76 fixed map */
#define MT_CONN_ON_LPCTL            0x000e0010 /* 0x7c060010 via mt7925 fixed map */
#define MT_CONN_ON_MISC             0x000e00f0 /* 0x7c0600f0 */
#define PCIE_LPCR_HOST_SET_OWN      BIT(0)
#define PCIE_LPCR_HOST_CLR_OWN      BIT(1)
#define PCIE_LPCR_HOST_OWN_SYNC     BIT(2)
#define MT_TOP_MISC2_FW_PWR_ON      BIT(0)
#define MT_TOP_MISC2_FW_N9_ON       BIT(1)

/* MCU WPDMA0 DUMMY_CR - host<->MCU sync flag
 * chip addr 0x54000120, bus2chip {0x54000000, 0x02000, 0x1000}
 * BAR0 offset = 0x02000 + 0x120 = 0x02120
 * Upstream mt792x_dma_enable sets BIT(1) (NEED_REINIT) after DMA enable
 * to signal MCU that WFDMA has been (re)initialized and needs re-sync.
 */
#define MT_MCU_WPDMA0_DUMMY_CR       0x02120   /* BAR0 offset */
#define MT_WFDMA_NEED_REINIT         BIT(1)

/* WFSYS software reset (CONN_INFRA region)
 * chip addr 0x7c000140, bus2chip {0x7c000000, 0xf0000, 0x10000}
 * BAR0 offset = 0xf0000 + 0x140 = 0xf0140
 * Upstream mt792x_wfsys_reset: clear BIT(0), wait 50ms, set BIT(0), poll BIT(4)
 */
#define MT_WFSYS_SW_RST              0xf0140   /* BAR0 offset */
#define WFSYS_SW_RST_B               BIT(0)
#define WFSYS_SW_INIT_DONE           BIT(4)

/* L1 remap - for accessing 0x18xxxxxx chip addresses via BAR0 window */
#define MT_HIF_REMAP_L1              0x155024  /* BAR0 offset */
#define MT_HIF_REMAP_L1_MASK         GENMASK(31, 16)
#define MT_HIF_REMAP_BASE_L1         0x130000  /* BAR0 window base */

/* EMI sleep protection (chip addr 0x18011100, needs L1 remap) */
#define MT_HW_EMI_CTL                0x18011100
#define MT_HW_EMI_CTL_SLPPROT_EN     BIT(1)

/* mt76/dma.h */
#define MT_DMA_CTL_SD_LEN0          GENMASK(29, 16)
#define MT_DMA_CTL_LAST_SEC0        BIT(30)
#define MT_DMA_CTL_DMA_DONE         BIT(31)
#define MT_DMA_MAX_LEN0             FIELD_MAX(MT_DMA_CTL_SD_LEN0)

#define MT_WFDMA_GLO_CFG_TX_DMA_EN  BIT(0)
#define MT_WFDMA_GLO_CFG_TX_DMA_BUSY BIT(1)
#define MT_WFDMA_GLO_CFG_RX_DMA_EN  BIT(2)
#define MT_WFDMA_GLO_CFG_RX_DMA_BUSY BIT(3)

/* CONNAC3X WPDMA_GLO_CFG field bits (from mt6639 coda header wf_wfdma_host_dma0.h) */
#define MT_GLO_CFG_PDMA_BT_SIZE              GENMASK(5, 4)   /* burst size */
#define MT_GLO_CFG_TX_WB_DDONE               BIT(6)          /* TX writeback done */
#define MT_GLO_CFG_FIFO_LITTLE_ENDIAN        BIT(12)
#define MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN BIT(15)        /* prefetch chain mode */
#define MT_GLO_CFG_OMIT_RX_INFO_PFET2        BIT(21)
#define MT_GLO_CFG_OMIT_TX_INFO              BIT(28)
#define MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL    BIT(9)   /* bypass DMA scheduler for FWDL */
#define MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN      BIT(20)  /* RX queue selection enable (vendor sets this) */
#define MT_GLO_CFG_CLK_GATE_DIS              BIT(30)

/* WPDMA RX pause thresholds (pair registers, each covers 2 rings) */
#define MT_WPDMA_PAUSE_RX_Q_TH(_n)  MT_WFDMA0(0x0260 + ((_n) << 2))
/* _n=0: rings 0-1, _n=1: rings 2-3, _n=2: rings 4-5, _n=3: rings 6-7, _n=4: rings 8-9, _n=5: rings 10-11 */
#define MT_WPDMA_PAUSE_RX_Q_TH_VAL  (2 | (2 << 16))  /* threshold=2 for both rings in each register */

/* Vendor mt6639WpdmaConfigExt0: SDO dispatch mode configuration
 * Controls how WFDMA dispatches data between MCU and host RX rings.
 * Without this, WFDMA may not route MCU responses to host RX rings.
 */
#define MT_WPDMA_GLO_CFG_EXT0_VAL   0x28C004DF  /* vendor value with CSR_SDO_DISP_MODE */

/* WF_HIF_DMASHDL_TOP - DMA Scheduler (coda/mt6639/wf_hif_dmashdl_top.h)
 * chip addr 0x7C026000, bus2chip {0x7c020000, 0xd0000, 0x10000}
 * BAR0 offset = 0xd0000 + (0x7C026000 - 0x7c020000) = 0xd6000
 *
 * Without proper DMASHDL init (or bypass), all host TX ring data is
 * blocked by quota enforcement and never reaches MCU DMA RX rings.
 * This explains: host TX DIDX advances but MCU DMA RX DIDX stays 0.
 */
#define MT_HIF_DMASHDL_BASE                  0xd6000
#define MT_HIF_DMASHDL(_ofs)                 (MT_HIF_DMASHDL_BASE + (_ofs))
#define MT_HIF_DMASHDL_SW_CONTROL            MT_HIF_DMASHDL(0x04)
#define MT_HIF_DMASHDL_BYPASS_EN             BIT(28)
#define MT_HIF_DMASHDL_OPTIONAL_CONTROL      MT_HIF_DMASHDL(0x08)
#define MT_HIF_DMASHDL_PAGE_SETTING          MT_HIF_DMASHDL(0x0c)
#define MT_HIF_DMASHDL_REFILL_CONTROL        MT_HIF_DMASHDL(0x10)
#define MT_HIF_DMASHDL_PKT_MAX_SIZE          MT_HIF_DMASHDL(0x1c)
#define MT_HIF_DMASHDL_GROUP_CONTROL(_n)     MT_HIF_DMASHDL(0x20 + ((_n) << 2))
#define MT_HIF_DMASHDL_QUEUE_MAP0            MT_HIF_DMASHDL(0x60)
#define MT_HIF_DMASHDL_QUEUE_MAP1            MT_HIF_DMASHDL(0x64)
#define MT_HIF_DMASHDL_QUEUE_MAP2            MT_HIF_DMASHDL(0x68)
#define MT_HIF_DMASHDL_QUEUE_MAP3            MT_HIF_DMASHDL(0x6c)
#define MT_HIF_DMASHDL_SCHED_SET0            MT_HIF_DMASHDL(0x70)
#define MT_HIF_DMASHDL_SCHED_SET1            MT_HIF_DMASHDL(0x74)
#define MT_HIF_DMASHDL_STATUS_RD             MT_HIF_DMASHDL(0x100)

/* CONNAC3X WFDMA diagnostic / error registers */
#define MT_WPDMA_GLO_CFG2                MT_WFDMA0(0x025c)
#define MT_WPDMA2HOST_ERR_INT_STA        MT_WFDMA0(0x01e8)
#define MT_MCU_INT_STA                   MT_WFDMA0(0x0110)
#define MT_CONN_HIF_BUSY_STATUS          MT_WFDMA0(0x0138)
#define MT_WPDMA_TIMEOUT_CFG             MT_WFDMA0(0x0230)

/* Windows-observed settings */
#define MT_WPDMA_GLO_CFG_WIN_OR     0x4000005
#define MT_WPDMA_GLO_CFG_EXT1_WIN   0x10000000
#define MT_WFDMA_MSI_CFG0_WIN       0x00660077
#define MT_WFDMA_MSI_CFG1_WIN       0x00001100
#define MT_WFDMA_MSI_CFG2_WIN       0x0030004f
#define MT_WFDMA_MSI_CFG3_WIN       0x00542200
#define MT_WPDMA_GLO_CFG_MT76_SET   (BIT(6) | BIT(11) | BIT(12) | BIT(13) | \
                                     BIT(15) | BIT(21) | BIT(28) | BIT(30) | \
                                     FIELD_PREP(GENMASK(5, 4), 3))

/* Queue mapping from mt7925 (CONNAC2) */
#define MT_TXQ_MCU_WM_RING          15
#define MT_TXQ_FWDL_RING            16
#define MT_TXQ_MCU_WM_RING_ALT      17
#define MT_RXQ_MCU_EVENT_RING_CONNAC2   0
#define MT_RXQ_MCU_EVENT_RING_CONNAC3   6   /* MT6639 uses RX ring 6 for events */
#define MT_TXQ_MCU_WM_RING_SIZE     256
#define MT_TXQ_FWDL_RING_SIZE       128
#define MT_RXQ_MCU_EVENT_RING_SIZE  128
#define MT_RX_BUF_SIZE              2048

/* mt76_connac2_mac.h */
#define MT_TXD0_Q_IDX               GENMASK(31, 25)
#define MT_TXD0_PKT_FMT             GENMASK(24, 23)
#define MT_TXD0_TX_BYTES            GENMASK(15, 0)
#define MT_TXD1_LONG_FORMAT         BIT(31)
#define MT_TXD1_HDR_FORMAT          GENMASK(17, 16)   /* CONNAC2 */
#define MT_TXD1_HDR_FORMAT_V3       GENMASK(15, 14)   /* CONNAC3 */
#define MT_HDR_FORMAT_CMD           1
#define MT_TX_TYPE_CMD              2
#define MT_TX_MCU_PORT_RX_Q0        0x20
#define MT_TX_PORT_IDX_MCU          1
#define MCU_PQ_ID(p, q)             (((p) << 15) | ((q) << 10))
#define MCU_PKT_ID                  0xa0

/* mt76_connac_mcu.h */
#define DL_MODE_ENCRYPT             BIT(0)
#define DL_MODE_KEY_IDX             GENMASK(2, 1)
#define DL_MODE_RESET_SEC_IV        BIT(3)
#define DL_MODE_WORKING_PDA_CR4     BIT(4)
#define DL_CONFIG_ENCRY_MODE_SEL    BIT(6)
#define DL_MODE_NEED_RSP            BIT(31)
#define FW_FEATURE_OVERRIDE_ADDR    BIT(5)
#define FW_FEATURE_NON_DL           BIT(6)
#define FW_START_OVERRIDE           BIT(0)
#define FW_START_WORKING_PDA_CR4    BIT(2)

#define PATCH_SEC_NOT_SUPPORT       GENMASK(31, 0)
#define PATCH_SEC_TYPE_MASK         GENMASK(15, 0)
#define PATCH_SEC_TYPE_INFO         0x2
#define PATCH_SEC_ENC_TYPE_MASK     GENMASK(31, 24)
#define PATCH_SEC_ENC_TYPE_PLAIN    0x00
#define PATCH_SEC_ENC_TYPE_AES      0x01
#define PATCH_SEC_ENC_TYPE_SCRAMBLE 0x02
#define PATCH_SEC_ENC_AES_KEY_MASK  GENMASK(7, 0)

#define PATCH_SEM_RELEASE           0
#define PATCH_SEM_GET               1

#define PATCH_NOT_DL_SEM_FAIL       0
#define PATCH_IS_DL                 1
#define PATCH_NOT_DL_SEM_SUCCESS    2
#define PATCH_REL_SEM_SUCCESS       3

#define MCU_CMD_TARGET_ADDRESS_LEN_REQ 0x01
#define MCU_CMD_FW_START_REQ        0x02
#define MCU_CMD_PATCH_START_REQ     0x05
#define MCU_CMD_PATCH_FINISH_REQ    0x07
#define MCU_CMD_PATCH_SEM_CONTROL   0x10
#define MCU_CMD_FW_SCATTER          0xee

#define MT_MCU_CMD_WAKE_RX_PCIE     BIT(0)

#define MCU_PATCH_ADDRESS           0x200000
/* MT6639 Patch address (mt6639.h line 41: 0x00900000) */
#define MCU_PATCH_ADDRESS_MT6639    0x00900000

struct mt76_desc {
    __le32 buf0;
    __le32 ctrl;
    __le32 buf1;
    __le32 info;
} __packed __aligned(4);

struct mt76_connac2_mcu_txd {
    __le32 txd[8];
    __le16 len;
    __le16 pq_id;
    u8 cid;
    u8 pkt_type;
    u8 set_query;
    u8 seq;
    u8 uc_d2b0_rev;
    u8 ext_cid;
    u8 s2d_index;
    u8 ext_cid_ack;
    __le32 rsv[5];
} __packed __aligned(4);

struct mt76_connac2_patch_hdr {
    char build_date[16];
    char platform[4];
    __be32 hw_sw_ver;
    __be32 patch_ver;
    __be16 checksum;
    u16 rsv;
    struct {
        __be32 patch_ver;
        __be32 subsys;
        __be32 feature;
        __be32 n_region;
        __be32 crc;
        u32 rsv[11];
    } desc;
} __packed;

struct mt76_connac2_patch_sec {
    __be32 type;
    __be32 offs;
    __be32 size;
    union {
        __be32 spec[13];
        struct {
            __be32 addr;
            __be32 len;
            __be32 sec_key_idx;
            __be32 align_len;
            u32 rsv[9];
        } info;
    };
} __packed;

struct mt76_connac2_fw_trailer {
    u8 chip_id;
    u8 eco_code;
    u8 n_region;
    u8 format_ver;
    u8 format_flag;
    u8 rsv[2];
    char fw_ver[10];
    char build_date[15];
    __le32 crc;
} __packed;

struct mt76_connac2_fw_region {
    __le32 decomp_crc;
    __le32 decomp_len;
    __le32 decomp_blk_sz;
    u8 rsv[4];
    __le32 addr;
    __le32 len;
    u8 feature_set;
    u8 type;
    u8 rsv1[14];
} __packed;

struct mt7927_ring {
    struct mt76_desc *desc;
    dma_addr_t desc_dma;
    void **buf;
    dma_addr_t *buf_dma;
    u32 buf_size;
    u16 qid;
    u16 ndesc;
    u16 head;
    u16 tail;   /* RX read position (where CPU reads next) */
};

struct mt7927_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_len;

    struct mt7927_ring ring_wm;
    struct mt7927_ring ring_fwdl;
    struct mt7927_ring ring_evt;
    /* Dummy RX rings 4,5,7 - needed for WFDMA prefetch chain to not stall.
     * Vendor halWpdmaInitRxRing allocates ALL rings 4-7; without valid
     * descriptors the prefetch engine may block the entire chain.
     */
    struct mt7927_ring ring_rx4;
    struct mt7927_ring ring_rx5;
    struct mt7927_ring ring_rx7;

    u8 mcu_seq;
};

#define PREFETCH(_base, _depth) (((_base) << 16) | (_depth))

static inline u32 mt7927_rr(struct mt7927_dev *dev, u32 reg)
{
    if (unlikely(reg + sizeof(u32) > dev->bar0_len)) {
        dev_warn(&dev->pdev->dev,
                 "mmio rr out-of-range: reg=0x%08x bar0_len=0x%llx\n",
                 reg, (u64)dev->bar0_len);
        return 0xffffffff;
    }
    return ioread32(dev->bar0 + reg);
}

static inline void mt7927_wr(struct mt7927_dev *dev, u32 reg, u32 val)
{
    if (unlikely(reg + sizeof(u32) > dev->bar0_len)) {
        dev_warn(&dev->pdev->dev,
                 "mmio wr out-of-range: reg=0x%08x val=0x%08x bar0_len=0x%llx\n",
                 reg, val, (u64)dev->bar0_len);
        return;
    }
    iowrite32(val, dev->bar0 + reg);
}

static char *fw_ram = "mediatek/WIFI_RAM_CODE_MT6639_2_1.bin";
static char *fw_patch = "mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin";
static bool enable_predl_regs;
static int wm_ring_qid = MT_TXQ_MCU_WM_RING;
static bool wait_mcu_event = true;
static bool scatter_via_wm;
static bool strict_dma_probe;
static bool tx_kick_use_didx;
static bool minimal_dma_cfg;
static bool use_mt6639_init = true;
static bool use_mt6639_patch_addr = true;
static bool force_wf_reset = true;
static bool no_long_format;
static int evt_ring_qid = MT_RXQ_MCU_EVENT_RING_CONNAC3;
module_param(enable_predl_regs, bool, 0644);
MODULE_PARM_DESC(enable_predl_regs, "Enable risky pre-download register sequence from MT6639 reverse");
module_param(wm_ring_qid, int, 0644);
MODULE_PARM_DESC(wm_ring_qid, "MCU WM TX ring qid (15 or 17)");
module_param(wait_mcu_event, bool, 0644);
MODULE_PARM_DESC(wait_mcu_event, "Wait one RX event after each MCU command");
module_param(scatter_via_wm, bool, 0644);
MODULE_PARM_DESC(scatter_via_wm,
                 "Send FW scatter via WM command format (Windows-like) instead of raw FWDL ring");
module_param(strict_dma_probe, bool, 0644);
MODULE_PARM_DESC(strict_dma_probe, "Fail probe if DMA path probe fails");
module_param(tx_kick_use_didx, bool, 0644);
MODULE_PARM_DESC(tx_kick_use_didx,
                 "Use DIDX doorbell for TX rings (legacy behavior observed on this platform)");
module_param(minimal_dma_cfg, bool, 0644);
MODULE_PARM_DESC(minimal_dma_cfg, "Use minimal DMA register init (debug path)");
module_param(use_mt6639_init, bool, 0644);
MODULE_PARM_DESC(use_mt6639_init, "Use MT6639 MCU init sequence (cbinfra remap + WF reset + MCU idle poll)");
module_param(use_mt6639_patch_addr, bool, 0644);
MODULE_PARM_DESC(use_mt6639_patch_addr, "Use MT6639 patch address 0x900000 instead of MT7925 0x200000");
module_param(force_wf_reset, bool, 0644);
MODULE_PARM_DESC(force_wf_reset, "Force WF subsystem reset even when MCU is already idle");
module_param(no_long_format, bool, 0644);
MODULE_PARM_DESC(no_long_format, "Do NOT set DW1 BIT(31) LONG_FORMAT in MCU TXD (vendor MT6639 doesn't set it)");
module_param(evt_ring_qid, int, 0644);
MODULE_PARM_DESC(evt_ring_qid, "RX event ring qid (0=connac2, 6=connac3/MT6639 default)");
module_param(fw_ram, charp, 0644);
MODULE_PARM_DESC(fw_ram, "RAM firmware path");
module_param(fw_patch, charp, 0644);
MODULE_PARM_DESC(fw_patch, "Patch firmware path");

static bool skip_ext0 = true;
module_param(skip_ext0, bool, 0644);
MODULE_PARM_DESC(skip_ext0, "Skip GLO_CFG_EXT0 write (vendor only writes with HOST_OFFLOAD, default=skip)");

static bool skip_logic_rst;
module_param(skip_logic_rst, bool, 0644);
MODULE_PARM_DESC(skip_logic_rst, "Skip WFDMA logic reset in dma_disable (preserves ROM bootloader state)");

static bool use_wfsys_reset;
module_param(use_wfsys_reset, bool, 0644);
MODULE_PARM_DESC(use_wfsys_reset, "Use upstream WFSYS reset at 0x7c000140 instead of CB_INFRA_RGU reset");

static bool use_emi_slpprot = true;
module_param(use_emi_slpprot, bool, 0644);
MODULE_PARM_DESC(use_emi_slpprot, "Enable EMI sleep protection via L1 remap (upstream does this before WFSYS reset)");

static int mcu_rx_qidx = 2;
module_param(mcu_rx_qidx, int, 0644);
MODULE_PARM_DESC(mcu_rx_qidx, "CONNAC3 TXD Q_IDX for MCU commands (ROM bootloader configures MCU DMA RX2/RX3, default=2)");

static u32 mcif_remap_reg = 0x000d1034;
static u32 mcif_remap_val = 0x18051803;
module_param(mcif_remap_reg, uint, 0644);
MODULE_PARM_DESC(mcif_remap_reg, "MCIF remap register BAR0 offset (default 0xd1034 = PCIE2AP_REMAP_WF_1_BA)");
module_param(mcif_remap_val, uint, 0644);
MODULE_PARM_DESC(mcif_remap_val, "MCIF remap value (default 0x18051803 from vendor mt6639.c)");

/*
 * Windows MT6639 reverse evidence (docs/reverse/mtkwecx_mt6639_fw_dma_reverse.md):
 *   0x7c024200/204/208/2b4 and 0x7c027030/0f0/0f4/0f8/0fc are key pre-FWDL regs.
 * Dump them explicitly to verify Linux prototype writes are latched as expected.
 */
static void mt7927_dump_win_key_regs(struct mt7927_dev *dev, const char *tag)
{
    dev_info(&dev->pdev->dev,
             "%s: [0x7c024200]=0x%08x [0x7c024204]=0x%08x [0x7c024208]=0x%08x [0x7c0242b4]=0x%08x\n",
             tag,
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA),
             mt7927_rr(dev, MT_WPDMA_GLO_CFG),
             mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1));

    dev_info(&dev->pdev->dev,
             "%s: [0x7c027030]=0x%08x [0x7c0270f0]=0x%08x [0x7c0270f4]=0x%08x [0x7c0270f8]=0x%08x [0x7c0270fc]=0x%08x\n",
             tag,
             mt7927_rr(dev, MT_WFDMA_HOST_CONFIG),
             mt7927_rr(dev, MT_WFDMA_MSI_INT_CFG0),
             mt7927_rr(dev, MT_WFDMA_MSI_INT_CFG1),
             mt7927_rr(dev, MT_WFDMA_MSI_INT_CFG2),
             mt7927_rr(dev, MT_WFDMA_MSI_INT_CFG3));
}

static void mt7927_wr_verify(struct mt7927_dev *dev, u32 reg, u32 val, const char *name)
{
    u32 rb;

    mt7927_wr(dev, reg, val);
    rb = mt7927_rr(dev, reg);
    dev_info(&dev->pdev->dev, "wr %s: reg=0x%08x val=0x%08x rb=0x%08x\n",
             name, reg, val, rb);
}

static void mt7927_apply_predl_cfg(struct mt7927_dev *dev)
{
    u32 v;

    /*
     * Evidence source:
     * docs/reverse/mtkwecx_mt6639_fw_dma_reverse.md
     * - MT6639ConfigIntMask / MT6639PreFirmwareDownloadInit.
     */
    mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_DIS, 0x2600f000, "cfg_intmask_222c");

    v = mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA);
    dev_info(&dev->pdev->dev, "rd cfg_intmask_2204: reg=0x%08x val=0x%08x\n",
             MT_WFDMA_HOST_INT_ENA, v);

    v = mt7927_rr(dev, MT_CONN_INFRA_30188);
    mt7927_wr_verify(dev, MT_CONN_INFRA_30188, v | BIT(16), "cfg_conn_30188_bit16");

    v = mt7927_rr(dev, MT_CONN_MISC_CFG);
    dev_info(&dev->pdev->dev, "rd predl_21000_before: reg=0x%08x val=0x%08x\n",
             MT_CONN_MISC_CFG, v);
    mt7927_wr_verify(dev, MT_CONN_MISC_CFG, 0x70011840, "predl_21000");

    dev_info(&dev->pdev->dev, "rd predl_10200: reg=0x%08x val=0x%08x\n",
             MT_CONN_CFG_10200, mt7927_rr(dev, MT_CONN_CFG_10200));
    dev_info(&dev->pdev->dev, "rd predl_10204: reg=0x%08x val=0x%08x\n",
             MT_CONN_CFG_10204, mt7927_rr(dev, MT_CONN_CFG_10204));
    dev_info(&dev->pdev->dev, "rd predl_10208: reg=0x%08x val=0x%08x\n",
             MT_CONN_CFG_10208, mt7927_rr(dev, MT_CONN_CFG_10208));
    dev_info(&dev->pdev->dev, "rd predl_10020: reg=0x%08x val=0x%08x\n",
             MT_CONN_CFG_10020, mt7927_rr(dev, MT_CONN_CFG_10020));
}

static void mt7927_dump_dma_state(struct mt7927_dev *dev, const char *tag, u16 qid)
{
    dev_info(&dev->pdev->dev,
             "%s: GLO=0x%08x EXT1=0x%08x INT_ENA=0x%08x INT_STA=0x%08x MCU_CMD=0x%08x SW_INT_ENA=0x%08x\n",
             tag,
             mt7927_rr(dev, MT_WPDMA_GLO_CFG),
             mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA),
             mt7927_rr(dev, MT_MCU_CMD_REG),
             mt7927_rr(dev, MT_MCU2HOST_SW_INT_ENA));

    dev_info(&dev->pdev->dev,
             "%s: q%u BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x TX_PRI=0x%08x RX_PRI=0x%08x\n",
             tag, qid,
             mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(qid)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_CNT(qid)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(qid)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(qid)),
             mt7927_rr(dev, MT_WFDMA_INT_TX_PRI),
             mt7927_rr(dev, MT_WFDMA_INT_RX_PRI));
}

/* WFDMA reset register (from mt7996/regs.h) */
#define MT_WFDMA0_RST              MT_WFDMA0(0x100)
#define MT_WFDMA0_RST_LOGIC_RST    BIT(4)
#define MT_WFDMA0_RST_DMASHDL_RST  BIT(5)

static void mt7927_dma_disable(struct mt7927_dev *dev)
{
    u32 val;
    int i;

    /* Diagnostic: log GLO_CFG before any modification */
    dev_info(&dev->pdev->dev,
             "dma_disable: GLO_CFG before=0x%08x RST before=0x%08x\n",
             mt7927_rr(dev, MT_WPDMA_GLO_CFG),
             mt7927_rr(dev, MT_WFDMA0_RST));

    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN);
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
    wmb();

    for (i = 0; i < 100; i++) {
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        if (!(val & (MT_WFDMA_GLO_CFG_TX_DMA_BUSY | MT_WFDMA_GLO_CFG_RX_DMA_BUSY)))
            break;
        usleep_range(500, 1000);
    }

    if (skip_logic_rst) {
        dev_info(&dev->pdev->dev,
                 "dma_disable: SKIPPING logic reset (skip_logic_rst=1), GLO_CFG=0x%08x\n",
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG));
        return;
    }

    /* WFDMA logic reset: clear internal FIFOs and DMA state.
     * From mt7996/dma.c mt7996_dma_disable(): pulse LOGIC_RST + DMASHDL_RST
     */
    val = mt7927_rr(dev, MT_WFDMA0_RST);
    val &= ~(MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST);
    mt7927_wr(dev, MT_WFDMA0_RST, val);
    val |= MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST;
    mt7927_wr(dev, MT_WFDMA0_RST, val);
    usleep_range(100, 200);

    dev_info(&dev->pdev->dev,
             "dma_disable: WFDMA logic reset, RST=0x%08x GLO_CFG=0x%08x\n",
             mt7927_rr(dev, MT_WFDMA0_RST),
             mt7927_rr(dev, MT_WPDMA_GLO_CFG));
}

static int mt7927_ring_alloc(struct mt7927_dev *dev, struct mt7927_ring *ring,
                             u16 qid, u16 ndesc)
{
    ring->qid = qid;
    ring->ndesc = ndesc;
    ring->head = 0;
    ring->desc = dma_alloc_coherent(&dev->pdev->dev,
                                    ndesc * sizeof(struct mt76_desc),
                                    &ring->desc_dma, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    memset(ring->desc, 0, ndesc * sizeof(struct mt76_desc));
    /*
     * Align with mt76 dma queue reset behavior:
     * non-posted descriptors start with DMA_DONE=1, and producer clears it
     * when posting a descriptor.
     */
    for (u16 i = 0; i < ndesc; i++)
        ring->desc[i].ctrl = cpu_to_le32(MT_DMA_CTL_DMA_DONE);

    mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(qid), lower_32_bits(ring->desc_dma));
    mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(qid), ndesc);
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(qid), 0);
    mt7927_wr(dev, MT_WPDMA_TX_RING_DIDX(qid), 0);

    return 0;
}

static void mt7927_ring_free(struct mt7927_dev *dev, struct mt7927_ring *ring)
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

static int mt7927_rx_ring_alloc(struct mt7927_dev *dev, struct mt7927_ring *ring,
                                u16 qid, u16 ndesc, u32 buf_size)
{
    u16 i;

    ring->qid = qid;
    ring->ndesc = ndesc;
    ring->head = ndesc - 1;  /* CIDX: all buffers available to hardware */
    ring->tail = 0;          /* CPU read position: start at 0 */
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
                                          &ring->buf_dma[i], GFP_KERNEL);
        if (!ring->buf[i])
            goto err;

        ring->desc[i].buf0 = cpu_to_le32(lower_32_bits(ring->buf_dma[i]));
        ring->desc[i].buf1 = cpu_to_le32(0);
        ring->desc[i].info = cpu_to_le32(0);
        ring->desc[i].ctrl = cpu_to_le32(FIELD_PREP(MT_DMA_CTL_SD_LEN0, buf_size));
    }

    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(qid), lower_32_bits(ring->desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(qid), ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(qid), 0);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(qid), ring->head);

    return 0;

err:
    mt7927_ring_free(dev, ring);
    return -ENOMEM;
}

static void mt7927_trace_mcu_event(struct mt7927_dev *dev, const char *tag)
{
    u32 host_int = mt7927_rr(dev, MT_WFDMA_HOST_INT_STA);
    u32 mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);

    dev_info(&dev->pdev->dev,
             "%s: HOST_INT_STA=0x%08x MCU_CMD=0x%08x\n",
             tag, host_int, mcu_cmd);

    if (host_int)
        mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, host_int);
    if (mcu_cmd)
        mt7927_wr(dev, MT_MCU_CMD_REG, mcu_cmd);
}

static int mt7927_wait_mcu_event(struct mt7927_dev *dev, int timeout_ms)
{
    struct mt7927_ring *ring = &dev->ring_evt;
    unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
    struct mt76_desc *d;
    u32 ctrl;
    u32 *evt;

    do {
        d = &ring->desc[ring->tail];
        ctrl = le32_to_cpu(d->ctrl);

        if (ctrl & MT_DMA_CTL_DMA_DONE) {
            u16 idx = ring->tail;

            evt = ring->buf[idx];
            dev_info(&dev->pdev->dev,
                     "mcu-evt: q%u idx=%u ctrl=0x%08x w0=0x%08x w1=0x%08x w2=0x%08x w3=0x%08x\n",
                     ring->qid, idx, ctrl, evt[0], evt[1], evt[2], evt[3]);

            /* Clear DMA_DONE and reset descriptor for reuse */
            d->ctrl = cpu_to_le32(FIELD_PREP(MT_DMA_CTL_SD_LEN0, ring->buf_size));

            /* Advance tail (CPU read position) */
            ring->tail = (ring->tail + 1) % ring->ndesc;

            /* Return this buffer to hardware by advancing CIDX */
            mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid), idx);

            return 0;
        }
        usleep_range(1000, 2000);
    } while (time_before(jiffies, timeout));

    dev_warn(&dev->pdev->dev,
             "mcu-evt timeout: q%u tail=%u cidx=0x%x didx=0x%x desc_ctrl=0x%08x\n",
             ring->qid, ring->tail,
             mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(ring->qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(ring->qid)),
             le32_to_cpu(ring->desc[ring->tail].ctrl));

    /* Dump WFDMA error/status registers */
    dev_info(&dev->pdev->dev,
             "mcu-evt diag: GLO_CFG2=0x%08x ERR_INT_STA=0x%08x MCU_INT_STA=0x%08x\n",
             mt7927_rr(dev, MT_WPDMA_GLO_CFG2),
             mt7927_rr(dev, MT_WPDMA2HOST_ERR_INT_STA),
             mt7927_rr(dev, MT_MCU_INT_STA));
    dev_info(&dev->pdev->dev,
             "mcu-evt diag: HIF_BUSY=0x%08x TIMEOUT_CFG=0x%08x GLO_CFG=0x%08x\n",
             mt7927_rr(dev, MT_CONN_HIF_BUSY_STATUS),
             mt7927_rr(dev, MT_WPDMA_TIMEOUT_CFG),
             mt7927_rr(dev, MT_WPDMA_GLO_CFG));
    /* FW sync value (Windows driver polls 0x7c0600f0 for status==3 after FWDL) */
    dev_info(&dev->pdev->dev,
             "mcu-evt diag: CONN_ON_MISC(fw_sync)=0x%08x ROMCODE_INDEX=0x%08x\n",
             mt7927_rr(dev, MT_CONN_ON_MISC),
             mt7927_rr(dev, MT_ROMCODE_INDEX));

    /* Dump ALL RX ring DIDX values to see if MCU wrote to any ring */
    {
        int r;
        for (r = 0; r <= 7; r++)
            dev_info(&dev->pdev->dev,
                     "mcu-evt diag: RX ring %d DIDX=0x%08x CIDX=0x%08x BASE=0x%08x CNT=0x%08x\n",
                     r,
                     mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(r)),
                     mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(r)),
                     mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(r)),
                     mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(r)));
    }
    return -ETIMEDOUT;
}

static int mt7927_kick_ring_buf(struct mt7927_dev *dev, struct mt7927_ring *ring,
                                dma_addr_t dma, u32 len, bool last_sec)
{
    struct mt76_desc *d;
    u16 idx = ring->head;
    u16 next = (idx + 1) % ring->ndesc;
    u32 ctrl;
    int i;

    mt7927_dump_dma_state(dev, "kick-before", ring->qid);

    d = &ring->desc[idx];
    memset(d, 0, sizeof(*d));

    ctrl = FIELD_PREP(MT_DMA_CTL_SD_LEN0, len);
    if (last_sec)
        ctrl |= MT_DMA_CTL_LAST_SEC0;

    d->buf0 = cpu_to_le32(lower_32_bits(dma));
    d->ctrl = cpu_to_le32(ctrl);
    wmb();

    ring->head = next;
    if (tx_kick_use_didx)
        mt7927_wr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid), ring->head);
    else
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid), ring->head);
    wmb();
    /* Notify MCU about new descriptor via HOST2MCU software interrupt */
    mt7927_wr(dev, MT_HOST2MCU_SW_INT_SET, BIT(0));
    mt7927_dump_dma_state(dev, "kick-after", ring->qid);

    for (i = 0; i < 100; i++) {
        u32 hw_idx;

        if (tx_kick_use_didx)
            hw_idx = mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid));
        else
            hw_idx = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid));

        if (hw_idx == ring->head)
            return 0;
        usleep_range(500, 1000);
    }

    dev_warn(&dev->pdev->dev, "ring%u not consumed: cpu_idx=0x%x dma_idx=0x%x\n",
             ring->qid,
             mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid)));
    return -ETIMEDOUT;
}

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
    memcpy((u8 *)buf + sizeof(*txd), payload, plen);

    /* CONNAC3X: Q_IDX routes to MCU DMA RX ring.
     * ROM bootloader only configures MCU DMA RX2/RX3 (RX0/RX1 have BASE=0).
     * Vendor used Q_IDX=0 but that routes to unconfigured RX0 → data lost.
     * Use mcu_rx_qidx (default=2) to target configured MCU DMA RX2.
     * CONNAC2 (mt7925): Q_IDX=0x20 (MT_TX_MCU_PORT_RX_Q0)
     */
    val = FIELD_PREP(MT_TXD0_TX_BYTES, len) |
          FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) |
          FIELD_PREP(MT_TXD0_Q_IDX,
                     (evt_ring_qid == MT_RXQ_MCU_EVENT_RING_CONNAC3) ?
                     mcu_rx_qidx : MT_TX_MCU_PORT_RX_Q0);
    txd->txd[0] = cpu_to_le32(val);
    if (evt_ring_qid == MT_RXQ_MCU_EVENT_RING_CONNAC3)
        val = (no_long_format ? 0 : MT_TXD1_LONG_FORMAT) |
              FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD);
    else
        val = MT_TXD1_LONG_FORMAT |
              FIELD_PREP(MT_TXD1_HDR_FORMAT, MT_HDR_FORMAT_CMD);
    txd->txd[1] = cpu_to_le32(val);

    txd->len = cpu_to_le16(len - sizeof(txd->txd));
    txd->pq_id = cpu_to_le16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU, MT_TX_MCU_PORT_RX_Q0));
    txd->cid = cid;
    txd->pkt_type = MCU_PKT_ID;
    txd->seq = ++dev->mcu_seq & 0xf;
    if (!txd->seq)
        txd->seq = ++dev->mcu_seq & 0xf;
    txd->s2d_index = 0; /* MCU_S2D_H2N */

    /* Hex dump first 80 bytes of MCU command for format debugging */
    dev_info(&dev->pdev->dev, "mcu-cmd-dump: cid=0x%02x len=%zu plen=%zu\n",
             cid, len, plen);
    print_hex_dump(KERN_INFO, "mcu-txd: ", DUMP_PREFIX_OFFSET,
                   16, 1, buf, min(len, (size_t)80), false);

    ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
    mt7927_trace_mcu_event(dev, "mcu-cmd");
    if (!ret && wait_mcu_event)
        ret = mt7927_wait_mcu_event(dev, 200);

    dma_free_coherent(&dev->pdev->dev, len, buf, dma);
    return ret;
}

static int mt7927_mcu_send_scatter(struct mt7927_dev *dev, const u8 *data, u32 len)
{
    dma_addr_t dma;
    struct mt76_connac2_mcu_txd *txd;
    void *buf;
    size_t pkt_len;
    int ret;

    if (!scatter_via_wm) {
        buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;

        memcpy(buf, data, len);
        ret = mt7927_kick_ring_buf(dev, &dev->ring_fwdl, dma, len, true);
        mt7927_trace_mcu_event(dev, "fw-scatter");

        dma_free_coherent(&dev->pdev->dev, len, buf, dma);
        return ret;
    }

    /*
     * Windows MT6639 path (v5.7.0.5275) handles CID 0xee with a dedicated
     * header branch in MtCmdSendSetQueryCmdAdv:
     *   - write 0xa000 at offset +0x24 (cid/pkt_type pair)
     *   - force seq/token byte to 0
     * Keep chunk payload in the same buffer, but send via WM ring.
     */
    pkt_len = sizeof(*txd) + len;
    buf = dma_alloc_coherent(&dev->pdev->dev, pkt_len, &dma, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    memset(buf, 0, pkt_len);
    txd = buf;
    memcpy((u8 *)buf + sizeof(*txd), data, len);

    txd->txd[0] = cpu_to_le32(FIELD_PREP(MT_TXD0_TX_BYTES, pkt_len) |
                              FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) |
                              FIELD_PREP(MT_TXD0_Q_IDX,
                                         (evt_ring_qid == MT_RXQ_MCU_EVENT_RING_CONNAC3) ?
                                         mcu_rx_qidx : MT_TX_MCU_PORT_RX_Q0));
    if (evt_ring_qid == MT_RXQ_MCU_EVENT_RING_CONNAC3)
        txd->txd[1] = cpu_to_le32((no_long_format ? 0 : MT_TXD1_LONG_FORMAT) |
            FIELD_PREP(MT_TXD1_HDR_FORMAT_V3, MT_HDR_FORMAT_CMD));
    else
        txd->txd[1] = cpu_to_le32(MT_TXD1_LONG_FORMAT |
                                  FIELD_PREP(MT_TXD1_HDR_FORMAT, MT_HDR_FORMAT_CMD));
    txd->len = cpu_to_le16(pkt_len - sizeof(txd->txd));
    txd->pq_id = cpu_to_le16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU, MT_TX_MCU_PORT_RX_Q0));
    *(__le16 *)&txd->cid = cpu_to_le16(0xa000);
    txd->set_query = 0;
    txd->seq = 0;
    txd->s2d_index = 0;

    ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, pkt_len, true);
    mt7927_trace_mcu_event(dev, "fw-scatter");

    dma_free_coherent(&dev->pdev->dev, pkt_len, buf, dma);
    return ret;
}

static int mt7927_mcu_send_firmware_chunks(struct mt7927_dev *dev,
                                           const u8 *data, u32 len, u32 max_len)
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
                           sec_info & PATCH_SEC_ENC_AES_KEY_MASK) & DL_MODE_KEY_IDX;
        mode |= DL_MODE_RESET_SEC_IV;
        break;
    case PATCH_SEC_ENC_TYPE_SCRAMBLE:
        mode |= DL_MODE_ENCRYPT;
        mode |= DL_CONFIG_ENCRY_MODE_SEL;
        mode |= DL_MODE_RESET_SEC_IV;
        break;
    default:
        break;
    }

    return mode;
}

static int mt7927_mcu_patch_sem_ctrl(struct mt7927_dev *dev, bool get)
{
    struct {
        __le32 op;
    } req = {
        .op = cpu_to_le32(get ? PATCH_SEM_GET : PATCH_SEM_RELEASE),
    };

    return mt7927_mcu_send_cmd(dev, MCU_CMD_PATCH_SEM_CONTROL, &req, sizeof(req));
}

static int mt7927_mcu_init_download(struct mt7927_dev *dev, u32 addr, u32 len, u32 mode)
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
    u8 cmd = (addr == MCU_PATCH_ADDRESS || addr == MCU_PATCH_ADDRESS_MT6639 || addr == 0xe0002800) ?
             MCU_CMD_PATCH_START_REQ : MCU_CMD_TARGET_ADDRESS_LEN_REQ;

    return mt7927_mcu_send_cmd(dev, cmd, &req, sizeof(req));
}

static int mt7927_mcu_start_patch(struct mt7927_dev *dev)
{
    struct {
        u8 check_crc;
        u8 rsv[3];
    } req = { 0 };

    return mt7927_mcu_send_cmd(dev, MCU_CMD_PATCH_FINISH_REQ, &req, sizeof(req));
}

static int mt7927_mcu_start_firmware(struct mt7927_dev *dev, u32 addr, u32 option)
{
    struct {
        __le32 option;
        __le32 addr;
    } req = {
        .option = cpu_to_le32(option),
        .addr = cpu_to_le32(addr),
    };

    return mt7927_mcu_send_cmd(dev, MCU_CMD_FW_START_REQ, &req, sizeof(req));
}

static int mt7927_load_patch(struct mt7927_dev *dev)
{
    const struct mt76_connac2_patch_hdr *hdr;
    const struct firmware *fw;
    int i, ret;
    u32 max_len = 0x800;

    ret = request_firmware(&fw, fw_patch, &dev->pdev->dev);
    if (ret)
        return ret;

    if (!fw || fw->size < sizeof(*hdr)) {
        ret = -EINVAL;
        goto out_rel;
    }

    hdr = (const void *)fw->data;
    dev_info(&dev->pdev->dev, "PATCH build %.16s\n", hdr->build_date);

    ret = mt7927_mcu_patch_sem_ctrl(dev, true);
    if (ret)
        goto out_rel;

    for (i = 0; i < be32_to_cpu(hdr->desc.n_region); i++) {
        const struct mt76_connac2_patch_sec *sec;
        u32 addr, len, mode, sec_info;
        const u8 *dl;

        sec = (const void *)(fw->data + sizeof(*hdr) + i * sizeof(*sec));
        if ((be32_to_cpu(sec->type) & PATCH_SEC_TYPE_MASK) != PATCH_SEC_TYPE_INFO) {
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
out_rel:
    release_firmware(fw);
    return ret;
}

static u32 mt7927_ram_dl_mode(u8 feature_set)
{
    u32 mode = DL_MODE_NEED_RSP;

    if (feature_set & FW_FEATURE_NON_DL)
        return mode;

    return mode;
}

static int mt7927_load_ram(struct mt7927_dev *dev)
{
    const struct mt76_connac2_fw_trailer *tr;
    const struct firmware *fw;
    u32 override = 0, option = 0;
    int ret, i, offset = 0;
    u32 max_len = 0x800;

    ret = request_firmware(&fw, fw_ram, &dev->pdev->dev);
    if (ret)
        return ret;

    if (!fw || fw->size < sizeof(*tr)) {
        ret = -EINVAL;
        goto out_rel;
    }

    tr = (const void *)(fw->data + fw->size - sizeof(*tr));
    dev_info(&dev->pdev->dev, "RAM fw %.10s build %.15s\n", tr->fw_ver, tr->build_date);

    for (i = 0; i < tr->n_region; i++) {
        const struct mt76_connac2_fw_region *region;
        u32 len, addr, mode;

        region = (const void *)((const u8 *)tr - (tr->n_region - i) * sizeof(*region));
        len = le32_to_cpu(region->len);
        addr = le32_to_cpu(region->addr);
        mode = mt7927_ram_dl_mode(region->feature_set);

        if (region->feature_set & FW_FEATURE_NON_DL)
            goto next;

        if (region->feature_set & FW_FEATURE_OVERRIDE_ADDR)
            override = addr;

        ret = mt7927_mcu_init_download(dev, addr, len, mode);
        if (ret)
            goto out_rel;

        ret = mt7927_mcu_send_firmware_chunks(dev, fw->data + offset, len, max_len);
        if (ret)
            goto out_rel;

next:
        offset += len;
    }

    if (override)
        option |= FW_START_OVERRIDE;

    ret = mt7927_mcu_start_firmware(dev, override, option);
    if (ret)
        goto out_rel;

out_rel:
    release_firmware(fw);
    return ret;
}

static int mt7927_mcu_fw_download(struct mt7927_dev *dev)
{
    int ret;

    dev_info(&dev->pdev->dev, "Starting connac2 MCU fw download flow\n");

    ret = mt7927_load_patch(dev);
    if (ret) {
        dev_err(&dev->pdev->dev, "Patch download failed: %d\n", ret);
        return ret;
    }

    ret = mt7927_load_ram(dev);
    if (ret) {
        dev_err(&dev->pdev->dev, "RAM download failed: %d\n", ret);
        return ret;
    }

    dev_info(&dev->pdev->dev, "MCU firmware download sequence completed\n");
    return 0;
}

static int mt7927_dma_path_probe(struct mt7927_dev *dev)
{
    void *buf;
    dma_addr_t dma;
    int ret;
    u32 len = 64;
    struct mt7927_ring *ring = &dev->ring_fwdl;

    /*
     * Raw DMA probe must stay on FWDL ring.
     * Probing WM ring with a raw buffer can block later real MCU commands.
     */
    if (scatter_via_wm)
        dev_info(&dev->pdev->dev,
                 "dma-probe: WM scatter enabled, probing FWDL ring only\n");

    buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    memset(buf, 0x5a, len);

    dev_info(&dev->pdev->dev,
             "dma-probe: kick q%u dma=0x%llx len=%u\n",
             ring->qid, (u64)dma, len);

    ret = mt7927_kick_ring_buf(dev, ring, dma, len, true);
    if (ret) {
        dev_warn(&dev->pdev->dev,
                 "dma-probe: q%u not consumed (ret=%d cidx=0x%x didx=0x%x)\n",
                 ring->qid, ret,
                 mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid)),
                 mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid)));
    } else {
        dev_info(&dev->pdev->dev,
                 "dma-probe: q%u consumed (cidx=0x%x didx=0x%x)\n",
                 ring->qid,
                 mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(ring->qid)),
                 mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(ring->qid)));
    }

    dma_free_coherent(&dev->pdev->dev, len, buf, dma);
    return ret;
}

static void mt7927_dma_cleanup(struct mt7927_dev *dev)
{
    mt7927_dma_disable(dev);
    mt7927_ring_free(dev, &dev->ring_wm);
    mt7927_ring_free(dev, &dev->ring_fwdl);
    mt7927_ring_free(dev, &dev->ring_evt);
    mt7927_ring_free(dev, &dev->ring_rx4);
    mt7927_ring_free(dev, &dev->ring_rx5);
    mt7927_ring_free(dev, &dev->ring_rx7);
}

/*
 * L1 remap helpers - access chip addresses in 0x18xxxxxx / 0x7xxxxxxx range
 * via the HIF_REMAP_L1 hardware window at BAR0 0x155024.
 * Data appears at BAR0 0x130000 + (offset within 64K page).
 */
static u32 mt7927_rr_l1(struct mt7927_dev *dev, u32 chip_addr)
{
    u32 base = (chip_addr >> 16) & 0xFFFF;
    u32 offset = chip_addr & 0xFFFF;
    u32 old_l1, val;

    old_l1 = ioread32(dev->bar0 + MT_HIF_REMAP_L1);
    iowrite32(FIELD_PREP(MT_HIF_REMAP_L1_MASK, base),
              dev->bar0 + MT_HIF_REMAP_L1);
    /* readback to ensure write landed */
    ioread32(dev->bar0 + MT_HIF_REMAP_L1);

    val = ioread32(dev->bar0 + MT_HIF_REMAP_BASE_L1 + offset);

    /* restore previous L1 mapping */
    iowrite32(old_l1, dev->bar0 + MT_HIF_REMAP_L1);

    return val;
}

static void mt7927_wr_l1(struct mt7927_dev *dev, u32 chip_addr, u32 val)
{
    u32 base = (chip_addr >> 16) & 0xFFFF;
    u32 offset = chip_addr & 0xFFFF;
    u32 old_l1;

    old_l1 = ioread32(dev->bar0 + MT_HIF_REMAP_L1);
    iowrite32(FIELD_PREP(MT_HIF_REMAP_L1_MASK, base),
              dev->bar0 + MT_HIF_REMAP_L1);
    /* readback to ensure write landed */
    ioread32(dev->bar0 + MT_HIF_REMAP_L1);

    iowrite32(val, dev->bar0 + MT_HIF_REMAP_BASE_L1 + offset);

    /* restore previous L1 mapping */
    iowrite32(old_l1, dev->bar0 + MT_HIF_REMAP_L1);
}

/*
 * MT6639 MCU initialization sequence.
 * Reference: chips/mt6639/mt6639.c :: mt6639_mcu_init() lines 3155-3234
 *
 * Flow:
 *   1. (TODO) set_cbinfra_remap - needs cb_infra_misc0.h addresses
 *   2. (TODO) EFUSE memory repair check - needs top_misc.h addresses
 *   3. WF subsystem reset (assert bit4 -> delay -> de-assert bit4)
 *   4. (TODO) MCU ownership set - needs cb_infra_slp_ctrl.h address
 *   5. Poll ROMCODE_INDEX for MCU_IDLE (0x1D1E)
 *   6. (TODO) MCIF interrupt remap - needs conn_bus_cr_von.h address
 */
static int mt7927_mcu_init_mt6639(struct mt7927_dev *dev)
{
    u32 val;
    int i;

    dev_info(&dev->pdev->dev, "MT6639 MCU init: starting\n");

    /* Step 0: Diagnostic - read ROMCODE_INDEX before any action */
    val = mt7927_rr(dev, MT_ROMCODE_INDEX);
    dev_info(&dev->pdev->dev,
             "MT6639 MCU init: ROMCODE_INDEX before reset = 0x%08x (expect != 0x1D1E)\n",
             val);
    if (val == MT_MCU_IDLE_VALUE && !force_wf_reset) {
        dev_info(&dev->pdev->dev, "MT6639 MCU init: MCU already idle, skipping reset\n");
        return 0;
    }
    if (val == MT_MCU_IDLE_VALUE)
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: MCU already idle but force_wf_reset=1, continuing\n");

    /* Step 1: Force wakeup CONNINFRA (must happen before cbinfra access) */
    dev_info(&dev->pdev->dev, "MT6639 MCU init: forcing CONNINFRA wakeup\n");
    mt7927_wr(dev, MT_WAKEPU_TOP, 0x1);
    usleep_range(1000, 2000);

    /* Step 2: cbinfra PCIe remap (from set_cbinfra_remap)
     * This configures the MCU's PCIe address remap so it can DMA to host memory.
     * Must happen after CONNINFRA wakeup but before WF reset.
     */
    dev_info(&dev->pdev->dev, "MT6639 MCU init: cbinfra PCIe remap\n");
    val = mt7927_rr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF);
    dev_info(&dev->pdev->dev,
             "MT6639 MCU init: PCIE_REMAP_WF before=0x%08x\n", val);
    mt7927_wr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF, MT_PCIE_REMAP_WF_VALUE);
    mt7927_wr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF_BT, MT_PCIE_REMAP_WF_BT_VALUE);
    val = mt7927_rr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF);
    dev_info(&dev->pdev->dev,
             "MT6639 MCU init: PCIE_REMAP_WF after=0x%08x REMAP_WF_BT=0x%08x\n",
             val, mt7927_rr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF_BT));

    /* Step 2b: EMI sleep protection (upstream mt7925/pci.c line 400)
     * chip addr 0x18011100 BIT(1) — enables AXI sleep protection
     * Must be set before WFSYS reset for proper initialization.
     */
    if (use_emi_slpprot) {
        u32 emi_val;

        emi_val = mt7927_rr_l1(dev, MT_HW_EMI_CTL);
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: EMI_CTL before=0x%08x (slpprot=%d)\n",
                 emi_val, !!(emi_val & MT_HW_EMI_CTL_SLPPROT_EN));
        emi_val |= MT_HW_EMI_CTL_SLPPROT_EN;
        mt7927_wr_l1(dev, MT_HW_EMI_CTL, emi_val);
        emi_val = mt7927_rr_l1(dev, MT_HW_EMI_CTL);
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: EMI_CTL after=0x%08x (slpprot=%d)\n",
                 emi_val, !!(emi_val & MT_HW_EMI_CTL_SLPPROT_EN));
    }

    /* Step 3: WF subsystem reset */
    if (use_wfsys_reset) {
        /* Upstream mt792x_wfsys_reset: WFSYS_SW_RST at 0x7c000140 (BAR0 0xf0140)
         * Clear BIT(0) → wait 50ms → set BIT(0) → poll BIT(4) for 500ms
         */
        dev_info(&dev->pdev->dev, "MT6639 MCU init: WFSYS reset (upstream method, 0xf0140)\n");
        val = mt7927_rr(dev, MT_WFSYS_SW_RST);
        dev_info(&dev->pdev->dev, "MT6639 MCU init: WFSYS_SW_RST before=0x%08x\n", val);

        /* Clear RST_B → triggers reset */
        val &= ~WFSYS_SW_RST_B;
        mt7927_wr(dev, MT_WFSYS_SW_RST, val);
        msleep(50);

        /* Set RST_B → release from reset */
        val |= WFSYS_SW_RST_B;
        mt7927_wr(dev, MT_WFSYS_SW_RST, val);

        /* Poll INIT_DONE */
        for (i = 0; i < 500; i++) {
            val = mt7927_rr(dev, MT_WFSYS_SW_RST);
            if (val & WFSYS_SW_INIT_DONE)
                break;
            msleep(1);
        }
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: WFSYS_SW_RST after=%d ms, val=0x%08x (INIT_DONE=%d)\n",
                 i, val, !!(val & WFSYS_SW_INIT_DONE));
        if (!(val & WFSYS_SW_INIT_DONE))
            dev_warn(&dev->pdev->dev, "MT6639 MCU init: WFSYS INIT_DONE timeout!\n");
    } else {
        /* Vendor mt6639_mcu_reset: CB_INFRA_RGU WF_SUBSYS_RST at BAR0 0x1f8600 */
        dev_info(&dev->pdev->dev, "MT6639 MCU init: WF subsystem reset (vendor method)\n");

        /* Assert reset: set bit4 */
        val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
        dev_info(&dev->pdev->dev, "MT6639 MCU init: WF_SUBSYS_RST before = 0x%08x\n", val);
        val |= MT_WF_SUBSYS_RST_BIT;
        mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
        msleep(1);

        /* De-assert reset: clear bit4 */
        val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
        val &= ~MT_WF_SUBSYS_RST_BIT;
        mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
        msleep(5);  /* Allow MCU ROM code to start */
    }

    val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
    dev_info(&dev->pdev->dev, "MT6639 MCU init: WF_SUBSYS_RST after = 0x%08x\n", val);

    /* Step 4: MCU ownership set (from cb_infra_slp_ctrl.h)
     * CB_INFRA_SLP_CTRL_CB_INFRA_CRYPTO_TOP_MCU_OWN_SET_ADDR = 0x70025034
     * bus2chip {0x70020000, 0x1f0000, 0x10000} → BAR0 0x1f5034
     * Write BIT(0) to transfer crypto/MCU ownership to firmware
     */
    {
        u32 mcu_own_before = mt7927_rr(dev, MT_CB_INFRA_MCU_OWN);
        mt7927_wr(dev, MT_CB_INFRA_MCU_OWN_SET, BIT(0));
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: MCU_OWN before=0x%08x, wrote BIT(0) to SET(0x%05x), after=0x%08x\n",
                 mcu_own_before, MT_CB_INFRA_MCU_OWN_SET,
                 mt7927_rr(dev, MT_CB_INFRA_MCU_OWN));
    }

    /* Step 5: Poll ROMCODE_INDEX for MCU_IDLE (0x1D1E) */
    dev_info(&dev->pdev->dev, "MT6639 MCU init: polling for MCU_IDLE (0x%04x)\n",
             MT_MCU_IDLE_VALUE);
    for (i = 0; i < 1000; i++) {
        val = mt7927_rr(dev, MT_ROMCODE_INDEX);
        if (val == MT_MCU_IDLE_VALUE) {
            dev_info(&dev->pdev->dev,
                     "MT6639 MCU init: MCU_IDLE reached after %d ms\n", i);
            goto mcu_ready;
        }
        if (i < 5 || (i % 100) == 0)
            dev_info(&dev->pdev->dev,
                     "MT6639 MCU init: ROMCODE_INDEX[%d] = 0x%08x\n", i, val);
        usleep_range(1000, 2000);
    }
    dev_warn(&dev->pdev->dev,
             "MT6639 MCU init: MCU_IDLE timeout, last ROMCODE_INDEX = 0x%08x\n", val);

    /* Clear CONNINFRA wakeup even on timeout */
    mt7927_wr(dev, MT_WAKEPU_TOP, 0x0);
    return -ETIMEDOUT;

mcu_ready:
    /* Verify cbinfra remap survived WF reset */
    val = mt7927_rr(dev, MT_CB_INFRA_MISC0_PCIE_REMAP_WF);
    dev_info(&dev->pdev->dev,
             "MT6639 MCU init: PCIE_REMAP_WF post-reset=0x%08x\n", val);

    /* Step 6: MCIF interrupt remap - critical for MCU→Host DMA (RX path)
     * From conn_bus_cr_von.h:
     *   CONN_BUS_CR_VON_BASE = chip 0x7C021000 → BAR0 0xd1000
     *   PCIE2AP_REMAP_WF_1_BA = base + 0x34 = BAR0 0xd1034
     * vendor mt6639.c line 3223: writes 0x18051803 to PCIE2AP_REMAP_WF_1_BA
     * Without this, MCU cannot DMA event responses back to host RX rings.
     *
     * Register: configurable via mcif_remap_reg (default BAR0 0xd1034)
     * Value:    configurable via mcif_remap_val (default 0x18051803)
     */
    {
        u32 before, after;

        /* Diagnostic: dump the full CONN_BUS_CR_VON remap set 0 and set 1 */
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: VON remap set0: [+00]=0x%08x [+04]=0x%08x [+08]=0x%08x [+0c]=0x%08x\n",
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x00),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x04),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x08),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x0c));
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: VON remap set1: [+20]=0x%08x [+24]=0x%08x [+28]=0x%08x [+2c]=0x%08x [+30]=0x%08x [+34]=0x%08x\n",
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x20),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x24),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x28),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x2c),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x30),
                 mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x34));

        /* Restore REMAP_WF_0_10 if corrupted (stale from prior test) */
        val = mt7927_rr(dev, MT_CONN_BUS_CR_VON_BASE + 0x00);
        if (val != 0x74031840) {
            dev_warn(&dev->pdev->dev,
                     "MT6639 MCU init: VON [+00]=0x%08x (expected 0x74031840), restoring\n",
                     val);
            mt7927_wr(dev, MT_CONN_BUS_CR_VON_BASE + 0x00, 0x74031840);
        }

        /* Write MCIF remap value to PCIE2AP_REMAP_WF_1_BA */
        before = mt7927_rr(dev, mcif_remap_reg);
        mt7927_wr(dev, mcif_remap_reg, mcif_remap_val);
        after = mt7927_rr(dev, mcif_remap_reg);
        dev_info(&dev->pdev->dev,
                 "MT6639 MCU init: MCIF remap [0x%05x]=0x%08x → wrote 0x%08x → readback 0x%08x\n",
                 mcif_remap_reg, before, mcif_remap_val, after);
    }

    /* Clear CONNINFRA forced wakeup */
    mt7927_wr(dev, MT_WAKEPU_TOP, 0x0);

    dev_info(&dev->pdev->dev, "MT6639 MCU init: complete, MCU is idle\n");
    return 0;
}

static int mt7927_drv_own(struct mt7927_dev *dev)
{
    int i;
    u32 lpctl;

    lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
    dev_info(&dev->pdev->dev, "drv_own: LPCTL before=0x%08x\n", lpctl);

    for (i = 0; i < 10; i++) {
        mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN);
        usleep_range(1000, 2000);

        lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
        if (!(lpctl & PCIE_LPCR_HOST_OWN_SYNC))
            break;
    }

    dev_info(&dev->pdev->dev,
             "drv_own: LPCTL after=0x%08x retries=%d CONN_ON_MISC=0x%08x\n",
             lpctl, i, mt7927_rr(dev, MT_CONN_ON_MISC));

    if (lpctl & PCIE_LPCR_HOST_OWN_SYNC) {
        dev_warn(&dev->pdev->dev, "drv_own not synced, continue for diagnostics\n");
        return -ETIMEDOUT;
    }

    return 0;
}

/*
 * Manual prefetch setup for CONNAC3X.
 * Reference: mt6639WfdmaManualPrefetch() in chips/mt6639/mt6639.c:575-616
 *
 * Must be called AFTER ring init (base/cnt/cidx written), BEFORE DMA enable.
 * The prefetch engine needs sequential base_ptr allocations for each ring.
 */
static void mt7927_wfdma_manual_prefetch(struct mt7927_dev *dev)
{
    u32 val;
    u32 prefetch_val = 0x00000004;  /* depth=4, base_ptr starts at 0 */
    int i;

    /* Step 1: Disable prefetch offset auto-mode (clear chain_en) */
    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val &= ~MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN;
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

    /* Step 2: RX rings 4-7 prefetch (sequential base_ptr allocation)
     * Ring 4: 0x00000004, Ring 5: 0x00400004,
     * Ring 6: 0x00800004, Ring 7: 0x00C00004
     */
    for (i = 4; i <= 7; i++) {
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(i), prefetch_val);
        prefetch_val += 0x00400000;
    }

    /* Step 3: TX rings 0-2 prefetch (continuing sequence) */
    for (i = 0; i <= 2; i++) {
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(i), prefetch_val);
        prefetch_val += 0x00400000;
    }

    /* Step 4: TX rings 15-16 prefetch (continuing sequence) */
    for (i = 15; i <= 16; i++) {
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(i), prefetch_val);
        prefetch_val += 0x00400000;
    }

    /* Step 5: Reset DMA TRX index pointers */
    mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
    mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);

    dev_info(&dev->pdev->dev,
             "wfdma_manual_prefetch: done, GLO_CFG=0x%08x\n",
             mt7927_rr(dev, MT_WPDMA_GLO_CFG));
}

static int mt7927_dma_init(struct mt7927_dev *dev)
{
    u32 val;
    u16 wm_qid;
    u16 inactive_wm_qid;
    int ret;
    u32 didx;

    dev_info(&dev->pdev->dev, "Initializing DMA/rings...\n");

    /* Keep hardware from touching stale rings across unload/reload cycles. */
    mt7927_dma_disable(dev);

    /* Reset both TX and RX DMA index pointers (separate registers) */
    mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xffffffff);
    mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xffffffff);
    wmb();
    msleep(10);

    if (enable_predl_regs) {
        dev_info(&dev->pdev->dev, "predl reg sequence enabled\n");
        mt7927_apply_predl_cfg(dev);
    } else {
        dev_info(&dev->pdev->dev, "predl reg sequence disabled (default)\n");
    }

    wm_qid = (wm_ring_qid == MT_TXQ_MCU_WM_RING_ALT) ?
             MT_TXQ_MCU_WM_RING_ALT : MT_TXQ_MCU_WM_RING;
    inactive_wm_qid = (wm_qid == MT_TXQ_MCU_WM_RING) ?
                      MT_TXQ_MCU_WM_RING_ALT : MT_TXQ_MCU_WM_RING;
    dev_info(&dev->pdev->dev, "WM ring qid=%u\n", wm_qid);

    ret = mt7927_ring_alloc(dev, &dev->ring_wm, wm_qid,
                            MT_TXQ_MCU_WM_RING_SIZE);
    if (ret)
        return ret;

    ret = mt7927_ring_alloc(dev, &dev->ring_fwdl, MT_TXQ_FWDL_RING,
                            MT_TXQ_FWDL_RING_SIZE);
    if (ret)
        return ret;

    ret = mt7927_rx_ring_alloc(dev, &dev->ring_evt, evt_ring_qid,
                               MT_RXQ_MCU_EVENT_RING_SIZE, MT_RX_BUF_SIZE);
    if (ret)
        return ret;

    /* Allocate dummy RX rings 4, 5, 7 so prefetch chain doesn't stall.
     * The vendor driver allocates ALL RX rings (4-7) before enabling WFDMA.
     * Without valid descriptors at rings preceding ring 6 in the prefetch
     * chain (base_ptr order: 4→5→6→7), the engine blocks on invalid BASE=0.
     */
    ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx4, 4, 4, 512);
    if (ret)
        return ret;
    ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx5, 5, 4, 512);
    if (ret)
        return ret;
    ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx7, 7, 4, 512);
    if (ret)
        return ret;

    dev_info(&dev->pdev->dev,
             "ring dma addr: q15=0x%016llx q16=0x%016llx (hi32 q15=0x%08x q16=0x%08x)\n",
             (u64)dev->ring_wm.desc_dma, (u64)dev->ring_fwdl.desc_dma,
             upper_32_bits(dev->ring_wm.desc_dma), upper_32_bits(dev->ring_fwdl.desc_dma));

    /*
     * Windows init reads TX_RING_DIDX and syncs software producer state to
     * the HW-reported index before first kick.
     */
    didx = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(dev->ring_wm.qid)) & 0xfff;
    dev->ring_wm.head = didx % dev->ring_wm.ndesc;
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), dev->ring_wm.head);

    didx = mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(dev->ring_fwdl.qid)) & 0xfff;
    dev->ring_fwdl.head = didx % dev->ring_fwdl.ndesc;
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), dev->ring_fwdl.head);

    /* Prevent stale queue state from previous runs from polluting diagnostics. */
    mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(inactive_wm_qid), 0);
    mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(inactive_wm_qid), 0);
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(inactive_wm_qid), 0);
    mt7927_wr(dev, MT_WPDMA_TX_RING_DIDX(inactive_wm_qid), 0);

    if (minimal_dma_cfg) {
        dev_info(&dev->pdev->dev, "minimal DMA cfg enabled\n");
    } else {
        /*
         * CONNAC3X DMA init sequence (from vendor mt6639WpdmaConfig +
         * mt6639WfdmaManualPrefetch + asicConnac3xWfdmaControl):
         *
         * Phase 1: Manual prefetch (disable chain_en → configure → reset)
         * Phase 2: Set GLO_CFG with CONNAC3X fields + chain_en + DMA enable
         * Phase 3: Enable interrupts
         */

        /* Diagnostic: read ROM bootloader's prefetch EXT_CTRL values */
        dev_info(&dev->pdev->dev,
                 "prefetch ROM: TX15=0x%08x TX16=0x%08x TX17=0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_TX_RING15_EXT_CTRL),
                 mt7927_rr(dev, MT_WFDMA_TX_RING16_EXT_CTRL),
                 mt7927_rr(dev, MT_WFDMA_TX_RING17_EXT_CTRL));
        dev_info(&dev->pdev->dev,
                 "prefetch ROM: RX0=0x%08x RX4=0x%08x RX5=0x%08x RX6=0x%08x RX7=0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(0)),
                 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4)),
                 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(5)),
                 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6)),
                 mt7927_rr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7)));

        /* Vendor mt6639WfdmaManualPrefetch: sequential base_ptr allocation
         * for ALL rings, with non-overlapping regions.
         * Order: RX4-7 → TX16 → TX0-3 → TX15
         * Depths: RX4-6=0x8, RX7=0x4, TX0-1=0x10, others=0x4
         * base_ptr increments: depth_8→+0x0080, depth_4→+0x0040, depth_16→+0x0100
         */
        mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(4),
                         PREFETCH(0x0000, 0x8), "rx4_ext");
        mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(5),
                         PREFETCH(0x0080, 0x8), "rx5_ext");
        mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(6),
                         PREFETCH(0x0100, 0x8), "rx6_ext");
        mt7927_wr_verify(dev, MT_WFDMA_RX_RING_EXT_CTRL(7),
                         PREFETCH(0x0180, 0x4), "rx7_ext");
        mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(16),
                         PREFETCH(0x01C0, 0x4), "tx16_ext");
        mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(0),
                         PREFETCH(0x0200, 0x10), "tx0_ext");
        mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(1),
                         PREFETCH(0x0300, 0x10), "tx1_ext");
        mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(2),
                         PREFETCH(0x0400, 0x4), "tx2_ext");
        mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(3),
                         PREFETCH(0x0440, 0x4), "tx3_ext");
        mt7927_wr_verify(dev, MT_WFDMA_TX_RING_EXT_CTRL(15),
                         PREFETCH(0x0480, 0x4), "tx15_ext");

        /* Reset DMA index pointers */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);

        /* Phase 2: Clear interrupt status, then set GLO_CFG (TWO-PHASE)
         *
         * Vendor sequence: first write GLO_CFG WITHOUT DMA_EN, configure
         * everything else, then add DMA_EN as a second write.
         *
         * MT76_SET provides: BIT(4,5)=BT_SIZE=3, BIT(6)=TX_WB_DDONE,
         *   BIT(11)=AXI_BUFRDY_BYP, BIT(12)=FIFO_LE, BIT(13)=RX_WB_DDONE,
         *   BIT(15)=CHAIN_EN, BIT(21)=OMIT_RX_PFET2, BIT(28)=OMIT_TX,
         *   BIT(30)=CLK_GATE_DIS
         * WIN_OR provides: BIT(0)=TX_EN, BIT(2)=RX_EN, BIT(26)=ADDR_EXT_EN
         * BIT(20): csr_lbk_rx_q_sel_en - vendor sets this unconditionally
         */
        mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_STA, 0xffffffff, "host_int_sta_clr");

        /* Phase 2a: GLO_CFG WITHOUT DMA_EN (configure before enabling) */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val |= MT_WPDMA_GLO_CFG_MT76_SET;
        val |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;     /* BIT(20) - vendor required */
        val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;   /* bypass DMASHDL for FWDL */
        val |= BIT(26);                              /* ADDR_EXT_EN from WIN_OR */
        val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN);
        mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG, val, "glo_cfg_phase1_no_dma_en");

        /* GLO_CFG_EXT0: SDO dispatch mode (vendor mt6639WpdmaConfigExt0)
         * IMPORTANT: vendor only writes this when CFG_SUPPORT_HOST_OFFLOAD=1.
         * Without HOST_OFFLOAD, EXT0 stays at hardware default.
         * Writing 0x28C004DF without HOST_OFFLOAD may disrupt WFDMA routing.
         */
        {
            u32 ext0_default = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT0);
            dev_info(&dev->pdev->dev,
                     "EXT0 default=0x%08x (before any write)\n", ext0_default);
            if (skip_ext0) {
                dev_info(&dev->pdev->dev,
                         "EXT0: SKIPPING write (skip_ext0=1, vendor only writes with HOST_OFFLOAD)\n");
            } else {
                mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG_EXT0,
                                 MT_WPDMA_GLO_CFG_EXT0_VAL, "glo_cfg_ext0");
            }
        }

        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
        val |= MT_WPDMA_GLO_CFG_EXT1_WIN;
        mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG_EXT1, val, "glo_cfg_ext1");

        /* Phase 3: Enable interrupts (matching vendor mt6639ConfigIntMask) */
        mt7927_wr_verify(dev, MT_WFDMA_PRI_DLY_INT_CFG0, 0x0, "pri_dly0");

        val = mt7927_rr(dev, MT_WFDMA_HOST_CONFIG);
        dev_info(&dev->pdev->dev,
                 "HOST_CONFIG default=0x%08x (pcie_int_en=%d, wfdma_int_mode=%d)\n",
                 val, !!(val & BIT(16)), !!(val & BIT(17)));
        mt7927_wr_verify(dev, MT_WFDMA_HOST_CONFIG, val, "host_cfg");
        mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG0, MT_WFDMA_MSI_CFG0_WIN, "msi_cfg0");
        mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG1, MT_WFDMA_MSI_CFG1_WIN, "msi_cfg1");
        mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG2, MT_WFDMA_MSI_CFG2_WIN, "msi_cfg2");
        mt7927_wr_verify(dev, MT_WFDMA_MSI_INT_CFG3, MT_WFDMA_MSI_CFG3_WIN, "msi_cfg3");

        mt7927_wr_verify(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE, "sw_int_ena");
        mt7927_wr_verify(dev, MT_WFDMA_HOST_INT_ENA,
                         HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                         HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                         HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                         HOST_TX_DONE_INT_ENA17, "host_int_ena");

        mt7927_wr_verify(dev, MT_WFDMA_INT_RX_PRI, mt7927_rr(dev, MT_WFDMA_INT_RX_PRI) | 0x0F00,
                         "int_rx_pri");
        mt7927_wr_verify(dev, MT_WFDMA_INT_TX_PRI, mt7927_rr(dev, MT_WFDMA_INT_TX_PRI) | 0x7F00,
                         "int_tx_pri");

        /* RX pause thresholds (vendor mt6639ConfigWfdmaRxRingThreshold)
         * Write threshold=2 for all RX ring pairs (0-1, 2-3, 4-5, 6-7, 8-9, 10-11)
         */
        {
            int th;
            for (th = 0; th < 6; th++)
                mt7927_wr(dev, MT_WPDMA_PAUSE_RX_Q_TH(th), MT_WPDMA_PAUSE_RX_Q_TH_VAL);
            dev_info(&dev->pdev->dev,
                     "RX pause thresholds: TH10=0x%08x TH76=0x%08x\n",
                     mt7927_rr(dev, MT_WPDMA_PAUSE_RX_Q_TH(0)),
                     mt7927_rr(dev, MT_WPDMA_PAUSE_RX_Q_TH(3)));
        }

        /* DMASHDL bypass: disable DMA scheduler quota enforcement.
         *
         * Without this, DMASHDL blocks ALL host TX ring data because no
         * queue-to-group quotas are configured. The host WFDMA consumes
         * descriptors (TX DIDX advances) but data never reaches MCU DMA
         * RX rings (MCU DMA RX DIDX stays 0).
         *
         * The vendor driver initializes DMASHDL with proper quotas in
         * mt6639DmashdlInit(). For firmware download, bypass is simpler.
         * The USB init path also uses this bypass (cmm_asic_connac3x.c:1350).
         *
         * Register: WF_HIF_DMASHDL_TOP SW_CONTROL (BAR0 0xd6004)
         * BIT(28) = DMASHDL_BYPASS_EN
         */
        {
            u32 dmashdl_sw;

            dmashdl_sw = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
            dev_info(&dev->pdev->dev,
                     "DMASHDL: SW_CONTROL before=0x%08x\n", dmashdl_sw);
            dmashdl_sw |= MT_HIF_DMASHDL_BYPASS_EN;
            mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, dmashdl_sw);
            dev_info(&dev->pdev->dev,
                     "DMASHDL: SW_CONTROL after=0x%08x (bypass=%s)\n",
                     mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL),
                     (mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL) & MT_HIF_DMASHDL_BYPASS_EN) ?
                     "ON" : "OFF");

            /* Diagnostic: dump key DMASHDL registers */
            dev_info(&dev->pdev->dev,
                     "DMASHDL: OPT_CTRL=0x%08x PAGE_SET=0x%08x REFILL=0x%08x PKT_MAX=0x%08x\n",
                     mt7927_rr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL),
                     mt7927_rr(dev, MT_HIF_DMASHDL_PAGE_SETTING),
                     mt7927_rr(dev, MT_HIF_DMASHDL_REFILL_CONTROL),
                     mt7927_rr(dev, MT_HIF_DMASHDL_PKT_MAX_SIZE));
            dev_info(&dev->pdev->dev,
                     "DMASHDL: QMAP0=0x%08x QMAP1=0x%08x GRP0=0x%08x GRP1=0x%08x\n",
                     mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0),
                     mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP1),
                     mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(0)),
                     mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(1)));
        }

        /* Phase 2b: NOW enable DMA (TX + RX) after all configuration is done */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
        mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG, val, "glo_cfg_phase2_dma_en");
    }

    /* Signal MCU that WFDMA has been (re)initialized.
     * Upstream mt792x_dma_enable sets MT_WFDMA_NEED_REINIT (BIT(1))
     * in MCU WPDMA0 DUMMY_CR (BAR0 0x02120) after DMA enable.
     * Without this, MCU ROM bootloader may not sync its DMA state
     * and won't set up its TX rings for sending events back to host.
     */
    {
        u32 dummy_cr;

        dummy_cr = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        dev_info(&dev->pdev->dev,
                 "WFDMA_DUMMY_CR before=0x%08x (NEED_REINIT=%d)\n",
                 dummy_cr, !!(dummy_cr & MT_WFDMA_NEED_REINIT));
        dummy_cr |= MT_WFDMA_NEED_REINIT;
        mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, dummy_cr);
        dummy_cr = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        dev_info(&dev->pdev->dev,
                 "WFDMA_DUMMY_CR after=0x%08x (NEED_REINIT=%d)\n",
                 dummy_cr, !!(dummy_cr & MT_WFDMA_NEED_REINIT));
    }

    wmb();
    msleep(10);

    dev_info(&dev->pdev->dev,
             "DMA ready: ring15=0x%llx ring16=0x%llx\n",
             (u64)dev->ring_wm.desc_dma, (u64)dev->ring_fwdl.desc_dma);
    mt7927_dump_win_key_regs(dev, "dma-ready-win-keys");
    mt7927_dump_dma_state(dev, "dma-ready-q15", MT_TXQ_MCU_WM_RING);
    mt7927_dump_dma_state(dev, "dma-ready-q17", MT_TXQ_MCU_WM_RING_ALT);
    mt7927_dump_dma_state(dev, "dma-ready-q16", MT_TXQ_FWDL_RING);
    dev_info(&dev->pdev->dev,
             "dma-ready-rxq%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
             evt_ring_qid,
             mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(evt_ring_qid)));

    /* Dump MCU-side DMA controller registers (BAR0 0x02000-0x03000)
     * bus2chip: {0x54000000, 0x02000, 0x1000} = WFDMA PCIE0 MCU DMA0
     * These show if the MCU ROM bootloader has configured its own DMA engine.
     *
     * Critical check: MCU DMA0 GLO_CFG must have RX_DMA_EN (BIT(2)) set
     * for the MCU DMA engine to receive data from host TX rings via WFDMA.
     */
    {
        int i;
        u32 mcu_glo, mcu_int;

        mcu_glo = ioread32(dev->bar0 + 0x02208);  /* MCU DMA0 GLO_CFG */
        mcu_int = ioread32(dev->bar0 + 0x02200);   /* MCU DMA0 HOST_INT_STA */
        dev_info(&dev->pdev->dev,
                 "MCU_DMA0: GLO_CFG=0x%08x (TX_EN=%d RX_EN=%d TX_BUSY=%d RX_BUSY=%d) INT_STA=0x%08x\n",
                 mcu_glo,
                 !!(mcu_glo & BIT(0)), !!(mcu_glo & BIT(2)),
                 !!(mcu_glo & BIT(1)), !!(mcu_glo & BIT(3)),
                 mcu_int);

        /* Also read MCU DMA0 EXT0 and INT_ENA */
        dev_info(&dev->pdev->dev,
                 "MCU_DMA0: INT_ENA=0x%08x EXT_CTRL=0x%08x CONN_HIF_ON_STATUS=0x%08x\n",
                 ioread32(dev->bar0 + 0x02204),  /* MCU DMA0 INT_ENA */
                 ioread32(dev->bar0 + 0x022b0),  /* MCU DMA0 GLO_CFG_EXT0 */
                 ioread32(dev->bar0 + 0x02138)); /* MCU DMA0 HIF busy */

        /* MCU DMA0 TX rings (MCU→host direction = our RX) */
        for (i = 0; i < 4; i++)
            dev_info(&dev->pdev->dev,
                     "MCU_DMA0 TX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                     i,
                     ioread32(dev->bar0 + 0x02300 + (i << 4)),
                     ioread32(dev->bar0 + 0x02304 + (i << 4)),
                     ioread32(dev->bar0 + 0x02308 + (i << 4)),
                     ioread32(dev->bar0 + 0x0230c + (i << 4)));

        /* MCU DMA0 RX rings (host→MCU direction = our TX) */
        for (i = 0; i < 4; i++)
            dev_info(&dev->pdev->dev,
                     "MCU_DMA0 RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                     i,
                     ioread32(dev->bar0 + 0x02500 + (i << 4)),
                     ioread32(dev->bar0 + 0x02504 + (i << 4)),
                     ioread32(dev->bar0 + 0x02508 + (i << 4)),
                     ioread32(dev->bar0 + 0x0250c + (i << 4)));

        /* MCU DMA1 (BAR0 0x03000) */
        dev_info(&dev->pdev->dev,
                 "MCU_DMA1: GLO_CFG=0x%08x INT_STA=0x%08x\n",
                 ioread32(dev->bar0 + 0x03208),
                 ioread32(dev->bar0 + 0x03200));
        for (i = 0; i < 4; i++)
            dev_info(&dev->pdev->dev,
                     "MCU_DMA1 TX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                     i,
                     ioread32(dev->bar0 + 0x03300 + (i << 4)),
                     ioread32(dev->bar0 + 0x03304 + (i << 4)),
                     ioread32(dev->bar0 + 0x03308 + (i << 4)),
                     ioread32(dev->bar0 + 0x0330c + (i << 4)));

        /* Also check host-side WFDMA RX ring 6 descriptor[0] content */
        if (dev->ring_evt.desc) {
            struct mt76_desc *d = &dev->ring_evt.desc[0];
            dev_info(&dev->pdev->dev,
                     "HOST RXQ6 desc[0]: buf0=0x%08x ctrl=0x%08x buf1=0x%08x info=0x%08x\n",
                     le32_to_cpu(d->buf0), le32_to_cpu(d->ctrl),
                     le32_to_cpu(d->buf1), le32_to_cpu(d->info));
        }

        /* WFSYS and CONN_INFRA state diagnostics */
        dev_info(&dev->pdev->dev,
                 "WFSYS_SW_RST(0xf0140)=0x%08x (RST_B=%d INIT_DONE=%d)\n",
                 mt7927_rr(dev, MT_WFSYS_SW_RST),
                 !!(mt7927_rr(dev, MT_WFSYS_SW_RST) & WFSYS_SW_RST_B),
                 !!(mt7927_rr(dev, MT_WFSYS_SW_RST) & WFSYS_SW_INIT_DONE));
        dev_info(&dev->pdev->dev,
                 "EMI_CTL(L1 0x18011100)=0x%08x (slpprot=%d)\n",
                 mt7927_rr_l1(dev, MT_HW_EMI_CTL),
                 !!(mt7927_rr_l1(dev, MT_HW_EMI_CTL) & MT_HW_EMI_CTL_SLPPROT_EN));
        dev_info(&dev->pdev->dev,
                 "WFDMA_DUMMY_CR(0x02120)=0x%08x (NEED_REINIT=%d)\n",
                 mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR),
                 !!(mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR) & MT_WFDMA_NEED_REINIT));

        /* MCU-side prefetch EXT_CTRL (BAR0 0x02600 + ring*4) */
        dev_info(&dev->pdev->dev,
                 "MCU_DMA0 RX_EXT_CTRL: RX0=0x%08x RX1=0x%08x RX2=0x%08x RX3=0x%08x\n",
                 ioread32(dev->bar0 + 0x02680),
                 ioread32(dev->bar0 + 0x02684),
                 ioread32(dev->bar0 + 0x02688),
                 ioread32(dev->bar0 + 0x0268c));
        dev_info(&dev->pdev->dev,
                 "MCU_DMA0 TX_EXT_CTRL: TX0=0x%08x TX1=0x%08x TX2=0x%08x TX3=0x%08x\n",
                 ioread32(dev->bar0 + 0x02600),
                 ioread32(dev->bar0 + 0x02604),
                 ioread32(dev->bar0 + 0x02608),
                 ioread32(dev->bar0 + 0x0260c));

        /* MCU WFDMA wrap CR (BAR0 0x05000) and PCIE1 MCU DMA0 (BAR0 0x06000) */
        dev_info(&dev->pdev->dev,
                 "MCU_WRAP: GLO(0x05208)=0x%08x INT_STA(0x05200)=0x%08x DUMMY(0x05120)=0x%08x\n",
                 ioread32(dev->bar0 + 0x05208),
                 ioread32(dev->bar0 + 0x05200),
                 ioread32(dev->bar0 + 0x05120));
        dev_info(&dev->pdev->dev,
                 "PCIE1_MCU: GLO(0x06208)=0x%08x INT_STA(0x06200)=0x%08x\n",
                 ioread32(dev->bar0 + 0x06208),
                 ioread32(dev->bar0 + 0x06200));

        /* WFDMA busy/status (host side) */
        dev_info(&dev->pdev->dev,
                 "HOST_WFDMA: BUSY_ENA(0xd413c)=0x%08x HIF_MISC(0xd7044)=0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA0_BASE + 0x013c),
                 mt7927_rr(dev, MT_WFDMA_EXT_CSR_BASE + 0x0044));
    }

    return 0;
}

static int mt7927_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct mt7927_dev *dev;
    int attempt;
    int ret;
    u32 val;

    dev_info(&pdev->dev, "MT7927 WiFi device found\n");

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->pdev = pdev;
    pci_set_drvdata(pdev, dev);

    ret = pci_enable_device(pdev);
    if (ret)
        goto err_free;

    pci_set_master(pdev);

    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if (ret)
        goto err_disable;
    dev_info(&pdev->dev,
             "dma masks: dma_mask=0x%llx coherent_dma_mask=0x%llx\n",
             (u64)*pdev->dev.dma_mask, (u64)pdev->dev.coherent_dma_mask);

    ret = pci_request_regions(pdev, "mt7927");
    if (ret)
        goto err_disable;

    dev->bar0 = pci_iomap(pdev, 0, 0);
    if (!dev->bar0) {
        ret = -ENOMEM;
        goto err_release;
    }
    dev->bar0_len = pci_resource_len(pdev, 0);

    val = ioread32(dev->bar0);
    dev_info(&pdev->dev, "Chip status: 0x%08x\n", val);
    if (val == 0xffffffff) {
        ret = -EIO;
        goto err_unmap;
    }

    /* Upstream mt7925: SET_OWN → CLR_OWN power cycle before any init.
     * This puts MCU to sleep then wakes it, ensuring clean power state.
     */
    {
        u32 lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
        dev_info(&pdev->dev, "fw_pmctrl: LPCTL before SET_OWN=0x%08x\n", lpctl);
        mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_SET_OWN);
        usleep_range(2000, 3000);
        lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
        dev_info(&pdev->dev, "fw_pmctrl: LPCTL after SET_OWN=0x%08x (OWN_SYNC=%d)\n",
                 lpctl, !!(lpctl & PCIE_LPCR_HOST_OWN_SYNC));
    }

    /* MT6639 MCU init: must happen BEFORE drv_own */
    if (use_mt6639_init) {
        ret = mt7927_mcu_init_mt6639(dev);
        if (ret)
            dev_warn(&pdev->dev,
                     "MT6639 MCU init failed: %d, continuing anyway\n", ret);
    }

    ret = mt7927_drv_own(dev);
    if (ret)
        dev_warn(&pdev->dev, "driver-own handshake failed: %d\n", ret);

    ret = -EIO;
    for (attempt = 0; attempt < 2; attempt++) {
        ret = mt7927_dma_init(dev);
        if (ret)
            break;

        ret = mt7927_dma_path_probe(dev);
        if (!ret)
            break;

        dev_warn(&pdev->dev, "dma path probe failed (attempt %d/2): %d\n",
                 attempt + 1, ret);
        if (attempt == 0) {
            dev_warn(&pdev->dev, "retrying DMA init with soft reset\n");
            mt7927_dma_cleanup(dev);
            msleep(20);
        }
    }
    if (ret && strict_dma_probe)
        goto err_unmap;
    if (ret)
        dev_warn(&pdev->dev, "continue despite dma path probe failure: %d\n", ret);

    ret = mt7927_mcu_fw_download(dev);

    dev_info(&pdev->dev, "HOST_INT_STA final: 0x%08x\n",
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

    if (ret)
        dev_warn(&pdev->dev, "Firmware flow incomplete (ret=%d), device kept bound\n", ret);
    else
        dev_info(&pdev->dev, "MT7927 firmware flow done\n");

    return 0;

err_unmap:
    mt7927_dma_cleanup(dev);
    if (dev->bar0)
        pci_iounmap(pdev, dev->bar0);
err_release:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free:
    kfree(dev);
    return ret;
}

static void mt7927_remove(struct pci_dev *pdev)
{
    struct mt7927_dev *dev = pci_get_drvdata(pdev);

    dev_info(&pdev->dev, "Removing MT7927 device\n");

    mt7927_dma_cleanup(dev);

    if (dev->bar0)
        pci_iounmap(pdev, dev->bar0);

    pci_release_regions(pdev);
    pci_disable_device(pdev);
    kfree(dev);
}

static struct pci_device_id mt7927_ids[] = {
    { PCI_DEVICE(MT7927_VENDOR_ID, MT7927_DEVICE_ID) },
    { PCI_DEVICE(MT7927_VENDOR_ID, MT7927_DEVICE_ID_6639) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, mt7927_ids);

static struct pci_driver mt7927_driver = {
    .name = "mt7927_init_dma",
    .id_table = mt7927_ids,
    .probe = mt7927_probe,
    .remove = mt7927_remove,
};

module_pci_driver(mt7927_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT7927 connac2-style MCU firmware download prototype");
MODULE_AUTHOR("MT7927 Linux Driver Project");
MODULE_FIRMWARE("mediatek/mt7925/WIFI_RAM_CODE_MT7925_1_1.bin");
MODULE_FIRMWARE("mediatek/mt7925/WIFI_MT7925_PATCH_MCU_1_1_hdr.bin");
