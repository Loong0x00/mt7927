# CONNAC3 UniCmd MCU 命令格式分析

**创建时间**: 2026-02-15
**目标**: 理解 MT6639/MT7927 固件启动后使用的 CONNAC3 UniCmd 格式

---

## 背景

我们的驱动当前状况：
- ✅ **固件下载 (FWDL) 成功** — fw_sync=0x3，使用 legacy connac2_mcu_txd 格式
- ❌ **固件启动后 MCU 命令超时** — NIC_CAPABILITY (class=0x8a) 等命令失败 (-110)

**根本原因**: MCU dispatch 有两条路径：
1. **Legacy 路径** (FWDL 用) — 0x40 字节 header，flag=0
2. **CONNAC3 UniCmd 路径** (固件启动后用) — 0x30 字节 header，flag=1

FWDL 命令用 legacy 格式，ROM 能处理。固件启动后需要切换到 UniCmd 格式。

---

## 一、UniCmd vs Legacy 结构对比

### 1.1 Legacy TXD (connac2_mcu_txd) — 0x40 字节

```c
struct mt76_connac2_mcu_txd {
	__le32 txd[8];           // 0x00-0x1F: 硬件描述符 (32 字节)

	// 0x20-0x2F: MCU 命令头 (16 字节)
	__le16 len;              // 0x20: payload 长度 (不含 txd[8])
	__le16 pq_id;            // 0x22: PQ_ID

	u8 cid;                  // 0x24: 命令 ID (class)
	u8 pkt_type;             // 0x25: 0xa0
	u8 set_query;            // 0x26: MCU_Q_SET/QUERY/NA
	u8 seq;                  // 0x27: 序列号

	u8 uc_d2b0_rev;          // 0x28: reserved
	u8 ext_cid;              // 0x29: ext_cid
	u8 s2d_index;            // 0x2A: MCU_S2D_H2N/H2C
	u8 ext_cid_ack;          // 0x2B: ext_cid_ack

	u32 rsv[5];              // 0x2C-0x3F: reserved (20 字节)
} __packed __aligned(4);     // 总大小: 64 字节 (0x40)
```

### 1.2 UniCmd TXD (mt76_connac2_mcu_uni_txd) — 0x30 字节

```c
struct mt76_connac2_mcu_uni_txd {
	__le32 txd[8];           // 0x00-0x1F: 硬件描述符 (32 字节)

	// DW8 (0x20-0x23): len + cid
	__le16 len;              // 0x20: payload 长度 (不含 txd[8])
	__le16 cid;              // 0x22: 命令 ID (UNI_CMD_ID_xxx)

	// DW9 (0x24-0x27): pkt_type + seq
	u8 rsv;                  // 0x24: reserved
	u8 pkt_type;             // 0x25: 0xa0 (MCU_PKT_ID)
	u8 frag_n;               // 0x26: fragment number
	u8 seq;                  // 0x27: 序列号

	// DW10 (0x28-0x2B): checksum + option
	__le16 checksum;         // 0x28: checksum (通常为 0)
	u8 s2d_index;            // 0x2A: MCU_S2D_H2N/H2C/H2N_AND_H2C
	u8 option;               // 0x2B: 命令选项 (关键字段!)

	// DW11 (0x2C-0x2F): reserved
	u8 rsv1[4];              // 0x2C-0x2F: reserved
} __packed __aligned(4);     // 总大小: 48 字节 (0x30)
```

### 1.3 关键差异总结

| 字段 | Legacy TXD | UniCmd TXD | 说明 |
|------|-----------|-----------|------|
| **总大小** | 64 字节 (0x40) | 48 字节 (0x30) | UniCmd 更紧凑 |
| **cid 字段** | u8 cid (0x24) | __le16 cid (0x22) | UniCmd 用 16-bit |
| **option 字段** | 无 | u8 option (0x2B) | UniCmd 核心控制字段 |
| **pq_id 字段** | __le16 pq_id (0x22) | 无 | Legacy 专用 |
| **ext_cid** | u8 ext_cid (0x29) | 无 | Legacy 专用 |
| **set_query** | u8 set_query (0x26) | 无 | 用 option 替代 |

---

## 二、option 字段详解 (UniCmd 核心)

`option` 字段 (0x2B) 控制命令类型和响应行为：

### 2.1 Bit 定义

```c
// mt6639/include/nic_uni_cmd_event.h
#define UNI_CMD_OPT_BIT_0_ACK        BIT(0)  // 请求 FW 响应
#define UNI_CMD_OPT_BIT_1_UNI_CMD    BIT(1)  // 1=UniCmd, 0=legacy
#define UNI_CMD_OPT_BIT_2_SET_QUERY  BIT(2)  // 1=SET, 0=QUERY

// mt76/mt76_connac_mcu.h
#define MCU_CMD_ACK        BIT(0)  // 0x01
#define MCU_CMD_UNI        BIT(1)  // 0x02
#define MCU_CMD_SET        BIT(2)  // 0x04
```

### 2.2 常用组合值

| 宏定义 | 值 | 含义 | 用途 |
|--------|---|------|------|
| **MCU_CMD_UNI_EXT_ACK** | 0x07 | ACK \| UNI \| SET | 标准 SET 命令 (需要响应) |
| **MCU_CMD_UNI_QUERY_ACK** | 0x03 | ACK \| UNI | QUERY 命令 (需要响应) |
| **UNI_CMD_OPT_BIT_1_UNI_CMD** | 0x02 | UNI | 无需响应的 UniCmd |

### 2.3 mt7925 实际逻辑 (参考)

```c
// mt76/mt7925/mcu.c:3498-3505
if (cmd & __MCU_CMD_FIELD_QUERY)
	uni_txd->option = MCU_CMD_UNI_QUERY_ACK;  // 0x03
else
	uni_txd->option = MCU_CMD_UNI_EXT_ACK;    // 0x07

// CHIP_CONFIG 和 HIF_CTRL 不需要 ACK
if (cmd == MCU_UNI_CMD(HIF_CTRL) || cmd == MCU_UNI_CMD(CHIP_CONFIG))
	uni_txd->option &= ~MCU_CMD_ACK;  // 变成 0x02 或 0x06
```

**关键发现**: mt7925 对 CHIP_CONFIG (0x0E) 命令 **清除 ACK bit**，说明 NIC_CAPABILITY 查询不应该等待标准响应！

### 2.4 mt6639 实际逻辑 (Android)

```c
// mt6639/nic/nic_uni_cmd_event.c:349-353
ucOption = UNI_CMD_OPT_BIT_1_UNI_CMD;  // 基础值 0x02
if (fgSetQuery)  // SET 命令
	ucOption |= (prCmdInfo->fgNeedResp ? UNI_CMD_OPT_BIT_0_ACK : 0);
ucOption |= (fgIsOid ? UNI_CMD_OPT_BIT_0_ACK : 0);
ucOption |= (fgSetQuery ? UNI_CMD_OPT_BIT_2_SET_QUERY : 0);

// NIC_CAP 查询示例:
// fgSetQuery=0 (QUERY), fgNeedResp=1, fgIsOid=0
// → ucOption = 0x02 | 0x01 = 0x03 (MCU_CMD_UNI_QUERY_ACK)
```

---

## 三、NIC_CAPABILITY 命令示例

### 3.1 命令格式 (mt7925 参考)

```c
// mt76/mt7925/mcu.c:924-946
static int mt7925_mcu_get_nic_capability(struct mt792x_dev *dev)
{
	struct {
		u8 _rsv[4];      // UNI_CMD_CHIP_CONFIG 固定 4 字节 padding
		__le16 tag;      // UNI_CMD_CHIP_CONFIG_TAG_NIC_CAPABILITY = 3
		__le16 len;      // sizeof(req) - 4 = 4
	} __packed req = {
		.tag = cpu_to_le16(UNI_CHIP_CONFIG_NIC_CAPA),  // 3
		.len = cpu_to_le16(sizeof(req) - 4),           // 4
	};

	ret = mt76_mcu_send_and_get_msg(&dev->mt76,
		MCU_UNI_CMD(CHIP_CONFIG),  // 0x0E
		&req, sizeof(req), true, &skb);
}
```

### 3.2 完整 TXD 布局

假设 seq=1，总包大小 = 0x30 (txd) + 0x08 (payload) = 0x38 字节：

```
偏移   字段             值             说明
--------------------------------------------------------------
// txd[0-7] (0x00-0x1F): 硬件描述符
0x00   TXD0            0x00380041     TX_BYTES=0x38, PKT_FMT=2, Q_IDX=0x20
0x04   TXD1            0x00004000     HDR_FORMAT=3, 无 LONG_FORMAT BIT(31)!
0x08   TXD2-7          ...            (清零或保留)

// DW8 (0x20-0x23): len + cid
0x20   len             0x0008         payload 长度 (8 字节)
0x22   cid             0x000E         UNI_CMD_ID_CHIP_CONFIG

// DW9 (0x24-0x27): pkt_type + seq
0x24   rsv             0x00
0x25   pkt_type        0xA0           MCU_PKT_ID
0x26   frag_n          0x00
0x27   seq             0x01           序列号

// DW10 (0x28-0x2B): checksum + option
0x28   checksum        0x0000
0x2A   s2d_index       0x00           MCU_S2D_H2N (HOST→WM)
0x2B   option          0x02           UNI (不需要 ACK!)

// DW11 (0x2C-0x2F): reserved
0x2C   rsv1[4]         0x00000000

// Payload (0x30-0x37): UNI_CMD_CHIP_CONFIG
0x30   _rsv[4]         0x00000000     固定 padding
0x34   tag             0x0003         NIC_CAPABILITY
0x36   len             0x0004         4 字节
```

### 3.3 关键要点

1. **option=0x02** (仅 UNI bit) — mt7925 对 CHIP_CONFIG **不设置 ACK bit**
2. **cid=0x000E** (16-bit) — UNI_CMD_ID_CHIP_CONFIG
3. **payload TLV 格式** — 4 字节 padding + tag/len + data
4. **响应机制不同** — 不是通过标准 MCU_RX0 响应，可能是不同的事件类型

---

## 四、mt6639 UniCmd ID 映射表

| UniCmd ID | 名称 | mt76 对应 | 说明 |
|-----------|------|-----------|------|
| 0x01 | UNI_CMD_ID_DEVINFO | MCU_UNI_CMD_DEV_INFO_UPDATE | 设备信息 |
| 0x02 | UNI_CMD_ID_BSSINFO | MCU_UNI_CMD_BSS_INFO_UPDATE | BSS 信息 |
| 0x03 | UNI_CMD_ID_STAREC_INFO | MCU_UNI_CMD_STA_REC_UPDATE | STA 记录 |
| 0x0E | **UNI_CMD_ID_CHIP_CONFIG** | **MCU_UNI_CMD_CHIP_CONFIG** | **芯片配置 (NIC_CAP)** |
| 0x0F | UNI_CMD_ID_POWER_CTRL | MCU_UNI_CMD_POWER_CTRL | 电源控制 |
| 0x28 | UNI_CMD_ID_MBMC | MCU_UNI_CMD_SET_DBDC_PARMS | DBDC (MT6639) |

### 4.1 CHIP_CONFIG 子标签

```c
// mt6639/include/nic_uni_cmd_event.h:1313-1318
enum ENUM_UNI_CMD_CHIP_CONFIG_TAG {
	UNI_CMD_CHIP_CONFIG_TAG_SW_DBG_CTRL = 0,
	UNI_CMD_CHIP_CONFIG_TAG_CUSTOMER_CFG = 1,
	UNI_CMD_CHIP_CONFIG_TAG_CHIP_CFG = 2,
	UNI_CMD_CHIP_CONFIG_TAG_NIC_CAPABILITY = 3,  // ← 我们需要的
};
```

---

## 五、代码修改建议

### 5.1 添加 UniCmd TXD 结构体

在 `src/mt7927_pci.h` 中添加：

```c
/* UniCmd TXD (CONNAC3) - 0x30 字节 */
struct mt7927_mcu_uni_txd {
	__le32 txd[8];           /* 0x00-0x1F: 硬件描述符 */

	/* DW8 (0x20-0x23) */
	__le16 len;              /* payload 长度 (不含 txd) */
	__le16 cid;              /* UNI_CMD_ID_xxx */

	/* DW9 (0x24-0x27) */
	u8 rsv;
	u8 pkt_type;             /* 0xa0 */
	u8 frag_n;
	u8 seq;

	/* DW10 (0x28-0x2B) */
	__le16 checksum;         /* 通常为 0 */
	u8 s2d_index;            /* MCU_S2D_H2N=0, H2C=2 */
	u8 option;               /* UNI_CMD_OPT_xxx */

	/* DW11 (0x2C-0x2F) */
	u8 rsv1[4];
} __packed __aligned(4);

/* UniCmd option bits */
#define UNI_CMD_OPT_ACK         BIT(0)  /* 请求响应 */
#define UNI_CMD_OPT_UNI         BIT(1)  /* UniCmd 标志 */
#define UNI_CMD_OPT_SET         BIT(2)  /* SET=1, QUERY=0 */

/* 常用组合 */
#define UNI_CMD_OPT_UNI_ONLY    0x02    /* 无 ACK */
#define UNI_CMD_OPT_QUERY_ACK   0x03    /* QUERY + ACK */
#define UNI_CMD_OPT_SET_ACK     0x07    /* SET + ACK */

/* UniCmd IDs */
#define UNI_CMD_ID_CHIP_CONFIG  0x0E
#define UNI_CMD_ID_POWER_CTRL   0x0F
#define UNI_CMD_ID_MBMC         0x28    /* MT6639 DBDC */

/* CHIP_CONFIG tags */
#define UNI_CHIP_CONFIG_TAG_NIC_CAP   0x03
#define UNI_CHIP_CONFIG_TAG_CHIP_CFG  0x02

/* s2d_index values */
#define MCU_S2D_H2N             0x00    /* HOST → WM */
#define MCU_S2D_H2C             0x02    /* HOST → WA */
#define MCU_S2D_H2N_AND_H2C     0x03    /* HOST → WM+WA */

#define MCU_PKT_ID              0xa0
```

### 5.2 实现 UniCmd 发送函数

在 `src/mt7927_pci.c` 中添加：

```c
/* 发送 UniCmd (简化版，无等待响应) */
static int mt7927_mcu_send_unicmd(struct mt7927_dev *dev,
				   u16 cmd_id, u8 option,
				   const void *payload, size_t len)
{
	struct mt7927_mcu_uni_txd *txd;
	struct mt7927_txd *tx_desc;
	size_t total_len = sizeof(*txd) + len;
	dma_addr_t dma_addr;
	void *buf;
	u32 val;
	u8 seq;

	/* 分配 DMA buffer */
	buf = dma_alloc_coherent(&dev->pdev->dev, total_len,
				 &dma_addr, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memset(buf, 0, total_len);
	txd = (struct mt7927_mcu_uni_txd *)buf;

	/* 填充硬件描述符 (txd[0-1]) */
	val = FIELD_PREP(MT_TXD0_TX_BYTES, total_len) |
	      FIELD_PREP(MT_TXD0_PKT_FMT, 2) |      /* CMD */
	      FIELD_PREP(MT_TXD0_Q_IDX, 0x20);      /* MT_TX_MCU_PORT_RX_Q0 */
	txd->txd[0] = cpu_to_le32(val);

	val = FIELD_PREP(MT_TXD1_HDR_FORMAT, 3);    /* V3, 无 BIT(31)! */
	txd->txd[1] = cpu_to_le32(val);

	/* 填充 UniCmd header */
	seq = ++dev->mcu_seq & 0xf;
	if (!seq)
		seq = ++dev->mcu_seq & 0xf;

	txd->len = cpu_to_le16(len);
	txd->cid = cpu_to_le16(cmd_id);
	txd->pkt_type = MCU_PKT_ID;
	txd->seq = seq;
	txd->s2d_index = MCU_S2D_H2N;
	txd->option = option;

	/* 复制 payload */
	if (len > 0 && payload)
		memcpy(buf + sizeof(*txd), payload, len);

	/* 写入 TX 环 15 */
	tx_desc = &dev->ring_tx15.desc[dev->ring_tx15.sw_idx];
	tx_desc->buf0 = cpu_to_le32(dma_addr);
	tx_desc->buf1 = 0;
	tx_desc->info = cpu_to_le32(FIELD_PREP(MT_TXD_INFO_LEN, total_len) |
				     MT_TXD_INFO_DMA_DONE);

	/* 更新环索引 */
	dev->ring_tx15.sw_idx = (dev->ring_tx15.sw_idx + 1) % MT7927_TX15_RING_SIZE;
	mt7927_wr(dev, MT_WFDMA_HOST_TX_RING15_CIDX, dev->ring_tx15.sw_idx);

	dev_info(&dev->pdev->dev,
		 "UniCmd: cid=0x%04x opt=0x%02x len=%zu seq=%u\n",
		 cmd_id, option, len, seq);

	/* TODO: 保存 dma_addr 供后续释放 */
	return 0;
}

/* NIC_CAPABILITY 查询 */
static int mt7927_mcu_get_nic_cap(struct mt7927_dev *dev)
{
	struct {
		u8 _rsv[4];
		__le16 tag;
		__le16 len;
	} __packed req = {
		.tag = cpu_to_le16(UNI_CHIP_CONFIG_TAG_NIC_CAP),
		.len = cpu_to_le16(4),
	};

	/* option=0x02: mt7925 对 CHIP_CONFIG 不设置 ACK bit */
	return mt7927_mcu_send_unicmd(dev, UNI_CMD_ID_CHIP_CONFIG,
				      UNI_CMD_OPT_UNI_ONLY,
				      &req, sizeof(req));
}
```

### 5.3 PostFwDownloadInit 集成

在 `mt7927_post_fwdl_init()` 中：

```c
static int mt7927_post_fwdl_init(struct mt7927_dev *dev)
{
	int ret;

	/* 1. DMASHDL enable (BAR0+0xd6060 = 0x10101) */
	mt7927_wr(dev, 0xd6060, 0x10101);
	dev_info(&dev->pdev->dev, "DMASHDL enabled\n");

	msleep(10);  /* Windows 有短暂延迟 */

	/* 2. NIC_CAPABILITY (CHIP_CONFIG tag=3) */
	ret = mt7927_mcu_get_nic_cap(dev);
	if (ret) {
		dev_err(&dev->pdev->dev, "NIC_CAP failed: %d\n", ret);
		return ret;
	}

	/* TODO: 等待并解析响应 (可能是异步事件，不是 MCU_RX0) */
	msleep(100);

	/* 3-9. 其他 Windows PostFwDownloadInit 命令... */
	return 0;
}
```

---

## 六、关键注意事项

### 6.1 ACK 机制差异

- **FWDL 命令** (legacy): 同步，等待 MCU_RX0 响应
- **CHIP_CONFIG** (UniCmd): mt7925 **不设置 ACK bit**，响应机制可能不同
  - 可能是异步事件 (UNI_EVENT_ID_CMD_RESULT)
  - 可能通过不同的 RX 环 (ring 4/6/7)
  - Windows 可能用中断驱动，不阻塞等待

### 6.2 mt7925 vs MT6639 差异

| 特性 | mt7925 (CONNAC2) | MT6639 (CONNAC3) |
|------|-----------------|-----------------|
| HOST RX ring 0 | 使用 (MCU 事件) | **不使用** |
| RX 环布局 | 0, 1 | **4, 6, 7** |
| DBDC 命令 | 无 | 0x28 (UNI_CMD_ID_MBMC) |
| UniCmd 格式 | 相同 | 相同 |

### 6.3 下一步调试方向

1. **验证 UniCmd 发送** — dmesg 确认 TXD 写入 ring 15
2. **监控 DIDX 变化** — 检查 ring 15 是否被 WFDMA 消费
3. **检查中断状态** — MCU_CMD 寄存器和 RX 环中断
4. **捕获响应事件** — 可能在 ring 4/6/7 中，而非 MCU_RX0
5. **对比 Windows 抓包** — 确认完整的 TXD 格式和时序

---

## 七、参考代码位置

### mt76 上游 (mt7925)
- **UniCmd 结构**: `mt76/mt76_connac_mcu.h:95-115`
- **填充逻辑**: `mt76/mt76_connac_mcu.c:3216-3226`
- **NIC_CAP**: `mt76/mt7925/mcu.c:925-992`
- **option 处理**: `mt76/mt7925/mcu.c:3498-3505`

### mt6639 Android
- **UniCmd 定义**: `mt6639/include/nic_uni_cmd_event.h:151-209`
- **发送函数**: `mt6639/nic/nic_uni_cmd_event.c:302-364`
- **option bits**: `mt6639/include/nic_uni_cmd_event.h:35-39`
- **CHIP_CONFIG**: `mt6639/include/nic_uni_cmd_event.h:1296-1359`

### Windows RE 文档
- **PostFwDownloadInit**: `docs/win_v5705275_mcu_dma_submit.md:460-590`
- **9 个 MCU 命令**: 全部使用 target=0xed (可能是 UniCmd 标志)

---

## 八、总结

**核心发现**:
1. UniCmd TXD 是 0x30 字节 (vs legacy 0x40)
2. option 字段 (0x2B) 控制 ACK/SET/QUERY 行为
3. mt7925 对 CHIP_CONFIG 使用 **option=0x02** (无 ACK bit)
4. cid 是 16-bit，直接用 UNI_CMD_ID_xxx 枚举值
5. payload 格式是 TLV (4 字节 padding + tag/len + data)

**立即行动**:
- 在 `src/mt7927_pci.h` 添加 UniCmd 结构体定义
- 在 `src/mt7927_pci.c` 实现 `mt7927_mcu_send_unicmd()`
- 修改 `mt7927_post_fwdl_init()` 发送 NIC_CAP 命令
- 监控 ring 15 DIDX 和 RX 环中断

**期望结果**:
- ring 15 DIDX 前进 (WFDMA 消费命令)
- 固件通过某个机制响应 (可能非 MCU_RX0)
- 解除当前的 -110 超时死锁

---

**文档版本**: v1.0
**最后更新**: 2026-02-15
**状态**: 待验证实现
