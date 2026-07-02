"""Patch ESP32-S3 firmware image byte 2 to QIO and recalc checksum + SHA-256.

Usage: python3 scripts/fix_qio.py path/to/firmware.bin
"""
import hashlib
import struct
import sys


def main():
    path = sys.argv[1]
    with open(path, "rb") as f:
        data = bytearray(f.read())

    if data[0] != 0xE9:
        print(f"{path}: not an ESP32 image (magic {data[0]:#04x})", file=sys.stderr)
        sys.exit(1)

    old = data[2]
    if old == 0x00:
        print(f"{path}: already QIO")
        return

    data[2] = 0x00

    segs = data[1]
    pos = 8 + data[9]  # header + extended header
    ck = 0xEF
    for _ in range(segs):
        if pos + 8 > len(data):
            break
        slen = struct.unpack_from("<I", data, pos + 4)[0]
        pos += 8
        for b in data[pos : pos + slen]:
            ck ^= b
        pos += slen

    data[-33] = ck & 0xFF
    data[-32:] = b"\x00" * 32
    data[-32:] = hashlib.sha256(data[:-32]).digest()

    with open(path, "wb") as f:
        f.write(data)

    print(f"{path}: byte 2 {old:#04x} -> 0x00 (QIO), hash recalculated")


if __name__ == "__main__":
    main()
