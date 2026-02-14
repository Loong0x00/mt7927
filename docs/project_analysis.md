# MT7927 Linux 驱动项目全面分析

## 1. 项目概述

本项目旨在为联发科 MT7927 WiFi 7 (802.11be) 无线网卡编写 Linux 驱动。该项目继承自 GitHub 上一个已暂停的开源项目，当前处于**固件 DMA 传输阶段受阻**状态。

### 硬件标识

| 属性 | 值 |
|---|---|
| 芯片 | MediaTek MT7927 WiFi 7 |
| PCI Vendor ID | `0x14c3` (MediaTek) |
| PCI Device ID (标称) | `0x7927` |
| PCI Device ID (实际枚举) | `0x6639` (MT6639 手机网卡) |
| 标准 | 802.11be (WiFi 7) |
| 特性差异 | 相比 MT7925 多出 320MHz 带宽支持 |

### 关键发现：MT7927 ≠ MT7925，而是 ≈ MT6639

社区最初认为 MT7927 与 MT7925 几乎相同（MT7925 自 Linux 6.7 起已有完整驱动支持），仅多出 320MHz 带宽。但本项目的深入逆向工程研究揭示了**本质差异**：

1. **PCI 枚举为 `14c3:6639`**，即联发科手机 SoC 中的 WiFi/BT combo 芯片 MT6639
2. **寄存器布局和 DMA 子系统与 MT7925 存在巨大差距**
3. 寄存器写保护行为与 MT7925 不同
4. Windows 驱动中 MT7927 的初始化路径引用了 MT6639 特有的函数（如 `MT6639InitTxRxRing`、`MT6639PreFirmwareDownloadInit`、`MT6639ConfigIntMask`）

这意味着 MT7927 的 PCIe 网卡本质上是**将 MT6639 手机 WiFi 芯片封装为 PCIe 外设**，而非 MT7925 的简单升级。

---

## 2. 项目结构

```
mt7927/
├── Makefile                    # 内核模块构建系统 (110行)
├── README.md                   # 项目主文档
├── PROMPT.md                   # AI 开发会话引导文件
├── LICENSE                     # GPL v2
│
├── tests/                      # 测试模块（按风险等级分类）
│   ├── 01_safe_basic/          # 安全只读测试 (4个)
│   │   ├── test_pci_enum.c     # PCI 枚举验证
│   │   ├── test_bar_map.c      # BAR 映射检查
│   │   ├── test_chip_id.c      # 芯片 ID 识别
│   │   └── test_scratch_rw.c   # Scratch 寄存器读写
│   ├── 02_safe_discovery/      # 安全发现测试 (3个)
│   │   ├── test_config_read.c  # 配置读取
│   │   ├── test_config_decode.c # 初始化命令解码
│   │   └── test_mt7925_patterns.c # MT7925 模式比对
│   ├── 03_careful_write/       # 中等风险 (1个)
│   │   └── test_memory_activate.c
│   └── 04_risky_ops/           # 高风险 / 主要开发区 (19个文件)
│       ├── mt7927_init_dma.c   # ★ 最完整的驱动实现 (~1314行)
│       ├── mt7927_init.c       # 前代驱动迭代
│       ├── mt7927_wrapper.c    # 基础绑定驱动
│       └── test_*.c            # 各项专项测试 (16个)
│
├── src/                        # 未来生产驱动位置 (目前仅占位)
│
├── docs/                       # 研究文档 (~20个 markdown)
│   ├── mt7927_dma_fw_status_2026-02-13.md  # 当前阶段总结
│   ├── mt76_vs_windows_mt7927.md           # Linux mt76 vs Windows 寄存器对比
│   ├── mt7925_vs_windows_mt7927.md         # MT7925 vs MT7927 寄存器分析
│   ├── windows_register_map.md             # Windows 驱动寄存器映射
│   ├── win_v5705275_*.md                   # v5.7.0.5275 驱动逆向 (10个文档)
│   ├── win_v5603998_fw_flow.md             # 旧版驱动固件流程
│   ├── windows_v5705275_*.md               # 深度逆向分析 (3个文档)
│   ├── reverse/                            # 逆向工程专项文档
│   │   ├── mtkwecx_mt6639_fw_dma_reverse.md
│   │   └── mtkwecx_fw_flow_auto.md
│   └── TEST_RESULTS_SUMMARY.md             # 23个测试模块汇总
│
├── scripts/                    # 辅助脚本
│   ├── mt7927_mt7925_test.sh   # 加载测试脚本
│   ├── mt7927_mt7925_rollback.sh # 回滚脚本
│   └── re/                     # 逆向工程工具
│       ├── extract_dispatch_table.py
│       ├── GhidraDumpFuncs.java
│       ├── GhidraExtractFwFlow.java
│       └── ghidra_extract_fw_flow.py
│
├── mt76/                       # Linux mt76 驱动源码树 (参考)
├── DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/  # Windows MT7927 驱动 (旧版)
├── WiFi_AMD-MediaTek_v5.7.0.5275/                            # Windows MT7925 驱动 (新版)
├── ghidra_12.0.3_PUBLIC/       # Ghidra 逆向工具
└── unmtk.rb                    # Ruby 逆向辅助
```

---

## 3. 当前状态与核心障碍

### 3.1 已实现的功能 ✅

| 功能 | 状态 | 说明 |
|---|---|---|
| PCI 枚举 | ✅ | 设备在 `14c3:6639` 上被正确检测 |
| 驱动绑定 | ✅ | 自定义驱动成功绑定到设备 |
| BAR 映射 | ✅ | BAR0: 2MB, BAR2: 32KB |
| 固件加载到内核内存 | ✅ | MT7925 固件兼容 (1.2MB + 212KB) |
| DMA 描述符环分配 | ✅ | 内存分配正常 |
| 芯片稳定性 | ✅ | 无崩溃或锁死 |

### 3.2 核心阻塞点 ❌

**DMA TX 描述符不被硬件消费**。

具体表现：
- 主机侧 `CIDX` 在 kick 后正常推进（`0 -> 1`）
- 设备侧 `DIDX` 始终为 `0`——硬件从未读取描述符
- `HOST_INT_STA = 0x00000000`——无中断产生
- `MCU_CMD = 0x00000000`——MCU 完全无响应
- 最终超时：`Patch download failed: -110`

### 3.3 已排除的变量

通过大量 A/B 测试，以下变量均已排除为根本原因：

| 已测试变量 | 结果 |
|---|---|
| WM ring 选择 (q15 / q17) | 均失败 |
| Scatter 发送路径 (raw FWDL / WM 封装) | 均失败 |
| Kick 方向 (写 DIDX / 写 CIDX) | 均失败 |
| Windows 预下载寄存器序列 (`enable_predl_regs=1`) | 无改善 |
| 降配初始化 (`minimal_dma_cfg=1`) | 无改善 |

**结论**：问题不在固件协议层（0xee/chunk 格式等），而在更底层的 **TX 队列激活/消费前置条件未满足**。

---

## 4. 已完成的逆向工程

### 4.1 Windows 驱动分析

对两个版本的 Windows 驱动进行了 Ghidra 逆向：

| 驱动版本 | 文件 | 用途 |
|---|---|---|
| v5.6.0.3998 | `DRV_WiFi_MTK_MT7925_MT7927_TP_W11_64_V5603998_20250709R/` | MT7927 专用旧版 |
| v5.7.0.5275 | `WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys` | MT7925 新版（含 MT7927 路径） |

#### 关键逆向结论

1. **发送总入口与后端分流**：`FUN_1400cdc4c` 是发送总入口，在至少两条后端间分流

2. **固件下载走统一命令封装**：`0x01/0x02/0x05/0x07/0x10/0xee/0xef` 均走统一命令队列模型，而非裸 DMA 数据

3. **`0xee` (FW_SCATTER) 有专门分支**：
   - 写 `0xa000` 到 `hdr + 0x24`
   - token/字段有特殊处理
   - 不是直接把 chunk 挂到 ring16 就完成

4. **Windows 有复杂的命令对象管理**：命令对象 + 入队 + 槽位状态 + 事件等待

5. **Ring 初始化关键寄存器**（Windows 地址空间）：
   - TX: `0x7c024300/304/308/30c`
   - FWDL/事件: `0x7c024500/504/508/50c`
   - 停机/清理: `0x7c02420c`, `0x7c024280`

### 4.2 寄存器对比分析

建立了完整的 Windows 绝对地址 ↔ Linux mt76 符号映射：

| Windows 地址 | Linux 符号 | 说明 |
|---|---|---|
| `0x7c024208` | `MT_WFDMA0_GLO_CFG` | DMA 全局配置 |
| `0x7c0242b4` | `MT_WFDMA0_GLO_CFG_EXT1` | DMA 扩展配置 |
| `0x7c024298` | `MT_WFDMA0_INT_RX_PRI` | RX 中断优先级 |
| `0x7c02429c` | `MT_WFDMA0_INT_TX_PRI` | TX 中断优先级 |
| `0x7c027030` | `MT_WFDMA_HOST_CONFIG` | WFDMA 主机配置 |
| `0x7c0270f0~fc` | (未定义) | MSI 中断配置 (Windows 特有) |

### 4.3 MT6639 特有路径

Windows 驱动中识别到 MT6639 特有的函数：
- `MT6639InitTxRxRing` — TX/RX 环初始化
- `MT6639PreFirmwareDownloadInit` — 固件下载前初始化
- `MT6639ConfigIntMask` — 中断掩码配置

这些函数操作的寄存器包括：
- `0x000d1000` (CONN_MISC_CFG)
- `0x00010200/204/208/020` (CONN_CFG)
- `0x000e0010` (CONN_ON_LPCTL) — 电源控制
- `0x000e00f0` (CONN_ON_MISC) — 杂项控制

---

## 5. 当前驱动实现分析

### 5.1 主驱动文件：`mt7927_init_dma.c`

这是项目中最完整的驱动实现（1314 行），采用 Connac2 风格的 MCU 固件下载流程：

**初始化流程**：
```
pci_enable_device → pci_set_master → dma_set_mask(32bit)
→ pci_iomap(BAR0) → chip_status 检查
→ mt7927_drv_own() [电源所有权握手]
→ mt7927_dma_init() [DMA 初始化]
→ mt7927_dma_path_probe() [DMA 路径验证]
→ mt7927_mcu_fw_download() [固件下载]
    ├── mt7927_load_patch() [补丁加载]
    └── mt7927_load_ram() [RAM 代码加载]
```

**支持的模块参数**（用于调试）：
- `enable_predl_regs` — 启用 MT6639 预下载寄存器序列
- `wm_ring_qid` — MCU WM TX ring 选择 (15 或 17)
- `wait_mcu_event` — 等待 MCU 事件响应
- `scatter_via_wm` — 通过 WM 命令格式发送固件块
- `strict_dma_probe` — DMA 探测失败时中止
- `tx_kick_use_didx` — 使用 DIDX 作为 doorbell
- `minimal_dma_cfg` — 最小化 DMA 配置

**设备 ID 匹配**：同时支持 `0x7927` 和 `0x6639`

### 5.2 已实现的关键子系统

| 子系统 | 状态 | 说明 |
|---|---|---|
| PCI 探测/绑定 | ✅ 完整 | 标准 PCI 驱动框架 |
| 电源所有权 (drv_own) | ⚠️ 部分 | LPCTL 握手有时不同步 |
| DMA 环管理 | ✅ 完整 | TX ring (q15/q16/q17) + RX ring (q0) |
| 描述符构建 | ✅ 完整 | Connac2 格式 TXD |
| MCU 命令发送 | ✅ 完整 | 统一命令封装 |
| 固件下载协议 | ✅ 完整 | Patch + RAM 完整流程 |
| DMA 传输 | ❌ 阻塞 | 硬件不消费描述符 |

---

## 6. 技术分析：为什么 DMA 不工作

### 6.1 根本原因假设

基于所有已知证据，DMA 不工作的最可能原因是：

1. **缺少某个"队列激活/ownership"额外步骤**
   - Windows 驱动中可能有额外的寄存器写入序列在 ring 设置之后、首次 kick 之前执行
   - MT6639 的初始化路径可能需要特定的电源/时钟使能步骤

2. **Ring 地址/属性对 DMA 引擎实例无效**
   - Windows 中可能有额外的路由选择机制
   - 地址映射可能不完全匹配

3. **芯片处于写保护/预初始化状态**
   - `FW_STATUS` 卡在 `0xffff10f1`（等待固件加载）
   - `WPDMA_GLO_CFG` 停留在 `0x00000000`（不接受使能）
   - 这暗示芯片需要一个未知的解锁序列

### 6.2 `0xffff10f1` 的含义

这个 FW_STATUS 值分解：
- `0xffff` 前缀 — 可能表示错误/预初始化状态
- `10f1` — 可能是状态机的特定阶段码
- 整体含义：**芯片在等待固件传输，但 DMA 通道尚未就绪**

---

## 7. 下一步研究方向

### 7.1 推荐的单线程策略

根据 `mt7927_dma_fw_status_2026-02-13.md` 的结论，应收束方向：

**先修 DMA 消费，再谈固件协议。**

固定参数基线：
```
wm_ring_qid=15 scatter_via_wm=0 wait_mcu_event=1
tx_kick_use_didx=0 minimal_dma_cfg=1
```

### 7.2 具体行动项

1. **从 Windows `MT6639InitTxRxRing` 路径提取"首包前必须写的额外寄存器"**
   - 逐条最小增量验证
   - 一次只改 1-2 个寄存器步骤

2. **深入分析 CONN_ON_LPCTL 电源状态机**
   - `drv_own` 握手的完整序列
   - MT6639 与 MT7925 的电源管理差异

3. **检查 PCIe 配置空间初始化**
   - 可能存在 PCIe 层面的使能步骤
   - Windows 驱动在 PCI config space 中的操作

4. **验证 BAR 映射的完整性**
   - MT6639 可能使用不同的 BAR 布局
   - 检查是否需要额外的地址窗口/bank 切换

### 7.3 验证标准

只要出现**"ring16 或 ring15 首次被稳定消费"**（即 DIDX 推进），即可切回协议层处理 `0xee` 固件散列传输。

---

## 8. 资源清单

### 参考固件
- `mediatek/mt7925/WIFI_MT7925_PATCH_MCU_1_1_hdr.bin` — 补丁 (~212KB)
- `mediatek/mt7925/WIFI_RAM_CODE_MT7925_1_1.bin` — RAM 代码 (~1.2MB)

### 逆向工具
- Ghidra 12.0.3 — 主要逆向平台
- `scripts/re/` — 自动化提取脚本
- `unmtk.rb` — Ruby 辅助工具

### 运行环境
- 内核：6.18+
- 平台：Linux x86_64 (Arch)
- PCI 插槽：`0a:00.0`

### 常用命令
```bash
# 构建
make clean && make tests

# 加载驱动
sudo rmmod mt7927_init_dma 2>/dev/null
sudo insmod tests/04_risky_ops/mt7927_init_dma.ko

# 查看日志
sudo dmesg | tail -40

# 芯片复位
echo 1 | sudo tee /sys/bus/pci/devices/0000:0a:00.0/remove
sleep 2
echo 1 | sudo tee /sys/bus/pci/rescan
```

---

## 9. 文档索引

| 文档 | 内容 |
|---|---|
| `docs/mt7927_dma_fw_status_2026-02-13.md` | 当前阻塞点分析与策略 |
| `docs/mt76_vs_windows_mt7927.md` | Linux mt76 vs Windows 寄存器对比 |
| `docs/mt7925_vs_windows_mt7927.md` | MT7925 vs MT7927 寄存器分析 |
| `docs/windows_register_map.md` | Windows 驱动完整寄存器映射 |
| `docs/win_v5705275_core_funcs.md` | v5.7.0.5275 核心函数分析 (166KB) |
| `docs/win_v5705275_dma_enqueue.md` | DMA 入队序列 |
| `docs/win_v5705275_dma_lowlevel.md` | DMA 底层操作 |
| `docs/win_v5705275_fw_flow.md` | 固件下载流程 |
| `docs/win_v5705275_fw_proto_funcs.md` | 固件协议函数 |
| `docs/win_v5705275_mcu_send_core.md` | MCU 发送核心 |
| `docs/win_v5705275_mcu_send_backends.md` | MCU 发送后端 (48KB) |
| `docs/win_v5705275_mcu_dma_submit.md` | MCU DMA 提交 |
| `docs/windows_v5705275_deep_reverse.md` | 深度逆向分析 |
| `docs/windows_v5705275_initial_findings.md` | 初始发现 |
| `docs/windows_v5705275_mcu_cmd_backend.md` | MCU 命令后端 |
| `docs/win_v5603998_fw_flow.md` | 旧版驱动固件流程 |
| `docs/reverse/mtkwecx_mt6639_fw_dma_reverse.md` | MT6639 DMA 逆向 |
| `docs/reverse/mtkwecx_fw_flow_auto.md` | 自动提取的固件流程 |
| `docs/TEST_RESULTS_SUMMARY.md` | 23个测试模块结果汇总 |

---

*文档生成日期：2026-02-13*
*项目状态：DMA 传输阶段受阻，需要找到硬件队列激活的前置条件*
