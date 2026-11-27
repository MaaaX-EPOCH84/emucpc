#!/usr/bin/env python3
# ================================================================
#     ______                 __________  ______
#    / ____/___ ___  __  __/ ____/ __ \/ ____/
#   / __/ / __ `__ \/ / / / /   / /_/ / /
#  / /___/ / / / / / /_/ / /___/ ____/ /___
# /_____/_/ /_/ /_/\__,_/\____/_/    \____/
#
# EmuCPC — Amstrad CPC, CPC Plus and GX4000, down to the 1/16 µs
# ================================================================
# Author: Maxime MARCONATO (MaaaX^^, aka EPOCH84)
# ================================================================
# License: GNU General Public License v3.0 or later — see LICENSE.
# SPDX-License-Identifier: GPL-3.0-or-later
# ================================================================

# Takes the z80test data that EmuCPC keeps in third_party/z80test/ out of
# the official release archive of Patrik Rak (z80test-1.2a.zip, MIT) and of
# the reference images of his repository (img/, at the commit of 1.2a; the
# archive does not hold them):
#
# - the code block of each .tap (the bytes loaded at 8000h), checked against
#   the block checksum of the TAP format;
# - from each reference image of z80ccfscr (352 x 240, Spectrum screen with
#   its border), the bitmap it shows in 4000h-4FFFh, the part the program
#   draws: a pixel in ink (black) is a set bit.
#
# Run once, by hand, with the release unpacked:
#   python3 -I extract_release.py <unpacked z80test-1.2a> <img folder> <third_party/z80test>
# The sums of what it writes are recorded in third_party/VERSIONS.md.

import struct
import sys
import zlib
from pathlib import Path

PROGRAMS = ("z80full", "z80flags", "z80doc", "z80docflags", "z80ccf", "z80memptr", "z80ccfscr")
SCREENS = {"zilog": "z80ccfscr-zilog.png", "nec": "z80ccfscr-nec.png", "st": "z80ccfscr-st-cmos.png"}
LOAD_ADDRESS = 0x8000

# The Spectrum screen inside the reference images.
BORDER_X, BORDER_Y = 48, 24
DRAWN_ROWS = 128  # 4000h-4FFFh: the top two thirds


def tap_code_block(tap: bytes) -> bytes:
    """The data block that follows the CODE header loaded at 8000h."""
    blocks = []
    pos = 0
    while pos < len(tap):
        (length,) = struct.unpack_from("<H", tap, pos)
        block = tap[pos + 2 : pos + 2 + length]
        if len(block) != length:
            raise ValueError("truncated TAP block")
        check = 0
        for b in block:
            check ^= b
        if check != 0:
            raise ValueError(f"bad TAP checksum at offset {pos}")
        blocks.append(block)
        pos += 2 + length
    for header, data in zip(blocks, blocks[1:]):
        if header[0] == 0x00 and header[1] == 3 and data[0] == 0xFF:
            size, start = struct.unpack_from("<HH", header, 12)
            if start != LOAD_ADDRESS:
                raise ValueError(f"CODE block loads at {start:04X}h, not {LOAD_ADDRESS:04X}h")
            code = data[1:-1]
            if len(code) != size:
                raise ValueError("CODE block length differs from its header")
            return code
    raise ValueError("no CODE block")


def png_indexed(path: Path) -> tuple[int, int, list[list[int]], list[tuple[int, int, int]]]:
    """Width, height, rows of palette indices and palette of a non-interlaced
    indexed PNG of bit depth 1, 2, 4 or 8."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    idat = b""
    palette = []
    width = height = depth = 0
    while pos < len(data):
        length, kind = struct.unpack_from(">I4s", data, pos)
        body = data[pos + 8 : pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if colour != 3 or interlace != 0 or depth not in (1, 2, 4, 8):
                raise ValueError(f"{path}: not a plain indexed PNG")
        elif kind == b"PLTE":
            palette = [tuple(body[i : i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = (width * depth + 7) // 8
    rows = []
    previous = bytearray(stride)
    pos = 0
    for _ in range(height):
        kind = raw[pos]
        line = bytearray(raw[pos + 1 : pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):  # filter unit: one byte when depth < 8
            left = line[i - 1] if i > 0 else 0
            up = previous[i]
            upleft = previous[i - 1] if i > 0 else 0
            if kind == 1:
                line[i] = (line[i] + left) & 0xFF
            elif kind == 2:
                line[i] = (line[i] + up) & 0xFF
            elif kind == 3:
                line[i] = (line[i] + (left + up) // 2) & 0xFF
            elif kind == 4:
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                pred = left if pa <= pb and pa <= pc else up if pb <= pc else upleft
                line[i] = (line[i] + pred) & 0xFF
            elif kind != 0:
                raise ValueError(f"{path}: bad filter {kind}")
        previous = line
        per_byte = 8 // depth
        mask = (1 << depth) - 1
        row = []
        for x in range(width):
            byte = line[x // per_byte]
            shift = 8 - depth * (x % per_byte + 1)
            row.append((byte >> shift) & mask)
        rows.append(row)
    return width, height, rows, palette


def screen_offset(y: int, x_byte: int) -> int:
    """Offset from 4000h of the byte holding pixels x_byte*8.. of row y."""
    return ((y & 0xC0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | x_byte


def drawn_bitmap(path: Path) -> bytes:
    width, height, rows, palette = png_indexed(path)
    if (width, height) != (352, 240):
        raise ValueError(f"{path}: {width} x {height}, 352 x 240 expected")
    black = {i for i, rgb in enumerate(palette) if rgb == (0, 0, 0)}
    if not black:
        raise ValueError(f"{path}: no black in the palette")
    screen = bytearray(DRAWN_ROWS * 32)
    for y in range(192):
        for x in range(256):
            ink = rows[BORDER_Y + y][BORDER_X + x] in black
            if y >= DRAWN_ROWS:
                if ink:
                    raise ValueError(f"{path}: ink below the drawn part at row {y}")
                continue
            if ink:
                screen[screen_offset(y, x // 8)] |= 0x80 >> (x % 8)
    return bytes(screen)


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: extract_release.py <unpacked z80test-1.2a> <img folder> <output folder>", file=sys.stderr)
        return 2
    release, images, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    for name in PROGRAMS:
        code = tap_code_block((release / f"{name}.tap").read_bytes())
        (out / f"{name}.bin").write_bytes(code)
        print(f"{name}.bin: {len(code)} bytes")
    for variant, image in SCREENS.items():
        screen = drawn_bitmap(images / image)
        (out / f"z80ccfscr-{variant}.scr").write_bytes(screen)
        print(f"z80ccfscr-{variant}.scr: {len(screen)} bytes from {image}")
    (out / "license.txt").write_bytes((release / "license.txt").read_bytes())
    return 0


if __name__ == "__main__":
    sys.exit(main())
