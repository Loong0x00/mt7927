# MT7927 TX Auth 帧发送问题诊断表

## 核心问题
auth 帧通过 TX ring 0 (CT mode, TXD+TXP) 提交后，TXFREE 返回 **stat=1 count=15**。
帧从提交到 TXFREE 约 30ms。三次尝试均失败，最终 authentication timed out。
**MIB TX 计数器全零** — 帧从未到达射频前端 (在固件内部即被丢弃/失败)。

## 硬件信息
- 芯片: MT7927 = MT6639 mobile chip (PCIe package), PCI ID: 14c3:6639
- AP: CMCC-Pg2Y-5G-FAST (5GHz ch161) / CMCC-Pg2Y-2.4G (2.4GHz ch6)

## TXFREE 分析
```
DW0: 0x30010014  → PKT_TYPE=TXRX_NOTIFY, MSDU_CNT=1
DW2: 0x90001000  → PAIR: wlan_id=1
DW3: 0x5f000000  → HEADER: stat=1, count=15, wlan_id=0
DW4: 0x3fff8000  → MSDU: token=0 (valid)
```
- stat=1 (2-bit, GENMASK 29:28) = 失败
- count=15 = 我们设的 REM_TX_COUNT=15
- 30ms 延迟 — 固件有处理但帧未成功发出

## 已排除 (全部测试确认无效)

### DMA/硬件层 (4项)
- [x] DMA 地址映射 (coherent buffer 同样 stat=1)
- [x] DMA 描述符格式 (buf0=TXD+TXP 64字节, SD_LEN0=64, LAST_SEC0)
- [x] TX ring 0 prefetch 配置 (PREFETCH 0x0280, 0x4)
- [x] TX ring 0 中断掩码 (BIT(4) 已加入)

### TXD 格式 (6项)
- [x] DW0: TX_BYTES=62, PKT_FMT=0(CT), Q_IDX=0x10(ALTX0)
- [x] DW1: WLAN_IDX=1, OWN_MAC=0, TGID=1(band1), HDR_FORMAT=2(802.11)
- [x] DW2: FRAME_TYPE=0(mgmt), SUB_TYPE=11(auth)
- [x] DW3: REM_TX_COUNT=15/30, REMAINING_LIFE_TIME=0/30
- [x] DW5: PID, TX_STATUS_HOST
- [x] DW6: MSDU_CNT=1, DAS=0/1, DIS_MAT=1

### TXP 格式 (3项)
- [x] msdu_id[0] = token | MT_MSDU_ID_VALID
- [x] ptr[0].buf0 = payload DMA 地址
- [x] ptr[0].len0 = 30 | MT_TXP_LEN_LAST

### 802.11 帧 (3项)
- [x] FC=0x00b0 (Authentication)
- [x] DA=AP MAC, SA=自身 MAC, BSSID=AP MAC
- [x] Body: auth_alg=0, seq=1, status=0 (Open System Auth)

### MCU 命令序列 (9项)
- [x] DEV_INFO (OMAC + band)
- [x] BSS_INFO (BASIC + RLM + MLD_TLV)
- [x] STA_REC conn_state=0/1/2 全测试 — 无区别
- [x] BSS_INFO MLD TLV (tag=0x1A) — 新增，无效果
- [x] WTBL ADM_COUNT_CLEAR
- [x] ROC acquire → ROC_GRANT 成功
- [x] ROC 后重发全部命令
- [x] CHANNEL_SWITCH 命令 — 加/去都无区别
- [x] KeepFullPwr 0→1 — 无区别

### TX 路径变体 (4项)
- [x] Ring 0 + CT mode (PKT_FMT=0) + TXP → stat=1 count=15
- [x] Ring 15 + SF mode (PKT_FMT=1) → 固件静默丢弃
- [x] Ring 15 + CMD Q_IDX=0x10 → 同上
- [x] Ring 15 + CMD Q_IDX=0x00 → 同上

### 频段 (2项)
- [x] 5GHz ch161 — stat=1
- [x] 2.4GHz ch6 — stat=1

---

## 高优先级未测方向 (按优先级排序)

### 1. DW7 TXD_LENGTH=1 ⭐⭐⭐
**最有可能的根因。** MT6639 Android 驱动 always sets `TXD_LEN_1_PAGE` (bits[31:30]=1 in DW7)。
我们 DW7 全零 — 固件无法确定 TXD 长度，可能导致:
- TXD/payload 边界解析错误
- TXP 无法被正确定位
- 帧内容被固件当作 TXD 的一部分或反之

**来源**: MT6639 `halTxFillDataTxd()` / `halTxFillCmdTxd()` — 所有帧都设此字段。

### 2. CMD ring + PKT_FMT=2 + Q_IDX=0x20 ⭐⭐
MT6639 管理帧走 CMD ring (TC4 → TX ring 15), 使用:
- PKT_FMT=2 (不是 CT=0 也不是 SF=1)
- Q_IDX=0x20 (MCU_Q0, 不是 ALTX0=0x10)
- 我们只测试过 Q_IDX=0x10, 0x00 — **Q_IDX=0x20 从未测试**

**但注意**: MT6639 是 AXI 总线 (内存共享)，PCIe 的 CMD ring 15 之前测试失败 (SF mode 静默丢弃)。PKT_FMT=2 在 PCIe 上是否可行需要验证。

### 3. BSS_INFO RATE TLV ⭐⭐
MT6639 BSS_INFO 包含 RATE TLV — 告诉固件用什么速率发管理帧。
如果没有 RATE 信息，固件可能不知道用什么物理层参数发帧。
MT6639 发 12 个 BSS_INFO TLV, 我们只有 3 个 (BASIC + RLM + MLD)。

### 4. 更多 BSS_INFO/STA_REC TLV ⭐
- BSS_INFO 缺少: PROTECT, IFS_TIME, SEC, QBSS, HE, BSS_COLOR 等
- STA_REC 缺少: HT_INFO, VHT_INFO, HE_BASIC, STATE_INFO 等
- 这些单独可能不致命，但累积缺少可能让固件 TX 逻辑异常

### 5. Windows 驱动 TX 对比 ⭐
- docs/win_v5705275_*.md 中可能有 TX 路径信息
- 对比 Windows 的 TXD DW7 设置、Q_IDX 值、ring 选择

## MT6639 关键发现 (Session 14, 2026-02-16)

### BSS_INFO 完整 TLV 列表 (12 个)
```
arSetBssInfoTable[] = {
  BASIC, RLM, PROTECT, IFS_TIME, RATE, SEC,
  QBSS, SAP, P2P, HE, BSS_COLOR, MLD
}
```

### STA_REC 完整 TLV 列表 (10 个)
```
arUpdateStaRecTable[] = {
  BASIC, HT_INFO, VHT_INFO, HE_BASIC, HE_6G_CAP,
  STATE_INFO, PHY_INFO, RA_INFO, BA_OFFLOAD, UAPSD
}
```

### conn_state 双枚举
```c
// BSS_INFO 用 (wsys_cmd_handler_fw.h):
MEDIA_STATE_CONNECTED = 0      // 注意: 0=已连接!
MEDIA_STATE_DISCONNECTED = 1

// STA_REC 用 (wlan_def.h):
STATE_CONNECTED = 1            // 注意: 不同的枚举!
STATE_DISCONNECT = 0
```

### MLD TLV (已实现)
```c
struct bss_mld_tlv {
    tag=0x1A, len=16,
    group_mld_id=0xff,   // MLD_GROUP_NONE
    own_mld_id=bss_idx,
    own_mld_addr[6],     // = vif->addr
    om_remap_idx=0xff    // OM_REMAP_IDX_NONE
};
```

## 快速命令
```bash
make driver
echo '123456' | sudo -S rmmod mt7927 2>/dev/null; sleep 1; echo '123456' | sudo -S insmod src/mt7927.ko
echo '123456' | sudo -S timeout 20 wpa_supplicant -i wlp9s0 -c /tmp/wpa_mt7927.conf -d
sudo dmesg | tail -60
```
