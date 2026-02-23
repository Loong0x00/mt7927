# RX Ring 4 DIDX=0 问题说明

**最后更新**: Session 37 (2026-02-23)
**阻塞天数**: 从 Session 31 起 (~7 sessions)

## 问题一句话描述

固件从不向 RX Ring 4 写入任何 WiFi 帧 (DIDX 始终=0)，导致 Auth-2 响应无法到达 host，认证失败。

## 现象

```
PRE-AUTH Ring4: BASE=0x50479000 CNT=256 CIDX=255 DIDX=0 | R6=71 R7=0
send auth to b4:ba:12:5b:63:c8 (try 1/3)
RX DIDX: R4=0 R6=72 R7=0
send auth to b4:ba:12:5b:63:c8 (try 2/3)
RX DIDX: R4=0 R6=73 R7=0
send auth to b4:ba:12:5b:63:c8 (try 3/3)
RX DIDX: R4=0 R6=74 R7=0
authentication with b4:ba:12:5b:63:c8 timed out
POST-AUTH RX DIDX: R4=0 R6=74 R7=0
```

- **Ring 4 (Band0 Data RX)**: DIDX=0，从 probe 到 auth 从未变化
- **Ring 7 (辅助 RX)**: DIDX=0，同上
- **Ring 6 (MCU Events)**: DIDX 正常前进 (每个 auth TX 后 +1，是 TXFREE 事件)
- **BIT(12)** Ring 4 中断: 从未触发
- **Band0 MIB**: TX_OK=30 (TX 正常)，RX_OK=0 (无接收)

## 已确认正常的部分

### TX 全链路 ✅ (Session 31)
- AR9271 2.4GHz 空口抓包: Auth-1 帧出现在空口
- AP (b4:ba:12:5b:63:c8) 回了多次 Auth-2
- TXFREE stat=1 count=15 — 发了但 AP 不 ACK (正常，因为 RX 没工作)

### Ring 4 DMA 配置 ✅ (Session 32)
全部与 Windows 匹配:
- BASE: 有效物理地址
- CNT: 256
- CIDX: 255 (全部 slot 开放给 DMA)
- GLO_CFG: 0x5430b875 (TX_EN=1, RX_EN=1)
- prefetch: 正确配置
- BIT(12) 在 INT_MASK 0x2600f050 中已使能

### 命令序列 ✅ (Session 36-37，对齐 Windows RE)
```
ChipConfig → DEV_INFO → BssActivateCtrl(BASIC+MLD) → PM_DISABLE
→ BSS_INFO(13 TLV) → BSS_RLM(3 TLV) → SCAN_CANCEL → ROC
→ [ROC_GRANT status=0] → STA_REC(13 TLV, STATE=0) → RX_FILTER(0x0B) → Auth TX
```
所有 MCU 命令返回 status=0。

### 扫描 ✅
- 160 BSS，走 Ring 6 MCU 内部路径 (PKT_TYPE_RX_EVENT+flag=0x1)
- 扫描不经过 Ring 4

## 已排除的原因 (每个都实测过)

| 排除项 | 测试方式 | Session |
|--------|----------|---------|
| TX 不工作 | AR9271 空口抓包确认 | S31 |
| Ring 4 DMA 配置错误 | BASE/CNT/CIDX/GLO_CFG/prefetch 全匹配 Windows | S32 |
| 缺 BSS_RLM (RLM+PROTECT+IFS_TIME) | 恢复调用，MCU ret=0，DIDX 不变 | S37 |
| 缺 RX_FILTER | 添加 0x0B filter，ret=0，DIDX 不变 | S37 |
| BSS_INFO link_idx=0 | 改为 band_idx，DIDX 不变 | S37 |
| STA_REC STATE=0 导致 STATION_PAUSE | 改 STATE=2，STA_PAUSE0 不变，DIDX 不变 | S37 |
| 缺 CHANNEL_SWITCH | 添加后返回 0xc00000bb 错误，移除 | S35 |
| 缺 RX_PATH | 添加后返回 0，但 DIDX 不变 | S35 |
| BSS_INFO TLV 格式/顺序 | 13 TLV 匹配 Windows dispatch table 顺序 | S36 |
| ROC TLV 格式 | Windows RE 格式反而导致 ROC_GRANT 超时 | S32 |
| PHY/RF enable 命令 | Windows RE 确认不存在此命令 | S32 |

## 关键诊断数据 (auth 窗口内)

### PLE 状态 (每次 auth 后都一致)
```
PLE_EMPTY   = 0xf133ffff    (有大量空闲页)
PSE_EMPTY   = 0x9ffffffb
PLE_STA0    = 0x0000000a    (WCID 1,3 有队列)
PLE_STA1    = 0x00000005
STA_PAUSE0  = 0x03000001    (BIT 0,24,25 置位 — 始终不变)
STA_PAUSE1  = 0x2fff0fff
DIS_STA0    = 0xdeadbeef    (无效标记 — 可能寄存器地址错或未初始化)
```

### WFDMA 状态
```
GLO_CFG     = 0x5430b875    (TX_EN=1, RX_EN=1, FWDL_BYPASS=0)
EXT1        = 0x9c800404    (BIT(28) set)
RX_EXT_CTRL: R4=0x00000008  R6=0x00800008  R7=0x01000004
PAUSE_R4    = 0x00010001    (Ring 4 暂停阈值)
INT_ENA     = 0x2600f050    (BIT(12) Ring4 中断已使能)
INT_STA     = 0x00000000    (Ring4 中断从未触发)
```

### DMASHDL 状态
```
SW_CTRL     = 0x10000000    (BYPASS=1, Windows 风格)
OPT_CTRL    = 0x700c8fff
GRP0        = 0x01ff0010
QMAP0       = 0x00010101
STATUS      = 0x002d002d    (正常)
```

## STA_PAUSE0=0x03000001 分析

**始终不变**，无论发什么命令 (BSS_INFO, STA_REC, BSS_RLM, STATE=0/2)：
- BIT(0)=1: WCID 0 (broadcast/mcast STA) 被暂停
- BIT(24)+BIT(25): 未知含义，可能是控制位或更高 WCID
- 这可能是固件的正常默认值 (WCID 0 可能本来就暂停)
- 也可能指示固件内部 RX 路由根本未配置

## DIS_STA0=0xdeadbeef 分析

PLE_DIS_STA_MAP0 (BAR0+0x08390) 值为 0xdeadbeef — 典型的"未初始化内存"标记。
可能原因:
1. 这个寄存器地址需要 bus2chip remap，我们读的是错误地址
2. 固件确实没初始化这个功能
3. 该功能在 CONNAC3 上不存在或位于不同地址

## PAUSE_R4=0x00010001 分析

`MT_WPDMA_PAUSE_RX_Q_TH(4)` = BAR0+0xd4270。
注意: 参数 n=4 对应的**不是** Ring 4！公式 `0x0260 + (n<<2)` 中，Ring 4 由 n=2 (0x0268) 控制。
值 0x00010001 是固件默认值 (Windows 初始化写 0x00020002)。
但阈值不会导致 DIDX 永远=0 — 它只影响流控暂停。

## 未排除的假设 (待调查)

### 假设 1: WFDMA RX 路由未配置 (概率: 高)
固件内部有独立于 MCU 命令的 RX 路由配置。可能需要特定寄存器写入告诉 WFDMA 把
Band0 WiFi 帧路由到 Ring 4。Ring 6 工作是因为 MCU 事件走独立路径。

**线索**: Windows 可能在 PostFwDownloadInit 或 WFDMA_CFG 中设置了我们没有的寄存器。
WFDMA_CFG payload `{0x820cc800, 0x3c200}` 的两个参数含义不明。

### 假设 2: 缺少某个 PostFwDownloadInit 步骤 (概率: 中)
Windows PostFwDownloadInit 可能有我们遗漏的 RX 使能步骤。
需要 Ghidra 深入分析 PostFwDownloadInit 的每个子步骤。

### 假设 3: BSS_INFO BASIC 字段值错误 (概率: 中低)
offset +0x19 (phymode): 我们发 0x0f, Windows 可能不同
offset +0x1A (nonht_basic_phy): 我们改为 0, Windows 值未知
这些值可能影响固件是否启用 RX 数据路径。

### 假设 4: 需要 MCU 命令触发 RX ring 路由建立 (概率: 中)
可能存在某个 MCU 命令 (不在已知列表中) 专门告诉固件开始向 Ring 4 投递帧。
需要通过 Ghidra 分析 Windows 驱动中 Ring 4 相关的所有代码路径。

### 假设 5: CONNAC3 RX ring 映射与预期不同 (概率: 低)
MT7927/MT6639 CONNAC3 可能使用不同于 vendor mt6639 的 ring 编号方案。
虽然 Ring 6 MCU 事件正常匹配 vendor 模式，但数据帧可能走不同 ring。
上游 mt7925 用 Ring 0 (MCU) 和 Ring 2 (Data)，完全不同的映射。

## 建议下一步

1. **Ghidra 分析 Windows WFDMA 初始化**: 找出 PostFwDownloadInit 中所有写入 WFDMA
   区域 (0xd4000-0xd5000) 的寄存器，与我们的实现逐一对比
2. **抓取 Windows 驱动 MMIO trace**: 如果可能，用 PCIe analyzer 或 Windows 驱动
   hook 记录所有 BAR0 写入，找出缺失的寄存器配置
3. **测试不同 RX ring 编号**: 尝试配置 Ring 0 或 Ring 2 作为数据 RX ring
4. **Ghidra 搜索 Ring 4 相关代码**: 在 Windows .sys 中搜索对 0xd4540 (Ring 4 偏移)
   的引用，追踪 RX ring 初始化流程
