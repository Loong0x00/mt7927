#!/usr/bin/env python3
"""Disassemble phy_mode_from_band function at 0x14014fdfc"""
import pefile
import capstone

SYS_PATH = "/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys"
VA = 0x14014fdfc
MAX_BYTES = 0x200

pe = pefile.PE(SYS_PATH)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

data = pe.get_data(VA - pe.OPTIONAL_HEADER.ImageBase, MAX_BYTES)

print(f"FUNCTION: phy_mode_from_band at 0x{VA:x}")
print("=" * 80)

ret_count = 0
for insn in md.disasm(data, VA):
    hex_bytes = insn.bytes.hex()
    print(f"  0x{insn.address:x}:  {hex_bytes:<24s}  {insn.mnemonic:<8s} {insn.op_str}")
    if insn.mnemonic == 'ret':
        ret_count += 1
        if ret_count >= 2:
            break
