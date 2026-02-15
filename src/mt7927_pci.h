/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * MT7927 (MT6639 CONNAC3) WiFi PCIe 驱动 - 寄存器和数据结构定义
 *
 * 基于 Windows RE 分析 (mtkwecx.sys v5603998 + v5705275)
 * 参考文档: docs/register_playbook.md, docs/analysis_windows_full_init.md
 */

#ifndef __MT7927_PCI_H
#define __MT7927_PCI_H

#include <linux/types.h>
#include <linux/bitfield.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <net/mac80211.h>

/* ============================================================================
 * PCI 设备 ID
 * ============================================================================ */
#define MT7927_PCI_VENDOR_ID        0x14c3
#define MT7927_PCI_DEVICE_ID        0x7927
#define MT7927_PCI_DEVICE_ID_6639   0x6639  /* MT7927 = MT6639 PCIe 封装 */

/* ============================================================================
 * BAR0 偏移定义 - WFDMA HOST DMA0
 * 基址: bus 0x7c024000 = BAR0 + 0xd4000
 * 来源: register_playbook.md, analysis_windows_full_init.md
 * ============================================================================ */

#define MT_WFDMA0_BASE              0xd4000
#define MT_WFDMA0(ofs)              (MT_WFDMA0_BASE + (ofs))
#define MT_WFDMA_EXT_CSR_BASE       0xd7000
#define MT_WFDMA_EXT_CSR(ofs)       (MT_WFDMA_EXT_CSR_BASE + (ofs))

/* ----------------------------------------------------------------------------
 * GLO_CFG - 全局配置寄存器
 * BAR0+0xd4208 = bus 0x7c024208
 * ---------------------------------------------------------------------------- */
#define MT_WPDMA_GLO_CFG            MT_WFDMA0(0x0208)

/* GLO_CFG 位域 (来源: wf_wfdma_host_dma0.h) */
#define MT_WFDMA_GLO_CFG_TX_DMA_EN              BIT(0)   /* TX DMA 启用 */
#define MT_WFDMA_GLO_CFG_TX_DMA_BUSY            BIT(1)   /* TX DMA 繁忙 */
#define MT_WFDMA_GLO_CFG_RX_DMA_EN              BIT(2)   /* RX DMA 启用 */
#define MT_WFDMA_GLO_CFG_RX_DMA_BUSY            BIT(3)   /* RX DMA 繁忙 */
#define MT_GLO_CFG_PDMA_BT_SIZE                 GENMASK(5, 4)  /* burst size */
#define MT_GLO_CFG_TX_WB_DDONE                  BIT(6)   /* TX writeback done */
#define MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL       BIT(9)   /* FWDL bypass 调度器 */
#define MT_GLO_CFG_FIFO_LITTLE_ENDIAN           BIT(12)
#define MT_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN   BIT(15)  /* 预取链模式 */
#define MT_GLO_CFG_CSR_LBK_RX_Q_SEL_EN          BIT(20)  /* RX 队列选择 */
#define MT_GLO_CFG_OMIT_RX_INFO_PFET2           BIT(21)
#define MT_GLO_CFG_ADDR_EXT_EN                  BIT(26)  /* 地址扩展使能 */
#define MT_GLO_CFG_OMIT_TX_INFO                 BIT(28)
#define MT_GLO_CFG_CLK_GATE_DIS                 BIT(30)

/* Windows 观察到的 GLO_CFG 值:
 * WpdmaConfig 中: glo_cfg |= 0x5 (TX_DMA_EN | RX_DMA_EN)
 * mt7925: glo_cfg |= 0x4000005 (多 BIT(26))
 */
#define MT_WPDMA_GLO_CFG_WIN_OR     0x5  /* Windows: TX_DMA_EN | RX_DMA_EN */

/* ----------------------------------------------------------------------------
 * GLO_CFG_EXT - 扩展配置寄存器
 * ---------------------------------------------------------------------------- */
#define MT_WPDMA_GLO_CFG_EXT0       MT_WFDMA0(0x02b0)
#define MT_WPDMA_GLO_CFG_EXT1       MT_WFDMA0(0x02b4)  /* bus 0x7c0242b4 */

/* GLO_CFG_EXT0: SDO dispatch mode (vendor mt6639WpdmaConfigExt0) */
#define MT_WPDMA_GLO_CFG_EXT0_VAL   0x28C004DF

/* GLO_CFG_EXT1 BIT(28): Windows 无条件设置 (WpdmaConfig 步骤 6)
 * 来源: analysis_windows_full_init.md 行 151
 */
#define MT_WPDMA_GLO_CFG_EXT1_WIN   BIT(28)

/* ----------------------------------------------------------------------------
 * 中断寄存器
 * ---------------------------------------------------------------------------- */
#define MT_WFDMA_HOST_INT_STA       MT_WFDMA0(0x0200)  /* 中断状态 */
#define MT_WFDMA_HOST_INT_ENA       MT_WFDMA0(0x0204)  /* 中断使能 (也叫 INT_STA_EXT) */
#define MT_WFDMA_INT_ENA_SET        MT_WFDMA0(0x0228)  /* bus 0x7c024228 */
#define MT_WFDMA_INT_ENA_CLR        MT_WFDMA0(0x022c)  /* bus 0x7c02422c */
#define MT_WFDMA_HOST_INT_DIS       MT_WFDMA0(0x022c)  /* 同 INT_ENA_CLR */

/* Windows 中断掩码值 (ConfigIntMask 步骤 1)
 * 来源: analysis_windows_full_init.md 行 182
 * 0x2600f000 = BIT(29) | BIT(25) | GENMASK(15,12)
 *   BIT(29) = MCU2HOST_SW_INT (MCU 命令完成中断)
 *   BIT(25) = 未确定 (可能 WDT)
 *   BIT(15:12) = 0xF = RX ring 4/5/6/7 完成中断
 */
#define MT_WFDMA_INT_MASK_WIN       0x2600f000

/* 中断位定义 */
#define HOST_RX_DONE_INT_ENA0       BIT(0)
#define HOST_RX_DONE_INT_ENA1       BIT(1)
#define HOST_RX_DONE_INT_ENA(n)     BIT(n)   /* RX done for ring n (0-7) */
#define HOST_TX_DONE_INT_ENA15      BIT(25)
#define HOST_TX_DONE_INT_ENA16      BIT(26)
#define HOST_TX_DONE_INT_ENA17      BIT(27)
#define MCU2HOST_SW_INT_STA         BIT(29)

/* ----------------------------------------------------------------------------
 * TX Ring 寄存器 (4 个 TX rings)
 * 公式: base_reg = 0x7c024300 + (hw_ring_idx << 4)
 * 来源: register_playbook.md 行 98-110
 * ---------------------------------------------------------------------------- */
#define MT_WPDMA_TX_RING_BASE(n)    MT_WFDMA0(0x0300 + ((n) << 4))
#define MT_WPDMA_TX_RING_CNT(n)     MT_WFDMA0(0x0304 + ((n) << 4))
#define MT_WPDMA_TX_RING_CIDX(n)    MT_WFDMA0(0x0308 + ((n) << 4))
#define MT_WPDMA_TX_RING_DIDX(n)    MT_WFDMA0(0x030c + ((n) << 4))

/* TX Ring 用途 (来源: Windows RE config[0xd0], config[0xd1])
 * Ring 0/1: 数据 TX
 * Ring 2/3: 可配置 (其中一个可能是 ring 15 MCU 命令 TX)
 */
#define MT_TXQ_MCU_WM_RING          15   /* MCU 命令 TX ring */
#define MT_TXQ_FWDL_RING            16   /* FWDL TX ring */
#define MT_TXQ_MCU_WM_RING_ALT      17   /* 备用 MCU ring */

/* ----------------------------------------------------------------------------
 * RX Ring 寄存器 (3 个 RX rings: 4, 6, 7)
 * ⚠️⚠️⚠️ Windows 使用 Ring 4, 6, 7 - 不使用 ring 0, 不使用 ring 5
 * 公式: base_reg = 0x7c024500 + (hw_ring_idx << 4)
 * 来源: analysis_windows_full_init.md 行 104-111, register_playbook.md 行 83-94
 * ---------------------------------------------------------------------------- */
#define MT_WPDMA_RX_RING_BASE(n)    MT_WFDMA0(0x0500 + ((n) << 4))
#define MT_WPDMA_RX_RING_CNT(n)     MT_WFDMA0(0x0504 + ((n) << 4))
#define MT_WPDMA_RX_RING_CIDX(n)    MT_WFDMA0(0x0508 + ((n) << 4))
#define MT_WPDMA_RX_RING_DIDX(n)    MT_WFDMA0(0x050c + ((n) << 4))

/* RX Ring 4 寄存器地址 (HW offset 0x40) */
#define MT_WPDMA_RX_RING4_BASE      MT_WFDMA0(0x0540)  /* bus 0x7c024540 */
#define MT_WPDMA_RX_RING4_CNT       MT_WFDMA0(0x0544)
#define MT_WPDMA_RX_RING4_CIDX      MT_WFDMA0(0x0548)
#define MT_WPDMA_RX_RING4_DIDX      MT_WFDMA0(0x054c)
#define MT_WFDMA_HOST_RX_RING4_DIDX MT_WPDMA_RX_RING4_DIDX  /* 别名 */

/* ⚠️⚠️⚠️ RX Ring 6 寄存器地址 (HW offset 0x60) - 不是 ring 5!
 * 来源: analysis_windows_full_init.md 行 109-111, 381-384
 * Ring 5 vs Ring 6 地址完全不同:
 *   Ring 5: BASE=0xd4550, CIDX=0xd4558, DIDX=0xd455c
 *   Ring 6: BASE=0xd4560, CIDX=0xd4568, DIDX=0xd456c
 */
#define MT_WPDMA_RX_RING6_BASE      MT_WFDMA0(0x0560)  /* bus 0x7c024560 */
#define MT_WPDMA_RX_RING6_CNT       MT_WFDMA0(0x0564)
#define MT_WPDMA_RX_RING6_CIDX      MT_WFDMA0(0x0568)
#define MT_WPDMA_RX_RING6_DIDX      MT_WFDMA0(0x056c)
#define MT_WFDMA_HOST_RX_RING6_DIDX MT_WPDMA_RX_RING6_DIDX  /* 别名 */

/* RX Ring 7 寄存器地址 (HW offset 0x70) */
#define MT_WPDMA_RX_RING7_BASE      MT_WFDMA0(0x0570)  /* bus 0x7c024570 */
#define MT_WPDMA_RX_RING7_CNT       MT_WFDMA0(0x0574)
#define MT_WPDMA_RX_RING7_CIDX      MT_WFDMA0(0x0578)
#define MT_WPDMA_RX_RING7_DIDX      MT_WFDMA0(0x057c)
#define MT_WFDMA_HOST_RX_RING7_DIDX MT_WPDMA_RX_RING7_DIDX  /* 别名 */

/* RX Ring 用途 (来源: Windows RE 分析)
 * Ring 4: WiFi 数据帧接收
 * Ring 6: MCU 事件接收 (MT6639/CONNAC3)
 * Ring 7: 数据/事件接收
 */
#define MT_RXQ_MCU_EVENT_RING_CONNAC3   6   /* MT6639 MCU 事件 ring */
#define MT_RXQ_MCU_EVENT_RING_CONNAC2   0   /* mt7925 用 ring 0 */

/* ----------------------------------------------------------------------------
 * WFDMA 预取配置 (Prefetch Configuration)
 * ⚠️⚠️ Windows 使用 packed 格式 (4 个 32 位寄存器)
 * 来源: register_playbook.md 行 145-148, analysis_windows_full_init.md 行 391-394
 * ---------------------------------------------------------------------------- */
#define MT_WFDMA_PREFETCH_CTRL      MT_WFDMA_EXT_CSR(0x0030)  /* bus 0x7c027030 */
#define MT_WFDMA_PREFETCH_CFG0      MT_WFDMA_EXT_CSR(0x00f0)  /* bus 0x7c0270f0 */
#define MT_WFDMA_PREFETCH_CFG1      MT_WFDMA_EXT_CSR(0x00f4)  /* bus 0x7c0270f4 */
#define MT_WFDMA_PREFETCH_CFG2      MT_WFDMA_EXT_CSR(0x00f8)  /* bus 0x7c0270f8 */
#define MT_WFDMA_PREFETCH_CFG3      MT_WFDMA_EXT_CSR(0x00fc)  /* bus 0x7c0270fc */

/* Windows 预取配置值 (100% 确定)
 * 来源: register_playbook.md 行 583-586
 */
#define MT_WFDMA_PREFETCH_VAL0      0x660077
#define MT_WFDMA_PREFETCH_VAL1      0x1100
#define MT_WFDMA_PREFETCH_VAL2      0x30004f
#define MT_WFDMA_PREFETCH_VAL3      0x542200

/* 旧的 per-ring EXT_CTRL 寄存器 (已被 Windows packed 格式替代) */
#define MT_WFDMA_TX_RING_EXT_CTRL(n)  MT_WFDMA0(0x0600 + ((n) << 2))
#define MT_WFDMA_RX_RING_EXT_CTRL(n)  MT_WFDMA0(0x0680 + ((n) << 2))

/* ----------------------------------------------------------------------------
 * WFDMA 其他配置寄存器
 * ---------------------------------------------------------------------------- */
#define MT_WPDMA_RST_DTX_PTR        MT_WFDMA0(0x020c)  /* TX ring 指针复位 */
#define MT_WPDMA_RST_DRX_PTR        MT_WFDMA0(0x0280)  /* RX ring 指针复位 */
#define MT_WFDMA0_RST               MT_WFDMA0(0x0100)  /* WFDMA 逻辑复位 (CONNAC3X_WPDMA_HIF_RST) */
#define MT_WFDMA0_RST_LOGIC_RST     BIT(4)
#define MT_WFDMA0_RST_DMASHDL_RST   BIT(5)

/* RX 暂停阈值寄存器 (每个寄存器控制 2 个 rings) */
#define MT_WPDMA_PAUSE_RX_Q_TH(n)   MT_WFDMA0(0x0260 + ((n) << 2))
#define MT_WPDMA_PAUSE_RX_Q_TH_VAL  (2 | (2 << 16))

/* 中断优先级和延迟配置 */
#define MT_WFDMA_INT_RX_PRI         MT_WFDMA0(0x0298)
#define MT_WFDMA_INT_TX_PRI         MT_WFDMA0(0x029c)
#define MT_WFDMA_PRI_DLY_INT_CFG0   MT_WFDMA0(0x02f0)

/* 诊断寄存器 */
#define MT_WPDMA_GLO_CFG2           MT_WFDMA0(0x025c)
#define MT_WPDMA2HOST_ERR_INT_STA   MT_WFDMA0(0x01e8)
#define MT_MCU_INT_STA              MT_WFDMA0(0x0110)
#define MT_CONN_HIF_BUSY_STATUS     MT_WFDMA0(0x0138)
#define MT_WPDMA_TIMEOUT_CFG        MT_WFDMA0(0x0230)

/* HOST2MCU 软件中断 */
#define MT_HOST2MCU_SW_INT_SET      MT_WFDMA0(0x0108)
#define MT_MCU2HOST_SW_INT_ENA      MT_WFDMA0(0x01f4)
#define MT_MCU_CMD_REG              MT_WFDMA0(0x01f0)

/* ============================================================================
 * DMA Scheduler (DMASHDL)
 * 基址: bus 0x7c026000 = BAR0 + 0xd6000
 * 来源: wf_hif_dmashdl_top.h
 * ============================================================================ */

#define MT_HIF_DMASHDL_BASE                  0xd6000
#define MT_HIF_DMASHDL(ofs)                  (MT_HIF_DMASHDL_BASE + (ofs))

/* DMASHDL 启用寄存器 (PostFwDownloadInit 唯一寄存器写入)
 * BAR0+0xd6060 = bus 0x7c026060
 * 来源: register_playbook.md 行 201-206, 389, analysis_windows_full_init.md 行 245-250
 * ⚠️⚠️ Windows PostFwDownloadInit: val |= 0x10101
 */
#define MT_DMASHDL_ENABLE                   MT_HIF_DMASHDL(0x0060)  /* bus 0x7c026060 */
#define MT_DMASHDL_ENABLE_VAL               0x10101  /* BIT(0)|BIT(8)|BIT(16) */

/* 其他 DMASHDL 寄存器 */
#define MT_HIF_DMASHDL_SW_CONTROL           MT_HIF_DMASHDL(0x04)
#define MT_HIF_DMASHDL_BYPASS_EN            BIT(28)
#define MT_HIF_DMASHDL_OPTIONAL_CONTROL     MT_HIF_DMASHDL(0x08)
#define MT_HIF_DMASHDL_PAGE_SETTING         MT_HIF_DMASHDL(0x0c)
#define MT_HIF_DMASHDL_REFILL_CONTROL       MT_HIF_DMASHDL(0x10)
#define MT_HIF_DMASHDL_PKT_MAX_SIZE         MT_HIF_DMASHDL(0x1c)
#define MT_HIF_DMASHDL_GROUP_CONTROL(n)     MT_HIF_DMASHDL(0x20 + ((n) << 2))
#define MT_HIF_DMASHDL_QUEUE_MAP0           MT_HIF_DMASHDL(0x60)
#define MT_HIF_DMASHDL_QUEUE_MAP1           MT_HIF_DMASHDL(0x64)
#define MT_HIF_DMASHDL_QUEUE_MAP2           MT_HIF_DMASHDL(0x68)
#define MT_HIF_DMASHDL_QUEUE_MAP3           MT_HIF_DMASHDL(0x6c)
#define MT_HIF_DMASHDL_SCHED_SET0           MT_HIF_DMASHDL(0x70)
#define MT_HIF_DMASHDL_SCHED_SET1           MT_HIF_DMASHDL(0x74)
#define MT_HIF_DMASHDL_STATUS_RD            MT_HIF_DMASHDL(0x100)

/* ============================================================================
 * CONN_ON - 连接子系统寄存器
 * 基址: bus 0x7c060000 = BAR0 + 0xe0000
 * ============================================================================ */

/* LPCTL - 低功耗控制 (SET_OWN/CLR_OWN)
 * BAR0+0xe0010 = bus 0x7c060010
 * 来源: register_playbook.md 行 213-215, 395
 */
#define MT_CONN_ON_LPCTL            0xe0010
#define PCIE_LPCR_HOST_SET_OWN      BIT(0)  /* 步骤 1: SET_OWN */
#define PCIE_LPCR_HOST_CLR_OWN      BIT(1)  /* 步骤 3: CLR_OWN */
#define PCIE_LPCR_HOST_OWN_SYNC     BIT(2)  /* 步骤 2: 等待 OWN_SYNC=1 */

/* fw_sync 寄存器 (推测)
 * 来源: register_playbook.md 行 404
 */
#define MT_CONN_ON_MISC             0xe00f0  /* bus 0x7c0600f0 */

/* CONNINFRA 唤醒寄存器
 * 来源: register_playbook.md 行 328
 */
#define MT_WAKEPU_TOP               0xe01A0
#define MT_WAKEPU_WF                0xe01A4

/* ⚠️ 注意: 0xe0010 也用于 CONN_INFRA 复位 (ToggleWfsysRst 步骤 16-18)
 * 但在正常初始化中只用于 SET_OWN/CLR_OWN
 * 来源: register_playbook.md 行 346-347
 */

/* ============================================================================
 * CB_INFRA_RGU - 芯片复位控制 ⚠️ 危险
 * 基址: bus 0x70028000 = BAR0 + 0x1f8000
 * 来源: cb_infra_rgu.h
 * ============================================================================ */

/* ⚠️⚠️⚠️ 致命操作 - WF_SUBSYS_RST 会使设备无法恢复 (除非重启)
 * 来源: register_playbook.md 行 336-341, 400
 */
#define MT_CB_INFRA_RGU_WF_SUBSYS_RST      0x1f8600  /* bus 0x70028600 */
#define MT_WF_SUBSYS_RST_BIT               BIT(4)    /* ⚠️ DANGEROUS */
#define MT_CB_INFRA_RGU_DEBUG              0x1f8610  /* bus 0x70028610 */

/* ============================================================================
 * ROMCODE_INDEX - MCU 状态寄存器
 * BAR0+0xc1604 = bus 0x81021604
 * 来源: register_playbook.md 行 343, 362
 * ============================================================================ */
#define MT_ROMCODE_INDEX                    0xc1604
#define MT_MCU_IDLE_VALUE                   0x1D1E  /* MCU 空闲状态值 */

/* ============================================================================
 * CB_INFRA_MISC0 - PCIe 地址重映射
 * 基址: bus 0x70026000 = BAR0 + 0x1f6000
 * 来源: cb_infra_misc0.h
 * ============================================================================ */
#define MT_CB_INFRA_MISC0_PCIE_REMAP_WF     0x1f6554
#define MT_CB_INFRA_MISC0_PCIE_REMAP_WF_BT  0x1f6558
#define MT_PCIE_REMAP_WF_VALUE              0x74037001
#define MT_PCIE_REMAP_WF_BT_VALUE           0x70007000

/* ============================================================================
 * CB_INFRA_SLP_CTRL - MCU ownership 和睡眠控制
 * 基址: bus 0x70025000 = BAR0 + 0x1f5000
 * 来源: cb_infra_slp_ctrl.h
 * ============================================================================ */

/* MCU ownership 寄存器 (MCU 初始化步骤 5)
 * 来源: reusable_code.md 行 493-494
 */
#define MT_CB_INFRA_MCU_OWN         0x1f5030  /* MCU_OWN 状态 */
#define MT_CB_INFRA_MCU_OWN_SET     0x1f5034  /* MCU_OWN 设置 */

/* PCIe MAC 中断使能寄存器
 * 来源: mt76/mt792x_regs.h — PCIe MAC 层中断转发
 * ⚠️ 必须设置为 0xff 才能将 WFDMA 中断转发到主机 CPU
 */
#define MT_PCIE_MAC_BASE            0x10000
#define MT_PCIE_MAC(ofs)            (MT_PCIE_MAC_BASE + (ofs))
#define MT_PCIE_MAC_INT_ENABLE      MT_PCIE_MAC(0x188)  /* BAR0+0x10188 */

/* PCIe 睡眠配置 (HIF Init)
 * 来源: register_playbook.md 行 399, reusable_code.md 行 643-647
 */
#define MT_CB_INFRA_SLP_CTRL        0x1f5018  /* bus 0x70025018 */
#define MT_CB_INFRA_SLP_CTRL_VAL    0xFFFFFFFF  /* 禁用所有睡眠 */

/* ============================================================================
 * 睡眠保护和 HIF 状态 (ToggleWfsysRst 使用)
 * 基址: bus 0x7c001000 = BAR0 + 0xf1000
 * 来源: register_playbook.md 行 329-331, 396-398
 * ============================================================================ */
#define MT_CONN_HIF_SLP_PROT        0xf1600  /* bus 0x7c001600, 睡眠保护使能 */
#define MT_CONN_HIF_STATUS1         0xf1620  /* HIF 状态 1 */
#define MT_CONN_HIF_STATUS2         0xf1630  /* HIF 状态 2 */

/* ============================================================================
 * Pre-reset MCU 寄存器 (ToggleWfsysRst 使用)
 * 基址: bus 0x81023000 = BAR0 + 0xc3000
 * 来源: register_playbook.md 行 333-334, 363-364
 * ============================================================================ */
#define MT_PRE_RESET_MCU_REG1       0xc3f00  /* bus 0x81023f00 */
#define MT_PRE_RESET_MCU_REG2       0xc3008  /* bus 0x81023008 */

/* ============================================================================
 * MCIF 中断重映射 - MCU DMA 写入 host PCIe 内存
 * BAR0+0xd1034 = bus 0x7c021034
 * 来源: conn_bus_cr_von.h, reusable_code.md 行 116-121, 499-500
 * ⚠️ 缺少此配置会导致 MCU 无法 DMA 事件响应到 host RX rings
 * ============================================================================ */
#define MT_CONN_BUS_CR_VON_BASE     0xd1000  /* bus 0x7c021000 */
#define MT_MCIF_REMAP_WF_1_BA       0xd1034  /* PCIE2AP_REMAP_WF_1_BA */
#define MT_MCIF_REMAP_VAL           0x18051803  /* vendor mt6639.c 值 */

/* ============================================================================
 * MT6639/MT7927 特定中断寄存器
 * 总线地址 0x74030188
 * ⚠️ 需要确认 BAR0 映射或 L1 remap
 * 来源: register_playbook.md 行 165-168, 403
 * ============================================================================ */
#define MT_CONN_INFRA_30188         0x010188  /* ⚠️ 映射待确认 */
#define MT_CONN_INFRA_30188_BIT16   BIT(16)

/* ============================================================================
 * MCU DMA0 DUMMY_CR - host<->MCU 同步标志
 * BAR0+0x02120 = bus 0x54000120
 * 来源: reusable_code.md 行 145-150
 * ============================================================================ */
#define MT_MCU_WPDMA0_DUMMY_CR       0x02120
#define MT_WFDMA_NEED_REINIT         BIT(1)  /* DMA 重新初始化标志 */

/* ============================================================================
 * L1 Remap - 访问 0x18xxxxxx 芯片地址
 * 来源: reusable_code.md 行 419-460
 * ============================================================================ */
#define MT_HIF_REMAP_L1              0x155024  /* L1 remap 控制 */
#define MT_HIF_REMAP_L1_MASK         GENMASK(31, 16)
#define MT_HIF_REMAP_BASE_L1         0x130000  /* BAR0 窗口基址 */

/* EMI 睡眠保护 (需要 L1 remap)
 * 芯片地址 0x18011100
 * 来源: reusable_code.md 行 485-488
 */
#define MT_HW_EMI_CTL                0x18011100
#define MT_HW_EMI_CTL_SLPPROT_EN     BIT(1)

/* ============================================================================
 * TXD/RXD 格式定义 - CONNAC3 格式
 * 来源: analysis_windows_full_init.md 行 276-314
 * ============================================================================ */

/* TXD[0] 字段 */
#define MT_TXD0_TX_BYTES            GENMASK(15, 0)   /* 总长度 */
#define MT_TXD0_PKT_FMT             GENMASK(24, 23)  /* 包格式 */
#define MT_TXD0_Q_IDX               GENMASK(31, 25)  /* 队列索引 */

/* TXD[1] 字段 */
#define MT_TXD1_HDR_FORMAT_V3       GENMASK(15, 14)  /* CONNAC3 头格式 */
#define MT_TXD1_HDR_FORMAT          GENMASK(17, 16)  /* CONNAC2 头格式 */
#define MT_TXD1_LONG_FORMAT         BIT(31)          /* ⚠️ Windows 永不设置 */

/* TXD 常量 */
#define MT_TX_TYPE_CMD              2       /* PKT_FMT=2: MCU 命令 */
#define MT_HDR_FORMAT_CMD           1       /* HDR_FORMAT_V3=1 */

/* Q_IDX 值 (Windows 使用 0x20, 不是 2)
 * 来源: analysis_windows_full_init.md 行 306-314
 * ⚠️ Windows: TXD[0] = total_len | 0x41000000
 *   BIT(30) = 1 (QUEUE_INDEX_EXT)
 *   BIT(24) = 1 (PKT_FMT=2)
 *   Q_IDX = 0x20 (bits [31:27,25] = 0b100000)
 */
#define MT_TX_MCU_PORT_RX_Q0        0x20
#define MT_TX_PORT_IDX_MCU          1
#define MCU_PQ_ID(p, q)             (((p) << 15) | ((q) << 10))
#define MCU_PKT_ID                  0xa0

/* RXD 字段 */
#define MT_DMA_CTL_SD_LEN0          GENMASK(29, 16)  /* buffer 大小 */
#define MT_DMA_CTL_LAST_SEC0        BIT(30)
#define MT_DMA_CTL_DMA_DONE         BIT(31)          /* DMA 完成标志 */
#define MT_DMA_MAX_LEN0             FIELD_MAX(MT_DMA_CTL_SD_LEN0)

/* ============================================================================
 * 固件下载相关常量
 * 来源: mt76_connac_mcu.h
 * ============================================================================ */

/* 下载模式标志 */
#define DL_MODE_ENCRYPT             BIT(0)   /* ⚠️ RAM 区域必需 */
#define DL_MODE_KEY_IDX             GENMASK(2, 1)
#define DL_MODE_RESET_SEC_IV        BIT(3)   /* ⚠️ RAM 区域必需 */
#define DL_MODE_WORKING_PDA_CR4     BIT(4)
#define DL_CONFIG_ENCRY_MODE_SEL    BIT(6)
#define DL_MODE_NEED_RSP            BIT(31)

/* 特性标志 */
#define FW_FEATURE_SET_ENCRYPT      BIT(0)
#define FW_FEATURE_SET_KEY_IDX      GENMASK(2, 1)
#define FW_FEATURE_ENCRY_MODE       BIT(4)
#define FW_FEATURE_OVERRIDE_ADDR    BIT(5)
#define FW_FEATURE_NON_DL           BIT(6)

/* 启动标志 */
#define FW_START_OVERRIDE           BIT(0)   /* option=1 */
#define FW_START_WORKING_PDA_CR4    BIT(2)

/* 补丁 section 类型 */
#define PATCH_SEC_NOT_SUPPORT       GENMASK(31, 0)
#define PATCH_SEC_TYPE_MASK         GENMASK(15, 0)
#define PATCH_SEC_TYPE_INFO         0x2
#define PATCH_SEC_ENC_TYPE_MASK     GENMASK(31, 24)
#define PATCH_SEC_ENC_TYPE_PLAIN    0x00
#define PATCH_SEC_ENC_TYPE_AES      0x01
#define PATCH_SEC_ENC_TYPE_SCRAMBLE 0x02
#define PATCH_SEC_ENC_AES_KEY_MASK  GENMASK(7, 0)

/* 补丁信号量控制 */
#define PATCH_SEM_RELEASE           0
#define PATCH_SEM_GET               1
#define PATCH_NOT_DL_SEM_FAIL       0
#define PATCH_IS_DL                 1
#define PATCH_NOT_DL_SEM_SUCCESS    2
#define PATCH_REL_SEM_SUCCESS       3

/* MCU 命令 class (PostFwDownloadInit 序列)
 * 来源: register_playbook.md 行 255-266
 */
#define MCU_CMD_TARGET_ADDRESS_LEN_REQ 0x01
#define MCU_CMD_FW_START_REQ        0x02
#define MCU_CMD_PATCH_START_REQ     0x05
#define MCU_CMD_PATCH_FINISH_REQ    0x07
#define MCU_CMD_PATCH_SEM_CONTROL   0x10
#define MCU_CMD_FW_SCATTER          0xee

/* PostFwDownloadInit MCU 命令 class
 * 来源: analysis_windows_full_init.md 行 739-747
 */
#define MT_MCU_CLASS_NIC_CAP        0x8a  /* NIC capability */
#define MT_MCU_CLASS_CONFIG         0x02  /* Config 命令 */
#define MT_MCU_CLASS_WFDMA_CFG      0xc0  /* WFDMA 配置 */
#define MT_MCU_CLASS_BUF_DL         0xed  /* Buffer 下载 (可选) */
#define MT_MCU_CLASS_DBDC           0x28  /* DBDC (仅 MT6639/MT7927) */
#define MT_MCU_CLASS_SCAN_CFG       0xca  /* Scan/chip/log config */
#define MT_MCU_CLASS_SET_DOMAIN     0x15  /* SET_DOMAIN_INFO — 信道域信息 */
#define MT_MCU_CLASS_BAND_CONFIG    0x08  /* BAND_CONFIG — RTS 阈值等 */
#define MT_MCU_CLASS_EFUSE_CTRL     0x2d  /* EFUSE_CTRL — EEPROM 模式 */
#define MT_MCU_TARGET               0xed  /* 所有命令使用 target=0xed */

#define MT_MCU_CMD_WAKE_RX_PCIE     BIT(0)

/* 补丁地址 */
#define MCU_PATCH_ADDRESS           0x200000       /* mt7925 */
#define MCU_PATCH_ADDRESS_MT6639    0x00900000     /* MT6639 (mt6639.h line 41) */

/* Ring 大小 */
#define MT_TXQ_MCU_WM_RING_SIZE     256
#define MT_TXQ_FWDL_RING_SIZE       128
#define MT_RXQ_MCU_EVENT_RING_SIZE  128
#define MT_RX_RING_SIZE             256  /* HOST RX ring 描述符数量 */
#define MT_RX_BUF_SIZE              2048

/* RX Ring HW 编号 (用于 mt7927_rx_ring_alloc qid 参数)
 * ⚠️⚠️ Windows 使用 Ring 4, 6, 7 — 不是 ring 0/5
 * 来源: analysis_windows_full_init.md 行 104-111
 */
#define MT_RXQ_DATA                 4   /* WiFi 数据帧接收 */
#define MT_RXQ_MCU_EVENT            6   /* MCU 事件 (CONNAC3) */
#define MT_RXQ_DATA2                7   /* 辅助数据接收 */

/* FWDL 最大分块长度 (PCIe) */
#define MT_FWDL_MAX_LEN             4096

/* ============================================================================
 * 数据结构定义
 * ============================================================================ */

/* DMA 描述符 (mt76/dma.h) */
struct mt76_desc {
	__le32 buf0;   /* buffer 物理地址低 32 位 */
	__le32 ctrl;   /* 控制字段 (长度, DONE 标志) */
	__le32 buf1;   /* 保留 */
	__le32 info;   /* 保留 */
} __packed __aligned(4);

/* MCU 命令 TXD (mt76_connac2_mac.h) */
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

/* UniCmd TXD (CONNAC3 固件启动后命令格式) - 0x30 字节
 * vs Legacy connac2_mcu_txd = 0x40 字节 (FWDL 用) */
struct mt7927_mcu_uni_txd {
	__le32 txd[8];           /* 0x00-0x1F: 硬件描述符 */
	__le16 len;              /* 0x20: payload 长度 (不含 txd) */
	__le16 cid;              /* 0x22: UNI_CMD_ID_xxx (16-bit!) */
	u8 rsv;                  /* 0x24 */
	u8 pkt_type;             /* 0x25: 0xa0 */
	u8 frag_n;               /* 0x26 */
	u8 seq;                  /* 0x27 */
	__le16 checksum;         /* 0x28: 通常 0 */
	u8 s2d_index;            /* 0x2A: MCU_S2D_H2N=0 */
	u8 option;               /* 0x2B: 关键! */
	u8 rsv1[4];              /* 0x2C-0x2F */
} __packed __aligned(4);

/* UniCmd option bits */
#define UNI_CMD_OPT_ACK         BIT(0)
#define UNI_CMD_OPT_UNI         BIT(1)
#define UNI_CMD_OPT_SET         BIT(2)
#define UNI_CMD_OPT_QUERY_ACK   (UNI_CMD_OPT_ACK | UNI_CMD_OPT_UNI)  /* 0x03 */
#define UNI_CMD_OPT_SET_ACK     (UNI_CMD_OPT_ACK | UNI_CMD_OPT_UNI | UNI_CMD_OPT_SET) /* 0x07 */

/* UniCmd IDs (class values from Windows CONNAC3 routing table)
 * 来源: ghidra_post_fw_init.md — CONNAC3 Command Routing Table */
#define UNI_CMD_ID_NIC_CAP     0x008a   /* NIC capability query (class=0x8a) */
#define UNI_CMD_ID_CONFIG      0x0002   /* Config cmd (class=0x02) */
#define UNI_CMD_ID_CHIP_CONFIG  0x000E
#define UNI_CMD_ID_POWER_CTRL   0x000F
#define UNI_CMD_ID_MBMC         0x0028   /* DBDC, MT6639 only */
#define UNI_CMD_ID_SCAN_CONFIG  0x00ca   /* Scan/chip/log config */

/* CHIP_CONFIG tags */
#define UNI_CHIP_CONFIG_NIC_CAP   3

/* 补丁头部 (mt76_connac_mcu.h) */
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

/* 补丁 section */
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

/* 固件 trailer */
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

/* 固件 region */
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

/* DMA Ring 结构 */
struct mt7927_ring {
	struct mt76_desc *desc;      /* 描述符虚拟地址 */
	dma_addr_t desc_dma;         /* 描述符物理地址 */
	void **buf;                  /* buffer 指针数组 */
	dma_addr_t *buf_dma;         /* buffer 物理地址数组 */
	u32 buf_size;                /* buffer 大小 */
	u16 qid;                     /* HW ring 编号 */
	u16 ndesc;                   /* 描述符数量 */
	u16 head;                    /* 生产者索引 (CPU) */
	u16 tail;                    /* 消费者索引 (RX 读位置) */
};

/* ============================================================================
 * mac80211 数据结构 — VIF, STA, WCID, PHY
 * ============================================================================ */

/* Forward declarations */
struct mt7927_dev;
struct mt7927_vif;

/* WCID (Wireless Client ID) — 硬件 WTBL 条目 */
struct mt7927_wcid {
	u16 idx;			/* WCID 索引 (0-19) */
	u8 hw_key_idx;			/* 硬件密钥索引 */
	u32 tx_info;			/* TX 信息标志 */
	struct ieee80211_sta *sta;	/* 关联的 STA */
	bool amsdu;			/* 支持 A-MSDU */
};

/* STA 记录 */
struct mt7927_sta {
	struct mt7927_wcid wcid;	/* 必须是第一个字段 */
	struct mt7927_vif *vif;		/* 所属 VIF */
};

/* 虚拟接口 */
struct mt7927_vif {
	u8 bss_idx;			/* BSS 索引 */
	u8 omac_idx;			/* OMAC 索引 */
	u8 wmm_idx;			/* WMM 集索引 */
	u8 band_idx;			/* 频段索引 */
	struct mt7927_sta sta;		/* 内嵌 STA (STA 模式自身) */
	struct ieee80211_vif *vif;	/* mac80211 VIF */
};

/* PHY 层信息 */
struct mt7927_phy {
	struct ieee80211_hw *hw;	/* mac80211 硬件 */
	struct mt7927_dev *dev;		/* 父设备指针 */

	struct {
		bool has_2ghz;
		bool has_5ghz;
		bool has_6ghz;
	} cap;

	u8 antenna_mask;		/* 天线掩码 (0x03 = 2x2) */
	u16 chainmask;			/* 链路掩码 */
	struct sk_buff_head scan_event_list;	/* 扫描事件队列 */
	u64 chip_cap;			/* 芯片能力位图 */
};

/* 设备主结构体 */
struct mt7927_dev {
	struct pci_dev *pdev;
	void __iomem *bar0;
	resource_size_t bar0_len;

	/* TX Rings */
	struct mt7927_ring ring_wm;      /* TX ring 15 - MCU 命令 */
	struct mt7927_ring ring_fwdl;    /* TX ring 16 - FWDL */

	/* RX Rings (Windows: 4, 6, 7 - 不使用 ring 0/5)
	 * ⚠️⚠️⚠️ 不要使用 ring_rx5, 应该是 ring_rx6
	 */
	struct mt7927_ring ring_rx4;     /* RX ring 4 - WiFi 数据 */
	struct mt7927_ring ring_rx6;     /* RX ring 6 - MCU 事件 (CONNAC3) */
	struct mt7927_ring ring_rx7;     /* RX ring 7 - 数据/事件 */

	/* 固件状态 */
	bool fw_loaded;
	u32 fw_sync;

	/* MCU 序列号 */
	u8 mcu_seq;

	/* mac80211 集成 */
	struct ieee80211_hw *hw;		/* mac80211 硬件抽象 */
	struct mt7927_phy phy;			/* PHY 层信息 */

	/* VIF/STA 管理 */
	u64 vif_mask;				/* VIF 分配位图 */
	u64 omac_mask;				/* OMAC 地址分配位图 */
	struct mt7927_wcid *wcid[20];		/* WCID 池 (MT792x_WTBL_SIZE=20) */

	/* 工作队列 */
	struct work_struct init_work;		/* 异步初始化 */
	struct delayed_work scan_work;		/* 扫描工作队列 */
	wait_queue_head_t mcu_wait;		/* MCU 等待队列 */
	bool hw_init_done;			/* 硬件初始化完成标志 */

	/* 扫描状态 */
	u8 scan_seq_num;			/* 扫描序列号 */
	unsigned long scan_state;		/* BIT(0)=SCANNING */

	/* 中断/NAPI */
	struct tasklet_struct irq_tasklet;	/* 中断下半部 */
	struct napi_struct napi_rx_data;	/* RX 数据 NAPI */
	struct napi_struct napi_rx_mcu;		/* RX MCU NAPI */
	struct napi_struct tx_napi;		/* TX NAPI */
	struct net_device *napi_dev;		/* NAPI dummy netdev */
	u32 int_mask;				/* 中断掩码 */

	/* 数据 TX ring */
	struct mt7927_ring ring_tx0;		/* TX ring 0 - 数据 */
};

/* ============================================================================
 * 辅助宏
 * ============================================================================ */

/* 预取配置宏 (旧的 per-ring 格式, 已被 packed 格式替代) */
#define PREFETCH(base, depth)       (((base) << 16) | (depth))

/* ============================================================================
 * CONNAC3 TXD 完整定义 (数据帧用)
 * 来源: mt76/mt76_connac3_mac.h
 * 注意: TXD0 部分宏已在上方定义 (MT_TXD0_TX_BYTES 等)
 * ============================================================================ */

#define MT_TXD_SIZE			(8 * 4)		/* 32 字节 */

/* TXD[0] 补充 */
#define MT_TXD0_ETH_TYPE_OFFSET		GENMASK(22, 16)

/* TXD[1] 完整字段 (CONNAC3) */
#define MT_TXD1_FIXED_RATE		BIT(31)
#define MT_TXD1_OWN_MAC			GENMASK(30, 25)
#define MT_TXD1_TID			GENMASK(24, 21)
#define MT_TXD1_BIP			BIT(24)
#define MT_TXD1_ETH_802_3		BIT(20)
#define MT_TXD1_HDR_INFO		GENMASK(20, 16)
#define MT_TXD1_TGID			GENMASK(13, 12)
#define MT_TXD1_WLAN_IDX		GENMASK(11, 0)

/* TXD[2] */
#define MT_TXD2_POWER_OFFSET		GENMASK(31, 26)
#define MT_TXD2_MAX_TX_TIME		GENMASK(25, 16)
#define MT_TXD2_FRAG			GENMASK(15, 14)
#define MT_TXD2_HTC_VLD			BIT(13)
#define MT_TXD2_DURATION		BIT(12)
#define MT_TXD2_HDR_PAD			GENMASK(11, 10)
#define MT_TXD2_RTS			BIT(9)
#define MT_TXD2_OWN_MAC_MAP		BIT(8)
#define MT_TXD2_BF_TYPE			GENMASK(7, 6)
#define MT_TXD2_FRAME_TYPE		GENMASK(5, 4)
#define MT_TXD2_SUB_TYPE		GENMASK(3, 0)

/* TXD[3] */
#define MT_TXD3_SN_VALID		BIT(31)
#define MT_TXD3_PN_VALID		BIT(30)
#define MT_TXD3_SW_POWER_MGMT		BIT(29)
#define MT_TXD3_BA_DISABLE		BIT(28)
#define MT_TXD3_SEQ			GENMASK(27, 16)
#define MT_TXD3_REM_TX_COUNT		GENMASK(15, 11)
#define MT_TXD3_TX_COUNT		GENMASK(10, 6)
#define MT_TXD3_HW_AMSDU		BIT(5)
#define MT_TXD3_BCM			BIT(4)
#define MT_TXD3_EEOSP			BIT(3)
#define MT_TXD3_EMRD			BIT(2)
#define MT_TXD3_PROTECT_FRAME		BIT(1)
#define MT_TXD3_NO_ACK			BIT(0)

/* TXD[4] */
#define MT_TXD4_PN_LOW			GENMASK(31, 0)

/* TXD[5] */
#define MT_TXD5_PN_HIGH			GENMASK(31, 16)
#define MT_TXD5_FL			BIT(15)
#define MT_TXD5_BYPASS_TBB		BIT(14)
#define MT_TXD5_BYPASS_RBB		BIT(13)
#define MT_TXD5_BSS_COLOR_ZERO		BIT(12)
#define MT_TXD5_TX_STATUS_HOST		BIT(10)
#define MT_TXD5_TX_STATUS_MCU		BIT(9)
#define MT_TXD5_TX_STATUS_FMT		BIT(8)
#define MT_TXD5_PID			GENMASK(7, 0)

/* TXD[6] */
#define MT_TXD6_TX_SRC			GENMASK(31, 30)
#define MT_TXD6_VTA			BIT(28)
#define MT_TXD6_FIXED_BW		BIT(25)
#define MT_TXD6_BW			GENMASK(24, 22)
#define MT_TXD6_TX_RATE			GENMASK(21, 16)
#define MT_TXD6_TIMESTAMP_OFS_EN	BIT(15)
#define MT_TXD6_TIMESTAMP_OFS_IDX	GENMASK(14, 10)
#define MT_TXD6_MSDU_CNT		GENMASK(9, 4)
#define MT_TXD6_MSDU_CNT_V2		GENMASK(15, 10)
#define MT_TXD6_DIS_MAT			BIT(3)
#define MT_TXD6_DAS			BIT(2)
#define MT_TXD6_AMSDU_CAP		BIT(1)

/* TXD[7] */
#define MT_TXD7_TXD_LEN			GENMASK(31, 30)
#define MT_TXD7_IP_SUM			BIT(29)
#define MT_TXD7_DROP_BY_SDO		BIT(28)
#define MT_TXD7_MAC_TXD			BIT(27)
#define MT_TXD7_CTXD			BIT(26)
#define MT_TXD7_CTXD_CNT		GENMASK(25, 22)
#define MT_TXD7_UDP_TCP_SUM		BIT(15)
#define MT_TXD7_TX_TIME			GENMASK(9, 0)

/* TXD[9] (long format) */
#define MT_TXD9_WLAN_IDX		GENMASK(23, 8)

/* TX Rate encoding */
#define MT_TX_RATE_STBC			BIT(14)
#define MT_TX_RATE_NSS			GENMASK(13, 10)
#define MT_TX_RATE_MODE			GENMASK(9, 6)
#define MT_TX_RATE_SU_EXT_TONE		BIT(5)
#define MT_TX_RATE_DCM			BIT(4)
#define MT_TX_RATE_IDX			GENMASK(5, 0)

/* ============================================================================
 * CONNAC3 RXD 定义
 * 来源: mt76/mt76_connac3_mac.h
 * ============================================================================ */

/* RXD[0] */
#define MT_RXD0_LENGTH			GENMASK(15, 0)
#define MT_RXD0_PKT_FLAG		GENMASK(19, 16)
#define MT_RXD0_PKT_TYPE		GENMASK(31, 27)
#define MT_RXD0_MESH			BIT(18)
#define MT_RXD0_MHCP			BIT(19)
#define MT_RXD0_NORMAL_ETH_TYPE_OFS	GENMASK(22, 16)
#define MT_RXD0_SW_PKT_TYPE_MASK	GENMASK(31, 16)
#define MT_RXD0_SW_PKT_TYPE_MAP		0x380F
#define MT_RXD0_SW_PKT_TYPE_FRAME	0x3801

/* RXD[1] */
#define MT_RXD1_NORMAL_WLAN_IDX		GENMASK(11, 0)
#define MT_RXD1_NORMAL_GROUP_1		BIT(16)
#define MT_RXD1_NORMAL_GROUP_2		BIT(17)
#define MT_RXD1_NORMAL_GROUP_3		BIT(18)
#define MT_RXD1_NORMAL_GROUP_4		BIT(19)
#define MT_RXD1_NORMAL_GROUP_5		BIT(20)
#define MT_RXD1_NORMAL_KEY_ID		GENMASK(22, 21)
#define MT_RXD1_NORMAL_CM		BIT(23)
#define MT_RXD1_NORMAL_CLM		BIT(24)
#define MT_RXD1_NORMAL_ICV_ERR		BIT(25)
#define MT_RXD1_NORMAL_TKIP_MIC_ERR	BIT(26)
#define MT_RXD1_NORMAL_BAND_IDX		GENMASK(28, 27)
#define MT_RXD1_NORMAL_SPP_EN		BIT(29)
#define MT_RXD1_NORMAL_ADD_OM		BIT(30)
#define MT_RXD1_NORMAL_SEC_DONE		BIT(31)

/* RXD[2] */
#define MT_RXD2_NORMAL_BSSID		GENMASK(5, 0)
#define MT_RXD2_NORMAL_HDR_TRANS	BIT(7)
#define MT_RXD2_NORMAL_MAC_HDR_LEN	GENMASK(12, 8)
#define MT_RXD2_NORMAL_HDR_OFFSET	GENMASK(15, 13)
#define MT_RXD2_NORMAL_SEC_MODE		GENMASK(20, 16)
#define MT_RXD2_NORMAL_MU_BAR		BIT(21)
#define MT_RXD2_NORMAL_SW_BIT		BIT(22)
#define MT_RXD2_NORMAL_AMSDU_ERR	BIT(23)
#define MT_RXD2_NORMAL_MAX_LEN_ERROR	BIT(24)
#define MT_RXD2_NORMAL_HDR_TRANS_ERROR	BIT(25)
#define MT_RXD2_NORMAL_INT_FRAME	BIT(26)
#define MT_RXD2_NORMAL_FRAG		BIT(27)
#define MT_RXD2_NORMAL_NULL_FRAME	BIT(28)
#define MT_RXD2_NORMAL_NDATA		BIT(29)
#define MT_RXD2_NORMAL_NON_AMPDU	BIT(30)
#define MT_RXD2_NORMAL_BF_REPORT	BIT(31)

/* RXD[3] */
#define MT_RXD3_NORMAL_RXV_SEQ		GENMASK(7, 0)
#define MT_RXD3_NORMAL_CH_FREQ		GENMASK(15, 8)
#define MT_RXD3_NORMAL_ADDR_TYPE	GENMASK(17, 16)
#define MT_RXD3_NORMAL_U2M		BIT(0)
#define MT_RXD3_NORMAL_HTC_VLD		BIT(18)
#define MT_RXD3_NORMAL_BEACON_MC	BIT(20)
#define MT_RXD3_NORMAL_BEACON_UC	BIT(21)
#define MT_RXD3_NORMAL_CO_ANT		BIT(22)
#define MT_RXD3_NORMAL_FCS_ERR		BIT(24)
#define MT_RXD3_NORMAL_IP_SUM		BIT(26)
#define MT_RXD3_NORMAL_UDP_TCP_SUM	BIT(27)
#define MT_RXD3_NORMAL_VLAN2ETH		BIT(31)

/* RXD[4] */
#define MT_RXD4_NORMAL_PAYLOAD_FORMAT	GENMASK(1, 0)
#define MT_RXD4_FIRST_AMSDU_FRAME	GENMASK(1, 0)
#define MT_RXD4_MID_AMSDU_FRAME		BIT(1)
#define MT_RXD4_LAST_AMSDU_FRAME	BIT(0)

/* RXD GROUP4 (802.11 header fields) */
#define MT_RXD8_FRAME_CONTROL		GENMASK(15, 0)
#define MT_RXD10_SEQ_CTRL		GENMASK(15, 0)
#define MT_RXD10_QOS_CTL		GENMASK(31, 16)
#define MT_RXD11_HT_CONTROL		GENMASK(31, 0)

/* RXV Header */
#define MT_RXV_HDR_BAND_IDX		BIT(24)

/* ============================================================================
 * P-RXV (Physical RX Vector) — 速率信息提取
 * 来源: mt76/mt76_connac3_mac.h
 * ============================================================================ */

#define MT_PRXV_TX_RATE			GENMASK(6, 0)
#define MT_PRXV_TX_DCM			BIT(4)
#define MT_PRXV_TX_ER_SU_106T		BIT(5)
#define MT_PRXV_NSTS			GENMASK(10, 7)
#define MT_PRXV_TXBF			BIT(11)
#define MT_PRXV_HT_AD_CODE		BIT(12)
#define MT_PRXV_HE_RU_ALLOC		GENMASK(30, 22)
#define MT_PRXV_RCPI3			GENMASK(31, 24)
#define MT_PRXV_RCPI2			GENMASK(23, 16)
#define MT_PRXV_RCPI1			GENMASK(15, 8)
#define MT_PRXV_RCPI0			GENMASK(7, 0)
#define MT_PRXV_HT_SHORT_GI		GENMASK(4, 3)
#define MT_PRXV_HT_STBC			GENMASK(10, 9)
#define MT_PRXV_TX_MODE			GENMASK(14, 11)
#define MT_PRXV_FRAME_MODE		GENMASK(2, 0)
#define MT_PRXV_DCM			BIT(5)

/* ============================================================================
 * TXS (TX Status) 宏定义
 * 来源: mt76/mt76_connac3_mac.h
 * ============================================================================ */

#define MT_TXS_HDR_SIZE			4	/* DWORDs */
#define MT_TXS_SIZE			12	/* DWORDs */

#define MT_TXS0_BW			GENMASK(31, 29)
#define MT_TXS0_TID			GENMASK(28, 26)
#define MT_TXS0_AMPDU			BIT(25)
#define MT_TXS0_TXS_FORMAT		GENMASK(24, 23)
#define MT_TXS0_BA_ERROR		BIT(22)
#define MT_TXS0_PS_FLAG			BIT(21)
#define MT_TXS0_TXOP_TIMEOUT		BIT(20)
#define MT_TXS0_BIP_ERROR		BIT(19)
#define MT_TXS0_QUEUE_TIMEOUT		BIT(18)
#define MT_TXS0_RTS_TIMEOUT		BIT(17)
#define MT_TXS0_ACK_TIMEOUT		BIT(16)
#define MT_TXS0_ACK_ERROR_MASK		GENMASK(18, 16)
#define MT_TXS0_TX_STATUS_HOST		BIT(15)
#define MT_TXS0_TX_STATUS_MCU		BIT(14)
#define MT_TXS0_TX_RATE			GENMASK(13, 0)

#define MT_TXS1_SEQNO			GENMASK(31, 20)
#define MT_TXS1_RESP_RATE		GENMASK(19, 16)
#define MT_TXS1_RXV_SEQNO		GENMASK(15, 8)
#define MT_TXS1_TX_POWER_DBM		GENMASK(7, 0)

#define MT_TXS2_BF_STATUS		GENMASK(31, 30)
#define MT_TXS2_BAND			GENMASK(29, 28)
#define MT_TXS2_WCID			GENMASK(27, 16)
#define MT_TXS2_TX_DELAY		GENMASK(15, 0)

#define MT_TXS3_PID			GENMASK(31, 24)
#define MT_TXS3_RATE_STBC		BIT(7)
#define MT_TXS3_FIXED_RATE		BIT(6)
#define MT_TXS3_SRC			GENMASK(5, 4)
#define MT_TXS3_SHARED_ANTENNA		BIT(3)
#define MT_TXS3_LAST_TX_RATE		GENMASK(2, 0)

#define MT_TXS4_TIMESTAMP		GENMASK(31, 0)

/* TXFREE (TX 完成事件) */
#define MT_TXFREE0_PKT_TYPE		GENMASK(31, 27)
#define MT_TXFREE0_MSDU_CNT		GENMASK(25, 16)
#define MT_TXFREE0_RX_BYTE		GENMASK(15, 0)

#define MT_TXFREE1_VER			GENMASK(19, 16)

#define MT_TXFREE_INFO_PAIR		BIT(31)
#define MT_TXFREE_INFO_HEADER		BIT(30)
#define MT_TXFREE_INFO_WLAN_ID		GENMASK(23, 12)
#define MT_TXFREE_INFO_MSDU_ID		GENMASK(14, 0)
#define MT_TXFREE_INFO_COUNT		GENMASK(27, 24)
#define MT_TXFREE_INFO_STAT		GENMASK(29, 28)

/* ============================================================================
 * 枚举定义
 * ============================================================================ */

/* TX 头格式 */
enum tx_header_format {
	MT_HDR_FORMAT_802_3,		/* 0: 以太网 */
	MT_HDR_FORMAT_802_11 = 2,	/* 2: 原生 WiFi */
	MT_HDR_FORMAT_802_11_EXT,	/* 3: 扩展 */
};

/* TX 包类型 */
enum tx_pkt_type {
	MT_TX_TYPE_CT,			/* 0: Cut-through (数据帧) */
	MT_TX_TYPE_SF,			/* 1: Store-and-forward */
	MT_TX_TYPE_FW = 3,		/* 3: 固件下载 */
};

/* TX 分片索引 */
enum tx_frag_idx {
	MT_TX_FRAG_NONE,
	MT_TX_FRAG_FIRST,
	MT_TX_FRAG_MID,
	MT_TX_FRAG_LAST,
};

/* RX 包类型 */
#define PKT_TYPE_NORMAL			0
#define PKT_TYPE_RX_EVENT		1
#define PKT_TYPE_NORMAL_MCU		1
#define PKT_TYPE_TXRX_NOTIFY		6
#define PKT_TYPE_TXS			7

/* PHY 类型 */
enum mt76_phy_type {
	MT_PHY_TYPE_CCK,
	MT_PHY_TYPE_OFDM,
	MT_PHY_TYPE_HT,
	MT_PHY_TYPE_HT_GF,
	MT_PHY_TYPE_VHT,
	MT_PHY_TYPE_HE_SU = 8,
	MT_PHY_TYPE_HE_EXT_SU,
	MT_PHY_TYPE_HE_TB,
	MT_PHY_TYPE_HE_MU,
	MT_PHY_TYPE_EHT_SU = 13,
	MT_PHY_TYPE_EHT_TRIG,
	MT_PHY_TYPE_EHT_MU,
};

/* ADDR_TYPE 地址类型 (RXD) */
#define MT_RXD_ADDR_U2M			1	/* unicast to me */
#define MT_RXD_ADDR_MCAST		2	/* multicast */
#define MT_RXD_ADDR_BCAST		3	/* broadcast */

/* 扫描状态位 */
#define MT7927_SCANNING			0

/* WCID 常量 */
#define MT7927_WTBL_SIZE		20

/* ============================================================================
 * 扫描 TLV 结构体
 * 来源: mt76/mt7925/mcu.c
 * ============================================================================ */

/* 扫描命令 CID */
#define MCU_UNI_CMD_SCAN_REQ		0x16

/* 扫描事件 EID */
#define MCU_UNI_EVENT_SCAN_DONE		0x0e

/* 扫描常量 */
#define MT7927_SCAN_MAX_SSIDS		10
#define MT7927_SCAN_MAX_CHANNELS	64
#define MT7927_SCAN_IE_LEN		600
#define MT7927_HW_SCAN_TIMEOUT		(30 * HZ)

/* 扫描请求 TLV 类型 */
enum {
	UNI_SCAN_REQ = 1,
	UNI_SCAN_CANCEL = 2,
	UNI_SCAN_SSID = 10,
	UNI_SCAN_BSSID = 11,
	UNI_SCAN_CHANNEL = 12,
	UNI_SCAN_MISC = 13,
	UNI_SCAN_IE = 14,
};

/* 扫描事件 TLV 类型 */
enum {
	UNI_EVENT_SCAN_DONE_BASIC = 0,
	UNI_EVENT_SCAN_DONE_CHNLINFO = 2,
	UNI_EVENT_SCAN_DONE_NLO = 3,
};

/* 扫描功能标志 */
#define SCAN_FUNC_RANDOM_MAC		BIT(0)
#define SCAN_FUNC_SPLIT_SCAN		BIT(2)

/* 通用 TLV 头 */
struct mt7927_tlv {
	__le16 tag;
	__le16 len;
} __packed;

/* 扫描头 (固定) */
struct scan_hdr_tlv {
	u8 seq_num;
	u8 bss_idx;
	u8 pad[2];
} __packed;

/* UNI_SCAN_REQ (tag=1) */
struct scan_req_tlv {
	__le16 tag;
	__le16 len;
	u8 scan_type;
	u8 probe_req_num;
	u8 scan_func;
	u8 src_mask;
	__le16 channel_min_dwell_time;
	__le16 channel_dwell_time;
	__le16 timeout_value;
	__le16 probe_delay_time;
	__le32 func_mask_ext;
} __packed;

/* SSID 结构 */
struct scan_ssid {
	__le32 ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
} __packed;

/* UNI_SCAN_SSID (tag=10) */
struct scan_ssid_tlv {
	__le16 tag;
	__le16 len;
	u8 ssid_type;
	u8 ssids_num;
	u8 is_short_ssid;
	u8 pad;
	struct scan_ssid ssids[MT7927_SCAN_MAX_SSIDS];
} __packed;

/* UNI_SCAN_BSSID (tag=11) */
struct scan_bssid_tlv {
	__le16 tag;
	__le16 len;
	u8 bssid[ETH_ALEN];
	u8 match_ch;
	u8 match_ssid_ind;
	u8 rcpi;
	u8 match_short_ssid_ind;
	u8 pad[2];
} __packed;

/* 信道描述 */
struct scan_channel {
	u8 band;
	u8 channel_num;
} __packed;

/* UNI_SCAN_CHANNEL (tag=12) */
struct scan_chan_info_tlv {
	__le16 tag;
	__le16 len;
	u8 channel_type;
	u8 channels_num;
	u8 pad[2];
	struct scan_channel channels[MT7927_SCAN_MAX_CHANNELS];
} __packed;

/* UNI_SCAN_MISC (tag=13) */
struct scan_misc_tlv {
	__le16 tag;
	__le16 len;
	u8 random_mac[ETH_ALEN];
	u8 rsv[2];
} __packed;

/* UNI_SCAN_IE (tag=14) */
struct scan_ie_tlv {
	__le16 tag;
	__le16 len;
	__le16 ies_len;
	u8 band;
	u8 pad;
	u8 ies[];
} __packed;

/* 扫描事件: 信道信息 */
struct mt7927_mcu_scan_chinfo_event {
	u8 nr_chan;
	u8 alpha2[3];
} __packed;

/* ============================================================================
 * 连接/密钥 TLV 结构体
 * 来源: mt76/mt7925/mcu.c, mt76/mt76_connac_mcu.h
 * ============================================================================ */

/* 连接相关 CID */
#define MCU_UNI_CMD_DEV_INFO		0x01
#define MCU_UNI_CMD_BSS_INFO		0x02
#define MCU_UNI_CMD_STA_REC		0x03

/* 网络类型 */
#define NETWORK_INFRA			BIT(16)
#define CONNECTION_INFRA_STA		(BIT(0) | BIT(16))	/* 0x10001 */
#define CONNECTION_INFRA_AP		(BIT(2) | BIT(16))

/* 连接状态 */
#define CONN_STATE_DISCONNECT		0
#define CONN_STATE_CONNECT		1
#define CONN_STATE_PORT_SECURE		2

/* BSS_INFO_UPDATE TLV tags */
enum {
	UNI_BSS_INFO_BASIC = 0,
	UNI_BSS_INFO_RLM = 2,
	UNI_BSS_INFO_BSS_COLOR = 5,
	UNI_BSS_INFO_HE = 7,
	UNI_BSS_INFO_RATE = 11,
	UNI_BSS_INFO_QBSS = 15,
	UNI_BSS_INFO_SEC = 16,
	UNI_BSS_INFO_MLD = 26,
};

/* DEV_INFO_UPDATE TLV tags */
enum {
	UNI_DEV_INFO_ACTIVE = 1,
};

/* STA_REC TLV tags */
enum {
	STA_REC_BASIC = 0,
	STA_REC_RA = 1,
	STA_REC_STATE = 2,
	STA_REC_HT = 7,
	STA_REC_VHT = 8,
	STA_REC_APPS = 9,
	STA_REC_PHY = 0x15,
	STA_REC_HE = 0x17,
	STA_REC_HE_6G = 0x25,
	STA_REC_KEY_V3 = 0x27,
	STA_REC_HDR_TRANS = 0x2b,
	STA_REC_EHT = 0x22,
	STA_REC_MLD = 0x20,
};

/* EXTRA_INFO 标志 */
#define EXTRA_INFO_VER			BIT(0)
#define EXTRA_INFO_NEW			BIT(1)

/* 密钥类型枚举 */
enum connac3_mcu_cipher_type {
	CONNAC3_CIPHER_NONE = 0,
	CONNAC3_CIPHER_WEP40 = 1,
	CONNAC3_CIPHER_TKIP = 2,
	CONNAC3_CIPHER_AES_CCMP = 4,
	CONNAC3_CIPHER_WEP104 = 5,
	CONNAC3_CIPHER_BIP_CMAC_128 = 6,
	CONNAC3_CIPHER_WEP128 = 7,
	CONNAC3_CIPHER_WAPI = 8,
	CONNAC3_CIPHER_CCMP_256 = 10,
	CONNAC3_CIPHER_GCMP = 11,
	CONNAC3_CIPHER_GCMP_256 = 12,
};

/* BSS 请求头 */
struct bss_req_hdr {
	u8 bss_idx;
	u8 __rsv[3];
} __packed;

/* STA 请求头 */
struct sta_req_hdr {
	u8 bss_idx;
	u8 wlan_idx_lo;
	__le16 tlv_num;
	u8 is_tlv_append;
	u8 muar_idx;
	u8 wlan_idx_hi;
	u8 __rsv;
} __packed;

/* DEV_INFO_ACTIVE TLV (tag=1) */
struct dev_info_active_tlv {
	__le16 tag;
	__le16 len;
	u8 active;
	u8 band_idx;
	u8 omac_idx;
	u8 __rsv;
	u8 omac_addr[ETH_ALEN];
	u8 pad[2];
} __packed;

/* BSS_INFO_BASIC TLV (tag=0)
 * 来源: mt76/mt76_connac_mcu.h line 1452 — 必须和固件结构体一致! */
struct mt76_connac_bss_basic_tlv {
	__le16 tag;
	__le16 len;
	u8 active;
	u8 omac_idx;
	u8 hw_bss_idx;
	u8 band_idx;
	__le32 conn_type;
	u8 conn_state;
	u8 wmm_idx;
	u8 bssid[ETH_ALEN];
	__le16 bmc_tx_wlan_idx;
	__le16 bcn_interval;
	u8 dtim_period;
	u8 phymode;
	__le16 sta_idx;
	__le16 nonht_basic_phy;
	u8 phymode_ext;
	u8 link_idx;
} __packed;

/* BSS_INFO_RLM TLV */
struct bss_rlm_tlv {
	__le16 tag;
	__le16 len;
	u8 control_channel;
	u8 center_chan;
	u8 center_chan2;
	u8 bw;
	u8 tx_streams;
	u8 rx_streams;
	u8 ht_op_info;
	u8 sco;
	u8 band;
	u8 pad[3];
} __packed;

/* STA_REC_BASIC TLV (tag=0) */
struct sta_rec_basic {
	__le16 tag;
	__le16 len;
	__le32 conn_type;
	u8 conn_state;
	u8 qos;
	__le16 aid;
	u8 peer_addr[ETH_ALEN];
	__le16 extra_info;
} __packed;

/* STA_REC_PHY TLV (tag=0x15) */
struct sta_rec_phy {
	__le16 tag;
	__le16 len;
	__le16 basic_rate;
	u8 phy_type;
	u8 ampdu;
	u8 rts_policy;
	u8 rcpi;
	u8 __rsv[2];
} __packed;

/* STA_REC_HDR_TRANS TLV (tag=0x2B) */
struct sta_rec_hdr_trans {
	__le16 tag;
	__le16 len;
	u8 from_ds;
	u8 to_ds;
	u8 dis_rx_hdr_tran;
	u8 __rsv;
} __packed;

/* STA_REC_KEY_V3 TLV (tag=0x27) — 密钥安装 */
struct sta_rec_sec_uni {
	__le16 tag;
	__le16 len;
	u8 add;
	u8 tx_key;
	u8 key_type;
	u8 is_authenticator;
	u8 peer_addr[ETH_ALEN];
	u8 bss_idx;
	u8 cipher_id;
	u8 key_id;
	u8 key_len;
	u8 wlan_idx;
	u8 mgmt_prot;
	u8 key[32];
	u8 key_rsc[16];
} __packed;

/* MCU RXD (UniCmd 事件头, 0x2c 字节) */
struct mt7927_mcu_rxd {
	__le32 rxd[8];
	__le16 len;
	__le16 pkt_type_id;
	u8 eid;
	u8 seq;
	u8 option;
	u8 __rsv;
	u8 ext_eid;
	u8 __rsv1[2];
	u8 s2d_index;
} __packed;

/* MCU unsolicited event 标志 */
#define MCU_UNI_CMD_UNSOLICITED_EVENT	BIT(0)

/* ============================================================================
 * 频段/信道常量
 * ============================================================================ */

/* 信道宏 — 在 .c 文件中用于定义信道数组 */
#define CHAN2G(chan, freq)					\
	{ .band = NL80211_BAND_2GHZ, .center_freq = (freq),	\
	  .hw_value = (chan), .max_power = 20 }

#define CHAN5G(chan, freq)					\
	{ .band = NL80211_BAND_5GHZ, .center_freq = (freq),	\
	  .hw_value = (chan), .max_power = 20 }

/* TX ring 常量 (数据) */
#define MT_TXQ_DATA_RING		0
#define MT_TXQ_DATA_RING_SIZE		256

/* ============================================================================
 * 函数声明 (mt7927_mac.c, mt7927_dma.c)
 * ============================================================================ */

/* RX queue ID (用于 mt7927_queue_rx_skb 参数) */
enum mt76_rxq_id {
	MT_RXQ_MAIN,		/* 数据 RX */
	MT_RXQ_MCU,		/* MCU 事件 RX */
};

/* mac.c — TXD 构造 */
void mt7927_mac_write_txwi(struct mt7927_dev *dev, __le32 *txwi,
			    struct sk_buff *skb, struct mt7927_wcid *wcid,
			    struct ieee80211_key_conf *key, int pid,
			    bool beacon);
int mt7927_tx_prepare_skb(struct mt7927_dev *dev, struct sk_buff *skb,
			  struct mt7927_wcid *wcid);

/* mac.c — RXD 解析 */
int mt7927_mac_fill_rx(struct mt7927_dev *dev, struct sk_buff *skb);
void mt7927_mac_fill_rx_rate(struct mt7927_dev *dev,
			     struct ieee80211_rx_status *status,
			     struct ieee80211_supported_band *sband,
			     __le32 *rxv);
void mt7927_queue_rx_skb(struct mt7927_dev *dev, enum mt76_rxq_id q,
			 struct sk_buff *skb);

/* dma.c — 中断/NAPI */
irqreturn_t mt7927_irq_handler(int irq, void *dev_instance);
void mt7927_irq_tasklet(unsigned long data);
int mt7927_poll_rx_data(struct napi_struct *napi, int budget);
int mt7927_poll_rx_mcu(struct napi_struct *napi, int budget);
int mt7927_poll_tx(struct napi_struct *napi, int budget);

/* dma.c — TX/RX */
int mt7927_tx_queue_skb(struct mt7927_dev *dev, struct mt7927_ring *ring,
			struct sk_buff *skb);
void mt7927_tx_kick(struct mt7927_dev *dev, struct mt7927_ring *ring);
void mt7927_tx_complete(struct mt7927_dev *dev, struct mt7927_ring *ring);
int mt7927_dma_init_data_rings(struct mt7927_dev *dev);
void mt7927_rx_refill(struct mt7927_dev *dev, struct mt7927_ring *ring);

#endif /* __MT7927_PCI_H */
