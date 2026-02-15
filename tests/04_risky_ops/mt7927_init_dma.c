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

/* ToggleWfsysRst registers (Ghidra RE of mtkwecx.sys v5603998)
 * chip 0x7c001600, bus2chip: {0x7c000000, 0xf0000, 0x10000} */
#define MT_CONN_HIF_SLP_PROT                0xf1600   /* Sleep protection enable */
#define MT_CONN_HIF_STATUS1                 0xf1620   /* HIF status 1 */
#define MT_CONN_HIF_STATUS2                 0xf1630   /* HIF status 2 */
/* chip 0x81023f00/0x81023008, bus2chip: {0x81020000, 0xc0000, 0x10000} */
#define MT_PRE_RESET_MCU_REG1               0xc3f00   /* Pre-reset MCU register */
#define MT_PRE_RESET_MCU_REG2               0xc3008   /* Pre-reset MCU register */
/* chip 0x7c060010, bus2chip: {0x7c060000, 0xe0000, 0x10000} */
#define MT_CONN_INFRA_WFSYS_SW_RST          0xe0010   /* CONN_INFRA WFSYS SW reset */
/* chip 0x70028610, bus2chip: {0x70020000, 0x1f0000, 0x10000} */
#define MT_CB_INFRA_RGU_DEBUG               0x1f8610  /* CB_INFRA_RGU debug reg */
/* chip 0x7c026060, bus2chip: {0x7c020000, 0xd0000, 0x10000} */
#define MT_DMASHDL_ENABLE                   0xd6060   /* DMASHDL enable (PostFwDownloadInit) */

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
#define FW_FEATURE_SET_ENCRYPT      BIT(0)
#define FW_FEATURE_SET_KEY_IDX      GENMASK(2, 1)
#define FW_FEATURE_ENCRY_MODE       BIT(4)
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
    /* HOST RX ring 0: MCU event receive (mt7925 uses ring 0 for MCU WM events).
     * FW may need this to be configured before it initializes MCU_RX0.
     */
    struct mt7927_ring ring_rx0;
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

static int mcu_rx_qidx = MT_TX_MCU_PORT_RX_Q0;  /* 0x20 = MCU command port */
module_param(mcu_rx_qidx, int, 0644);
MODULE_PARM_DESC(mcu_rx_qidx, "TXD Q_IDX for MCU commands (0x20=MCU port, matches mt7925+Windows)");

static u32 mcif_remap_reg = 0x000d1034;
static u32 mcif_remap_val = 0x18051803;
module_param(mcif_remap_reg, uint, 0644);
MODULE_PARM_DESC(mcif_remap_reg, "MCIF remap register BAR0 offset (default 0xd1034 = PCIE2AP_REMAP_WF_1_BA)");
module_param(mcif_remap_val, uint, 0644);
MODULE_PARM_DESC(mcif_remap_val, "MCIF remap value (default 0x18051803 from vendor mt6639.c)");

static int reinit_mode;
module_param(reinit_mode, int, 0644);
MODULE_PARM_DESC(reinit_mode, "Experiment mode (40=CB_INFRA_RGU post-FWDL diagnostics + NIC_CAP test, 51=Second CLR_OWN after NEED_REINIT)");

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

    /* Vendor asicConnac3xWfdmaControl(FALSE): clears DMA_EN, chain_en,
     * omit_tx_info, omit_rx_info, omit_rx_info_pfet2.
     * Clearing chain_en is CRITICAL: without it, WFDMA dispatch can use
     * stale prefetch state when we reconfigure rings + prefetch entries.
     */
    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN |
             MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
             MT_GLO_CFG_OMIT_TX_INFO | MT_GLO_CFG_OMIT_RX_INFO_PFET2);
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

    /* Q_IDX in TXD routes through WFDMA dispatch to MCU DMA.
     * Q_IDX=0x20 (MT_TX_MCU_PORT_RX_Q0) = MCU command port.
     * Both upstream mt7925 and Windows MT6639 use Q_IDX=0x20 for MCU cmds.
     * Q_IDX=2 is a raw ring index that does NOT go through MCU dispatch.
     * The WFDMA dispatch for Q_IDX=0x20 routes to MCU DMA RX2/RX3
     * (which the ROM bootloader configures for FWDL commands).
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

    /* Encryption flags - matches upstream mt76_connac_mcu_gen_dl_mode() */
    if (feature_set & FW_FEATURE_SET_ENCRYPT) {
        mode |= DL_MODE_ENCRYPT;
        mode |= DL_MODE_RESET_SEC_IV;
    }
    /* MT7925/MT6639 uses ENCRY_MODE for AES-CBC vs scramble selection */
    if (feature_set & FW_FEATURE_ENCRY_MODE)
        mode |= DL_CONFIG_ENCRY_MODE_SEL;
    mode |= FIELD_PREP(DL_MODE_KEY_IDX,
                       FIELD_GET(FW_FEATURE_SET_KEY_IDX, feature_set));

    return mode;
}

static int mt7927_load_ram(struct mt7927_dev *dev)
{
    const struct mt76_connac2_fw_trailer *tr;
    const struct firmware *fw;
    u32 override = 0, option = 0;
    int ret, i, offset = 0;
    u32 max_len = 4096; /* upstream uses 4096 for PCIe */

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

        dev_info(&dev->pdev->dev,
                 "RAM region %d: addr=0x%08x len=0x%x feature=0x%02x mode=0x%08x\n",
                 i, addr, len, region->feature_set, mode);

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
    mt7927_ring_free(dev, &dev->ring_rx0);
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

    /* HOST RX ring 0: MCU event ring (mt7925 uses 512 entries at ring 0).
     * FW may require this ring to exist before configuring MCU_RX0.
     */
    ret = mt7927_rx_ring_alloc(dev, &dev->ring_rx0, 0, 128, MT_RX_BUF_SIZE);
    if (ret)
        return ret;
    dev_info(&dev->pdev->dev, "HOST RX ring 0 (MCU event): BASE=0x%08x ndesc=%d\n",
             lower_32_bits(dev->ring_rx0.desc_dma), dev->ring_rx0.ndesc);

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
         *
         * CRITICAL: Must disable chain_en BEFORE writing prefetch entries.
         * Vendor asicConnac3xWfdmaControl(FALSE) clears chain_en, omit bits,
         * and DMA_EN. Without this, WFDMA dispatch can silently fail because
         * the prefetch engine uses stale internal state while we rewrite entries.
         */
        {
            u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
            glo &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN |
                      MT_WFDMA_GLO_CFG_RX_DMA_EN |
                      MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
                      MT_GLO_CFG_OMIT_TX_INFO |
                      MT_GLO_CFG_OMIT_RX_INFO_PFET2);
            mt7927_wr_verify(dev, MT_WPDMA_GLO_CFG, glo,
                             "glo_cfg_prefetch_prep");
        }

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

/*
 * Mode 40: Full Windows-equivalent post-FWDL sequence.
 *
 * After FW boots (fw_sync=0x3), this performs:
 *   A. Full 16-step ToggleWfsysRst (CB_INFRA_RGU, from Ghidra RE)
 *   B. Re-download firmware after reset
 *   C. DMASHDL enable (BAR0+0xd6060 |= 0x10101)
 *   D. TXD-format-corrected NIC_CAPABILITY MCU command (Q_IDX=0x20, no LONG_FORMAT)
 *   E. Comprehensive diagnostic register dump
 *
 * Reference: docs/references/ghidra_post_fw_init.md
 */

/*
 * mt7927_mode40_toggle_wfsys_rst — Full 16-step ToggleWfsysRst from Windows driver.
 *
 * Ghidra RE of AsicConnac3xToggleWfsysRst (mtkwecx.sys v5603998 @ 0x1401cb360).
 * Step 1 (CONN_INFRA wakeup at 0x7c011100) is SKIPPED — needs L1 remap and
 * BAR0 offset unknown. Sleep protection clear should suffice.
 *
 * Returns 0 on success, -ETIMEDOUT on poll failure.
 */
static int mt7927_mode40_toggle_wfsys_rst(struct mt7927_dev *dev)
{
    u32 val;
    int i;

    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: === ToggleWfsysRst START (full 16-step) ===\n");

    /* Step 1: SKIP — CONN_INFRA wakeup at 0x7c011100 needs L1 remap */

    /* Step 2a: Clear sleep protection (0x7c001600 AND ~0xF) */
    val = mt7927_rr(dev, MT_CONN_HIF_SLP_PROT);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step2a SLP_PROT before=0x%08x\n", val);
    mt7927_wr(dev, MT_CONN_HIF_SLP_PROT, val & ~0xF);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step2a SLP_PROT after=0x%08x\n",
             mt7927_rr(dev, MT_CONN_HIF_SLP_PROT));

    /* Step 2b: Clear HIF status 1 (0x7c001620, clear low 2 bits if set) */
    val = mt7927_rr(dev, MT_CONN_HIF_STATUS1);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step2b HIF_STATUS1=0x%08x\n", val);
    if (val & 0x3) {
        mt7927_wr(dev, MT_CONN_HIF_STATUS1, val & 0x3);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: step2b HIF_STATUS1 cleared, now=0x%08x\n",
                 mt7927_rr(dev, MT_CONN_HIF_STATUS1));
    }

    /* Step 2c: Clear HIF status 2 (0x7c001630, clear low 2 bits if set) */
    val = mt7927_rr(dev, MT_CONN_HIF_STATUS2);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step2c HIF_STATUS2=0x%08x\n", val);
    if (val & 0x3) {
        mt7927_wr(dev, MT_CONN_HIF_STATUS2, val & 0x3);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: step2c HIF_STATUS2 cleared, now=0x%08x\n",
                 mt7927_rr(dev, MT_CONN_HIF_STATUS2));
    }

    /* Step 3: (driver own check — skipped, we already have ownership) */

    /* Step 4: Pre-reset MCU register 1 (0x81023f00 = 0xc0000100) */
    mt7927_wr(dev, MT_PRE_RESET_MCU_REG1, 0xc0000100);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step4 PRE_RESET_REG1(0x%05x) = 0xc0000100, readback=0x%08x\n",
             MT_PRE_RESET_MCU_REG1,
             mt7927_rr(dev, MT_PRE_RESET_MCU_REG1));

    /* Step 5: Pre-reset MCU register 2 (0x81023008 = 0) */
    mt7927_wr(dev, MT_PRE_RESET_MCU_REG2, 0);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step5 PRE_RESET_REG2(0x%05x) = 0, readback=0x%08x\n",
             MT_PRE_RESET_MCU_REG2,
             mt7927_rr(dev, MT_PRE_RESET_MCU_REG2));

    /* Step 6: Read current CB_INFRA_RGU state */
    val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step6 CB_INFRA_RGU before assert=0x%08x\n", val);

    /* Step 7: (abort check — not applicable in our driver) */

    /* Step 8: ASSERT WFSYS RESET — set BIT(4) */
    val |= MT_WF_SUBSYS_RST_BIT;
    mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step8 ASSERT reset, wrote=0x%08x\n", val);

    /* Step 9: Sleep 1ms */
    msleep(1);

    /* Step 10: Verify BIT(4) still set (up to 5 retries) */
    for (i = 0; i < 5; i++) {
        val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
        if (val & MT_WF_SUBSYS_RST_BIT)
            break;
        dev_warn(&dev->pdev->dev,
                 "[MT7927] MODE40: step10 BIT(4) cleared! retry %d, val=0x%08x\n",
                 i, val);
        val |= MT_WF_SUBSYS_RST_BIT;
        mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
        udelay(100);
    }
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step10 verify BIT(4): val=0x%08x (retries=%d)\n",
             mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST), i);

    /* Step 11: Sleep 20ms — wait for reset completion */
    msleep(20);

    /* Step 12: DEASSERT WFSYS RESET — clear BIT(4) */
    val = mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST);
    val &= ~MT_WF_SUBSYS_RST_BIT;
    mt7927_wr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST, val);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step12 DEASSERT reset, wrote=0x%08x\n", val);

    /* Step 13: Sleep 200us */
    udelay(200);

    /* Step 14: Poll ROMCODE_INDEX == 0x1D1E (500 iterations, 100us each, 50ms timeout) */
    for (i = 0; i < 500; i++) {
        val = mt7927_rr(dev, MT_ROMCODE_INDEX);
        if (val == MT_MCU_IDLE_VALUE) {
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE40: step14 ROMCODE_INDEX=0x%04x (MCU_IDLE) after %d iterations\n",
                     val, i);
            break;
        }
        udelay(100);
    }
    if (val != MT_MCU_IDLE_VALUE) {
        dev_err(&dev->pdev->dev,
                "[MT7927] MODE40: step14 ROMCODE_INDEX TIMEOUT! last=0x%08x (expect 0x1D1E)\n",
                val);
        /* Debug: read CB_INFRA_RGU and debug reg on failure */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: step14 debug: RGU=0x%08x RGU_DBG=0x%08x\n",
                 mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST),
                 mt7927_rr(dev, MT_CB_INFRA_RGU_DEBUG));
        return -ETIMEDOUT;
    }

    /* Step 15: Assert CONN_INFRA reset — write BIT(0) to 0x7c060010 */
    mt7927_wr(dev, MT_CONN_INFRA_WFSYS_SW_RST, BIT(0));
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step15 CONN_INFRA assert BIT(0), reg=0x%08x\n",
             mt7927_rr(dev, MT_CONN_INFRA_WFSYS_SW_RST));

    /* Step 16a: Poll BIT(2) set at 0x7c060010 (up to 50 iterations) */
    for (i = 0; i < 50; i++) {
        val = mt7927_rr(dev, MT_CONN_INFRA_WFSYS_SW_RST);
        if (val & BIT(2)) {
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE40: step16a CONN_INFRA BIT(2) set after %d iterations, val=0x%08x\n",
                     i, val);
            break;
        }
        udelay(100);
    }
    if (!(val & BIT(2)))
        dev_warn(&dev->pdev->dev,
                 "[MT7927] MODE40: step16a CONN_INFRA BIT(2) timeout! val=0x%08x\n", val);

    /* Step 16b: Deassert CONN_INFRA reset — write BIT(1) to 0x7c060010 */
    mt7927_wr(dev, MT_CONN_INFRA_WFSYS_SW_RST, BIT(1));
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: step16b CONN_INFRA deassert BIT(1), reg=0x%08x\n",
             mt7927_rr(dev, MT_CONN_INFRA_WFSYS_SW_RST));

    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: === ToggleWfsysRst COMPLETE ===\n");

    return 0;
}

/*
 * mt7927_mode40_send_nic_cap — Send NIC_CAPABILITY MCU command with
 * Windows-correct TXD format (Q_IDX=0x20, no LONG_FORMAT, legacy 0x40 header).
 *
 * Ghidra RE: PostFwDownloadInit sub2 — class=0x8a, target=0xed, empty payload.
 * TXD[0] = total_len | 0x41000000  (Q_IDX=0x20<<25 | PKT_FMT=2<<23)
 * TXD[1] = 0x4000                  (HDR_FORMAT_V3=1, NO BIT(31) LONG_FORMAT)
 * Header: class at +0x24, type=0xa0 at +0x25, seq at +0x27
 *
 * Returns 0 on success, negative on error.
 */
static int mt7927_mode40_send_nic_cap(struct mt7927_dev *dev)
{
    struct mt76_connac2_mcu_txd *txd;
    dma_addr_t dma;
    void *buf;
    size_t len = sizeof(*txd);  /* 0x40 bytes, no payload for NIC_CAP */
    int ret;

    buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    memset(buf, 0, len);
    txd = buf;

    /* TXD[0]: Windows uses 0x41000000 = (Q_IDX=0x20 << 25) | (PKT_FMT=2 << 23)
     * Q_IDX=0x20 = MT_TX_MCU_PORT_RX_Q0, routes to MCU command RX ring.
     * Total length includes the 0x40 TXD header (no payload for NIC_CAP).
     */
    txd->txd[0] = cpu_to_le32(len | 0x41000000);

    /* TXD[1]: Windows = 0x4000 (HDR_FORMAT_V3=1 only, NO BIT(31) LONG_FORMAT)
     * This is critical — setting BIT(31) causes FW to misparse the command.
     */
    txd->txd[1] = cpu_to_le32(0x4000);

    /* Legacy header at +0x20..+0x3f (inside mt76_connac2_mcu_txd)
     *
     * Ghidra RE of CONNAC3 UniCmd path (FUN_14014866c):
     *   +0x20: (header_size - 0x20 + payload_len) — for 0x40 hdr, 0 payload = 0x20
     *   +0x22: class (e.g., 0x8a) as u16
     *   +0x24: class byte
     *   +0x25: 0xa0 (MCU_PKT_ID)
     *   +0x26: param_5 (extra control, 0 for NIC_CAP)
     *   +0x27: sequence number
     *   +0x29: subcmd (0 for NIC_CAP)
     *   +0x2a: 0
     *   +0x2b: option flags: 2=default | 1=need_ack | 4=need_response
     *
     * NIC_CAP uses flags=5 → need_ack(1) | need_response(4) → option = 2|1|4 = 7
     */
    txd->len = cpu_to_le16(len - sizeof(txd->txd));  /* 0x20: packet len after TXD */
    txd->pq_id = cpu_to_le16(0x008a);  /* class ID as u16 (Ghidra: +0x22 = class) */
    txd->cid = 0x8a;       /* class = NIC_CAPABILITY (target=0xed format) */
    txd->pkt_type = 0xa0;  /* MCU_PKT_ID */
    txd->set_query = 0;    /* param_5 / extra control = 0 for NIC_CAP */
    txd->seq = ++dev->mcu_seq & 0xf;
    if (!txd->seq)
        txd->seq = ++dev->mcu_seq & 0xf;
    txd->uc_d2b0_rev = 0;  /* +0x28: reserved */
    txd->ext_cid = 0;      /* +0x29: subcmd = 0 for NIC_CAP */
    txd->s2d_index = 0;    /* +0x2a: zero */
    txd->ext_cid_ack = 0x07;  /* +0x2b: option = 2(default)|1(need_ack)|4(need_response) */

    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: NIC_CAP cmd: len=%zu TXD[0]=0x%08x TXD[1]=0x%08x cid=0x%02x pq_id=0x%04x seq=%u opt=0x%02x\n",
             len, le32_to_cpu(txd->txd[0]), le32_to_cpu(txd->txd[1]),
             txd->cid, le16_to_cpu(txd->pq_id), txd->seq, txd->ext_cid_ack);
    print_hex_dump(KERN_INFO, "[MT7927] MODE40 nic-cap-txd: ", DUMP_PREFIX_OFFSET,
                   16, 1, buf, len, false);

    ret = mt7927_kick_ring_buf(dev, &dev->ring_wm, dma, len, true);
    mt7927_trace_mcu_event(dev, "mode40-nic-cap");
    if (!ret)
        ret = mt7927_wait_mcu_event(dev, 500);

    dma_free_coherent(&dev->pdev->dev, len, buf, dma);
    return ret;
}

/*
 * mt7927_mode40_post_fwdl — Full post-FWDL sequence modeled on Windows driver.
 *
 * Sequence:
 *   1. Dump pre-reset state (MCU DMA rings, WFSYS status)
 *   2. Full 16-step ToggleWfsysRst (CB_INFRA_RGU)
 *   3. Re-initialize DMA rings (reset wipes them)
 *   4. Re-download firmware (reset puts MCU back to ROM)
 *   5. DMASHDL enable (BAR0+0xd6060 |= 0x10101) — Windows PostFwDownloadInit step 1
 *   6. Diagnostic dump (MCU_RX0 BASE is the key metric)
 *   7. NIC_CAPABILITY MCU command with corrected TXD format
 */
static void mt7927_mode40_post_fwdl(struct mt7927_dev *dev)
{
    int i, ret;
    u32 val;
    u32 mcu_rx0_base;

    dev_info(&dev->pdev->dev,
             "[MT7927] === MODE 40: POST-FWDL WITH FULL WFSYS RESET ===\n");

    /* ---- Phase 0: Pre-reset state dump ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: PRE-RESET STATE:\n");
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:   ROMCODE_INDEX=0x%08x (expect 0x1D1E)\n",
             mt7927_rr(dev, MT_ROMCODE_INDEX));
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:   fw_sync(CONN_ON_MISC)=0x%08x (expect 0x3)\n",
             mt7927_rr(dev, MT_CONN_ON_MISC));
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:   CB_INFRA_RGU=0x%08x WFSYS_SW_RST=0x%08x\n",
             mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST),
             mt7927_rr(dev, MT_WFSYS_SW_RST));

    /* MCU DMA0 RX rings before reset */
    for (i = 0; i < 4; i++)
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40:   MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                 i,
                 ioread32(dev->bar0 + 0x02500 + (i << 4)),
                 ioread32(dev->bar0 + 0x02504 + (i << 4)),
                 ioread32(dev->bar0 + 0x02508 + (i << 4)),
                 ioread32(dev->bar0 + 0x0250c + (i << 4)));

    /* ---- Phase 1: Full 16-step ToggleWfsysRst ---- */
    ret = mt7927_mode40_toggle_wfsys_rst(dev);
    if (ret) {
        dev_err(&dev->pdev->dev,
                "[MT7927] MODE40: ToggleWfsysRst FAILED (%d), aborting\n", ret);
        goto diag_dump;
    }

    /* ---- Phase 2: Re-initialize DMA rings (reset wipes all HOST ring BASEs) ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: Re-initializing DMA rings after WFSYS reset...\n");

    /* CLR_OWN to wake MCU after reset */
    {
        u32 lpctl;

        /* SET_OWN first (required before CLR_OWN) */
        mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_SET_OWN);
        usleep_range(2000, 3000);
        lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: SET_OWN done, LPCTL=0x%08x (OWN_SYNC=%d)\n",
                 lpctl, !!(lpctl & PCIE_LPCR_HOST_OWN_SYNC));

        /* CLR_OWN */
        for (i = 0; i < 10; i++) {
            mt7927_wr(dev, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN);
            usleep_range(1000, 2000);
            lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
            if (!(lpctl & PCIE_LPCR_HOST_OWN_SYNC))
                break;
        }
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: CLR_OWN done, LPCTL=0x%08x retries=%d\n",
                 lpctl, i);
    }

    /* Reprogram HOST DMA rings — CLR_OWN side-effect zeroes all BASEs.
     *
     * Follow vendor init sequence: disable DMA + chain_en, do logic reset,
     * reset pointers, write rings, write prefetch, configure GLO_CFG,
     * then enable DMA.
     */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: Reprogramming HOST ring BASEs...\n");

    /* Step 1: Disable DMA and clear chain_en (vendor flow) */
    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN |
             MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
             MT_GLO_CFG_OMIT_TX_INFO | MT_GLO_CFG_OMIT_RX_INFO_PFET2);
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
    wmb();
    usleep_range(500, 1000);

    /* Step 2: WFDMA logic reset to clear internal dispatch state */
    {
        u32 rst = mt7927_rr(dev, MT_WFDMA0_RST);
        rst &= ~(MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST);
        mt7927_wr(dev, MT_WFDMA0_RST, rst);
        rst |= MT_WFDMA0_RST_LOGIC_RST | MT_WFDMA0_RST_DMASHDL_RST;
        mt7927_wr(dev, MT_WFDMA0_RST, rst);
        usleep_range(100, 200);
    }

    /* Step 3: Reset DMA index pointers */
    mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
    mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
    wmb();

    /* Step 4: Write ring BASEs */
    /* TX ring WM (q15) */
    mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_wm.qid),
              lower_32_bits(dev->ring_wm.desc_dma));
    mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_wm.qid), dev->ring_wm.ndesc);
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), 0);
    dev->ring_wm.head = 0;

    /* TX ring FWDL (q16) */
    mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_fwdl.qid),
              lower_32_bits(dev->ring_fwdl.desc_dma));
    mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_fwdl.qid), dev->ring_fwdl.ndesc);
    mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), 0);
    dev->ring_fwdl.head = 0;

    /* RX ring event */
    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(dev->ring_evt.qid),
              lower_32_bits(dev->ring_evt.desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(dev->ring_evt.qid), dev->ring_evt.ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(dev->ring_evt.qid), 0);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(dev->ring_evt.qid), dev->ring_evt.head);
    dev->ring_evt.tail = 0;

    /* HOST RX ring 0 (MCU event) */
    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(0), lower_32_bits(dev->ring_rx0.desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(0), dev->ring_rx0.ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(0), 0);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(0), dev->ring_rx0.head);

    /* Dummy RX rings 4, 5, 7 */
    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(4), lower_32_bits(dev->ring_rx4.desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(4), dev->ring_rx4.ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(4), dev->ring_rx4.head);
    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(5), lower_32_bits(dev->ring_rx5.desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(5), dev->ring_rx5.ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(5), dev->ring_rx5.head);
    mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(7), lower_32_bits(dev->ring_rx7.desc_dma));
    mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(7), dev->ring_rx7.ndesc);
    mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(7), dev->ring_rx7.head);

    /* Step 5: Write prefetch entries (chain_en is already cleared) */
    mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
    mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(5), PREFETCH(0x0080, 0x8));
    mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0100, 0x8));
    mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0180, 0x4));
    mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x01C0, 0x4));
    mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0200, 0x4));

    /* Step 6: Reset DMA pointers again after prefetch */
    mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
    mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);

    /* Step 7: Clear interrupt status */
    mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, 0xFFFFFFFF);

    /* Step 8: GLO_CFG phase 1 — configure everything, no DMA_EN yet */
    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val |= MT_WPDMA_GLO_CFG_MT76_SET;
    val |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;
    val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
    val |= BIT(26);  /* ADDR_EXT_EN */
    val &= ~(MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN);
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

    /* Step 9: Enable interrupts */
    mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA,
              HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
              HOST_RX_DONE_INT_ENA(evt_ring_qid) |
              HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
              HOST_TX_DONE_INT_ENA17);
    mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE);

    /* Step 10: DMASHDL bypass for FWDL */
    val = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
    val |= MT_HIF_DMASHDL_BYPASS_EN;
    mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, val);

    /* Step 11: GLO_CFG phase 2 — enable DMA */
    val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
    val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
    mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

    /* Step 12: Signal NEED_REINIT */
    val = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
    val |= MT_WFDMA_NEED_REINIT;
    mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, val);
    wmb();
    msleep(10);

    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: DMA re-init done, GLO_CFG=0x%08x\n",
             mt7927_rr(dev, MT_WPDMA_GLO_CFG));

    /* ---- Phase 3: Re-download firmware ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: Re-downloading firmware after WFSYS reset...\n");
    ret = mt7927_mcu_fw_download(dev);
    if (ret) {
        dev_err(&dev->pdev->dev,
                "[MT7927] MODE40: FW re-download FAILED (%d)\n", ret);
        goto diag_dump;
    }
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: FW re-download SUCCESS, fw_sync=0x%08x\n",
             mt7927_rr(dev, MT_CONN_ON_MISC));

    /* ---- Phase 3.5: Clear FWDL bypass residuals (Experiment A) ---- */
    {
        u32 glo, dma_sw;

        /* Clear GLO_CFG BIT(9) (FW_DWLD_BYPASS_DMASHDL) */
        glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: GLO_CFG before bypass clear: 0x%08x\n", glo);
        glo &= ~MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;  /* Clear BIT(9) */
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: GLO_CFG after bypass clear: 0x%08x\n",
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG));

        /* Clear DMASHDL_SW_CONTROL BIT(28) (BYPASS) */
        dma_sw = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: DMASHDL_SW_CONTROL before: 0x%08x\n", dma_sw);
        dma_sw &= ~MT_HIF_DMASHDL_BYPASS_EN;  /* Clear BIT(28) */
        mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, dma_sw);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: DMASHDL_SW_CONTROL after: 0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL));
    }

    /* ---- Phase 3.6: GLO_CFG_EXT1 BIT(28) TX_FCTRL_MODE (Experiment B) ---- */
    {
        u32 ext1;

        ext1 = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: GLO_CFG_EXT1 before: 0x%08x\n", ext1);
        ext1 |= MT_WPDMA_GLO_CFG_EXT1_WIN;  /* BIT(28) TX_FCTRL_MODE */
        mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, ext1);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: GLO_CFG_EXT1 after: 0x%08x\n",
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1));
    }

    /* ---- Phase 4: DMASHDL enable (PostFwDownloadInit step 1) ---- */
    if (reinit_mode == 50) {
        /* Mode 50: Full vendor DMASHDL configuration sequence */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: === FULL VENDOR DMASHDL CONFIGURATION ===\n");

        /* Step 1: Clear DMASHDL bypass in GLO_CFG (if still set) */
        {
            u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
            if (glo & MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL) {
                glo &= ~MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
                mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo);
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE50: Cleared GLO_CFG BIT(9) bypass: 0x%08x\n",
                         mt7927_rr(dev, MT_WPDMA_GLO_CFG));
            }
        }

        /* Step 2: Packet max page size (0xd601c) */
        /* PLE_MAX_PAGE=0x1, PSE_MAX_PAGE=0x18 */
        mt7927_wr(dev, MT_HIF_DMASHDL_PKT_MAX_SIZE, (0x1 << 0) | (0x18 << 16));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: PKT_MAX_SIZE(0xd601c)=0x%08x (PLE=0x1 PSE=0x18)\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_PKT_MAX_SIZE));

        /* Step 3: Group refill enable (0xd6010) */
        /* Enable groups 0/1/2, disable 3-15 */
        /* BIT(16+i) = DISABLE refill for group i */
        /* Disable 3-15: 0xFFF80000 (BIT(19)..BIT(31)) */
        mt7927_wr(dev, MT_HIF_DMASHDL_REFILL_CONTROL, 0xFFF80000);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: REFILL_CONTROL(0xd6010)=0x%08x (G0/1/2=en G3-15=dis)\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_REFILL_CONTROL));

        /* Step 4: Group quota (0xd6020 + group*4) */
        /* Format: [27:16]=MAX_QUOTA [11:0]=MIN_QUOTA */
        /* Group 0/1/2: max=0xfff, min=0x10 */
        mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(0), (0xfff << 16) | 0x10);
        mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(1), (0xfff << 16) | 0x10);
        mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(2), (0xfff << 16) | 0x10);
        /* Groups 3-14: max=0, min=0 */
        for (i = 3; i < 15; i++)
            mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i), 0);
        /* Group 15: max=0x30, min=0 */
        mt7927_wr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(15), (0x30 << 16) | 0);

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: GROUP quota: G0/1/2=0x%08x G15=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(0)),
                 mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(15)));

        /* Step 5: Queue -> Group mapping (0xd6060 + reg*4, 8 queues per reg) */
        /* PCIe variant from vendor:
         * Q0->G0, Q1->G0, Q2->G0, Q3->G2, Q4->G1, Q5->G1, Q6->G1, Q7->G2
         * Format: each queue is 4 bits, 8 queues per register (32 bits)
         * QUEUE_MAPPING0 bits: Q0[3:0] Q1[7:4] Q2[11:8] Q3[15:12] Q4[19:16] Q5[23:20] Q6[27:24] Q7[31:28]
         * = 0x21110000
         */
        mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP0, 0x21110000);  /* Q0-7 */
        mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP1, 0x00000000);  /* Q8-15 -> G0 */
        mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP2, 0x00000000);  /* Q16-23 -> G0 */
        mt7927_wr(dev, MT_HIF_DMASHDL_QUEUE_MAP3, 0x00000000);  /* Q24-31 -> G0 */

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: QUEUE_MAP: 0=0x%08x 1=0x%08x 2=0x%08x 3=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0),
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP1),
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP2),
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP3));

        /* Step 6: Priority -> Group mapping (0xd6070 + reg*4) */
        /* PCIe: direct mapping (priority N -> group N) */
        /* SCHED_SET0: P0-7 -> G0-7 = 0x76543210 */
        /* SCHED_SET1: P8-15 -> G8-15 = 0xfedcba98 */
        mt7927_wr(dev, MT_HIF_DMASHDL_SCHED_SET0, 0x76543210);
        mt7927_wr(dev, MT_HIF_DMASHDL_SCHED_SET1, 0xfedcba98);

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: SCHED_SET: 0=0x%08x 1=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_SCHED_SET0),
                 mt7927_rr(dev, MT_HIF_DMASHDL_SCHED_SET1));

        /* Step 7: Slot arbiter disable */
        /* Read SW_CONTROL, clear BIT(0) (SLOT_ARBITER_EN) */
        {
            u32 sw_ctrl = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
            sw_ctrl &= ~BIT(0);  /* Disable slot arbiter */
            mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, sw_ctrl);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE50: SW_CONTROL(0xd6004)=0x%08x (arbiter disabled)\n",
                     mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL));
        }

        /* Step 8: Optional control (0xd6008) */
        /* HIF_ACK_CNT_TH=4 (bits 23:16), HIF_GUP_ACT_MAP=0x8007 (bits 15:0) */
        mt7927_wr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL, (0x4 << 16) | 0x8007);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: OPT_CTRL(0xd6008)=0x%08x (ACK_TH=4 GUP_ACT=0x8007)\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL));

        /* Step 9: Diagnostic dump — read back all DMASHDL registers */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: === DMASHDL VERIFICATION ===\n");
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50:   PKT_MAX=0x%08x REFILL=0x%08x PAGE_SET=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_PKT_MAX_SIZE),
                 mt7927_rr(dev, MT_HIF_DMASHDL_REFILL_CONTROL),
                 mt7927_rr(dev, MT_HIF_DMASHDL_PAGE_SETTING));
        for (i = 0; i < 16; i += 4)
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE50:   GROUP%d-%d: 0x%08x 0x%08x 0x%08x 0x%08x\n",
                     i, i+3,
                     mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i)),
                     mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i+1)),
                     mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i+2)),
                     mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i+3)));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50:   STATUS_RD=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_STATUS_RD));

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE50: === DMASHDL INIT COMPLETE ===\n");
    } else {
        /* Original Mode 40-49: simple 0xd6060 |= 0x10101 */
        u32 dmashdl_before, dmashdl_after;

        dmashdl_before = mt7927_rr(dev, MT_DMASHDL_ENABLE);
        mt7927_wr(dev, MT_DMASHDL_ENABLE, dmashdl_before | 0x10101);
        dmashdl_after = mt7927_rr(dev, MT_DMASHDL_ENABLE);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: DMASHDL(0x%05x): 0x%08x -> 0x%08x (wrote |= 0x10101)\n",
                 MT_DMASHDL_ENABLE, dmashdl_before, dmashdl_after);
    }

diag_dump:
    /* ---- Phase 5: Diagnostic register dump ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: === DIAGNOSTIC DUMP ===\n");

    /* Key metric: MCU_RX0 BASE */
    mcu_rx0_base = ioread32(dev->bar0 + 0x02500);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40: *** MCU_RX0 BASE=0x%08x *** (%s)\n",
             mcu_rx0_base,
             mcu_rx0_base ? "FW configured MCU command ring!" : "STILL ZERO - blocker");
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     MCU_RX1 BASE=0x%08x\n",
             ioread32(dev->bar0 + 0x02510));

    /* DMASHDL enable register */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     DMASHDL_ENABLE(0x%05x)=0x%08x\n",
             MT_DMASHDL_ENABLE, mt7927_rr(dev, MT_DMASHDL_ENABLE));

    /* ROMCODE_INDEX */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     ROMCODE_INDEX(0x%05x)=0x%08x (expect 0x1D1E)\n",
             MT_ROMCODE_INDEX, mt7927_rr(dev, MT_ROMCODE_INDEX));

    /* CONN_INFRA_WFSYS_SW_RST */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     CONN_INFRA_WFSYS_SW_RST(0x%05x)=0x%08x\n",
             MT_CONN_INFRA_WFSYS_SW_RST, mt7927_rr(dev, MT_CONN_INFRA_WFSYS_SW_RST));

    /* fw_sync */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     fw_sync(CONN_ON_MISC)=0x%08x\n",
             mt7927_rr(dev, MT_CONN_ON_MISC));

    /* MCU DMA0 control */
    {
        u32 mcu_glo = ioread32(dev->bar0 + 0x02208);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40:     MCU_DMA0 GLO_CFG=0x%08x (TX_EN=%d RX_EN=%d)\n",
                 mcu_glo, !!(mcu_glo & BIT(0)), !!(mcu_glo & BIT(2)));
    }

    /* All MCU DMA0 RX rings */
    for (i = 0; i < 4; i++)
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40:     MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                 i,
                 ioread32(dev->bar0 + 0x02500 + (i << 4)),
                 ioread32(dev->bar0 + 0x02504 + (i << 4)),
                 ioread32(dev->bar0 + 0x02508 + (i << 4)),
                 ioread32(dev->bar0 + 0x0250c + (i << 4)));

    /* All MCU DMA0 TX rings */
    for (i = 0; i < 4; i++)
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40:     MCU_TX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                 i,
                 ioread32(dev->bar0 + 0x02300 + (i << 4)),
                 ioread32(dev->bar0 + 0x02304 + (i << 4)),
                 ioread32(dev->bar0 + 0x02308 + (i << 4)),
                 ioread32(dev->bar0 + 0x0230c + (i << 4)));

    /* CB_INFRA_RGU and debug reg */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     CB_INFRA_RGU=0x%08x RGU_DBG=0x%08x\n",
             mt7927_rr(dev, MT_CB_INFRA_RGU_WF_SUBSYS_RST),
             mt7927_rr(dev, MT_CB_INFRA_RGU_DEBUG));

    /* HOST-side WFDMA state */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     HOST GLO_CFG=0x%08x INT_ENA=0x%08x INT_STA=0x%08x\n",
             mt7927_rr(dev, MT_WPDMA_GLO_CFG),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

    /* Host RX ring 6 state */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE40:     HOST RXQ%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
             evt_ring_qid,
             mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(evt_ring_qid)));

    /* ---- Phase 6: NIC_CAPABILITY with corrected TXD ---- */
    if (ret) {
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: Skipping NIC_CAPABILITY (earlier step failed)\n");
    } else if (mcu_rx0_base == 0) {
        dev_warn(&dev->pdev->dev,
                 "[MT7927] MODE40: MCU_RX0 BASE still 0 — attempting NIC_CAP anyway with Q_IDX=0x20\n");
        ret = mt7927_mode40_send_nic_cap(dev);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: NIC_CAPABILITY (Q_IDX=0x20) result=%d\n", ret);
    } else {
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: MCU_RX0 configured! Sending NIC_CAPABILITY (Q_IDX=0x20)...\n");
        ret = mt7927_mode40_send_nic_cap(dev);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE40: NIC_CAPABILITY result=%d\n", ret);
    }

    dev_info(&dev->pdev->dev,
             "[MT7927] === MODE 40: COMPLETE ===\n");
}

/*
 * mt7927_mode46_send_nic_cap_fwdl — Send NIC_CAP via FWDL path (Q_IDX=2).
 *
 * Q_IDX=2 routes to MCU_RX2 which has valid BASE from ROM bootloader.
 * This tests if WFDMA will consume the descriptor when MCU_RX has a valid BASE.
 * Uses ring_fwdl (TX ring 16) instead of ring_wm (TX ring 15).
 */
static int mt7927_mode46_send_nic_cap_fwdl(struct mt7927_dev *dev)
{
    struct mt76_connac2_mcu_txd *txd;
    dma_addr_t dma;
    void *buf;
    size_t len = sizeof(*txd);
    int ret;

    buf = dma_alloc_coherent(&dev->pdev->dev, len, &dma, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    memset(buf, 0, len);
    txd = buf;

    /* TXD[0]: Q_IDX=2 (bits 31-25 = 0b0000010 = 0x04000000), PKT_FMT=2 (0x01000000)
     * Combined: 0x05000000
     */
    txd->txd[0] = cpu_to_le32(len | 0x05000000);

    /* TXD[1]: same as Windows — HDR_FORMAT_V3=1, no LONG_FORMAT */
    txd->txd[1] = cpu_to_le32(0x4000);

    /* Legacy header — same as mode40 NIC_CAP */
    txd->len = cpu_to_le16(len - sizeof(txd->txd));
    txd->pq_id = cpu_to_le16(0x008a);
    txd->cid = 0x8a;
    txd->pkt_type = 0xa0;
    txd->set_query = 0;
    txd->seq = ++dev->mcu_seq & 0xf;
    if (!txd->seq)
        txd->seq = ++dev->mcu_seq & 0xf;
    txd->uc_d2b0_rev = 0;
    txd->ext_cid = 0;
    txd->s2d_index = 0;
    txd->ext_cid_ack = 0x07;

    dev_info(&dev->pdev->dev,
             "[MT7927] MODE46: NIC_CAP via FWDL: len=%zu TXD[0]=0x%08x TXD[1]=0x%08x cid=0x%02x seq=%u\n",
             len, le32_to_cpu(txd->txd[0]), le32_to_cpu(txd->txd[1]),
             txd->cid, txd->seq);
    print_hex_dump(KERN_INFO, "[MT7927] MODE46 nic-cap-txd: ", DUMP_PREFIX_OFFSET,
                   16, 1, buf, len, false);

    /* Send via FWDL ring (TX ring 16) since Q_IDX=2 goes to MCU_RX2 */
    ret = mt7927_kick_ring_buf(dev, &dev->ring_fwdl, dma, len, true);
    mt7927_trace_mcu_event(dev, "mode46-nic-cap");
    if (!ret)
        ret = mt7927_wait_mcu_event(dev, 500);

    dma_free_coherent(&dev->pdev->dev, len, buf, dma);
    return ret;
}

/*
 * mt7927_mode43_vendor_order — Vendor init order experiment.
 *
 * Key insight: Vendor does CLR_OWN → ring setup → WFSYS_RST → FWDL → MCU cmd.
 * WFSYS reset (CB_INFRA_RGU) does NOT affect WFDMA HOST rings because WFDMA
 * lives in CONN_INFRA, not WFSYS. So HOST rings should survive WFSYS reset.
 *
 * Mode 43: After ToggleWfsysRst, do NOT do CLR_OWN (which wipes HOST rings).
 *   Instead, verify rings survived, re-download FW, clear FWDL bypasses,
 *   then send NIC_CAP.
 *
 * Mode 44: Same as 43 but also clear INT_STA and kick MCU via HOST2MCU_SW_INT_SET
 *   before NIC_CAP.
 */
static void mt7927_mode43_vendor_order(struct mt7927_dev *dev, int mode)
{
    int i, ret;
    u32 val;
    u32 mcu_rx0_base;
    bool rings_ok = true;

    dev_info(&dev->pdev->dev,
             "[MT7927] === MODE %d: VENDOR INIT ORDER (skip CLR_OWN after WFSYS reset) ===\n",
             mode);

    /* ---- Phase 0: Pre-reset state dump ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: PRE-RESET STATE:\n", mode);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:   ROMCODE_INDEX=0x%08x fw_sync=0x%08x\n",
             mode,
             mt7927_rr(dev, MT_ROMCODE_INDEX),
             mt7927_rr(dev, MT_CONN_ON_MISC));
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:   GLO_CFG=0x%08x INT_STA=0x%08x INT_ENA=0x%08x\n",
             mode,
             mt7927_rr(dev, MT_WPDMA_GLO_CFG),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA));

    /* Save HOST ring BASEs before reset for comparison */
    {
        u32 tx15_base = mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(15));
        u32 tx16_base = mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(16));
        u32 rx6_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid));

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:   PRE-RESET HOST rings: TX15_BASE=0x%08x TX16_BASE=0x%08x RX%d_BASE=0x%08x\n",
                 mode, tx15_base, tx16_base, evt_ring_qid, rx6_base);
    }

    /* MCU DMA0 RX rings before reset */
    for (i = 0; i < 4; i++)
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:   MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                 mode, i,
                 ioread32(dev->bar0 + 0x02500 + (i << 4)),
                 ioread32(dev->bar0 + 0x02504 + (i << 4)),
                 ioread32(dev->bar0 + 0x02508 + (i << 4)),
                 ioread32(dev->bar0 + 0x0250c + (i << 4)));

    /* ---- Phase 1: Full 16-step ToggleWfsysRst (NO CLR_OWN after!) ---- */
    ret = mt7927_mode40_toggle_wfsys_rst(dev);
    if (ret) {
        dev_err(&dev->pdev->dev,
                "[MT7927] MODE%d: ToggleWfsysRst FAILED (%d), aborting\n", mode, ret);
        goto diag_dump;
    }

    /* ---- Phase 2: Verify HOST rings survived WFSYS reset ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: Verifying HOST rings survived WFSYS reset (NO CLR_OWN)...\n",
             mode);
    {
        u32 tx15_base = mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(15));
        u32 tx16_base = mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(16));
        u32 rx6_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid));
        u32 rx4_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(4));
        u32 rx5_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(5));
        u32 rx7_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(7));
        u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        u32 int_ena = mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA);

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:   POST-RESET HOST rings: TX15=0x%08x TX16=0x%08x RX%d=0x%08x\n",
                 mode, tx15_base, tx16_base, evt_ring_qid, rx6_base);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:   POST-RESET HOST rings: RX4=0x%08x RX5=0x%08x RX7=0x%08x\n",
                 mode, rx4_base, rx5_base, rx7_base);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:   POST-RESET: GLO_CFG=0x%08x INT_ENA=0x%08x\n",
                 mode, glo, int_ena);

        /* Check if rings were wiped */
        if (tx15_base == 0 || tx16_base == 0 || rx6_base == 0) {
            dev_warn(&dev->pdev->dev,
                     "[MT7927] MODE%d: WARNING: HOST ring BASEs zeroed by WFSYS reset! Will reprogram.\n",
                     mode);
            rings_ok = false;
        } else {
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE%d: HOST rings survived WFSYS reset! No reprogram needed.\n",
                     mode);
        }
    }

    /* ---- Phase 2b: If rings were wiped, reprogram them ---- */
    if (!rings_ok) {
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: Reprogramming HOST rings (WFSYS reset DID wipe them)...\n",
                 mode);

        /* Reset DMA pointers */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        /* TX ring WM (q15) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_wm.qid),
                  lower_32_bits(dev->ring_wm.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_wm.qid), dev->ring_wm.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), 0);
        dev->ring_wm.head = 0;

        /* TX ring FWDL (q16) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_fwdl.qid),
                  lower_32_bits(dev->ring_fwdl.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_fwdl.qid), dev->ring_fwdl.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), 0);
        dev->ring_fwdl.head = 0;

        /* RX ring event */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(dev->ring_evt.qid),
                  lower_32_bits(dev->ring_evt.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(dev->ring_evt.qid), dev->ring_evt.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(dev->ring_evt.qid), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(dev->ring_evt.qid), dev->ring_evt.head);
        dev->ring_evt.tail = 0;

        /* HOST RX ring 0 (MCU event) */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(0), lower_32_bits(dev->ring_rx0.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(0), dev->ring_rx0.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(0), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(0), dev->ring_rx0.head);

        /* Dummy RX rings 4, 5, 7 */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(4), lower_32_bits(dev->ring_rx4.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(4), dev->ring_rx4.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(4), dev->ring_rx4.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(5), lower_32_bits(dev->ring_rx5.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(5), dev->ring_rx5.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(5), dev->ring_rx5.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(7), lower_32_bits(dev->ring_rx7.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(7), dev->ring_rx7.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(7), dev->ring_rx7.head);

        /* Prefetch entries */
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(5), PREFETCH(0x0080, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0100, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0180, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x01C0, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0200, 0x4));

        /* Reset pointers again after prefetch */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: Ring reprogram done.\n", mode);
    } else {
        /* Rings survived — just reset head/tail pointers for our tracking */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: Resetting software head/tail pointers...\n", mode);
        dev->ring_wm.head = 0;
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), 0);
        dev->ring_fwdl.head = 0;
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), 0);
        dev->ring_evt.tail = 0;
    }

    /* ---- Phase 2c: Ensure DMA is properly configured for FWDL ---- */
    {
        u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);

        /* Make sure TX/RX DMA are enabled with FWDL bypass */
        glo |= MT_WPDMA_GLO_CFG_MT76_SET;
        glo |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;
        glo |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
        glo |= BIT(26);  /* ADDR_EXT_EN */
        glo |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo);

        /* Enable DMASHDL bypass for FWDL */
        val = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
        val |= MT_HIF_DMASHDL_BYPASS_EN;
        mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, val);

        /* Enable interrupts */
        mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA,
                  HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                  HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                  HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                  HOST_TX_DONE_INT_ENA17);
        mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE);

        /* Signal NEED_REINIT */
        val = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        val |= MT_WFDMA_NEED_REINIT;
        mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, val);
        wmb();
        msleep(10);

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: DMA config for FWDL: GLO_CFG=0x%08x DMASHDL_SW=0x%08x\n",
                 mode,
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG),
                 mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL));
    }

    /* ---- Phase 2d: Mode 51 - Second CLR_OWN after NEED_REINIT ---- */
    if (mode == 51) {
        u32 dummy, lpctl;

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51: === SECOND CLR_OWN AFTER NEED_REINIT ===\n");

        /* Step 1: Record current NEED_REINIT and MCU_RX0 status */
        dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51: BEFORE second CLR_OWN:\n");
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   DUMMY_CR = 0x%08x (NEED_REINIT=%d)\n",
                 dummy, !!(dummy & BIT(1)));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX0 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02500));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX1 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02510));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX2 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02540));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX3 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02550));

        /* Step 2: Ensure NEED_REINIT=1 (should already be set from Phase 2c) */
        if (!(dummy & BIT(1))) {
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE51:   NEED_REINIT not set, setting it now...\n");
            mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, dummy | BIT(1));
            wmb();
            msleep(10);
            dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE51:   DUMMY_CR after set = 0x%08x\n", dummy);
        }

        /* Step 3: Do second SET_OWN/CLR_OWN cycle */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51: Starting second SET_OWN/CLR_OWN cycle...\n");

        /* SET_OWN: BIT(0) -> LPCTL (0xd4010) */
        mt7927_wr(dev, MT_CONN_ON_LPCTL, BIT(0));
        wmb();

        /* Wait for OWN_SYNC (BIT(2)) to be set */
        for (i = 0; i < 2000; i++) {
            lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
            if (lpctl & BIT(2))
                break;
            udelay(100);
        }
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   SET_OWN: OWN_SYNC=%d after %d polls, LPCTL=0x%08x\n",
                 !!(lpctl & BIT(2)), i, lpctl);

        /* CLR_OWN: BIT(1) -> LPCTL */
        mt7927_wr(dev, MT_CONN_ON_LPCTL, BIT(1));
        wmb();

        /* Wait for CLR_OWN complete: OWN_SYNC goes to 0, LPCTL BIT(0) clears */
        for (i = 0; i < 2000; i++) {
            lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
            if (!(lpctl & BIT(0)))
                break;
            udelay(100);
        }
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   CLR_OWN: complete after %d polls, LPCTL=0x%08x\n",
                 i, lpctl);

        /* Step 4: Check if MCU_RX0 is NOW configured */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51: AFTER second CLR_OWN:\n");
        dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   DUMMY_CR = 0x%08x (NEED_REINIT=%d)\n",
                 dummy, !!(dummy & BIT(1)));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX0 BASE = 0x%08x  *** KEY METRIC ***\n",
                 ioread32(dev->bar0 + 0x02500));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX1 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02510));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX2 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02540));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   MCU_RX3 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02550));

        /* Step 5: CLR_OWN wipes HOST rings, reprogram them */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51: Reprogramming HOST rings (CLR_OWN wiped them)...\n");

        /* Reset DMA pointers */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        /* TX ring WM (q15) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_wm.qid),
                  lower_32_bits(dev->ring_wm.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_wm.qid), dev->ring_wm.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), 0);
        dev->ring_wm.head = 0;

        /* TX ring FWDL (q16) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_fwdl.qid),
                  lower_32_bits(dev->ring_fwdl.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_fwdl.qid), dev->ring_fwdl.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), 0);
        dev->ring_fwdl.head = 0;

        /* RX ring event */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(dev->ring_evt.qid),
                  lower_32_bits(dev->ring_evt.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(dev->ring_evt.qid), dev->ring_evt.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(dev->ring_evt.qid), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(dev->ring_evt.qid), dev->ring_evt.head);
        dev->ring_evt.tail = 0;

        /* HOST RX ring 0 (MCU event) */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(0), lower_32_bits(dev->ring_rx0.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(0), dev->ring_rx0.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(0), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(0), dev->ring_rx0.head);

        /* Dummy RX rings 4, 5, 7 */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(4), lower_32_bits(dev->ring_rx4.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(4), dev->ring_rx4.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(4), dev->ring_rx4.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(5), lower_32_bits(dev->ring_rx5.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(5), dev->ring_rx5.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(5), dev->ring_rx5.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(7), lower_32_bits(dev->ring_rx7.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(7), dev->ring_rx7.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(7), dev->ring_rx7.head);

        /* Prefetch entries */
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(5), PREFETCH(0x0080, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0100, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0180, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x01C0, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0200, 0x4));

        /* Reset pointers again after prefetch */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   HOST rings reprogrammed\n");

        /* Step 6: Read HOST GLO_CFG/INT_ENA to see what CLR_OWN changed */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   HOST GLO_CFG = 0x%08x\n",
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51:   HOST INT_ENA = 0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA));

        /* Re-enable DMA for FWDL (CLR_OWN wiped GLO_CFG) */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val |= MT_WPDMA_GLO_CFG_MT76_SET;
        val |= MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN;
        val |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
        val |= BIT(26);  /* ADDR_EXT_EN */
        val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

        /* Re-enable DMASHDL bypass for FWDL */
        val = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
        val |= MT_HIF_DMASHDL_BYPASS_EN;
        mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, val);

        /* Re-enable interrupts */
        mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA,
                  HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                  HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                  HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                  HOST_TX_DONE_INT_ENA17);
        mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE);

        /* Re-signal NEED_REINIT (CLR_OWN may have cleared it) */
        val = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        val |= MT_WFDMA_NEED_REINIT;
        mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, val);
        wmb();
        msleep(10);

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE51: Second CLR_OWN complete, DMA re-configured for FWDL\n");
    }

    /* ---- Phase 3: Re-download firmware ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: Re-downloading firmware after WFSYS reset...\n", mode);
    ret = mt7927_mcu_fw_download(dev);
    if (ret) {
        dev_err(&dev->pdev->dev,
                "[MT7927] MODE%d: FW re-download FAILED (%d)\n", mode, ret);
        goto diag_dump;
    }
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: FW re-download SUCCESS, fw_sync=0x%08x\n",
             mode, mt7927_rr(dev, MT_CONN_ON_MISC));

    /* ---- Phase 4: Clear FWDL bypasses, enable DMASHDL, set GLO_CFG_EXT1 ---- */
    {
        u32 glo, dma_sw, ext1;

        /* Clear GLO_CFG BIT(9) (FW_DWLD_BYPASS_DMASHDL) */
        glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: GLO_CFG before bypass clear: 0x%08x\n", mode, glo);
        glo &= ~MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo);

        /* Clear DMASHDL_SW_CONTROL BIT(28) (BYPASS) */
        dma_sw = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
        dma_sw &= ~MT_HIF_DMASHDL_BYPASS_EN;
        mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, dma_sw);

        /* Set GLO_CFG_EXT1 BIT(28) (TX_FCTRL_MODE) */
        ext1 = mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1);
        ext1 |= MT_WPDMA_GLO_CFG_EXT1_WIN;
        mt7927_wr(dev, MT_WPDMA_GLO_CFG_EXT1, ext1);

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: After bypass clear: GLO_CFG=0x%08x DMASHDL_SW=0x%08x EXT1=0x%08x\n",
                 mode,
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG),
                 mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL),
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG_EXT1));
    }

    /* ---- Phase 4b: DMASHDL enable (PostFwDownloadInit step 1) ---- */
    {
        u32 dmashdl_before = mt7927_rr(dev, MT_DMASHDL_ENABLE);
        mt7927_wr(dev, MT_DMASHDL_ENABLE, dmashdl_before | 0x10101);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: DMASHDL(0x%05x): 0x%08x -> 0x%08x\n",
                 mode, MT_DMASHDL_ENABLE, dmashdl_before,
                 mt7927_rr(dev, MT_DMASHDL_ENABLE));
    }

    /* ---- Phase 4c: Clear INT_STA (both modes) ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: INT_STA before clear: 0x%08x\n",
             mode, mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));
    mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, 0xFFFFFFFF);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: INT_STA after clear: 0x%08x\n",
             mode, mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

    /* ---- Phase 4d: Mode 44 extra — kick MCU via HOST2MCU_SW_INT_SET ---- */
    if (mode == 44) {
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE44: Kicking MCU via HOST2MCU_SW_INT_SET BIT(0)...\n");
        mt7927_wr(dev, MT_HOST2MCU_SW_INT_SET, BIT(0));
        msleep(10);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE44: MCU_CMD after kick: 0x%08x INT_STA: 0x%08x\n",
                 mt7927_rr(dev, MT_MCU_CMD_REG),
                 mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));
    }

    /* ---- Phase 4e: Poll WFSYS_SW_INIT_DONE ---- */
    {
        u32 sw_rst;

        for (i = 0; i < 50; i++) {
            sw_rst = mt7927_rr(dev, MT_WFSYS_SW_RST);
            if (sw_rst & WFSYS_SW_INIT_DONE) {
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE%d: WFSYS_SW_INIT_DONE set after %d polls, val=0x%08x\n",
                         mode, i, sw_rst);
                break;
            }
            msleep(10);
        }
        if (!(sw_rst & WFSYS_SW_INIT_DONE))
            dev_warn(&dev->pdev->dev,
                     "[MT7927] MODE%d: WFSYS_SW_INIT_DONE still 0 after 500ms, val=0x%08x\n",
                     mode, sw_rst);
    }

    /* ---- Phase 4f: Mode 45 — consume pending RX ring 6 events + DMASHDL dump ---- */
    if (mode == 45) {
        struct mt7927_ring *ring = &dev->ring_evt;
        u32 rx6_cidx, rx6_didx, rx6_base, rx6_cnt;
        u32 dmashdl_status, dmashdl_sw, dmashdl_opt;

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: === DRAIN PENDING RX RING 6 EVENTS ===\n");

        /* Dump RX ring 6 state */
        rx6_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid));
        rx6_cnt  = mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(evt_ring_qid));
        rx6_cidx = mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid));
        rx6_didx = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(evt_ring_qid));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: RX%d BASE=0x%08x CNT=0x%x CIDX=0x%x DIDX=0x%x tail=%u\n",
                 evt_ring_qid, rx6_base, rx6_cnt, rx6_cidx, rx6_didx, ring->tail);

        /* Check for unconsumed events (DIDX != tail means HW wrote data) */
        while (ring->tail != rx6_didx) {
            struct mt76_desc *d = &ring->desc[ring->tail];
            u32 ctrl = le32_to_cpu(d->ctrl);
            u32 *evt_data = ring->buf[ring->tail];

            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE45: Consuming RX%d[%u]: ctrl=0x%08x (DMA_DONE=%d len=%u)\n",
                     evt_ring_qid, ring->tail, ctrl,
                     !!(ctrl & MT_DMA_CTL_DMA_DONE),
                     (ctrl >> 16) & 0x3FFF);

            /* Dump first 64 bytes of event data */
            if (evt_data) {
                print_hex_dump(KERN_INFO,
                               "[MT7927] MODE45 rx-evt: ",
                               DUMP_PREFIX_OFFSET, 16, 1,
                               evt_data, min_t(u32, 64, ring->buf_size), false);
            }

            /* Reset descriptor for reuse */
            d->ctrl = cpu_to_le32(FIELD_PREP(MT_DMA_CTL_SD_LEN0, ring->buf_size));

            /* Advance tail */
            ring->tail = (ring->tail + 1) % ring->ndesc;
        }

        /* Advance CIDX to match new tail (return consumed buffers to HW) */
        if (rx6_cidx != ring->tail) {
            u16 new_cidx = (ring->tail == 0) ? (ring->ndesc - 1) : (ring->tail - 1);
            mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid), new_cidx);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE45: Advanced RX%d CIDX: 0x%x -> 0x%x (tail=%u)\n",
                     evt_ring_qid, rx6_cidx, new_cidx, ring->tail);
        } else {
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE45: No pending events on RX%d (CIDX==tail)\n",
                     evt_ring_qid);
        }

        /* Clear INT_STA */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: INT_STA before clear: 0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));
        mt7927_wr(dev, MT_WFDMA_HOST_INT_STA, 0xFFFFFFFF);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: INT_STA after clear: 0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

        /* DMASHDL status dump for back-pressure info */
        dmashdl_status = mt7927_rr(dev, MT_HIF_DMASHDL_STATUS_RD);
        dmashdl_sw = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
        dmashdl_opt = mt7927_rr(dev, MT_HIF_DMASHDL_OPTIONAL_CONTROL);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: DMASHDL STATUS_RD=0x%08x SW_CONTROL=0x%08x OPT_CTRL=0x%08x\n",
                 dmashdl_status, dmashdl_sw, dmashdl_opt);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: DMASHDL PAGE=0x%08x REFILL=0x%08x PKT_MAX=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_PAGE_SETTING),
                 mt7927_rr(dev, MT_HIF_DMASHDL_REFILL_CONTROL),
                 mt7927_rr(dev, MT_HIF_DMASHDL_PKT_MAX_SIZE));
        /* Group quotas */
        for (i = 0; i < 4; i++)
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE45: DMASHDL GROUP%d=0x%08x\n",
                     i, mt7927_rr(dev, MT_HIF_DMASHDL_GROUP_CONTROL(i)));
        /* Queue maps */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: DMASHDL QMAP0=0x%08x QMAP1=0x%08x QMAP2=0x%08x QMAP3=0x%08x\n",
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP0),
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP1),
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP2),
                 mt7927_rr(dev, MT_HIF_DMASHDL_QUEUE_MAP3));

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE45: === DRAIN COMPLETE ===\n");
    }

diag_dump:
    /* ---- Phase 5: Diagnostic register dump ---- */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: === DIAGNOSTIC DUMP ===\n", mode);

    mcu_rx0_base = ioread32(dev->bar0 + 0x02500);
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d: *** MCU_RX0 BASE=0x%08x *** (%s)\n",
             mode, mcu_rx0_base,
             mcu_rx0_base ? "FW configured MCU command ring!" : "STILL ZERO - blocker");
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:     MCU_RX1 BASE=0x%08x\n",
             mode, ioread32(dev->bar0 + 0x02510));

    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:     ROMCODE_INDEX=0x%08x fw_sync=0x%08x\n",
             mode, mt7927_rr(dev, MT_ROMCODE_INDEX),
             mt7927_rr(dev, MT_CONN_ON_MISC));

    /* MCU DMA0 control */
    {
        u32 mcu_glo = ioread32(dev->bar0 + 0x02208);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:     MCU_DMA0 GLO_CFG=0x%08x (TX_EN=%d RX_EN=%d)\n",
                 mode, mcu_glo, !!(mcu_glo & BIT(0)), !!(mcu_glo & BIT(2)));
    }

    /* All MCU DMA0 RX rings */
    for (i = 0; i < 4; i++)
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d:     MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                 mode, i,
                 ioread32(dev->bar0 + 0x02500 + (i << 4)),
                 ioread32(dev->bar0 + 0x02504 + (i << 4)),
                 ioread32(dev->bar0 + 0x02508 + (i << 4)),
                 ioread32(dev->bar0 + 0x0250c + (i << 4)));

    /* HOST-side WFDMA state */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:     HOST GLO_CFG=0x%08x INT_ENA=0x%08x INT_STA=0x%08x\n",
             mode,
             mt7927_rr(dev, MT_WPDMA_GLO_CFG),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA),
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

    /* Host RX ring 6 state */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:     HOST RXQ%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
             mode, evt_ring_qid,
             mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid)),
             mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(evt_ring_qid)));

    /* Host TX ring 15 state (WM command ring) */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:     HOST TXQ15: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
             mode,
             mt7927_rr(dev, MT_WPDMA_TX_RING_BASE(15)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_CNT(15)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_CIDX(15)),
             mt7927_rr(dev, MT_WPDMA_TX_RING_DIDX(15)));

    /* WFSYS_SW_INIT_DONE */
    dev_info(&dev->pdev->dev,
             "[MT7927] MODE%d:     WFSYS_SW_RST(0x%05x)=0x%08x (INIT_DONE=%d)\n",
             mode, MT_WFSYS_SW_RST, mt7927_rr(dev, MT_WFSYS_SW_RST),
             !!(mt7927_rr(dev, MT_WFSYS_SW_RST) & WFSYS_SW_INIT_DONE));

    /* ---- Phase 6a: Mode 47 — ACK MCU_CMD=0x8000 before NIC_CAP ---- */
    if (mode == 47 && !ret) {
        u32 mcu_cmd, mcu_rx0_after;

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: === ACK MCU_CMD BEFORE NIC_CAP ===\n");

        /* Read MCU_CMD_REG (MCU2HOST_SW_INT_STA at 0xd41f0) */
        mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: MCU_CMD_REG=0x%08x before ACK\n", mcu_cmd);

        /* ACK by writing the value back (W1C — write-1-to-clear) */
        if (mcu_cmd) {
            mt7927_wr(dev, MT_MCU_CMD_REG, mcu_cmd);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE47: Wrote 0x%08x to MCU_CMD_REG (ACK)\n", mcu_cmd);
        }

        /* Also try MCU2HOST_SW_INT_ENA area — read nearby regs */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: MCU2HOST_SW_INT_ENA(0xd41f4)=0x%08x\n",
                 mt7927_rr(dev, MT_MCU2HOST_SW_INT_ENA));
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: HOST2MCU_SW_INT_SET(0xd4108)=0x%08x\n",
                 mt7927_rr(dev, MT_HOST2MCU_SW_INT_SET));

        /* Wait for FW to react */
        msleep(50);

        /* Re-read MCU_CMD — did FW set new bits? */
        mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: MCU_CMD_REG=0x%08x after ACK + 50ms\n", mcu_cmd);

        /* Re-read MCU_RX0 BASE — did FW configure it after ACK? */
        mcu_rx0_after = ioread32(dev->bar0 + 0x02500);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: MCU_RX0 BASE=0x%08x after ACK (%s)\n",
                 mcu_rx0_after,
                 mcu_rx0_after ? "*** FW CONFIGURED MCU_RX0! ***" : "still zero");

        /* If MCU_RX0 still 0, try longer wait */
        if (mcu_rx0_after == 0) {
            msleep(200);
            mcu_rx0_after = ioread32(dev->bar0 + 0x02500);
            mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE47: After 250ms total: MCU_RX0=0x%08x MCU_CMD=0x%08x\n",
                     mcu_rx0_after, mcu_cmd);
        }

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE47: === ACK COMPLETE ===\n");
    }

    /* ---- Phase 6a2: Mode 49 — Read FWDL-phase HOST RX ring 6 events ---- */
    if (mode == 49 && !ret) {
        struct mt7927_ring *ring = &dev->ring_evt;
        u32 rx6_cidx, rx6_didx, rx6_base, rx6_cnt;

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE49: === READ HOST RX RING 6 EVENTS (FWDL phase) ===\n");

        /* Dump RX ring 6 hardware state */
        rx6_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(evt_ring_qid));
        rx6_cnt  = mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(evt_ring_qid));
        rx6_cidx = mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid));
        rx6_didx = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(evt_ring_qid));

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE49: RX%d BASE=0x%08x CNT=0x%x CIDX=0x%x DIDX=0x%x (ring tail=%u)\n",
                 evt_ring_qid, rx6_base, rx6_cnt, rx6_cidx, rx6_didx, ring->tail);

        /* Check for unconsumed events (DIDX > CIDX means HW wrote data) */
        if (rx6_didx != rx6_cidx) {
            u32 pending = (rx6_didx >= rx6_cidx) ?
                          (rx6_didx - rx6_cidx) :
                          (ring->ndesc - rx6_cidx + rx6_didx);

            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE49: Found %u pending event(s) in RX%d (DIDX=0x%x > CIDX=0x%x)\n",
                     pending, evt_ring_qid, rx6_didx, rx6_cidx);

            /* Read descriptors from CIDX to DIDX */
            u16 idx = rx6_cidx;
            u32 count = 0;

            while (idx != rx6_didx && count < pending) {
                struct mt76_desc *d = &ring->desc[idx];
                u32 buf0 = le32_to_cpu(d->buf0);
                u32 ctrl = le32_to_cpu(d->ctrl);
                u32 buf1 = le32_to_cpu(d->buf1);
                u32 info = le32_to_cpu(d->info);
                u32 pkt_len = (ctrl >> 16) & 0x3FFF;
                void *evt_data = ring->buf[idx];

                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE49: === RX%d descriptor[%u] ===\n",
                         evt_ring_qid, idx);
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE49:   buf0=0x%08x ctrl=0x%08x buf1=0x%08x info=0x%08x\n",
                         buf0, ctrl, buf1, info);
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE49:   DMA_DONE=%d pkt_len=%u\n",
                         !!(ctrl & MT_DMA_CTL_DMA_DONE), pkt_len);

                /* Dump event data buffer (first 128 bytes) */
                if (evt_data && pkt_len > 0) {
                    u32 dump_len = min_t(u32, 128, pkt_len);
                    dev_info(&dev->pdev->dev,
                             "[MT7927] MODE49:   Event data (%u bytes, dumping %u):\n",
                             pkt_len, dump_len);
                    print_hex_dump(KERN_INFO,
                                   "[MT7927] MODE49:     ",
                                   DUMP_PREFIX_OFFSET, 16, 1,
                                   evt_data, dump_len, false);
                } else {
                    dev_info(&dev->pdev->dev,
                             "[MT7927] MODE49:   (no event data or len=0)\n");
                }

                /* Advance to next descriptor */
                idx = (idx + 1) % ring->ndesc;
                count++;
            }

            /* Consume events by advancing CIDX to DIDX */
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE49: Consuming %u event(s): advancing CIDX 0x%x -> 0x%x\n",
                     count, rx6_cidx, rx6_didx);

            mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid), rx6_didx);

            /* Update ring tail to match */
            ring->tail = rx6_didx;

            /* Verify update */
            u32 new_cidx = mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(evt_ring_qid));
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE49: RX%d CIDX after update=0x%x (requested 0x%x) tail=%u\n",
                     evt_ring_qid, new_cidx, rx6_didx, ring->tail);
        } else {
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE49: No pending events (CIDX=DIDX=0x%x)\n", rx6_cidx);
        }

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE49: === READ COMPLETE ===\n");
    }

    /* ---- Phase 6b: NIC_CAPABILITY ---- */
    if (ret) {
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: Skipping NIC_CAPABILITY (earlier step failed)\n", mode);
    } else if (mode == 46) {
        /* Mode 46: NIC_CAP via FWDL path (Q_IDX=2, TX ring 16) */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE46: Sending NIC_CAPABILITY via FWDL ring (Q_IDX=2)...\n");

        /* Need DMASHDL bypass for FWDL ring */
        {
            u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
            glo |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
            mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo);
            val = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
            val |= MT_HIF_DMASHDL_BYPASS_EN;
            mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, val);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE46: Re-enabled FWDL bypass for Q_IDX=2: GLO=0x%08x DMASHDL_SW=0x%08x\n",
                     mt7927_rr(dev, MT_WPDMA_GLO_CFG),
                     mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL));
        }

        ret = mt7927_mode46_send_nic_cap_fwdl(dev);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE46: NIC_CAPABILITY (Q_IDX=2) result=%d\n", ret);

        /* Also try standard Q_IDX=0x20 on ring 15 for comparison */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE46: Also sending via ring 15 (Q_IDX=0x20) for comparison...\n");
        {
            int ret2 = mt7927_mode40_send_nic_cap(dev);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE46: NIC_CAP (Q_IDX=0x20) result=%d\n", ret2);
        }
    } else if (mode == 48) {
        /* Mode 48: NIC_CAP via ring 15 (Q_IDX=0x20) with DMASHDL bypass + enhanced diagnostics */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: Sending NIC_CAPABILITY via ring 15 (Q_IDX=0x20) with bypass + diagnostics...\n");

        /* Ensure DMASHDL bypass is enabled (needed for ring 15) */
        {
            u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
            glo |= MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL;
            mt7927_wr(dev, MT_WPDMA_GLO_CFG, glo);
            val = mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL);
            val |= MT_HIF_DMASHDL_BYPASS_EN;
            mt7927_wr(dev, MT_HIF_DMASHDL_SW_CONTROL, val);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48: DMASHDL bypass enabled: GLO=0x%08x DMASHDL_SW=0x%08x\n",
                     mt7927_rr(dev, MT_WPDMA_GLO_CFG),
                     mt7927_rr(dev, MT_HIF_DMASHDL_SW_CONTROL));
        }

        /* Dump baseline state */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: === BASELINE STATE (before NIC_CAP) ===\n");
        {
            int i;
            /* HOST RX rings 0-7 */
            for (i = 0; i < 8; i++) {
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE48:   HOST_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                         i,
                         mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(i)),
                         mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(i)),
                         mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(i)),
                         mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(i)));
            }
            /* MCU RX rings 0-3 (offset 0x540, 0x550, 0x560, 0x570) */
            for (i = 0; i < 4; i++) {
                u32 base_ofs = 0x540 + (i << 4);
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE48:   MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                         i,
                         ioread32(dev->bar0 + 0xd4000 + base_ofs),
                         ioread32(dev->bar0 + 0xd4000 + base_ofs + 4),
                         ioread32(dev->bar0 + 0xd4000 + base_ofs + 8),
                         ioread32(dev->bar0 + 0xd4000 + base_ofs + 12));
            }
        }

        /* Send NIC_CAPABILITY */
        ret = mt7927_mode40_send_nic_cap(dev);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: NIC_CAP (Q_IDX=0x20, ring 15) send result=%d\n", ret);

        /* Enhanced diagnostics after send */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: === ENHANCED DIAGNOSTICS (after send) ===\n");

        /* Kick MCU via HOST2MCU_SW_INT_SET */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: Kicking MCU via HOST2MCU_SW_INT_SET BIT(0)...\n");
        mt7927_wr(dev, MT_HOST2MCU_SW_INT_SET, BIT(0));

        /* Wait for MCU to process */
        msleep(100);

        /* Dump all HOST RX ring states */
        {
            int i;
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48: === HOST RX RING STATE (after 100ms) ===\n");
            for (i = 0; i < 8; i++) {
                u32 didx = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(i));
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE48:   HOST_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x%s\n",
                         i,
                         mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(i)),
                         mt7927_rr(dev, MT_WPDMA_RX_RING_CNT(i)),
                         mt7927_rr(dev, MT_WPDMA_RX_RING_CIDX(i)),
                         didx,
                         (i == 6 || i == 7) ? " <-- MCU EVENT CANDIDATE" : "");
            }
        }

        /* Dump MCU RX ring states */
        {
            int i;
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48: === MCU RX RING STATE (after 100ms) ===\n");
            for (i = 0; i < 4; i++) {
                u32 base_ofs = 0x540 + (i << 4);
                dev_info(&dev->pdev->dev,
                         "[MT7927] MODE48:   MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x%s\n",
                         i,
                         ioread32(dev->bar0 + 0xd4000 + base_ofs),
                         ioread32(dev->bar0 + 0xd4000 + base_ofs + 4),
                         ioread32(dev->bar0 + 0xd4000 + base_ofs + 8),
                         ioread32(dev->bar0 + 0xd4000 + base_ofs + 12),
                         (i == 0) ? " <-- BLOCKER (should be non-zero)" : "");
            }
        }

        /* Dump MCU2HOST_SW_INT_STA */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: MCU2HOST_SW_INT_STA(MCU_CMD_REG 0xd41f0)=0x%08x\n",
                 mt7927_rr(dev, MT_MCU_CMD_REG));

        /* Dump INT_STA */
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: WFDMA_HOST_INT_STA=0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

        /* Wait and check for DIDX changes on HOST RX ring 6/7 */
        {
            u32 ring6_didx_before = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));
            u32 ring7_didx_before = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(7));

            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48: Waiting for HOST RX ring 6/7 DIDX changes...\n");
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48:   Ring 6 DIDX baseline=0x%08x\n", ring6_didx_before);
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48:   Ring 7 DIDX baseline=0x%08x\n", ring7_didx_before);

            msleep(500);

            u32 ring6_didx_after = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(6));
            u32 ring7_didx_after = mt7927_rr(dev, MT_WPDMA_RX_RING_DIDX(7));

            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48: After 500ms:\n");
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48:   Ring 6 DIDX=0x%08x %s\n",
                     ring6_didx_after,
                     (ring6_didx_after != ring6_didx_before) ? "CHANGED!" : "(no change)");
            dev_info(&dev->pdev->dev,
                     "[MT7927] MODE48:   Ring 7 DIDX=0x%08x %s\n",
                     ring7_didx_after,
                     (ring7_didx_after != ring7_didx_before) ? "CHANGED!" : "(no change)");
        }

        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE48: === DIAGNOSTICS COMPLETE ===\n");
    } else {
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: Sending NIC_CAPABILITY (Q_IDX=0x20)...\n", mode);
        ret = mt7927_mode40_send_nic_cap(dev);
        dev_info(&dev->pdev->dev,
                 "[MT7927] MODE%d: NIC_CAPABILITY result=%d\n", mode, ret);
    }

    dev_info(&dev->pdev->dev,
             "[MT7927] === MODE %d: COMPLETE ===\n", mode);
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

    dev_info(&pdev->dev, "[MT7927] reinit_mode check: reinit_mode=%d\n", reinit_mode);

    /* Mode 52: Full Windows 16-step ToggleWfsysRst + FWDL + diagnostics */
    if (reinit_mode == 52) {
        u32 val;
        int i;

        dev_info(&pdev->dev, "[MT7927] MODE52: === FULL 16-STEP ToggleWfsysRst ===\n");

        /* Step 1-3: Pre-reset (sleep protection, HIF clear) */
        /* 0x7c001600 area - sleep protection. Bus→BAR0: 0x7c001600→0xf1600 */
        /* For now, just log current state */
        dev_info(&pdev->dev, "[MT7927] MODE52: Sleep prot: 0x%08x\n",
                 mt7927_rr(dev, 0xf1600));

        /* Step 4-5: Pre-reset MCU regs */
        /* 0x81023f00→BAR0+0x0c3f00, 0x81023008→BAR0+0x0c3008 */
        mt7927_wr(dev, 0x0c3f00, 0xc0000100);
        mt7927_wr(dev, 0x0c3008, 0);
        dev_info(&pdev->dev, "[MT7927] MODE52: Pre-reset MCU regs written\n");

        /* Step 6-7: Assert WFSYS reset via CB_INFRA_RGU */
        /* 0x70028600→BAR0+0x1f8600 */
        val = mt7927_rr(dev, 0x1f8600);
        dev_info(&pdev->dev, "[MT7927] MODE52: CB_INFRA_RGU before=0x%08x\n", val);
        mt7927_wr(dev, 0x1f8600, val | BIT(4));

        /* Step 8: Sleep 1ms */
        usleep_range(1000, 1500);

        /* Step 9: Verify BIT(4) set (retry 5x) */
        for (i = 0; i < 5; i++) {
            val = mt7927_rr(dev, 0x1f8600);
            if (val & BIT(4)) break;
            usleep_range(100, 200);
        }
        dev_info(&pdev->dev, "[MT7927] MODE52: RGU after assert=0x%08x (BIT4=%d, %d retries)\n",
                 val, !!(val & BIT(4)), i);

        /* Step 10: Wait for reset to complete */
        msleep(20);

        /* Step 11: Deassert WFSYS reset */
        val = mt7927_rr(dev, 0x1f8600);
        mt7927_wr(dev, 0x1f8600, val & ~BIT(4));

        /* Step 12: Short wait */
        usleep_range(200, 300);

        /* Step 13: Poll ROMCODE_INDEX for 0x1D1E (MCU_IDLE) */
        /* 0x81021604→BAR0+0x0c1604 */
        for (i = 0; i < 500; i++) {
            val = mt7927_rr(dev, 0x0c1604);
            if (val == 0x1d1e) break;
            usleep_range(100, 200);
        }
        dev_info(&pdev->dev, "[MT7927] MODE52: ROMCODE_INDEX=0x%08x after %d polls (%s)\n",
                 val, i, (val == 0x1d1e) ? "MCU_IDLE OK" : "FAILED");

        if (val != 0x1d1e) {
            dev_err(&pdev->dev, "[MT7927] MODE52: ROMCODE_INDEX not 0x1D1E, aborting\n");
            goto skip_mode52_fwdl;
        }

        /* ============================================ */
        /* Steps 14-16: CONN_INFRA Reset (THE KEY PART) */
        /* 0x7c060010→BAR0+0xe0010 */
        /* ============================================ */
        dev_info(&pdev->dev, "[MT7927] MODE52: === CONN_INFRA RESET (steps 14-16) ===\n");

        val = mt7927_rr(dev, 0xe0010);
        dev_info(&pdev->dev, "[MT7927] MODE52: CONN_INFRA before=0x%08x\n", val);

        /* Step 14: Assert CONN_INFRA init */
        mt7927_wr(dev, 0xe0010, BIT(0));

        /* Step 15: Poll BIT(2) for done (49 retries) */
        for (i = 0; i < 49; i++) {
            val = mt7927_rr(dev, 0xe0010);
            if (val & BIT(2)) break;
            usleep_range(100, 200);
        }
        dev_info(&pdev->dev, "[MT7927] MODE52: CONN_INFRA after assert=0x%08x (BIT2=%d, %d polls)\n",
                 val, !!(val & BIT(2)), i);

        /* Step 16: Deassert */
        mt7927_wr(dev, 0xe0010, BIT(1));
        usleep_range(100, 200);
        val = mt7927_rr(dev, 0xe0010);
        dev_info(&pdev->dev, "[MT7927] MODE52: CONN_INFRA final=0x%08x\n", val);

        /* === Post-reset: reprogram HOST rings (WFSYS reset wipes them) === */
        dev_info(&pdev->dev, "[MT7927] MODE52: Reprogramming HOST rings...\n");
        /* Reset DMA pointers */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();
        /* TX ring WM (q15) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_wm.qid), lower_32_bits(dev->ring_wm.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_wm.qid), dev->ring_wm.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), 0);
        dev->ring_wm.head = 0;
        /* TX ring FWDL (q16) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_fwdl.qid), lower_32_bits(dev->ring_fwdl.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_fwdl.qid), dev->ring_fwdl.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), 0);
        dev->ring_fwdl.head = 0;
        /* RX ring event */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(dev->ring_evt.qid), lower_32_bits(dev->ring_evt.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(dev->ring_evt.qid), dev->ring_evt.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(dev->ring_evt.qid), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(dev->ring_evt.qid), dev->ring_evt.head);
        dev->ring_evt.tail = 0;
        /* HOST RX ring 0 (MCU event) */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(0), lower_32_bits(dev->ring_rx0.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(0), dev->ring_rx0.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(0), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(0), dev->ring_rx0.head);
        /* Dummy RX rings 4, 5, 7 */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(4), lower_32_bits(dev->ring_rx4.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(4), dev->ring_rx4.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(4), dev->ring_rx4.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(5), lower_32_bits(dev->ring_rx5.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(5), dev->ring_rx5.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(5), dev->ring_rx5.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(7), lower_32_bits(dev->ring_rx7.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(7), dev->ring_rx7.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(7), dev->ring_rx7.head);
        /* Prefetch entries */
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(5), PREFETCH(0x0080, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0100, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0180, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x01C0, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0200, 0x4));
        /* Reset pointers */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        /* Re-enable DMA */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

        /* Re-enable interrupts */
        mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA,
                  HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                  HOST_RX_DONE_INT_ENA(6) |
                  HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                  HOST_TX_DONE_INT_ENA17);
        mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE);

        dev_info(&pdev->dev, "[MT7927] MODE52: Rings reprogrammed, DMA re-enabled\n");

        /* Check MCU_RX0 BEFORE fwdl */
        dev_info(&pdev->dev, "[MT7927] MODE52: MCU_RX0 before FWDL: BASE=0x%08x\n",
                 mt7927_rr(dev, 0x54000 + 0x100));

skip_mode52_fwdl:
        ; /* label needs statement */
    }
    /* Mode 51: Second CLR_OWN before FWDL (after NEED_REINIT=1) */
    else if (reinit_mode == 51) {
        u32 dummy, lpctl;
        int i;

        dev_info(&pdev->dev,
                 "[MT7927] MODE51: === SECOND CLR_OWN BEFORE FWDL ===\n");

        /* Step 1: Record current NEED_REINIT and MCU_RX0 status */
        dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        dev_info(&pdev->dev,
                 "[MT7927] MODE51: BEFORE second CLR_OWN:\n");
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   DUMMY_CR = 0x%08x (NEED_REINIT=%d)\n",
                 dummy, !!(dummy & BIT(1)));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX0 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02500));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX1 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02510));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX2 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02540));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX3 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02550));

        /* Step 2: Ensure NEED_REINIT=1 (should already be set from dma_enable) */
        if (!(dummy & BIT(1))) {
            dev_info(&pdev->dev,
                     "[MT7927] MODE51:   NEED_REINIT not set, setting it now...\n");
            mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, dummy | BIT(1));
            wmb();
            msleep(10);
            dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
            dev_info(&pdev->dev,
                     "[MT7927] MODE51:   DUMMY_CR after set = 0x%08x\n", dummy);
        }

        /* Step 3: Do second SET_OWN/CLR_OWN cycle */
        dev_info(&pdev->dev,
                 "[MT7927] MODE51: Starting second SET_OWN/CLR_OWN cycle...\n");

        /* SET_OWN: BIT(0) -> LPCTL (0xe0010) */
        mt7927_wr(dev, MT_CONN_ON_LPCTL, BIT(0));
        wmb();

        /* Wait for OWN_SYNC (BIT(2)) to be set */
        for (i = 0; i < 2000; i++) {
            lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
            if (lpctl & BIT(2))
                break;
            udelay(100);
        }
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   SET_OWN: OWN_SYNC=%d after %d polls, LPCTL=0x%08x\n",
                 !!(lpctl & BIT(2)), i, lpctl);

        /* CLR_OWN: BIT(1) -> LPCTL */
        mt7927_wr(dev, MT_CONN_ON_LPCTL, BIT(1));
        wmb();

        /* Wait for CLR_OWN complete: OWN_SYNC goes to 0, LPCTL BIT(0) clears */
        for (i = 0; i < 2000; i++) {
            lpctl = mt7927_rr(dev, MT_CONN_ON_LPCTL);
            if (!(lpctl & BIT(0)))
                break;
            udelay(100);
        }
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   CLR_OWN: complete after %d polls, LPCTL=0x%08x\n",
                 i, lpctl);

        /* Step 4: Check if MCU_RX0 is NOW configured */
        dev_info(&pdev->dev,
                 "[MT7927] MODE51: AFTER second CLR_OWN:\n");
        dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   DUMMY_CR = 0x%08x (NEED_REINIT=%d)\n",
                 dummy, !!(dummy & BIT(1)));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX0 BASE = 0x%08x  *** KEY METRIC ***\n",
                 ioread32(dev->bar0 + 0x02500));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX1 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02510));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX2 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02540));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   MCU_RX3 BASE = 0x%08x\n",
                 ioread32(dev->bar0 + 0x02550));

        /* Step 5: CLR_OWN wipes HOST rings, reprogram them */
        dev_info(&pdev->dev,
                 "[MT7927] MODE51: Reprogramming HOST rings after second CLR_OWN...\n");

        /* Reset DMA pointers */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        /* TX ring WM (q15) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_wm.qid),
                  lower_32_bits(dev->ring_wm.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_wm.qid), dev->ring_wm.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_wm.qid), 0);
        dev->ring_wm.head = 0;

        /* TX ring FWDL (q16) */
        mt7927_wr(dev, MT_WPDMA_TX_RING_BASE(dev->ring_fwdl.qid),
                  lower_32_bits(dev->ring_fwdl.desc_dma));
        mt7927_wr(dev, MT_WPDMA_TX_RING_CNT(dev->ring_fwdl.qid), dev->ring_fwdl.ndesc);
        mt7927_wr(dev, MT_WPDMA_TX_RING_CIDX(dev->ring_fwdl.qid), 0);
        dev->ring_fwdl.head = 0;

        /* RX ring event */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(dev->ring_evt.qid),
                  lower_32_bits(dev->ring_evt.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(dev->ring_evt.qid), dev->ring_evt.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(dev->ring_evt.qid), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(dev->ring_evt.qid), dev->ring_evt.head);
        dev->ring_evt.tail = 0;

        /* HOST RX ring 0 (MCU event) */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(0), lower_32_bits(dev->ring_rx0.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(0), dev->ring_rx0.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_DIDX(0), 0);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(0), dev->ring_rx0.head);

        /* Dummy RX rings 4, 5, 7 */
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(4), lower_32_bits(dev->ring_rx4.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(4), dev->ring_rx4.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(4), dev->ring_rx4.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(5), lower_32_bits(dev->ring_rx5.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(5), dev->ring_rx5.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(5), dev->ring_rx5.head);
        mt7927_wr(dev, MT_WPDMA_RX_RING_BASE(7), lower_32_bits(dev->ring_rx7.desc_dma));
        mt7927_wr(dev, MT_WPDMA_RX_RING_CNT(7), dev->ring_rx7.ndesc);
        mt7927_wr(dev, MT_WPDMA_RX_RING_CIDX(7), dev->ring_rx7.head);

        /* Prefetch entries */
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(4), PREFETCH(0x0000, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(5), PREFETCH(0x0080, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(6), PREFETCH(0x0100, 0x8));
        mt7927_wr(dev, MT_WFDMA_RX_RING_EXT_CTRL(7), PREFETCH(0x0180, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(16), PREFETCH(0x01C0, 0x4));
        mt7927_wr(dev, MT_WFDMA_TX_RING_EXT_CTRL(15), PREFETCH(0x0200, 0x4));

        /* Reset pointers again after prefetch */
        mt7927_wr(dev, MT_WPDMA_RST_DTX_PTR, 0xFFFFFFFF);
        mt7927_wr(dev, MT_WPDMA_RST_DRX_PTR, 0xFFFFFFFF);
        wmb();

        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   HOST rings reprogrammed\n");

        /* Step 6: Read HOST GLO_CFG/INT_ENA to see what CLR_OWN changed */
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   HOST GLO_CFG = 0x%08x\n",
                 mt7927_rr(dev, MT_WPDMA_GLO_CFG));
        dev_info(&pdev->dev,
                 "[MT7927] MODE51:   HOST INT_ENA = 0x%08x\n",
                 mt7927_rr(dev, MT_WFDMA_HOST_INT_ENA));

        /* Re-enable DMA (CLR_OWN may have changed GLO_CFG) */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val |= MT_WFDMA_GLO_CFG_TX_DMA_EN | MT_WFDMA_GLO_CFG_RX_DMA_EN;
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

        /* Re-enable interrupts */
        mt7927_wr(dev, MT_WFDMA_HOST_INT_ENA,
                  HOST_RX_DONE_INT_ENA0 | HOST_RX_DONE_INT_ENA1 |
                  HOST_RX_DONE_INT_ENA(evt_ring_qid) |
                  HOST_TX_DONE_INT_ENA15 | HOST_TX_DONE_INT_ENA16 |
                  HOST_TX_DONE_INT_ENA17);
        mt7927_wr(dev, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE);

        /* Re-signal NEED_REINIT */
        dummy = mt7927_rr(dev, MT_MCU_WPDMA0_DUMMY_CR);
        mt7927_wr(dev, MT_MCU_WPDMA0_DUMMY_CR, dummy | MT_WFDMA_NEED_REINIT);
        wmb();
        msleep(10);

        dev_info(&pdev->dev,
                 "[MT7927] MODE51: Second CLR_OWN complete, proceeding to FWDL...\n");
    }

    ret = mt7927_mcu_fw_download(dev);

    dev_info(&pdev->dev, "HOST_INT_STA final: 0x%08x\n",
             mt7927_rr(dev, MT_WFDMA_HOST_INT_STA));

    if (ret)
        dev_warn(&pdev->dev, "Firmware flow incomplete (ret=%d), device kept bound\n", ret);
    else
        dev_info(&pdev->dev, "MT7927 firmware flow done\n");

    /* Post-FWDL experiment modes */
    if (reinit_mode == 40 || reinit_mode == 50)
        mt7927_mode40_post_fwdl(dev);
    else if (reinit_mode >= 43 && reinit_mode <= 49)
        mt7927_mode43_vendor_order(dev, reinit_mode);
    else if (reinit_mode == 51) {
        /* Mode 51: Skip WFSYS reset — just check MCU_RX0 after second CLR_OWN + FWDL */
        u32 mcu_rx0_base = mt7927_rr(dev, 0x54000 + 0x100);
        u32 mcu_rx1_base = mt7927_rr(dev, 0x54000 + 0x150);
        u32 fw_sync = mt7927_rr(dev, MT_CONN_ON_MISC);
        u32 glo = mt7927_rr(dev, MT_WPDMA_GLO_CFG);

        dev_info(&pdev->dev,
                 "[MT7927] MODE51 POST-FWDL: *** MCU_RX0 BASE=0x%08x *** MCU_RX1=0x%08x\n",
                 mcu_rx0_base, mcu_rx1_base);
        dev_info(&pdev->dev,
                 "[MT7927] MODE51 POST-FWDL: fw_sync=0x%08x GLO_CFG=0x%08x\n",
                 fw_sync, glo);

        if (mcu_rx0_base != 0) {
            dev_info(&pdev->dev,
                     "[MT7927] MODE51: *** MCU_RX0 CONFIGURED! Second CLR_OWN worked! ***\n");
        } else {
            dev_info(&pdev->dev,
                     "[MT7927] MODE51: MCU_RX0 still 0 — second CLR_OWN didn't help\n");
        }

        /* Try DMASHDL enable + NIC_CAP anyway */
        {
            u32 val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
            /* Clear DMASHDL bypass */
            val &= ~BIT(9);
            mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);
            /* Enable DMASHDL refill */
            val = mt7927_rr(dev, 0xd4404); /* GLO_CFG_EXT1 */
            val |= BIT(28);
            mt7927_wr(dev, 0xd4404, val);
            /* DMASHDL queue mapping */
            val = mt7927_rr(dev, 0xd6060);
            mt7927_wr(dev, 0xd6060, val | 0x10101);
        }

        /* Send NIC_CAPABILITY with Q_IDX=0x20 */
        dev_info(&pdev->dev, "[MT7927] MODE51: Sending NIC_CAPABILITY...\n");
        {
            int nic_ret = mt7927_mode40_send_nic_cap(dev);
            dev_info(&pdev->dev,
                     "[MT7927] MODE51: NIC_CAPABILITY result=%d\n", nic_ret);
        }
    }
    else if (reinit_mode == 52) {
        u32 mcu_rx0 = mt7927_rr(dev, 0x54000 + 0x100);
        u32 mcu_rx1 = mt7927_rr(dev, 0x54000 + 0x150);
        u32 fw_sync = mt7927_rr(dev, MT_CONN_ON_MISC);

        dev_info(&pdev->dev, "[MT7927] MODE52 POST-FWDL: *** MCU_RX0 BASE=0x%08x *** MCU_RX1=0x%08x fw_sync=0x%08x\n",
                 mcu_rx0, mcu_rx1, fw_sync);

        if (mcu_rx0 != 0) {
            dev_info(&pdev->dev, "[MT7927] MODE52: *** SUCCESS! MCU_RX0 CONFIGURED! ***\n");
        }

        /* DMASHDL enable */
        val = mt7927_rr(dev, 0xd6060);
        mt7927_wr(dev, 0xd6060, val | 0x10101);

        /* Clear bypass */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val &= ~BIT(9);
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

        /* NIC_CAP */
        dev_info(&pdev->dev, "[MT7927] MODE52: Sending NIC_CAPABILITY...\n");
        ret = mt7927_mode40_send_nic_cap(dev);
        dev_info(&pdev->dev, "[MT7927] MODE52: NIC_CAPABILITY result=%d\n", ret);
    }
    else if (reinit_mode == 53) {
        u32 mcu_rx0_base = ioread32(dev->bar0 + 0x02500);
        u32 host_rx0_base = mt7927_rr(dev, MT_WPDMA_RX_RING_BASE(0));
        u32 fw_sync = mt7927_rr(dev, MT_CONN_ON_MISC);
        u32 mcu_cmd = mt7927_rr(dev, MT_MCU_CMD_REG);

        dev_info(&pdev->dev, "[MT7927] MODE53: === HOST RX RING 0 TEST ===\n");
        dev_info(&pdev->dev, "[MT7927] MODE53: fw_sync=0x%08x MCU_CMD=0x%08x\n",
                 fw_sync, mcu_cmd);
        dev_info(&pdev->dev, "[MT7927] MODE53: HOST_RX0 BASE=0x%08x (ring_rx0 programmed)\n",
                 host_rx0_base);
        dev_info(&pdev->dev, "[MT7927] MODE53: *** MCU_RX0 BASE=0x%08x *** (KEY METRIC)\n",
                 mcu_rx0_base);

        /* Dump all MCU DMA0 RX rings */
        {
            int i;
            for (i = 0; i < 4; i++) {
                dev_info(&pdev->dev, "[MT7927] MODE53: MCU_RX%d: BASE=0x%08x CNT=0x%08x CIDX=0x%08x DIDX=0x%08x\n",
                         i,
                         ioread32(dev->bar0 + 0x02500 + (i << 4)),
                         ioread32(dev->bar0 + 0x02504 + (i << 4)),
                         ioread32(dev->bar0 + 0x02508 + (i << 4)),
                         ioread32(dev->bar0 + 0x0250c + (i << 4)));
            }
        }

        if (mcu_rx0_base != 0) {
            dev_info(&pdev->dev, "[MT7927] MODE53: *** SUCCESS! MCU_RX0 CONFIGURED! ***\n");
        }

        /* DMASHDL enable */
        val = mt7927_rr(dev, 0xd6060);
        mt7927_wr(dev, 0xd6060, val | 0x10101);

        /* Clear DMASHDL bypass */
        val = mt7927_rr(dev, MT_WPDMA_GLO_CFG);
        val &= ~BIT(9);
        mt7927_wr(dev, MT_WPDMA_GLO_CFG, val);

        /* PCIe sleep disable (from Windows HIF init) */
        mt7927_wr(dev, 0x1f5018, 0xFFFFFFFF);

        /* Wait for FW to detect ring 0 and potentially configure MCU_RX0 */
        msleep(100);

        /* Re-check MCU_RX0 after wait */
        mcu_rx0_base = ioread32(dev->bar0 + 0x02500);
        dev_info(&pdev->dev, "[MT7927] MODE53: MCU_RX0 after 100ms wait: 0x%08x\n",
                 mcu_rx0_base);

        /* Send NIC_CAPABILITY */
        dev_info(&pdev->dev, "[MT7927] MODE53: Sending NIC_CAPABILITY...\n");
        ret = mt7927_mode40_send_nic_cap(dev);
        dev_info(&pdev->dev, "[MT7927] MODE53: NIC_CAPABILITY result=%d\n", ret);
    }

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
