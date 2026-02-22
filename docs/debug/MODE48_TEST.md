# Mode 48 Test Implementation

## 实现完成时间
2026-02-15

## 分支
`experiment/mode43-vendor-order`

## 实现位置
`/home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.c`
- Mode 48 代码: line 3407-3547
- reinit_mode 范围更新: line 3527 (43-48)

## Mode 48 设计

### 基础
在 Mode 46 基础上（DMASHDL bypass 让 TX ring 15 工作）添加全面诊断。

### 关键步骤

1. **启用 DMASHDL bypass**（必需，让 ring 15 描述符被消费）
   - GLO_CFG BIT(9) = MT_GLO_CFG_FW_DWLD_BYPASS_DMASHDL
   - DMASHDL_SW BIT(28) = MT_HIF_DMASHDL_BYPASS_EN

2. **Baseline dump**（发送前）
   - 所有 HOST RX ring 0-7 状态 (BASE/CNT/CIDX/DIDX)
   - 所有 MCU RX ring 0-3 状态（特别关注 MCU_RX0 BASE=0 blocker）

3. **发送 NIC_CAPABILITY**
   - 通过 `mt7927_mode40_send_nic_cap()`
   - Q_IDX=0x20, ring 15

4. **增强诊断**（发送后）
   - 写 HOST2MCU_SW_INT_SET (0xd4108) = BIT(0) — 通知 MCU
   - 等 100ms
   - Dump 所有 HOST RX ring 0-7（标记 ring 6/7 为 MCU event 候选）
   - Dump 所有 MCU RX ring 0-3（标记 MCU_RX0 为 blocker）
   - Dump MCU2HOST_SW_INT_STA (MCU_CMD_REG 0xd41f0)
   - Dump WFDMA_HOST_INT_STA
   - 等 500ms，检查 HOST RX ring 6/7 DIDX 变化（正常 MCU event 路径）

### 寄存器映射
```
HOST RX rings: MT_WPDMA_RX_RING_BASE/CNT/CIDX/DIDX(n)
  - Ring 0-7: BAR0+0xd4500 + n*0x10

MCU RX rings: 直接 ioread32(dev->bar0 + 0xd4000 + offset)
  - MCU_RX0: 0x540
  - MCU_RX1: 0x550
  - MCU_RX2: 0x560
  - MCU_RX3: 0x570
```

## 编译
```bash
make -C /home/user/mt7927 tests
```

## 测试方法
```bash
# 卸载旧模块
sudo rmmod mt7927_init_dma

# 加载 Mode 48
sudo insmod /home/user/mt7927/tests/04_risky_ops/mt7927_init_dma.ko reinit_mode=48

# 查看日志
sudo dmesg | grep MODE48
```

## 预期诊断输出

### 成功标志
- **TX ring 15 DIDX 增长**（bypass 工作，描述符被消费）
- **HOST RX ring 6/7 DIDX 变化**（MCU 写了响应到 event ring）
- **MCU2HOST_SW_INT_STA 非零**（MCU 产生了中断信号）
- **WFDMA_HOST_INT_STA 有 RX done bit**

### 失败标志
- MCU_RX0 BASE 仍然 = 0（主要 blocker）
- HOST RX ring 6/7 DIDX 不变（MCU 没有写响应）
- 所有 HOST RX ring DIDX = 0（MCU 可能根本没收到命令）

## 核心假设
Mode 48 测试：在 DMASHDL bypass 环境下（已知让 TX ring 15 工作），MCU 是否：
1. 收到了命令（检查 MCU_RX0 DIDX 或其他 MCU RX ring）
2. 生成了响应（检查 HOST RX ring 6/7 DIDX，MCU2HOST_SW_INT_STA）
3. 响应被路由到了哪里（全面 dump 所有 RX ring）

## 相关 Mode
- Mode 46: 发现 bypass 让 ring 15 工作，但 NIC_CAP 仍超时
- Mode 47: 尝试 ACK 0x0000e000 + Mode 40，失败
- Mode 48: 在 bypass 环境下全面诊断 MCU 响应路径
