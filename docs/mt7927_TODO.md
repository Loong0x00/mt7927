# MT7927 Driver TODO - Priority Order

## P0: Post-Boot MCU Command Infrastructure

Firmware已启动(fw_sync=0x3)，但驱动还不能与运行中的固件通信。
固件启动后使用 UNI_CMD + TLV 格式，与下载阶段的 CID 格式不同。

### T1: 实现 UNI_CMD 发送函数
- 新增 `mt76_connac2_mcu_uni_txd` 结构体 (56字节: 32B TXD + 24B header)
- 新增 `mt7927_mcu_send_uni_cmd()`: 构建 UNI_TXD + TLV payload，发到 q15
- option 字段: SET=0x07, QUERY=0x03, CHIP_CONFIG/HIF_CTRL 不需要 ACK
- TLV payload: 4字节保留 + tag(u16) + len(u16) + data

### T2: 实现 NIC Capability 查询
- 发送 `MCU_UNI_CMD(CHIP_CONFIG)` (cid=0x0e), tag=0x03
- 解析响应 TLV: MAC地址(tag=0x07), PHY能力(0x08), 6G支持(0x18), 芯片能力(0x20)
- 这是固件启动后的第一条命令，验证 UNI_CMD 通路

### T3: EEPROM/eFuse 配置
- 发送 `MCU_UNI_CMD(EFUSE_CTRL)` (cid=0x2d), tag=0x02
- buffer_mode=0(EFUSE), format=1(WHOLE)

### T4: 启用固件日志
- 发送 `MCU_UNI_CMD(WSYS_CONFIG)` (cid=0x0b), tag=0x00
- ctrl=1 启用日志转发到 host

## P1: MAC 层初始化

### T5: MAC 寄存器初始化
- MDP_BNRCFR0/1: de-AMSDU 配置
- SEC_SCR_MAP0: 安全翻译表
- WTBL 清零
- Band 初始化 (2.4G + 5G/6G)
- 基本速率表

### T6: ASPM L0s 禁用
- 写 MT_PCIE_MAC_PM 禁用 L0s (上游在 pci_mcu.c 做)

## P2: ieee80211 注册

### T7: 注册无线硬件
- `ieee80211_alloc_hw()` + `ieee80211_register_hw()`
- 配置 wiphy: bands, rates, HT/VHT/HE/EHT caps
- 实现最小 `ieee80211_ops` (start/stop/add_interface/config)
- 从 NIC Capability 获取的 MAC 地址赋值

## P3: 中断与数据通路

### T8: IRQ 处理
- `devm_request_irq()` 注册中断
- IRQ tasklet 底半部
- NAPI poll 机制

### T9: 数据 TX/RX Ring
- TX q0 (BAND0 数据帧)
- RX q2 (BAND0 数据帧)
- TX/RX NAPI

### T10: TX/RX 数据通路
- TX: 802.11 帧构建, DMA 提交, 完成回调
- RX: DMA 消费, 帧解析, 上报 mac80211

## P4: Station Mode

### T11: 扫描
- MCU 辅助信道扫描 (`MCU_UNI_CMD(SCAN_REQ)`)

### T12: 认证/关联
- Open/WPA2/WPA3 认证
- BSS 加入/离开
- 密钥管理 (PTK/GTK)

## P5: 电源管理与杂项

### T13: 电源管理
- Runtime PM, 深度睡眠, WoWLAN, Suspend/Resume

### T14: 杂项
- Thermal 监控, debugfs, LED, BT 共存, 固件日志转发
