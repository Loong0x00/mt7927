#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FW_SRC_DIR="$ROOT_DIR/tmp/fw_7927"

echo "[1/5] Copy MT7927 firmware files to /lib/firmware/mediatek"
install -Dm0644 "$FW_SRC_DIR/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin" \
  /lib/firmware/mediatek/WIFI_MT6639_PATCH_MCU_2_1_hdr.bin
install -Dm0644 "$FW_SRC_DIR/WIFI_RAM_CODE_MT6639_2_1.bin" \
  /lib/firmware/mediatek/WIFI_RAM_CODE_MT6639_2_1.bin

echo "[2/5] Unload possible in-kernel mt76 stack (ignore failures)"
modprobe -r mt7927_init_dma 2>/dev/null || true
rmmod mt7927_init_dma 2>/dev/null || true
modprobe -r mt7925e mt7925-common mt7921e mt7921-common mt792x-lib mt76-connac-lib mt76 2>/dev/null || true
rmmod mt7925e mt7925_common mt7921e mt7921_common mt792x_lib mt76_connac_lib mt76 2>/dev/null || true

echo "[3/5] Load patched local modules"
modprobe cfg80211
modprobe mac80211
insmod "$ROOT_DIR/mt76/mt76.ko"
insmod "$ROOT_DIR/mt76/mt76-connac-lib.ko"
insmod "$ROOT_DIR/mt76/mt792x-lib.ko"
insmod "$ROOT_DIR/mt76/mt7925/mt7925-common.ko"
insmod "$ROOT_DIR/mt76/mt7925/mt7925e.ko"

echo "[4/5] Show loaded mt76 modules"
lsmod | grep -E 'mt76|mt7925|mt792x' || true

echo "[5/5] Recent dmesg (mt76/mt792)"
dmesg | tail -n 120 | grep -Ei 'mt76|mt792|firmware|patch|timeout|failed|dma|wfdma' || true

echo "Done."
