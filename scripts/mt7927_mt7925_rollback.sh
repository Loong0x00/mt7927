#!/usr/bin/env bash
set -euo pipefail

echo "[1/3] Unload local mt76 stack (ignore failures)"
modprobe -r mt7925e mt7925-common mt7921e mt7921-common mt792x-lib mt76-connac-lib mt76 2>/dev/null || true
rmmod mt7925e mt7925_common mt7921e mt7921_common mt792x_lib mt76_connac_lib mt76 2>/dev/null || true

echo "[2/3] Try to load distro modules"
modprobe mt76 2>/dev/null || true
modprobe mt76_connac_lib 2>/dev/null || true
modprobe mt792x_lib 2>/dev/null || true
modprobe mt7925_common 2>/dev/null || true
modprobe mt7925e 2>/dev/null || true

echo "[3/3] Current module state"
lsmod | grep -E 'mt76|mt7925|mt792x' || true

echo "Rollback done. If module state is still odd, reboot once to fully restore stock state."
