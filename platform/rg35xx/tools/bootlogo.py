#!/usr/bin/env python3
"""Converts Onion's boot screen (an 8-bit RGB/RGBA PNG) into the RG35XX boot images.

Usage: bootlogo.py rle <png> <out>   init's /initlogo.rle: (count, RGB565) uint16 pairs
       bootlogo.py bmp <png> <out>   U-Boot's boot_logo.bmp: 24-bit bottom-up BMP
Plain Python (zlib only), so building the ramdisk needs no imaging library.
"""
import struct
import sys
import zlib


def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    pos, idat = 8, b''
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
            assert depth == 8 and color in (2, 6) and not interlace, 'needs 8-bit RGB(A)'
        elif kind == b'IDAT':
            idat += chunk
        pos += 12 + length
    bpp = 4 if color == 6 else 3
    raw, stride = zlib.decompress(idat), width * bpp
    rows, prev = [], bytearray(stride)
    for y in range(height):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if kind == 1:
                line[i] = (line[i] + a) & 0xff
            elif kind == 2:
                line[i] = (line[i] + b) & 0xff
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xff
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xff
        # Alpha over black, as the boot screen is drawn on a black screen
        rows.append([tuple(line[x * bpp + k] * (line[x * bpp + 3] if bpp == 4 else 255) // 255
                           for k in range(3)) for x in range(width)])
        prev = line
    return width, height, rows


def to_rle(rows):
    out, last, count = b'', None, 0
    for row in rows:
        for r, g, b in row:
            color = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
            if color == last and count < 0xffff:
                count += 1
                continue
            if count:
                out += struct.pack('<HH', count, last)
            last, count = color, 1
    return out + struct.pack('<HH', count, last)


def to_bmp(width, height, rows):
    pad = (4 - width * 3 % 4) % 4
    pixels = b''.join(bytes(c for r, g, b in row for c in (b, g, r)) + b'\0' * pad
                      for row in reversed(rows))
    header = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
    info = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    return header + info + pixels


if __name__ == '__main__':
    mode, src, dst = sys.argv[1:4]
    width, height, rows = read_png(src)
    open(dst, 'wb').write(to_rle(rows) if mode == 'rle' else to_bmp(width, height, rows))
