#!/usr/bin/env python3
"""
Reconstruct the NES Elite dashboard BMP from source data.

Maps NES tile colors to the GFX_COL_xxx palette indices used by
the C code, so tile colors and dynamically drawn elements (gauges,
text, etc.) share the same palette.

Pattern table 0 layout:
  Patterns 0-4:   System/blank tiles
  Patterns 5-68:  Icon bar tiles (from iconBarImage1_pattern0.bin)
  Patterns 69-255: Dashboard tiles (from dashImage_pattern0.bin)
"""

import struct
import os
import shutil

NES_SRC = "/tmp/elite-source-code-nes/1-source-files"

# ---------- NES standard NTSC color palette ----------
NES_PALETTE = [
    (84,84,84),    (0,30,116),    (8,16,144),    (48,0,136),
    (68,0,100),    (92,0,48),     (84,4,0),      (60,24,0),
    (32,42,0),     (8,58,0),      (0,64,0),      (0,60,0),
    (0,50,60),     (0,0,0),       (0,0,0),        (0,0,0),
    (152,150,152), (8,76,196),    (48,50,236),   (92,30,228),
    (136,20,176),  (160,20,100),  (152,34,32),   (120,60,0),
    (84,90,0),     (40,114,0),    (8,124,0),     (0,118,40),
    (0,102,120),   (0,0,0),       (0,0,0),        (0,0,0),
    (236,238,236), (76,154,236),  (120,124,236), (176,98,236),
    (228,84,236),  (236,88,180),  (236,106,100), (212,136,32),
    (160,170,0),   (116,196,0),   (76,208,32),   (56,204,108),
    (56,180,204),  (60,60,60),    (0,0,0),        (0,0,0),
    (236,238,236), (168,204,236), (188,188,236), (212,178,236),
    (236,174,236), (236,174,212), (236,180,176), (228,196,144),
    (204,210,120), (180,222,120), (168,226,144), (152,226,180),
    (160,214,228), (160,162,160), (0,0,0),        (0,0,0),
]

# ---------- NES color code to BMP palette index mapping ----------
# Maps each NES color code used in the space view to a GFX_COL_xxx
# palette index, so tile colors match the C code's expectations.
NES_TO_BMP = {
    0x0F: 0,    # black          → GFX_COL_BLACK
    0x1A: 2,    # dark green     → GFX_COL_GREEN_1 (gauge fill!)
    0x16: 28,   # dark red       → GFX_COL_DARK_RED
    0x10: 235,  # grey           → GFX_COL_GREY_2
    0x00: 234,  # dark grey      → GFX_COL_GREY_3
    0x2C: 11,   # light blue     → GFX_COL_CYAN
    0x28: 37,   # yellow-green   → GFX_COL_YELLOW_1
    0x1C: 17,   # dark cyan      → GFX_COL_GREEN_2
}

# ---------- viewPalettes set 0 (space view) ----------
VIEW_PALETTES_0 = [
    [0x0F, 0x2C, 0x0F, 0x2C],  # BG 0
    [0x0F, 0x28, 0x00, 0x1A],  # BG 1
    [0x0F, 0x10, 0x00, 0x16],  # BG 2
    [0x0F, 0x10, 0x00, 0x1C],  # BG 3
]

# Pre-compute: for each (palette_num, color_idx), what BMP palette index?
PAL_TO_BMP = {}
for pal in range(4):
    for col in range(4):
        nes_code = VIEW_PALETTES_0[pal][col]
        PAL_TO_BMP[(pal, col)] = NES_TO_BMP[nes_code]

# ---------- barNames1 (flight icon bar) ----------
BAR_NAMES_1 = [
    0x09, 0x0B, 0x0C, 0x06, 0x0D, 0x0E, 0x0F, 0x10,
    0x06, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x08, 0x1C, 0x1D, 0x06,
    0x1E, 0x1F, 0x20, 0x21, 0x06, 0x22, 0x23, 0x0A,
    0x28, 0x2A, 0x2B, 0x26, 0x2C, 0x2D, 0x2E, 0x2F,
    0x26, 0x30, 0x31, 0x32, 0x33, 0x26, 0x34, 0x35,
    0x36, 0x37, 0x26, 0x38, 0x39, 0x3A, 0x3B, 0x26,
    0x3C, 0x3D, 0x3E, 0x3F, 0x26, 0x40, 0x27, 0x29,
]

# ---------- dashNames (dashboard rows 22-28) ----------
DASH_NAMES = [
    0x45, 0x46, 0x47, 0x48, 0x47, 0x49, 0x4A, 0x4B,
    0x4C, 0x4D, 0x4E, 0x4F, 0x4D, 0x4C, 0x4D, 0x4E,
    0x4F, 0x4D, 0x4C, 0x4D, 0x50, 0x4F, 0x4D, 0x4C,
    0x51, 0x52, 0x46, 0x47, 0x48, 0x47, 0x49, 0x53,
    0x54, 0x55, 0x55, 0x55, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x00, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x00, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x85, 0x85, 0x6E,
    0x54, 0x55, 0x55, 0x55, 0x55, 0x6F, 0x70, 0x00,
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80,
    0x00, 0x81, 0x82, 0x83, 0x84, 0x85, 0x85, 0x6E,
    0x54, 0x55, 0x55, 0x55, 0x55, 0x86, 0x70, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8C, 0x8E,
    0x8F, 0x8C, 0x90, 0x8C, 0x91, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x55, 0x55, 0x55, 0x55, 0x98,
    0x54, 0x55, 0x55, 0x55, 0x55, 0x99, 0x9A, 0x9B,
    0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA2,
    0xA3, 0xA2, 0xA4, 0xA0, 0xA5, 0xA6, 0xA7, 0xA8,
    0xA9, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0x98,
    0x54, 0x55, 0x55, 0x55, 0x55, 0xAC, 0xAD, 0x58,
    0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
    0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD,
    0x67, 0xBE, 0xBF, 0x55, 0x55, 0x55, 0x55, 0x98,
    0x54, 0x55, 0x55, 0x55, 0x55, 0xC0, 0xC1, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xCA, 0x97, 0x55, 0x55, 0x55, 0x55, 0x98,
]

# ---------- viewAttributes0 (8x8 unpacked) ----------
VIEW_ATTRS_0 = [
    0x3F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xBF, 0xAF, 0xAF, 0xAF, 0xAB, 0xAB, 0xAE,
    0x77, 0x99, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x5A,
    0x07, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0F,
]


def decode_nes_tile(data_16bytes):
    """Decode 16-byte NES 2bpp tile into 8x8 of color indices (0-3)."""
    pixels = [[0]*8 for _ in range(8)]
    for row in range(8):
        p0 = data_16bytes[row]
        p1 = data_16bytes[row + 8]
        for col in range(8):
            bit0 = (p0 >> (7 - col)) & 1
            bit1 = (p1 >> (7 - col)) & 1
            pixels[row][col] = bit0 | (bit1 << 1)
    return pixels


def get_palette_for_tile(tile_row, tile_col):
    """Get BG palette number (0-3) for NES nametable position."""
    attr_row = tile_row // 4
    attr_col = tile_col // 4
    attr_byte = VIEW_ATTRS_0[attr_row * 8 + attr_col]
    sub_row = (tile_row % 4) // 2
    sub_col = (tile_col % 4) // 2
    shift = (sub_row * 2 + sub_col) * 2
    return (attr_byte >> shift) & 0x03


def build_full_palette():
    """Build 256-entry RGB palette compatible with GFX_COL_xxx indices.

    Sets correct RGB values at ALL indices used by both NES tiles
    and the C drawing code.
    """
    pal = [(0, 0, 0)] * 256

    # Set NES-mapped colors at their GFX_COL indices
    for nes_code, bmp_idx in NES_TO_BMP.items():
        pal[bmp_idx] = NES_PALETTE[nes_code]

    # Set additional GFX_COL colors needed by the C code
    pal[255] = (236, 238, 236)   # GFX_COL_WHITE
    pal[49]  = (236, 106, 100)   # GFX_COL_RED ($26 approx)
    pal[39]  = (212, 136, 32)    # GFX_COL_GOLD ($27)
    pal[1]   = (160, 20, 100)    # GFX_COL_RED_3 ($15)
    pal[4]   = (8, 76, 196)      # GFX_COL_BLUE_4 ($11)
    pal[45]  = (76, 154, 236)    # GFX_COL_BLUE_1 ($21)
    pal[46]  = (120, 124, 236)   # GFX_COL_BLUE_2 ($22)
    pal[133] = (48, 50, 236)     # GFX_COL_BLUE_3 ($12)
    pal[71]  = (152, 34, 32)     # GFX_COL_RED_4 ($16)
    pal[242] = (236, 238, 236)   # GFX_COL_WHITE_2
    pal[248] = (84, 84, 84)      # GFX_COL_GREY_1 (dark grey $00)
    pal[237] = (120, 120, 120)   # GFX_COL_GREY_4
    pal[86]  = (0, 64, 0)        # GFX_COL_GREEN_3 ($0A)
    pal[183] = (236, 88, 180)    # GFX_COL_PINK_1 ($25)
    pal[89]  = (204, 210, 120)   # GFX_COL_YELLOW_3 ($38)
    pal[160] = (228, 196, 144)   # GFX_COL_YELLOW_4 ($37)
    pal[251] = (180, 222, 120)   # GFX_COL_YELLOW_5 ($39)
    pal[76]  = (212, 136, 32)    # GFX_ORANGE_1
    pal[77]  = (236, 106, 100)   # GFX_ORANGE_2
    pal[122] = (160, 170, 0)     # GFX_ORANGE_3

    return pal


def write_bmp(pixels, width, height, palette, filename):
    row_size = (width + 3) & ~3
    pixel_data_size = row_size * height
    palette_size = 256 * 4
    header_size = 14 + 40
    file_size = header_size + palette_size + pixel_data_size

    with open(filename, "wb") as f:
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))
        f.write(struct.pack('<HH', 0, 0))
        f.write(struct.pack('<I', header_size + palette_size))
        f.write(struct.pack('<I', 40))
        f.write(struct.pack('<i', width))
        f.write(struct.pack('<i', height))
        f.write(struct.pack('<HH', 1, 8))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<I', pixel_data_size))
        f.write(struct.pack('<i', 2835))
        f.write(struct.pack('<i', 2835))
        f.write(struct.pack('<I', 256))
        f.write(struct.pack('<I', 0))
        for r, g, b in palette:
            f.write(struct.pack('BBBB', b, g, r, 0))
        for y in range(height - 1, -1, -1):
            row = bytes(pixels[y])
            if len(row) < row_size:
                row += b'\x00' * (row_size - len(row))
            f.write(row)


def main():
    dash_bin = os.path.join(NES_SRC, "images/other-images/binaries/dashImage_pattern0.bin")
    icon_bin = os.path.join(NES_SRC, "images/other-images/binaries/iconBarImage1_pattern0.bin")

    with open(dash_bin, "rb") as f:
        dash_data = f.read()
    with open(icon_bin, "rb") as f:
        icon_data = f.read()

    print(f"Dashboard tiles: {len(dash_data)//16} ({len(dash_data)} bytes)")
    print(f"Icon bar tiles: {len(icon_data)//16} ({len(icon_data)} bytes)")

    # Build pattern table (256 entries, each 8x8 of 2-bit color indices)
    patterns = [[[0]*8 for _ in range(8)] for _ in range(256)]

    # Icon bar tiles at pattern indices 5-68
    for i in range(len(icon_data) // 16):
        idx = 5 + i
        if idx < 256:
            patterns[idx] = decode_nes_tile(icon_data[i*16:(i+1)*16])

    # Dashboard tiles at pattern indices 69-255
    for i in range(len(dash_data) // 16):
        idx = 69 + i
        if idx < 256:
            patterns[idx] = decode_nes_tile(dash_data[i*16:(i+1)*16])

    # Build screen nametable (10 rows x 32 cols)
    nametable = [[0]*32 for _ in range(10)]
    for col in range(32):
        nametable[0][col] = BAR_NAMES_1[col]
        nametable[1][col] = BAR_NAMES_1[32 + col]
    for row in range(7):
        for col in range(32):
            nametable[2 + row][col] = DASH_NAMES[row * 32 + col]

    # Render NES pixels (256 x 80) using GFX_COL-compatible palette indices
    nes_w, nes_h = 256, 80
    nes_pixels = [[0]*nes_w for _ in range(nes_h)]

    for tile_row_local in range(10):
        tile_row_global = 20 + tile_row_local
        for tile_col in range(32):
            tile_idx = nametable[tile_row_local][tile_col]
            tile_pixels = patterns[tile_idx]
            palette_num = get_palette_for_tile(tile_row_global, tile_col)

            for py in range(8):
                for px in range(8):
                    color_idx = tile_pixels[py][px]
                    bmp_idx = PAL_TO_BMP[(palette_num, color_idx)]
                    sx = tile_col * 8 + px
                    sy = tile_row_local * 8 + py
                    if sx < nes_w and sy < nes_h:
                        nes_pixels[sy][sx] = bmp_idx

    # Scale 2x → 512 x 160
    out_w, out_h = 512, 160
    out_pixels = [[0]*out_w for _ in range(out_h)]
    for y in range(nes_h):
        for x in range(nes_w):
            val = nes_pixels[y][x]
            out_pixels[y*2][x*2] = val
            out_pixels[y*2][x*2+1] = val
            out_pixels[y*2+1][x*2] = val
            out_pixels[y*2+1][x*2+1] = val

    palette = build_full_palette()
    write_bmp(out_pixels, out_w, out_h, palette, "scanner.bmp")
    print(f"Wrote scanner.bmp ({out_w}x{out_h})")

    shutil.copy("scanner.bmp", "filesystem/scanner.bmp")
    print("Copied to filesystem/scanner.bmp")

    # Print color mapping for verification
    print("\nNES color → BMP palette index mapping:")
    for nes_code, bmp_idx in sorted(NES_TO_BMP.items()):
        r, g, b = NES_PALETTE[nes_code]
        print(f"  NES ${nes_code:02X} ({r:3d},{g:3d},{b:3d}) → BMP index {bmp_idx:3d}")

    print("\nPalette→BMP tile color mapping:")
    for pal in range(4):
        codes = VIEW_PALETTES_0[pal]
        indices = [PAL_TO_BMP[(pal, c)] for c in range(4)]
        print(f"  BG{pal}: NES ${codes[0]:02X},${codes[1]:02X},${codes[2]:02X},${codes[3]:02X} → BMP {indices}")


if __name__ == "__main__":
    main()
