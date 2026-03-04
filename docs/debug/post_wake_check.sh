#!/bin/bash
# 唤醒后自动执行的检查脚本
LOG=/home/user/mt7927/docs/debug/suspend_wake_test_$(date +%Y%m%d_%H%M%S).log

echo "=== MT7927 Suspend/Wake Test ===" | tee "$LOG"
echo "Wake time: $(date)" | tee -a "$LOG"
echo "Kernel: $(uname -r)" | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo "--- dmesg: suspend/resume path ---" | tee -a "$LOG"
journalctl -k --since "5 minutes ago" 2>/dev/null \
  | grep -iE "mt7925|mt6639|PM: suspend|PM: resume|PM: Waking|hif|aspm|disabling ASPM" \
  | tee -a "$LOG"
echo "" | tee -a "$LOG"

echo "--- rfkill ---" | tee -a "$LOG"
rfkill list | tee -a "$LOG"

echo "--- interface state ---" | tee -a "$LOG"
ip link show wlp9s0 | tee -a "$LOG"

echo "--- wpa state ---" | tee -a "$LOG"
wpa_cli -i wlp9s0 status 2>/dev/null | grep -E "ssid|wpa_state|freq" | tee -a "$LOG"

echo "--- IP ---" | tee -a "$LOG"
ip addr show wlp9s0 | grep inet | tee -a "$LOG"

echo "--- ping test ---" | tee -a "$LOG"
ping -c 3 -I wlp9s0 223.5.5.5 2>&1 | tail -3 | tee -a "$LOG"

echo "" | tee -a "$LOG"
echo "Log: $LOG"
