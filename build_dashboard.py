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

# ---------- NES NTSC color palette (CRT-accurate values) ----------
# Using CRT-desaturated values for more authentic TV appearance.
# $28 in particular is less vivid than typical emulator palettes.
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
    # $28: CRT-accurate olive/drab instead of vivid yellow-green
    (156,156, 72), (116,196,0),   (76,208,32),   (56,204,108),
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
    """Build 256-entry palette: original Elite palette as base, with NES tile
    color overrides at specific indices.

    The original Elite palette is needed because sprite data (Elite logo,
    compass dots, missiles, etc.) was created using it. We override only
    the 8 indices used by NES dashboard tile rendering.
    """
    # Start with the full original Elite palette (from git initial commit)
    pal = [
        (  0,  0,  0),(128,  0,  0),(  0,128,  0),(128,128,  0),  # 0-3
        (  0,  0,128),(128,  0,128),(  0,128,128),(192,192,192),  # 4-7
        (192,220,192),(166,202,240),(  0, 33,206),(  0,255,206),  # 8-11
        ( 66, 66, 66),( 66, 99,  0),( 66, 99,206),( 66,206, 66),  # 12-15
        ( 99, 49,  0),( 99,173,  0),( 99,255, 99),(107, 66,  8),  # 16-19
        (123, 74,  8),(123,123,123),(132, 82,  8),(132,132,132),  # 20-23
        (140, 90,  8),(140,173,239),(140,239,  0),(148, 99,  0),  # 24-27
        (156,  0,  0),(165,107,  0),(173, 82,  0),(173,123,  0),  # 28-31
        (181,132,  0),(189,140,  0),(189,189,189),(198,198,198),  # 32-35
        (206,  0,206),(206,156,  0),(214,165,  0),(222,181,  0),  # 36-39
        (239,239,239),( 96, 96, 96),(  0,157,157),(133, 36,240),  # 40-43
        (  0,117,117),(  1, 73,163),(  0,  0,140),(223,179,  2),  # 44-47
        (219,109,  0),(206,  0,  0),(176,176,176),(128, 64,  0),  # 48-51
        (255,255,128),(102,204,  0),(153,204,  0),(204,204,  0),  # 52-55
        (255,204,  0),(102,255,  0),(153,255,  0),(204,255,  0),  # 56-59
        (  0,  0, 51),( 51,  0, 51),(102,  0, 51),(153,  0, 51),  # 60-63
        (204,  0, 51),(255,  0, 51),(  0, 51, 51),( 51, 51, 51),  # 64-67
        (102, 51, 51),(153, 51, 51),(204, 51, 51),(255, 51, 51),  # 68-71
        (  0,102, 51),( 51,102, 51),(102,102, 51),(153,102, 51),  # 72-75
        (204,102, 51),(255,102, 51),(  0,153, 51),( 51,153, 51),  # 76-79
        (102,153, 51),(153,153, 51),(204,153, 51),(255,153, 51),  # 80-83
        (  0,204, 51),( 51,204, 51),(102,204, 51),(153,204, 51),  # 84-87
        (204,204, 51),(255,204, 51),( 51,255, 51),(102,255, 51),  # 88-91
        (153,255, 51),(204,255, 51),(255,255, 51),(  0,  0,102),  # 92-95
        ( 51,  0,102),(102,  0,102),(153,  0,102),(204,  0,102),  # 96-99
        (255,  0,102),(  0, 51,102),( 51, 51,102),(102, 51,102),  # 100-103
        (153, 51,102),(204, 51,102),(255, 51,102),(  0,102,102),  # 104-107
        ( 51,102,102),(102,102,102),(153,102,102),(204,102,102),  # 108-111
        (  0,153,102),( 51,153,102),(102,153,102),(153,153,102),  # 112-115
        (204,153,102),(255,153,102),(  0,204,102),( 51,204,102),  # 116-119
        (153,204,102),(204,204,102),(255,204,102),(  0,255,102),  # 120-123
        ( 51,255,102),(153,255,102),(204,255,102),(255,  0,204),  # 124-127
        (204,  0,255),(  0,153,153),(153, 51,153),(153,  0,153),  # 128-131
        (204,  0,153),(  0,  0,153),( 51, 51,153),(102,  0,153),  # 132-135
        (204, 51,153),(255,  0,153),(  0,102,153),( 51,102,153),  # 136-139
        (102, 51,153),(153,102,153),(204,102,153),(255, 51,153),  # 140-143
        ( 51,153,153),(102,153,153),(153,153,153),(204,153,153),  # 144-147
        (255,153,153),(  0,204,153),( 51,204,153),(102,204,102),  # 148-151
        (153,204,153),(204,204,153),(255,204,153),(  0,255,153),  # 152-155
        ( 51,255,153),(102,204,153),(153,255,153),(204,255,153),  # 156-159
        (255,255,153),(  0,  0,204),( 51,  0,153),(102,  0,204),  # 160-163
        (153,  0,204),(204,  0,204),(  0, 51,153),( 51, 51,204),  # 164-167
        (102, 51,204),(153, 51,204),(204, 51,204),(255, 51,204),  # 168-171
        (  0,102,204),( 51,102,204),(102,102,153),(153,102,204),  # 172-175
        (204,102,204),(255,102,153),(  0,153,204),( 51,153,204),  # 176-179
        (102,153,204),(153,153,204),(204,153,204),(255,153,204),  # 180-183
        (  0,204,204),( 51,204,204),(102,204,204),(153,204,204),  # 184-187
        (204,204,204),(255,204,204),(  0,255,204),( 51,255,204),  # 188-191
        (102,255,153),(153,255,204),(204,255,204),(255,255,204),  # 192-195
        ( 51,  0,204),(102,  0,255),(153,  0,255),(  0, 51,204),  # 196-199
        ( 51, 51,255),(102, 51,255),(153, 51,255),(204, 51,255),  # 200-203
        (255, 51,255),(  0,102,255),(206,  0,  0),(102,102,204),  # 204-207
        (153,102,255),(204,102,255),(255,102,204),(  0,153,255),  # 208-211
        ( 51,153,255),(102,153,255),(153,153,255),(204,153,255),  # 212-215
        (255,153,255),(  0,204,255),( 51,204,255),(102,204,255),  # 216-219
        (153,204,255),(204,204,255),(255,204,255),( 51,255,255),  # 220-223
        (102,255,204),(153,255,255),(204,255,255),(255,102,102),  # 224-227
        (102,255,102),(255,255,102),(102,102,255),(255,102,255),  # 228-231
        (102,255,255),(165,  0, 33),( 95, 95, 95),(119,119,119),  # 232-235
        (134,134,134),(150,150,150),(203,203,203),(178,178,178),  # 236-239
        (215,215,215),(221,221,221),(227,227,227),(234,234,234),  # 240-243
        (241,241,241),(248,248,248),(255,251,240),(160,160,164),  # 244-247
        (128,128,128),(255,  0,  0),(  0,255,  0),(255,255,  0),  # 248-251
        (  0,  0,255),(255,  0,255),(  0,255,255),(255,255,255),  # 252-255
    ]

    # Override the 8 indices used by NES dashboard tile colors.
    # These must match NES_TO_BMP mapping for tiles to render correctly.
    for nes_code, bmp_idx in NES_TO_BMP.items():
        pal[bmp_idx] = NES_PALETTE[nes_code]

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
