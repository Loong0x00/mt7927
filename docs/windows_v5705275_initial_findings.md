# Windows 驱动 v5.7.0.5275 初步逆向记录

## 样本
- 路径: `WiFi_AMD-MediaTek_v5.7.0.5275/`
- 核心文件: `mtkwecx.sys`, `mtkwecx.inf`, `mtkwlan.dat`
- `inf` 版本: `DriverVer = 01/05/2026,5.7.0.5275`

## 结论（先回答你的问题）
- 这个包已经同时覆盖 MT7925 + MT7927（含 6639 别名），可以直接用于辅助对比。
- 所以你暂时不需要再单独提供一份 7925 Windows 驱动。

## INF 侧确认
- MT7927/6639 设备 ID 存在（含 `DEV_6639`, `DEV_7927` 及多个子系统 ID）。
- MT7925 设备 ID 也完整存在（`DEV_7925`，以及 AMD `DEV_0717` 等）。
- 说明这是“同一驱动二进制 + 多机型配置”的统一包。

## SYS 侧初步逆向（字符串 + Ghidra 自动提取）
- 已提取到关键流程符号/字符串：
  - `MT6639PreFirmwareDownloadInit`
  - `MT6639ConfigIntMask`
  - `MT6639InitTxRxRing`
  - `MT6639WpdmaConfig`
  - `HalDownloadPatchFirmware`
  - `AsicConnac3xLoadRomPatch`
  - `AsicConnac3xLoadFirmware`
  - `AsicConnac3xWpdmaInitRing`
  - `AsicConnac3xWfdmaWaitIdle`
- 与我们当前 Linux 侧卡点直接相关的调试字符串存在：
  - `HOST_INT_STA`
  - `WPDMA_GLO_CFG_EXT*`
  - `WFDMA_MSI_CONFIG`
  - `WFDMA_HOST_CONFIG`
  - `WFDMA_DLY_IDX_CFG_*`
  - `P0R0:FWDL`

## 相比旧包 `V5603998_20250709R` 的差异
- 两个 `mtkwecx.sys` 的 `sha256` 不同，确认不是同一二进制。
- 主流程字符串整体一致（固件下载与 WFDMA 路径仍是同一套框架）。
- 新版额外可见一些调试字符串（如 `WFDMA_AXI_IO`、`WFDMA2CONN_GALS_RX/TX`），对定位门控/通路问题有价值。

## 产物
- 新版自动提取: `docs/win_v5705275_fw_flow.md`
- 旧版自动提取: `docs/win_v5603998_fw_flow.md`

## 下一步（建议）
- 以 `win_v5705275_fw_flow.md` 中以下函数为主做反编译细化并落寄存器序列：
  - `FUN_1401e5430` (`MT6639PreFirmwareDownloadInit` xref)
  - `FUN_1401e5be0` (`MT6639WpdmaConfig` xref)
  - `FUN_1401e4580` (`MT6639InitTxRxRing` xref)
  - `FUN_1401e43e0` (`MT6639ConfigIntMask` xref)
  - `FUN_1401fe1a4` (`HalDownloadPatchFirmware` xref)

