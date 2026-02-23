#!/usr/bin/env python3
"""
MT7927 Windows Driver (mtkwecx.sys) Disassembly Helper
Usage:
    python3 tools/disasm_helper.py disasm <VA_hex> [num_bytes]
    python3 tools/disasm_helper.py read <VA_hex> [num_bytes]
    python3 tools/disasm_helper.py table <VA_hex> <num_entries> <entry_size>
"""

import sys
import struct

BINARY = "/home/user/mt7927/WiFi_AMD-MediaTek_v5.7.0.5275/mtkwecx.sys"

# PE section mapping: (VMA_start, VMA_end, file_offset)
SECTIONS = [
    (0x140001000, 0x140001000 + 0x2286a1, 0x400),       # .text
    (0x14022a000, 0x14022a000 + 0x1b568,  0x228c00),    # .rdata
    (0x140246000, 0x140246000 + 0x15200,  0x244200),    # .data
    (0x14039d000, 0x14039d000 + 0x8df0,   0x259400),    # .pdata
    (0x1403a6000, 0x1403a6000 + 0x6cc,    0x262200),    # PAGE
    (0x1403a7000, 0x1403a7000 + 0xf32,    0x262a00),    # INIT
]

def va_to_offset(va):
    for vma_start, vma_end, foff in SECTIONS:
        if vma_start <= va < vma_end:
            return foff + (va - vma_start)
    raise ValueError(f"VA 0x{va:x} not in any section")

def read_bytes(va, size):
    off = va_to_offset(va)
    with open(BINARY, 'rb') as f:
        f.seek(off)
        return f.read(size)

def cmd_disasm(va, size=256):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    data = read_bytes(va, size)
    cs = Cs(CS_ARCH_X86, CS_MODE_64)
    cs.detail = True
    print(f"=== Disassembly at 0x{va:x} ({size} bytes) ===")
    for insn in cs.disasm(data, va):
        print(f"  0x{insn.address:x}:  {insn.mnemonic:8s} {insn.op_str}")
        # Stop at ret instruction
        if insn.mnemonic == 'ret':
            break

def cmd_read(va, size=64):
    data = read_bytes(va, size)
    print(f"=== Data at 0x{va:x} ({size} bytes) ===")
    for i in range(0, len(data), 16):
        hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
        print(f"  0x{va+i:x}: {hex_str:<48s} {ascii_str}")

def cmd_table(va, num_entries, entry_size):
    """Read a dispatch table with function pointers"""
    data = read_bytes(va, num_entries * entry_size)
    print(f"=== Dispatch Table at 0x{va:x} ({num_entries} entries × {entry_size} bytes) ===")
    for i in range(num_entries):
        entry = data[i*entry_size:(i+1)*entry_size]
        # First 4 bytes: size field (int32)
        size = struct.unpack_from('<i', entry, 0)[0]
        # Function pointer: depends on entry layout
        # For 16-byte entries: offset 8 is function pointer (8 bytes)
        if entry_size >= 16:
            func_ptr = struct.unpack_from('<Q', entry, 8)[0]
            extra = entry[4:8]
            extra_hex = ' '.join(f'{b:02x}' for b in extra)
            print(f"  [{i:2d}] size={size:3d} (0x{size:02x})  extra=[{extra_hex}]  func=0x{func_ptr:x}")
        else:
            hex_str = ' '.join(f'{b:02x}' for b in entry)
            print(f"  [{i:2d}] size={size}  raw: {hex_str}")

def cmd_disasm_func(va, max_size=2048):
    """Disassemble a complete function until we hit the function epilogue pattern"""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    data = read_bytes(va, max_size)
    cs = Cs(CS_ARCH_X86, CS_MODE_64)
    cs.detail = True
    print(f"=== Function at 0x{va:x} ===")
    ret_count = 0
    last_was_ret = False
    for insn in cs.disasm(data, va):
        print(f"  0x{insn.address:x}:  {insn.mnemonic:8s} {insn.op_str}")
        if insn.mnemonic == 'ret':
            ret_count += 1
            last_was_ret = True
        elif last_was_ret:
            # After ret, check if next instruction looks like function padding
            if insn.mnemonic == 'int3' or insn.mnemonic == 'nop':
                break
            last_was_ret = False
        # Safety: if we see multiple rets and padding, stop
        if ret_count >= 3:
            break

def cmd_strings(va, size=256):
    """Read and show strings near a VA"""
    data = read_bytes(va, size)
    print(f"=== Strings near 0x{va:x} ({size} bytes) ===")
    current = []
    start = 0
    for i, b in enumerate(data):
        if 32 <= b < 127:
            if not current:
                start = i
            current.append(chr(b))
        else:
            if len(current) >= 4:
                print(f"  0x{va+start:x}: {''.join(current)}")
            current = []
    if len(current) >= 4:
        print(f"  0x{va+start:x}: {''.join(current)}")

def cmd_xref_search(target_va, search_va, search_size=0x200000):
    """Search for references to target_va in code section"""
    # Search for LEA patterns that reference the target
    data = read_bytes(search_va, search_size)
    target_bytes_le = struct.pack('<Q', target_va)
    results = []
    # Search for direct 8-byte address references
    for i in range(len(data) - 7):
        if data[i:i+8] == target_bytes_le:
            results.append(search_va + i)
    # Search for RIP-relative references (LEA patterns)
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    cs = Cs(CS_ARCH_X86, CS_MODE_64)
    cs.detail = True
    for insn in cs.disasm(data, search_va):
        if insn.mnemonic == 'lea':
            # Check if the operand references our target
            op_str = insn.op_str
            if f'0x{target_va:x}' in op_str:
                results.append(insn.address)
    print(f"=== XREFs to 0x{target_va:x} ===")
    for r in results[:20]:
        print(f"  0x{r:x}")
    return results

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]
    va = int(sys.argv[2], 16)

    if cmd == 'disasm':
        size = int(sys.argv[3]) if len(sys.argv) > 3 else 512
        cmd_disasm(va, size)
    elif cmd == 'func':
        max_size = int(sys.argv[3]) if len(sys.argv) > 3 else 2048
        cmd_disasm_func(va, max_size)
    elif cmd == 'read':
        size = int(sys.argv[3]) if len(sys.argv) > 3 else 64
        cmd_read(va, size)
    elif cmd == 'table':
        num = int(sys.argv[3])
        entry_size = int(sys.argv[4]) if len(sys.argv) > 4 else 16
        cmd_table(va, num, entry_size)
    elif cmd == 'strings':
        size = int(sys.argv[3]) if len(sys.argv) > 3 else 256
        cmd_strings(va, size)
    elif cmd == 'xref':
        search_va = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x140001000
        search_size = int(sys.argv[4]) if len(sys.argv) > 4 else 0x200000
        cmd_xref_search(va, search_va, search_size)
    else:
        print(f"Unknown command: {cmd}")
        print(__doc__)
