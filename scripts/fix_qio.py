"""Patch ESP32-S3 firmware image byte 2 to QIO and recalc checksum + SHA-256.
"""
import hashlib
import sys

from esptool.bin_image import LoadFirmwareImage


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

    # Parse image to get segment data
    img = LoadFirmwareImage("esp32s3", path)

    # Recalculate XOR checksum over all segment data
    ck = 0xEF
    for seg in img.segments:
        for b in seg.data:
            ck ^= b

    # Find checksum position: the byte before the trailing SHA-256
    chk_off = len(data) - 33
    sha_off = len(data) - 32

    # Patch byte 2
    data[2] = 0x00

    # Write new checksum
    data[chk_off] = ck & 0xFF

    # Zero out old hash and recalculate over everything up to checksum
    data[sha_off:] = b"\x00" * 32
    h = hashlib.sha256(data[:sha_off]).digest()
    data[sha_off:] = h

    with open(path, "wb") as f:
        f.write(data)

    print(f"{path}: byte 2 {old:#04x} -> 0x00 (QIO), hash recalculated")


if __name__ == "__main__":
    main()
