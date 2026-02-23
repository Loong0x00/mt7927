#!/usr/bin/env python3
"""Extract MTK command dispatch table from mtkwecx.sys (.data VA table)."""

import argparse
import struct

DATA_BASE_VA = 0x140246000
DATA_FILE_OFF = 0x244200
TABLE_VA = 0x1402507E0
ENTRY_SIZE = 0x0D
ENTRY_COUNT = 0x3A


def parse(path: str):
    off = DATA_FILE_OFF + (TABLE_VA - DATA_BASE_VA)
    with open(path, "rb") as f:
        f.seek(off)
        blob = f.read(ENTRY_SIZE * ENTRY_COUNT)

    for i in range(ENTRY_COUNT):
        ent = blob[i * ENTRY_SIZE : (i + 1) * ENTRY_SIZE]
        cmd = struct.unpack_from("<H", ent, 0)[0]
        mark = struct.unpack_from("<H", ent, 2)[0]
        sub = ent[4]
        fn = struct.unpack_from("<Q", ent, 5)[0]
        if mark == 0xA5:
            continue
        print(f"{i:02d}: cmd=0x{cmd:02x} sub=0x{sub:02x} fn=0x{fn:016x}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("sys", help="path to mtkwecx.sys")
    args = ap.parse_args()
    parse(args.sys)
