# MT7927/MT6639 Web Research
Date: 2026-02-16

## Summary

MT7927 = MT6639 移动芯片的 PCIe 封装，PCI ID 14c3:6639。**MediaTek 从未发布官方 Linux WiFi 驱动**，目前全球无可用驱动。有一个社区研究项目（ehausig/mt7927）已暂停，仅完成固件加载但未实现 DMA 传输。MT7927 架构上与 MT7925 相同（仅 320MHz vs 160MHz 差异），理论上可以改编 mt7925 驱动（已在 kernel 6.7+ 支持）。

找到重要资源：
1. **mt76 上游驱动源码** — mt7925 PCIe TX 路径实现（含 TXP 机制）
2. **MediaTek WiFi RE 项目** — 固件结构逆向工程工具
3. **WCID/WTBL 管理细节** — STA_REC MCU 命令架构
4. **TXP (TX Page) 结构定义** — mt76_connac_hw_txp 24 字节 scatter-gather

## Findings

### 1. MT7927 Linux Driver Research Project (GitHub - ehausig/mt7927)
- **URL**: https://github.com/ehausig/mt7927
- **状态**: **项目已无限期暂停** — 开发者转向其他项目
- **进展**:
  - ✅ PCI 枚举 + BAR 映射
  - ✅ 驱动绑定设备（PCI ID: 14c3:7927）
  - ✅ MT7925 固件加载到内核内存
  - ✅ 芯片稳定不崩溃，FW_STATUS=0xffff10f1
  - ❌ **DMA 传输未实现** — 固件在内核内存但未传输到芯片
  - ❌ Main memory (0x000000) 不可访问
  - ❌ 无法创建网络接口
- **关键发现**: MT7927 架构上与 MT7925 **完全相同**，仅支持 320MHz vs 160MHz 信道宽度差异 → 可以改编现有 mt7925 代码而非从零逆向
- **对我们的价值**: 证实了 MT7927=MT6639 的关系，确认无官方驱动。但该项目未深入 TX/RX 路径，未能提供 auth 问题线索。

### 2. MT7927 Community Discussions
#### GitHub Issue #517 (morrownr/USB-WiFi)
- **URL**: https://github.com/morrownr/USB-WiFi/issues/517
- **用户痛点**: TP-Link Archer TBE550E WiFi 7 卡（MT7927）在 Linux 完全不可用
- **硬件捆绑**: Gigabyte X670E, ASUS ProArt X870E 等主板自带此卡
- **驱动时间线**: MediaTek 通常在产品上市 6 个月前提供驱动，但 MT7927 已出货超 1 年仍无支持 → **异常延迟**
- **2025-01 内核补丁**: 出现 MT7927 互操作性补丁（提交者非 MediaTek），可能暗示逆向工程或测试活动
- **对我们的价值**: 确认社区需求强烈，但无官方时间表。说明我们的从零驱动开发在正确方向上。

#### GitHub Issue #927 (openwrt/mt76)
- **URL**: https://github.com/openwrt/mt76/issues/927
- **关键信息**: MT7927 可能是 MT6639 移动芯片的变体，mt76 项目**不支持移动芯片**
- **固件发现**: 从 Windows 驱动逆向提取固件文件 `mtkwl7927_2_1ss1t.dat` 等二进制文件
- **用户受影响**: ASUS ROG CROSSHAIR X870E HERO 主板 WiFi 卡不工作
- **对我们的价值**: 确认 mt76 项目不会主动支持 MT7927（因为是移动芯片）。固件文件名格式有参考价值。

#### Arch Linux Forums
- **URL**: https://bbs.archlinux.org/viewtopic.php?id=303402
- **建议方案**:
  1. USB WiFi dongle（Linux 兼容芯片）
  2. 手机网络共享
  3. 尝试 MT7925 驱动（架构相似）
- **社区呼吁**: 联系 MediaTek 直接要求官方驱动
- **对我们的价值**: 无新技术信息。

### 3. MT7925 TX Path Implementation (Linux Kernel Source)
#### mt7925/pci.c (torvalds/linux)
- **URL**: https://github.com/torvalds/linux/blob/master/drivers/net/wireless/mediatek/mt76/mt7925/pci.c
- **TX 回调注册**:
  ```c
  .tx_prepare_skb = mt7925e_tx_prepare_skb,
  .tx_complete_skb = mt76_connac_tx_complete_skb,
  ```
- **DMA 初始化** (`mt7925_dma_init`):
  1. 调用 `mt76_connac_init_tx_queues()` 初始化 TX 队列（MT7925_TX_RING_SIZE, MT_TX_RING_BASE）
  2. 配置 MCU WM 队列（固件控制消息）
  3. 配置 FWDL 队列（固件下载）
  4. 写入 `MT_WFDMA0_TX_RING0_EXT_CTRL = 0x4` （TX ring 0 扩展控制）
  5. 使能 TX NAPI 轮询
- **关键发现**: 实际 TX 准备逻辑在 `pci_mac.c` 中（未在此文件），需要查看 `mt7925e_tx_prepare_skb()` 实现
- **对我们的价值**: 确认 TX ring 0 扩展控制寄存器配置（0x4），与我们的实现一致。但需要找到 `pci_mac.c` 获取 TXD+TXP 构建细节。

#### mt76_connac.h (openwrt/mt76)
- **URL**: https://github.com/openwrt/mt76/blob/master/mt76_connac.h
- **TXP 结构定义**:
  ```c
  #define MT_HW_TXP_MAX_MSDU_NUM    4
  #define MT_HW_TXP_MAX_BUF_NUM     4

  struct mt76_connac_txp_ptr {
      __le32 buf0;      // DMA 地址（低 32 位）
      __le16 len0;      // buf0 长度
      __le16 len1;      // buf1 长度
      __le32 buf1;      // DMA 地址（高 32 位）
  } __packed __aligned(4);

  struct mt76_connac_hw_txp {
      __le16 msdu_id[MT_HW_TXP_MAX_MSDU_NUM];  // 4 个 MSDU ID（8 字节）
      struct mt76_connac_txp_ptr ptr[MT_HW_TXP_MAX_BUF_NUM / 2];  // 2 个指针（16 字节）
  } __packed __aligned(4);
  ```
  **总大小**: 8 + 16 = **24 字节**
- **对我们的价值**: **关键发现！** TXP 是 24 字节 scatter-gather 描述符，紧跟在 32 字节 TXD 后面。DMA 描述符应该：
  - `buf0` = TXD+TXP 物理地址（56 字节）
  - `buf1` = skb data 物理地址
  - 这与我们当前简化实现（仅 TXD）不同，可能是 auth 帧发不出去的根本原因！

### 4. MT7925 Station and Link Management (DeepWiki)
- **URL**: https://deepwiki.com/openwrt/mt76/10.3-mt7925-implementation
- **WCID (Wireless Client ID) 分配**:
  - 池大小: **1088 个 WCID**
  - 索引范围: 0-14 用于链路（15 个条目，减去保留项）
  - 每个 WCID 跟踪: 聚合会话、速率信息、TX pending 队列、RX 重排序状态
- **STA_REC MCU 命令**:
  - 通过 **TLV (Type-Length-Value)** 结构发送站点信息到固件
  - 配置项: 链路标识符、power save 参数、DTIM period、U-APSD
  - 每个 BSS 的 power save 通过 MCU 命令配置
- **MLO (Multi-Link Operation) 支持**:
  - 链路状态: DISASSOC, ASSOC, power state 转换
  - 每链路独立 WCID 配置 + 速率控制参数
  - 跨链路同步 power 管理和 BSS 配置
- **管理帧处理**:
  - 固件触发连接丢失检测 → 驱动调用 `ieee80211_connection_loss()` 通知 mac80211
  - 硬件级连接监控，减少深度睡眠时 CPU 开销
- **对我们的价值**: **重要！** 我们缺少 STA_REC MCU 命令实现。固件可能需要知道 VIF/STA 存在后才能发射管理帧。这可能是 auth 帧发不出去的原因之一。

### 5. MediaTek WiFi Firmware Reverse Engineering (GitHub - cyrozap/mediatek-wifi-re)
- **URL**: https://github.com/cyrozap/mediatek-wifi-re
- **项目目的**: 逆向 MediaTek WiFi 固件结构（PCIe/USB/SDIO 芯片 + 独立 MCU + SoC 内置 WiFi）
- **工具**:
  - `extract_fw.py` — 固件提取工具，处理 `WIFI_RAM_CODE*` 二进制文件
  - Kaitai Struct 定义: `mediatek_external_wifi_firmware.ksy`, `mediatek_linkit_wifi_firmware.ksy`, `mediatek_linux_wifi_bt_patch.ksy`, `mediatek_soc_wifi_firmware.ksy`
- **固件来源**: Android 设备 `/system/etc/firmware` 或 ROM 开发者发布的 `vendor.zip`
- **许可证**: GPL-3.0（软件）+ CC-BY-SA 4.0（文档）
- **对我们的价值**: 工具可用于分析 MT6639 固件结构，但项目未包含 DMA 机制、TX/RX 描述符或 MT6639 特定细节。可作为固件格式参考。

### 6. MT76 WFDMA DMA Implementation (Linux Wireless Patch)
- **URL**: https://www.spinics.net/lists/linux-wireless/msg217640.html
- **补丁目的**: 重构 MT7915 DMA 实现以支持 MT7916 芯片变体
- **TX Ring 控制寄存器** (抽象映射):
  - `TX_RING_CTRL_FWDL` — 固件下载 ring
  - `TX_RING_CTRL_WM` — Wireless management ring
  - `TX_RING_CTRL_BAND0/BAND1` — 每频段 ring
  - `TX_RING_CTRL_WA` — Wireless authentication ring
  - 替代硬编码 `MT_WFDMA1_TX_RING16_EXT_CTRL` ~ `MT_WFDMA1_TX_RING23_EXT_CTRL`
- **RX 队列分离**:
  - `MT_RXQ_MAIN` — 主数据接收
  - `MT_RXQ_MCU_WA` — MCU wireless authentication 事件
  - `MT_RXQ_MAIN_WA` — Main band WA 通知（MT7916+ 新增）
  - `MT_RXQ_EXT/EXT_WA` — 双频段支持
- **DMA enable/disable 函数**: 提取为专用函数，支持芯片变体检测（`is_mt7915()` 宏）
- **对我们的价值**: 提供 WFDMA ring 管理参考架构，但未包含 MT7927/MT7925 特定实现。确认 TX ring prefetch 配置需要针对每个 ring。

### 7. Linux Kernel Mailing List - MT7925 Patches
#### PATCH v3 00/17: Add support for Mediatek Wi-Fi7 driver mt7925
- **URL**: https://lore.kernel.org/all/821586cb6aaddbbf6f77c70464aff91697e365bc.1695024367.git.deren.wu@mediatek.com/T/
- **驱动特性**:
  - 新 mac80211 驱动，支持 Station, AP, P2P, monitor 模式
  - MediaTek Filogic 360: 802.11be, max 4096-QAM/160MHz @ 6GHz/5GHz/2.4GHz
  - 2x2 天线，支持 PCIe 和 USB 总线
- **合并时间**: 驱动在 **kernel 6.7** 合并
- **对我们的价值**: 确认 mt7925 是官方上游驱动，可作为 MT7927 参考（架构相同）。

#### Channel Context Management (MT7921 chanctx support)
- **URL**: https://lore.kernel.org/all/CAGp9LzqecHa4DzAcugth4EOua8n-tPnS4TmgijETMxkT7fn8gQ@mail.gmail.com/T/
- **管理帧处理**: 驱动必须从固件获取特权以占用当前信道上下文，直到帧握手完成
- **对我们的价值**: **重要！** 说明管理帧发送需要**信道上下文特权**。我们可能缺少 `UNI_CHANNEL_SWITCH` 或类似 MCU 命令来请求发送管理帧的信道访问权。

#### MT7925 MCU Commands
- **BSS_INFO_UPDATE**: https://patchwork.kernel.org/project/linux-mediatek/patch/a3d03b0a1ca916b2b8b2e7c0afcdcd7e258d97c3.1720248331.git.sean.wang@kernel.org/
- **STA_UPDATE**: https://patchwork.kernel.org/project/linux-wireless/patch/20241211011926.5002-10-sean.wang@kernel.org/
- **对我们的价值**: 确认 BSS_INFO_UPDATE 和 STA_UPDATE 是关键 MCU 命令。我们当前实现的 BSS_INFO_UPDATE 在 session 12 发现有毒（破坏固件事件输出），需要重新检查 TLV 格式。

### 8. MediaTek Driver Repositories
#### Windows Driver Firmware (Station-Drivers)
- **URL**: https://www.station-drivers.com/index.php/en/component/remository/Drivers/MediaTek/MediaTek-MT7927-MT7925-Wireless-Lan/MediaTek-Wi-Fi-7-MT7927---MT7925-Wireless-LAN-Version-5.3.0.1498/lang,en-gb/
- **最新版本**: v5.7.0.5275 WHQL (2026-05-01)
- **支持芯片**: MT6639, MT7927, MT7925 — **三者在同一驱动包中**
- **固件文件格式**: `mtkwl7927_2_1ss1t.dat` 等
- **Windows 要求**: Windows 11 64-bit version SV3 (23H2) 或更高
- **对我们的价值**: 确认 MT6639/MT7927/MT7925 共享驱动，支持我们的"MT7927=MT6639"假设。固件文件名格式可参考。

### 9. MT7927 = MT6639 关系确认
- **URL**: https://www.necacom.net/index.php/mediatek/mediatek-wi-fi-7-mt7927-mt7925/mediatek-wi-fi-7-mt7927-mt7925-wireless-lan-version-5-7-0-5275
- **驱动包芯片列表**: MT6639, MT7927, MT7925 始终打包在一起
- **推断**: MT6639, MT7927, MT7925 可能是同芯片不同封装/配置（类似 MT7927=MT6639 PCIe 版，MT7925=MT6639 160MHz 版）
- **对我们的价值**: 强化 MT7927=MT6639 假设，确认使用 MT6639 Android 驱动代码作为参考的正确性。

## Key Takeaways

### 对 Auth 问题最有价值的发现

1. **TXP (TX Page) 缺失 — 最可能的根本原因**:
   - mt7925 PCIe 使用 **CT 模式 (PKT_FMT=0) + 24 字节 TXP** scatter-gather 描述符
   - TXP 结构: `mt76_connac_hw_txp` (8 字节 MSDU IDs + 16 字节 DMA 指针)
   - DMA 描述符布局应该是:
     - `buf0` = TXD(32) + TXP(24) = 56 字节
     - `buf1` = skb data 物理地址
   - **我们当前实现**: 仅 TXD (32 字节) 无 TXP → 固件可能无法正确解析或发射帧
   - **下一步**: 实现 `mt76_connac_hw_txp` 构建 + 修改 DMA 描述符为两段式

2. **缺少 STA_REC MCU 命令**:
   - mt7925 通过 TLV 格式发送站点信息到固件
   - 固件需要知道 VIF/STA 存在后才能分配 WCID 和 WTBL 条目
   - **我们当前实现**: 仅 DEV_INFO_UPDATE, 未实现 STA_REC
   - **下一步**: 在 `add_interface` 后调用 `mt7927_mcu_sta_update()` 发送 STA_REC (state=NOTEXIST→NONE)

3. **信道上下文特权缺失**:
   - 管理帧发送需要从固件获取信道访问权（"privilege to occupy channel context"）
   - 可能需要 `UNI_CHANNEL_SWITCH` 或类似 MCU 命令
   - **我们当前实现**: 无信道切换命令
   - **下一步**: 研究 mt7925 `hw_scan` 和 `mgd_prepare_tx` 实现（ROC - Remain On Channel）

4. **BSS_INFO_UPDATE TLV 格式错误**:
   - Session 12 发现我们的 BSS_INFO_UPDATE 实现有毒（破坏固件事件输出）
   - 需要对照 mt7925 上游驱动重新检查 TLV 字段顺序和大小
   - **下一步**: 精确复制 mt7925 `mt7925_mcu_add_bss_info()` 实现

### 代码参考优先级（更新）

| 优先级 | 资源 | 用途 |
|--------|------|------|
| **1** | `mt76/mt7925/pci_mac.c` | **mt7925e_tx_prepare_skb()** — TXD+TXP 构建 |
| **2** | `mt76/mt7925/mcu.c` | STA_REC, BSS_INFO, DEV_INFO MCU 命令 |
| **3** | `mt76/mt76_connac_mac.c` | TXD 填充通用逻辑 |
| **4** | `mt76/mt76_connac.h` | TXP 结构定义（已获取） |
| **5** | `mt76/mt7925/main.c` | `hw_scan`, `mgd_prepare_tx` 信道管理 |
| **6** | `mt6639/` Android 驱动 | 硬件寄存器参考（我们已有） |
| **7** | Windows RE 文档 | TXD 格式参考（我们已有） |

### 立即行动项

1. **获取 mt7925 pci_mac.c 源码** — 查看 `mt7925e_tx_prepare_skb()` 完整实现
2. **实现 TXP 构建** — 在 `mt7927_tx_prepare_skb()` 中添加 24 字节 TXP
3. **修复 DMA 描述符** — 改为两段式（buf0=TXD+TXP, buf1=skb data）
4. **实现 STA_REC 命令** — 参考 mt7925 `mt7925_mcu_sta_update()`
5. **修复 BSS_INFO_UPDATE** — 对照上游重新实现 TLV

### 不推荐的方向

- ❌ 继续简化 TX 路径（SF 模式已测试失败，CT 模式需要 TXP）
- ❌ 等待 MediaTek 官方驱动（根据社区讨论，短期内不太可能）
- ❌ 尝试 mt7925 驱动直接用于 MT7927（PCI ID 不匹配，需要内核补丁）

## Sources

### GitHub Repositories
- [ehausig/mt7927 - Linux driver research project](https://github.com/ehausig/mt7927)
- [openwrt/mt76 - MT76 WiFi driver](https://github.com/openwrt/mt76)
- [torvalds/linux - MT7925 PCI driver](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/mediatek/mt76/mt7925/pci.c)
- [cyrozap/mediatek-wifi-re - Firmware reverse engineering](https://github.com/cyrozap/mediatek-wifi-re)

### Community Discussions
- [MT7927 Issue #517 - morrownr/USB-WiFi](https://github.com/morrownr/USB-WiFi/issues/517)
- [MT7927 Support Issue #927 - openwrt/mt76](https://github.com/openwrt/mt76/issues/927)
- [Arch Linux Forums - MT7927 support](https://bbs.archlinux.org/viewtopic.php?id=303402)
- [Framework Community - WiFi 7 chipset recommendations](https://community.frame.work/t/wifi-7-chipset-recommendations/65752)

### Linux Kernel & Patches
- [LKML: Add support for MT7925 WiFi 7 driver](https://lore.kernel.org/all/821586cb6aaddbbf6f77c70464aff91697e365bc.1695024367.git.deren.wu@mediatek.com/T/)
- [MT7915 WFDMA DMA implementation patch](https://www.spinics.net/lists/linux-wireless/msg217640.html)
- [MT7925 chanctx support](https://lore.kernel.org/all/CAGp9LzqecHa4DzAcugth4EOua8n-tPnS4TmgijETMxkT7fn8gQ@mail.gmail.com/T/)
- [MT7925 BSS_INFO_UPDATE patch](https://patchwork.kernel.org/project/linux-mediatek/patch/a3d03b0a1ca916b2b8b2e7c0afcdcd7e258d97c3.1720248331.git.sean.wang@kernel.org/)
- [MT7925 STA_UPDATE patch](https://patchwork.kernel.org/project/linux-wireless/patch/20241211011926.5002-10-sean.wang@kernel.org/)

### Technical Documentation
- [DeepWiki: MT7925 Station and Link Management](https://deepwiki.com/openwrt/mt76/10.3-mt7925-implementation)
- [LWN: Add support for MT7925 driver](https://lwn.net/Articles/944390/)
- [Station-Drivers: MT7927/MT7925 Windows drivers](https://www.station-drivers.com/index.php/en/component/remository/Drivers/MediaTek/MediaTek-MT7927-MT7925-Wireless-Lan/)
- [Necacom: MT7927/MT7925 driver v5.7.0.5275](https://www.necacom.net/index.php/mediatek/mediatek-wi-fi-7-mt7927-mt7925/mediatek-wi-fi-7-mt7927-mt7925-wireless-lan-version-5-7-0-5275)
