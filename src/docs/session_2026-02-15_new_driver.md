# Session 10: 新驱动从头实现 (2026-02-15)

## 决策：放弃 mt7925 移植，从头写

### 原因
- 现有驱动继承自 mt7925 (CONNAC2)，架构与 MT6639 (CONNAC3) 完全不同
- 54 个实验 mode (11-54) 全部失败，根因是参考对象错误
- BT 驱动的成功先例证明"直接实现 vendor 行为"是正确路径

### 5 个 agent 并行分析结果（Session 10 前半段）

| Agent | 关键发现 |
|-------|---------|
| **win-reverser** (Opus) | Windows 使用 RX ring 4, 6, 7（不含 ring 0!）；预取配置 0xd70f0-fc；INT_ENA=0x2600f000 |
| **upstream-dma** (Sonnet) | mt7925 使用 ring 0 — 但这是 CONNAC2，不适用于我们 |
| **fw-analyst** (Sonnet) | 固件正确（MT6639, 2026/01/05），比 Windows 更新，不是 root cause |
| **mt7925-mcu** (Sonnet) | 确认 mt7925 架构与 MT6639 完全不同 |
| **mode54-dev** (Sonnet) | Mode 54 代码完成但方向错误（ring 0 不需要） |

### 核心矛盾解决
- **mt7925 说要 ring 0** vs **Windows RE 说不要 ring 0** → Windows RE 是权威
- **我们用 ring 5** vs **Windows 用 ring 6** → ring 5 是错的！
- **我们缺少预取配置** → 0xd70f0-fc 从未写入
- **中断掩码完全不同** → 0x2600f000 vs 0x0E000043

## 新驱动实现

### Phase 1: 并行研究（3 个 Sonnet agent）
| 产出文件 | 内容 |
|----------|------|
| `src/docs/register_playbook.md` | 完整寄存器序列蓝图（60+ 寄存器，7 个阶段） |
| `src/docs/reusable_code.md` | 可复用代码清单（9 个模块 + 行号范围） |
| `src/docs/bt_driver_patterns.md` | BT 驱动设计模式分析 |

### Phase 2: 并行实现（header-dev Sonnet + driver-dev Opus）
| 文件 | 行数 | 说明 |
|------|------|------|
| `src/mt7927_pci.h` | 644 | 寄存器定义、结构体、宏 — 零 mt7925 污染 |
| `src/mt7927_pci.c` | 1474 | 完整 probe→NIC_CAP 驱动 — Windows 初始化序列 |
| `src/Kbuild` | 2 | 编译配置 |
| `src/mt7927_pci.ko` | 523KB | 编译通过 ✅ |

### 新驱动 vs 旧驱动对比
| | 旧 (mt7927_init_dma.c) | 新 (mt7927_pci.c) |
|---|---|---|
| 代码量 | 4000+ 行 | 1474 行 |
| 参考来源 | mt7925 (CONNAC2) ❌ | Windows RE (CONNAC3) ✅ |
| 初始化路径 | 54 个 mode 分支 | 单一路径 |
| RX rings | 0(mode53), 4, 5, 6(evt), 7 | 4, 6, 7 |
| 预取配置 | per-ring EXT_CTRL | packed (0xd70f0-fc) ✅ |
| 中断掩码 | 0x0E000043 | 0x2600f000 ✅ |
| GLO_CFG_EXT1 BIT(28) | 有 | 有 ✅ |
| DMASHDL enable | 有 | 有 ✅ |

### Windows 初始化序列（新驱动实现）
```
1. PCI probe + BAR0 mapping
2. mt7927_pre_fw_init()      — PreFirmwareDownloadInit（芯片状态检查）
3. mt7927_init_tx_rx_ring()  — InitTxRxRing（rings 4, 6, 7 + TX 15）
4. mt7927_wpdma_config()     — WpdmaConfig（预取 + GLO_CFG + EXT1 BIT(28)）
5. mt7927_config_int_mask()  — ConfigIntMask（0x2600f000）
6. mt7927_fw_download()      — FWDL（搬运自旧驱动，已验证 work）
7. mt7927_post_fw_init()     — PostFwDownloadInit（DMASHDL + NIC_CAP）
```

## 编译命令
```bash
make driver    # 编译新驱动
```

## 测试命令（重启后）
```bash
sudo insmod src/mt7927_pci.ko
sudo dmesg | tail -100
```

## 预期结果
- fw_sync=0x3（FWDL 成功）✅ — 已在旧驱动验证过
- MCU_RX0 BASE != 0 — 这是关键测试点
- NIC_CAP 返回 0（成功）而不是 -110（超时）

## 如果 NIC_CAP 仍然失败
可能原因和下一步：
1. 0x74030188 BAR0 映射不对 → 需要确认 bus2chip 表
2. PreInit 状态轮询不完整 → 需要更多 RE
3. PostFW Intermediate 阶段缺失（vtable +0x28）→ 需要分析
4. Ring 大小不对 → 调整 entries 数量
5. FWDL 后需要 re-init rings（CLR_OWN 副作用）→ 检查 reprogram

## Git 状态
- 当前分支: `experiment/mode43-vendor-order`
- 新驱动在 `src/` 目录，尚未 commit
- 旧驱动在 `tests/04_risky_ops/` 保留作为对照
