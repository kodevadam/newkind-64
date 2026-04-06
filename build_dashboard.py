#!/usr/bin/env python3
"""
Reconstruct the NES Elite dashboard BMP from source data.

Uses the exact binary tile data, nametable layout, attribute table,
and palette from the NES Elite disassembled source code.

CRITICAL: The BMP palette indices MUST match the GFX_COL_* constants
in gfx.h, because the game uses those indices to draw gauge fills,
text, ships, etc. The BMP palette IS the master palette for the game.

Output: scanner.bmp (512x160, 8-bit indexed color)
  - Rows 0-31:   Icon bar (NES rows 20-21) at 2x scale
  - Rows 32-47:  Dashboard top border (NES row 22) at 2x scale
  - Rows 48-143: Dashboard gauges/scanner (NES rows 23-28) at 2x scale
  - Rows 144-159: Bottom border (NES row 29) at 2x scale
"""

import struct

# === NES PPU COLOR PALETTE (2C02) ===
NES_PALETTE = {
    0x00: (84, 84, 84),    0x01: (0, 30, 116),    0x02: (8, 16, 144),
    0x03: (48, 0, 136),    0x04: (68, 0, 100),    0x05: (92, 0, 48),
    0x06: (84, 4, 0),      0x07: (60, 24, 0),     0x08: (32, 42, 0),
    0x09: (8, 58, 0),      0x0A: (0, 64, 0),      0x0B: (0, 60, 0),
    0x0C: (0, 50, 60),     0x0D: (0, 0, 0),       0x0E: (0, 0, 0),
    0x0F: (0, 0, 0),
    0x10: (152, 150, 152), 0x11: (8, 76, 196),    0x12: (48, 50, 236),
    0x13: (92, 30, 228),   0x14: (136, 20, 176),  0x15: (160, 20, 100),
    0x16: (152, 34, 32),   0x17: (120, 60, 0),    0x18: (84, 90, 0),
    0x19: (40, 114, 0),    0x1A: (8, 124, 0),     0x1B: (0, 118, 40),
    0x1C: (0, 102, 120),   0x1D: (0, 0, 0),       0x1E: (0, 0, 0),
    0x1F: (0, 0, 0),
    0x20: (236, 238, 236), 0x21: (76, 154, 236),  0x22: (120, 124, 236),
    0x23: (176, 98, 236),  0x24: (228, 84, 236),  0x25: (236, 88, 180),
    0x26: (236, 106, 100), 0x27: (212, 136, 32),  0x28: (160, 170, 0),
    0x29: (116, 196, 0),   0x2A: (76, 208, 32),   0x2B: (56, 204, 108),
    0x2C: (56, 180, 204),  0x2D: (60, 60, 60),    0x2E: (0, 0, 0),
    0x2F: (0, 0, 0),
    0x30: (236, 238, 236), 0x31: (168, 204, 236), 0x32: (188, 188, 236),
    0x33: (212, 178, 236), 0x34: (236, 174, 236), 0x35: (236, 174, 212),
    0x36: (236, 180, 176), 0x37: (228, 196, 144), 0x38: (204, 210, 120),
    0x39: (180, 222, 120), 0x3A: (168, 226, 144), 0x3B: (152, 226, 180),
    0x3C: (160, 214, 228), 0x3D: (160, 162, 160), 0x3E: (0, 0, 0),
    0x3F: (0, 0, 0),
}

# === FIXED BMP PALETTE matching gfx.h GFX_COL_* constants ===
# Index : gfx.h constant : NES equivalent : RGB
BMP_PALETTE = [(0, 0, 0)] * 256  # Start all black

# Core game colors (from gfx.h)
BMP_PALETTE[0]   = (0, 0, 0)         # GFX_COL_BLACK
BMP_PALETTE[1]   = (152, 34, 32)     # GFX_COL_RED_3 (NES $16)
BMP_PALETTE[2]   = (8, 124, 0)       # GFX_COL_GREEN_1 (NES $1A)
BMP_PALETTE[4]   = (0, 0, 180)       # GFX_COL_BLUE_4
BMP_PALETTE[11]  = (0, 102, 120)     # GFX_COL_CYAN (NES $1C)
BMP_PALETTE[17]  = (0, 180, 0)       # GFX_COL_GREEN_2
BMP_PALETTE[28]  = (152, 34, 32)     # GFX_COL_DARK_RED (NES $16)
BMP_PALETTE[37]  = (160, 170, 0)     # GFX_COL_YELLOW_1 (NES $28)
BMP_PALETTE[39]  = (212, 136, 32)    # GFX_COL_GOLD (NES $27)
BMP_PALETTE[45]  = (0, 50, 200)      # GFX_COL_BLUE_1
BMP_PALETTE[46]  = (0, 30, 170)      # GFX_COL_BLUE_2
BMP_PALETTE[49]  = (255, 0, 0)       # GFX_COL_RED
BMP_PALETTE[71]  = (200, 50, 50)     # GFX_COL_RED_4
BMP_PALETTE[86]  = (0, 100, 0)       # GFX_COL_GREEN_3
BMP_PALETTE[89]  = (200, 200, 0)     # GFX_COL_YELLOW_3
BMP_PALETTE[133] = (0, 0, 140)       # GFX_COL_BLUE_3
BMP_PALETTE[160] = (180, 180, 0)     # GFX_COL_YELLOW_4
BMP_PALETTE[183] = (200, 100, 150)   # GFX_COL_PINK_1
BMP_PALETTE[234] = (84, 84, 84)      # GFX_COL_GREY_3 (NES $00 dark grey)
BMP_PALETTE[235] = (130, 130, 130)   # GFX_COL_GREY_2
BMP_PALETTE[237] = (150, 150, 150)   # GFX_COL_GREY_4
BMP_PALETTE[242] = (220, 220, 220)   # GFX_COL_WHITE_2
BMP_PALETTE[248] = (152, 150, 152)   # GFX_COL_GREY_1 (NES $10 grey)
BMP_PALETTE[251] = (230, 230, 100)   # GFX_COL_YELLOW_5
BMP_PALETTE[255] = (236, 238, 236)   # GFX_COL_WHITE (NES $20)

# Map NES PPU colors to BMP palette indices for dashboard rendering
NES_TO_BMP = {
    0x0F: 0,    # Black -> GFX_COL_BLACK
    0x0D: 0,    # Black -> GFX_COL_BLACK
    0x10: 248,  # Grey -> GFX_COL_GREY_1
    0x00: 234,  # Dark grey -> GFX_COL_GREY_3
    0x1C: 11,   # Dark cyan -> GFX_COL_CYAN
    0x16: 28,   # Dark red -> GFX_COL_DARK_RED
    0x1A: 2,    # Green -> GFX_COL_GREEN_1
    0x28: 37,   # Yellow -> GFX_COL_YELLOW_1
    0x2C: 45,   # Light blue -> GFX_COL_BLUE_1 (approximate)
    0x20: 255,  # White -> GFX_COL_WHITE
}

# === NES PALETTE DATA (space view = palette set 0) ===
bg_palettes = [
    [0x0F, 0x2C, 0x0F, 0x2C],  # BG 0: space/text
    [0x0F, 0x28, 0x00, 0x1A],  # BG 1: gauges (yellow, dark grey, GREEN)
    [0x0F, 0x10, 0x00, 0x16],  # BG 2: scanner (grey, dark grey, dark RED)
    [0x0F, 0x10, 0x00, 0x1C],  # BG 3: frame (grey, dark grey, dark CYAN)
]

# === DASHBOARD NAMETABLE (dashNames) - 7 rows x 32 cols (rows 22-28) ===
dashNames = [
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

# === ICON BAR NAMETABLE (barNames1 = Flight mode) ===
barNames1 = [
    0x09, 0x0B, 0x0C, 0x06, 0x0D, 0x0E, 0x0F, 0x10,
    0x06, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x08, 0x1C, 0x1D, 0x06,
    0x1E, 0x1F, 0x20, 0x21, 0x06, 0x22, 0x23, 0x0A,
    0x28, 0x2A, 0x2B, 0x26, 0x2C, 0x2D, 0x2E, 0x2F,
    0x26, 0x30, 0x31, 0x32, 0x33, 0x26, 0x34, 0x35,
    0x36, 0x37, 0x26, 0x38, 0x39, 0x3A, 0x3B, 0x26,
    0x3C, 0x3D, 0x3E, 0x3F, 0x26, 0x40, 0x27, 0x29,
]

# === ATTRIBUTE TABLE (viewAttributes0) ===
viewAttributes0 = [
    0x3F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xBF, 0xAF, 0xAF, 0xAF, 0xAB, 0xAB, 0xAE,
    0x77, 0x99, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x5A,
    0x07, 0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0F,
]


def get_palette_for_tile(tile_row, tile_col):
    """Get the BG palette number (0-3) for a tile at nametable position."""
    attr_row = min(tile_row // 4, 7)
    attr_col = min(tile_col // 4, 7)
    attr_byte = viewAttributes0[attr_row * 8 + attr_col]
    quad_row = (tile_row // 2) & 1
    quad_col = (tile_col // 2) & 1
    quadrant = quad_row * 2 + quad_col
    return (attr_byte >> (quadrant * 2)) & 0x03


def decode_nes_tile(tile_data):
    """Decode 16-byte NES 2bpp tile into 8x8 palette index array."""
    pixels = [[0] * 8 for _ in range(8)]
    for row in range(8):
        lo = tile_data[row]
        hi = tile_data[row + 8]
        for col in range(8):
            bit = 7 - col
            pixels[row][col] = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1)
    return pixels


def load_tiles(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
    tiles = {}
    for i in range(len(data) // 16):
        tiles[i] = decode_nes_tile(data[i * 16:(i + 1) * 16])
    return tiles


def make_blank_tile():
    return [[0] * 8 for _ in range(8)]


def nes_color_to_bmp_index(nes_color):
    """Map NES PPU color to BMP palette index."""
    if nes_color in NES_TO_BMP:
        return NES_TO_BMP[nes_color]
    # Fallback: find closest or use a spare index
    print("WARNING: unmapped NES color 0x%02X" % nes_color)
    return 0


def render_dashboard():
    """Render dashboard as 512x160 indexed pixel array using BMP palette indices."""
    # Load tile data
    dash_tile_bin = load_tiles(
        '/tmp/elite-source-code-nes/1-source-files/images/other-images/binaries/dashImage_pattern0.bin'
    )
    icon_tile_bin = load_tiles(
        '/tmp/elite-source-code-nes/1-source-files/images/other-images/binaries/iconBarImage0_pattern0.bin'
    )

    DASH_TILE_BASE = 0x45
    dash_tiles = {}
    for i, pixels in dash_tile_bin.items():
        dash_tiles[DASH_TILE_BASE + i] = pixels
    dash_tiles[0x00] = make_blank_tile()

    icon_tiles = dict(icon_tile_bin)

    NES_W = 256
    NES_H = 80
    # Store as BMP palette indices (not RGB)
    img = [[0] * NES_W for _ in range(NES_H)]

    def render_tile(tile_pixels, pal, bmp_y_base, screen_x):
        for py in range(8):
            for px in range(8):
                color_idx = tile_pixels[py][px]
                if color_idx == 0:
                    bmp_idx = 0  # Background = black
                else:
                    nes_color = pal[color_idx]
                    bmp_idx = nes_color_to_bmp_index(nes_color)
                bmp_x = screen_x + px
                bmp_y = bmp_y_base + py
                if 0 <= bmp_x < NES_W and 0 <= bmp_y < NES_H:
                    img[bmp_y][bmp_x] = bmp_idx

    # --- Icon Bar (rows 20-21) ---
    for row in range(2):
        for col in range(32):
            tile_idx = barNames1[row * 32 + col]
            nes_tile_row = 20 + row
            screen_col = (col - 1) % 32
            screen_x = screen_col * 8
            pal_num = get_palette_for_tile(nes_tile_row, col)
            pal = bg_palettes[pal_num]
            tile_pixels = icon_tiles.get(tile_idx, make_blank_tile())
            render_tile(tile_pixels, pal, row * 8, screen_x)

    # --- Dashboard (rows 22-28) ---
    # Build nametable buffer with scroll compensation (matches DrawDashNames)
    namebuf = [0] * (32 * 32)
    for y in range(7 * 32, 0, -1):
        namebuf[22 * 32 + y] = dashNames[y - 1]

    # Column 0 wrap-around fixes
    namebuf[22 * 32] = namebuf[23 * 32]
    namebuf[23 * 32] = namebuf[24 * 32]
    namebuf[24 * 32] = namebuf[25 * 32]
    namebuf[25 * 32] = namebuf[26 * 32]
    # Row 26 col 0 intentionally not fixed (NES source quirk)
    namebuf[27 * 32] = namebuf[28 * 32]
    namebuf[28 * 32] = namebuf[29 * 32]
    namebuf[29 * 32] = 0x00

    for row_idx in range(7):
        nes_tile_row = 22 + row_idx
        for col in range(32):
            tile_idx = namebuf[nes_tile_row * 32 + col]
            screen_col = (col - 1) % 32
            screen_x = screen_col * 8
            pal_num = get_palette_for_tile(nes_tile_row, col)
            pal = bg_palettes[pal_num]
            tile_pixels = dash_tiles.get(tile_idx, make_blank_tile())
            render_tile(tile_pixels, pal, (2 + row_idx) * 8, screen_x)

    # === Scale 2x to 512x160 ===
    OUT_W = 512
    OUT_H = 160
    img2x = [[0] * OUT_W for _ in range(OUT_H)]
    for y in range(NES_H):
        for x in range(NES_W):
            idx = img[y][x]
            img2x[y * 2][x * 2] = idx
            img2x[y * 2][x * 2 + 1] = idx
            img2x[y * 2 + 1][x * 2] = idx
            img2x[y * 2 + 1][x * 2 + 1] = idx

    return img2x


def write_bmp(filepath, width, height, palette, indexed_pixels):
    """Write an 8-bit indexed color BMP file."""
    pal = list(palette)
    while len(pal) < 256:
        pal.append((0, 0, 0))

    row_size = (width + 3) & ~3
    pixel_data_size = row_size * height
    palette_size = 256 * 4
    offset = 14 + 40 + palette_size
    file_size = offset + pixel_data_size

    with open(filepath, 'wb') as f:
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))
        f.write(struct.pack('<HH', 0, 0))
        f.write(struct.pack('<I', offset))
        f.write(struct.pack('<I', 40))
        f.write(struct.pack('<i', width))
        f.write(struct.pack('<i', height))
        f.write(struct.pack('<HH', 1, 8))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<I', pixel_data_size))
        f.write(struct.pack('<ii', 2835, 2835))
        f.write(struct.pack('<I', 256))
        f.write(struct.pack('<I', 0))

        for r, g, b in pal:
            f.write(struct.pack('BBBB', b, g, r, 0))

        for y in range(height - 1, -1, -1):
            row = indexed_pixels[y]
            row_bytes = bytes(row)
            row_bytes += b'\x00' * (row_size - len(row_bytes))
            f.write(row_bytes)


def main():
    print("Reconstructing NES Elite dashboard from source data...")
    print("Using fixed palette matching gfx.h GFX_COL_* constants")

    img = render_dashboard()
    print("Image size: %dx%d pixels" % (len(img[0]), len(img)))

    # Count unique palette indices used
    used = set()
    for row in img:
        for idx in row:
            used.add(idx)
    print("Palette indices used: %s" % sorted(used))

    for path in ['/home/user/newkind-64/scanner.bmp',
                 '/home/user/newkind-64/filesystem/scanner.bmp']:
        write_bmp(path, 512, 160, BMP_PALETTE, img)
        print("Written: %s" % path)

    print("\nPalette index mapping:")
    print("  0   = black (background)")
    print("  2   = green  (GFX_COL_GREEN_1, NES $1A) - gauge fill")
    print("  11  = cyan   (GFX_COL_CYAN, NES $1C)    - frame color 3")
    print("  28  = dk red (GFX_COL_DARK_RED, NES $16) - scanner color 3")
    print("  37  = yellow (GFX_COL_YELLOW_1, NES $28) - gauge label")
    print("  234 = dk grey(GFX_COL_GREY_3, NES $00)   - gauge border")
    print("  248 = grey   (GFX_COL_GREY_1, NES $10)   - frame color 1")


if __name__ == '__main__':
    main()
