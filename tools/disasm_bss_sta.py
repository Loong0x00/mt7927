#!/usr/bin/env python3
"""
Disassemble Windows mtkwecx.sys functions for BSS_INFO / STA_REC / BssActivateCtrl analysis.
Target functions:
  1. BSS_INFO BASIC TLV builder: 0x14014c610
  2. BssActivateCtrl:            0x140143540
  3. STA_REC BASIC TLV:          0x14014d6d0
  4. conn_type helper:           0x140151608
"""
import pefile
import capstone
import sys

SYS_PATH = "/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys"

TARGETS = {
    "BSS_INFO_BASIC_TLV_builder_0x14014c610": (0x14014c610, 0x300),
    "BssActivateCtrl_0x140143540": (0x140143540, 0x600),
    "STA_REC_BASIC_TLV_0x14014d6d0": (0x14014d6d0, 0x300),
    "conn_type_helper_0x140151608": (0x140151608, 0x200),
}

def va_to_offset(pe, va):
    """Convert virtual address to file offset."""
    for section in pe.sections:
        sec_va_start = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
        sec_va_end = sec_va_start + section.Misc_VirtualSize
        if sec_va_start <= va < sec_va_end:
            return section.PointerToRawData + (va - sec_va_start)
    return None

def disassemble_function(pe, md, name, va, max_bytes):
    """Disassemble a function starting at va, up to max_bytes or a RET instruction."""
    offset = va_to_offset(pe, va)
    if offset is None:
        print(f"\n{'='*80}")
        print(f"FUNCTION: {name}")
        print(f"ERROR: VA 0x{va:x} not found in any section")
        return

    data = pe.get_data(va - pe.OPTIONAL_HEADER.ImageBase, max_bytes)

    print(f"\n{'='*80}")
    print(f"FUNCTION: {name}")
    print(f"VA: 0x{va:x}, File offset: 0x{offset:x}, Disasm bytes: {len(data)}")
    print(f"{'='*80}")

    ret_count = 0
    for insn in md.disasm(data, va):
        # Format: address  hex_bytes  mnemonic  operands
        hex_bytes = insn.bytes.hex()
        print(f"  0x{insn.address:x}:  {hex_bytes:<24s}  {insn.mnemonic:<8s} {insn.op_str}")

        # Stop after 'ret' (but allow some after first ret for multiple exit points)
        if insn.mnemonic == 'ret':
            ret_count += 1
            if ret_count >= 2:
                break

    print(f"{'='*80}\n")

def main():
    pe = pefile.PE(SYS_PATH)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True

    print(f"Analyzing: {SYS_PATH}")
    print(f"ImageBase: 0x{pe.OPTIONAL_HEADER.ImageBase:x}")
    print(f"Sections:")
    for s in pe.sections:
        name = s.Name.decode('utf-8', errors='replace').rstrip('\x00')
        print(f"  {name}: VA=0x{s.VirtualAddress:x} Size=0x{s.Misc_VirtualSize:x} "
              f"RawOff=0x{s.PointerToRawData:x} RawSize=0x{s.SizeOfRawData:x}")

    for name, (va, max_bytes) in TARGETS.items():
        disassemble_function(pe, md, name, va, max_bytes)

if __name__ == "__main__":
    main()
