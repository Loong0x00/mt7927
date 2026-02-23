# EAPOL 超时 + 双向 ACK 失败

**创建**: Session 41 (2026-02-23)
**最后更新**: Session 43 (2026-02-23)
**前置**: Auth + Association 成功 (S40 突破)

## 问题一句话描述

802.11 Auth + Assoc 均成功，但 WPA2 EAPOL 4-way handshake 超时。
AR9271 空口抓包证实 **AP 从不发 EAPOL Key 1/4**，且**我们不发 ACK**。

## 现象

### wpa_supplicant 日志
```
Associated with b4:ba:12:5b:63:c8 (SSID='CMCC-Pg2Y-2.4G')
[10 秒无任何 EAPOL 帧]
CTRL-EVENT-DISCONNECTED bssid=b4:ba:12:5b:63:c8 reason=15 (4WAY_HANDSHAKE_TIMEOUT)
```

### AR9271 空口抓包 (wlp16s0u9, ch6 monitor mode)
| 帧类型 | 方向 | 次数 | 备注 |
|--------|------|------|------|
| Auth-1 | STA→AP | 7+ | REM_TX_COUNT=30 导致重传 |
| Auth-2 | AP→STA | 多份 | AP 重传 (没收到我们的 ACK) |
| Assoc Req | STA→AP | 17+ | 大量重传 |
| Assoc Resp | AP→STA | 15+ | AP 重传 (没收到我们的 ACK) |
| ACK | AP→STA | **33** | AP 确实在发 ACK (S42 发现) |
| ACK | STA→AP | **0** | 我们从不发 ACK |
| EAPOL Key 1/4 | AP→STA | **0** | AP 从未启动 EAPOL |

### MIB 计数器
```
Band0: TX_OK=0 RX_OK=0 MDRDY=0 (始终全零)
```

## 根因分析

### 确定的因果链
```
硬件不认识我们的 MAC (MUAR 未编程 或 RX 路径异常)
    ↓
收到 AP 帧 (因为 DROP_OTHER_UC 已清除) 但硬件不发 ACK
    ↓
AP 认为帧未送达, 不停重传
    ↓
AP 不确认 Assoc 完成 → 不启动 EAPOL
    ↓
WPA 4-way handshake 超时
```

### ACK 不对称性 (S42 关键发现)
- **AP 发 ACK**: AR9271 捕获 33 个 ACK (RA=我们的 MAC, -66dBm) → AP 收到我们的帧
- **我们不发 ACK**: AR9271 零个来自我们的 ACK → 硬件不响应 AP 的帧
- **我们检测不到 ACK**: MIB MPDU_ERR=30 → 尽管 AP 发了 ACK，我们的硬件不处理

### MIB 全零的含义
MIB RX_OK=0 且 MDRDY=0 说明硬件 MAC 层**完全不统计收帧**。帧到达 Ring 4
是因为 S39 BNRCFR 旁路，不是通过正常 RMAC 处理路径。这可能意味着 RMAC 没有正常
处理帧 → 不做 MUAR 匹配 → 不生成 ACK。

## S43 调查: DEV_INFO byte[8] 修复 + RMAC 寄存器扫描

### DEV_INFO byte[8] bug (Ghidra 反编译确认)

从 `nicUniCmdBssActivateCtrl` (0x140143540) Ghidra 反编译，确认 16 字节 wire format:
```
pcVar2[0x00] = pcVar1[3]       → omac_idx (MUAR slot)
pcVar2[0x01] = 0xFF/0xFE       → type_flag (STA=0xFF, AP=0xFE, 不是 activate/deactivate!)
pcVar2[0x04-0x07] = 0x000C0000 → conn_info
pcVar2[0x08] = pcVar1[1]       → ucActive (1=activate, 0=deactivate)
pcVar2[0x0a-0x0f] = MAC[6]
```

我们之前 byte[8] = ownmac_idx = 0（错误），Windows 发 byte[8] = ucActive = 1。
**已修复**: byte[8] = 1 (activate), byte[1] deactivate 路径从 0xFE 改为 0xFF。

**修复结果**: Auth+Assoc 正常，但 EAPOL 仍超时。MUAR 仍未编程。

### RMAC 全范围寄存器扫描

扫描 RMAC Band0 偏移 0x000-0x3FF（256 个 32-bit 寄存器），打印所有非零值（约 50 个）。
**结论: 我们的 MAC 地址不在任何 RMAC 寄存器中。**

注意: RMAC+0x025c/0x0260 (mt7615 MAR0/MAR1) 在 CONNAC3 上可能是完全不同的寄存器。
用户指出 mt6639 CODA 寄存器偏移不一定可靠（手机芯片 vs PCIe 封装，内部总线不同）。

## 已排除的方向 ✅

| 方向 | 排除证据 | Session |
|------|---------|---------|
| DEV_INFO byte[8] | Ghidra 确认, 已修复 active=1, EAPOL 仍超时 | S43 |
| DEV_INFO 格式 | 16B flat, Ghidra 逐字节确认匹配 | S42-S43 |
| DEV_INFO 独立 MUAR 命令 | Windows dispatch table 无 CID 0x09 (REPT_MUAR) | S42 |
| RXD 解析器 | rxd-reverser 交叉验证 4 来源, 结构正确 | S41 |
| STA_REC STATE | 已修复: AUTH→ASSOC 发 state=2 | S41 |
| RFCR DROP_OTHER_UC | 已清除, 帧到达 Ring 4 | S40 |
| BSS_INFO/STA_REC TLV 格式 | 匹配 Windows dispatch table 13/14 TLV | S36-S38 |
| ROC_GRANT | status=0 正常 | S32 |
| TX 全链路 | S31 AR9271 确认空口 | S31 |
| RX Ring 4 DMA | BNRCFR 修复后 DIDX 活跃 | S39 |
| RMAC MAR0/MAR1 (mt7615 offset) | 全零, 但偏移可能对 CONNAC3 不适用 | S42-S43 |
| WIUCR/WMUDR/WMUMR 手动写 | WTBL-SRAM, 不是真正 MUAR, 无效果 | S42 |

## 已确认正常的部分 ✅

- Ring 4 DMA 投递 (CIDX=DIDX, NAPI 消费)
- Ring 6 MCU 事件 (TXFREE, scan results, ROC_GRANT)
- 扫描 67-137 BSS
- Auth-2 到达 mac80211 → 进入 ASSOC
- Assoc Response 到达 mac80211 → 进入 AUTHORIZED
- STA_REC state=2 post-assoc
- AP 发 ACK (33 个, AR9271 确认)
- DEV_INFO flat 格式正确发送 (omac=0, active=1, MAC=正确)

## 待调查方向

### 方向 A: BNRCFR bypass 导致 RMAC 不正常处理帧 (概率: 高)

**假设**: S39 修改 BNRCFR 让管理帧绕过 MCU 直接到 Host Ring 4。这个旁路
可能让帧跳过了 RMAC 正常处理流程（MUAR 匹配 + ACK 生成）。

**证据**:
- MIB RX_OK=0, MDRDY=0 → 硬件 MAC 层完全不统计收帧
- 帧确实到达 Ring 4 → 通过 BNRCFR 旁路而非正常 RMAC→MDP→DMA 路径
- AP 的 ACK 我们也检测不到 (MPDU_ERR=30) → 接收路径异常

**验证方法**:
1. 检查 BNRCFR 具体设了哪些位
2. 尝试恢复正常 RX 路径（不做 BNRCFR 旁路），看 MIB 是否恢复
3. 如果正常路径帧去 MCU，看是否需要固件配合路由到 Host

### 方向 B: MUAR 在不同的硬件地址 (概率: 中)

**假设**: CONNAC3 MUAR 不在 RMAC 寄存器空间，而在 WTBLON/UWTBL 或其他区域。

**验证方法**:
1. 扫描 TMAC (0x021000-0x0213FF)
2. 扫描 WTBLON 更大范围 (0x030000-0x035000)
3. 搜索我们的 MAC 字节模式
4. 检查 WF_UWTBL_TOP MUAR 寄存器 (CODA: 0x034110/0x034114/0x034118)

### 方向 C: 需要原子批次提交 (概率: 低)

**假设**: Windows 把 DEV_INFO + BSS_INFO 作为 linked list 批量提交给固件。
分开发两个命令可能导致固件不完成 MUAR 编程。

**验证方法**:
1. 发 DEV_INFO 后不做任何诊断 dump，立即发 BssActivateCtrl
2. 或尝试在一个 skb 中构造两个独立 UniCmd

## DEV_INFO wire 格式 (Ghidra 确认, S43)

```c
struct dev_info_req {        /* 16 bytes, CID=0x01 */
    u8 omac_idx;             /* [0x00] MUAR slot (ownmac_idx) */
    u8 type_flag;            /* [0x01] 0xFF=STA, 0xFE=AP */
    u8 rsv1[2];              /* [0x02-0x03] */
    __le32 conn_info;        /* [0x04-0x07] = 0x000C0000 */
    u8 active;               /* [0x08] 1=activate, 0=deactivate (关键!) */
    u8 phy_idx;              /* [0x09] */
    u8 mac_addr[ETH_ALEN];   /* [0x0a-0x0f] */
} __packed;
```

来源: Ghidra 反编译 `nicUniCmdBssActivateCtrl` (0x140143540)
- pcVar2[0] = pcVar1[3] = ownmac_idx
- pcVar2[1] = (*pcVar1 != 4) ? 0xFF : 0xFE
- pcVar2[8] = pcVar1[1] = ucActive (1 or 0)
- pcVar2[0xa-0xf] = MAC from input[4:10]

## RMAC 寄存器扫描数据 (S43, Band 0)

RMAC base = BAR0 0x021400, 扫描 0x000-0x3FF, 非零值:
```
[0x000]=RFCR  [0x004]=RFCR1  [0x010-0x014]=timing
[0x034]=unknown  [0x040-0x044]=unknown  [0x050-0x068]=various
[0x088-0x094]=various  [0x0d0]=unknown  [0x0e4-0x0e8]=unknown
[0x100-0x114]=various  [0x124-0x144]=various  [0x160]=unknown
[0x174]=unknown  [0x198-0x1b0]=various  [0x1c8-0x1d4]=various
[0x2b0]=unknown  [0x2c4]=unknown  [0x380-0x3a0]=various
[0x3e4]=unknown
```
MAC 地址 (06:43:63:54:42:07) 不在任何寄存器值中。
