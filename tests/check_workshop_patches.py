"""Read-only verification of Workshop hook sites in an unmodified SMACX 2.0 exe."""

import argparse
from pathlib import Path
import re
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    image = args.executable.read_bytes()
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Not a PE executable")
    count, = struct.unpack_from("<H", image, pe + 6)
    optional_size, = struct.unpack_from("<H", image, pe + 20)
    base, = struct.unpack_from("<I", image, pe + 24 + 28)
    sections = [
        struct.unpack_from("<8sIIII", image, pe + 24 + optional_size + 40 * i)
        for i in range(count)
    ]

    def read(address, size):
        rva = address - base
        for _, _, start, raw_size, offset in sections:
            if start <= rva and rva + size <= start + raw_size:
                return image[offset + rva - start:offset + rva - start + size]
        raise ValueError(f"Unmapped address: {address:08X}")

    source = (Path(__file__).resolve().parents[1] / "src/workshop.cpp").read_text()
    arrays = {
        name: bytes(int(value.strip(), 0) for value in values.split(",") if value.strip())
        for name, values in re.findall(r"const byte (\w+)\[\] = \{([^}]+)\};", source)
    }
    checked = 0

    def check(address, expected):
        nonlocal checked
        actual = read(address, len(expected))
        if actual != expected:
            raise ValueError(
                f"Mismatch at {address:08X}: expected {expected.hex()}, got {actual.hex()}"
            )
        checked += 1

    for address, name in re.findall(
        r"write_bytes\((0x[0-9A-F]+), (\w+), NULL, sizeof\(\2\)\)", source
    ):
        check(int(address, 16), arrays[name])

    def call(address, target):
        check(address, b"\xE8" + struct.pack("<i", target - address - 5))

    setups = re.search(r"const int setup_calls\[\] = \{([^}]+)\}", source).group(1)
    for value in setups.split(","):
        call(int(value.strip(), 0), 0x43E490)
    for address in (0x43853F, 0x438563, 0x4385E4):
        call(address, 0x437BF0)
    call(0x43798B, 0x5DCEA0)
    check(0x66B194, struct.pack("<I", 0x439960))
    check(0x66B1A0, struct.pack("<I", 0x439700))
    check(0x580B52, struct.pack("<I", 0x0044AE1B))
    check(0x580BF9, struct.pack("<I", 0x0048841A))
    print(f"Verified {checked} Workshop hook sites (no files modified).")


if __name__ == "__main__":
    main()
