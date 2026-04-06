#!/usr/bin/env python3
"""Check gauge bar tile patterns from NES Elite."""

with open('/tmp/elite-source-code-nes/1-source-files/images/other-images/binaries/dashImage_pattern0.bin', 'rb') as f:
    data = f.read()

DASH_BASE = 0x45

def show_tile(pattern_num, name):
    idx = pattern_num - DASH_BASE
    if idx < 0 or idx >= len(data) // 16:
        print("Pattern %d (%s): OUT OF RANGE (idx=%d)" % (pattern_num, name, idx))
        return
    tile = data[idx * 16:(idx + 1) * 16]
    print("Pattern %d (0x%02X) - %s:" % (pattern_num, pattern_num, name))
    for row in range(8):
        lo = tile[row]
        hi = tile[row + 8]
        line = ''
        for col in range(8):
            bit = 7 - col
            val = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1)
            line += str(val)
        print("  Row %d: %s" % (row, line))
    print()

show_tile(85, 'empty indicator')
show_tile(236, 'full bar safe')
show_tile(237, 'end cap 1 safe (smallest)')
show_tile(238, 'end cap 2 safe')
show_tile(239, 'end cap 3 safe')
show_tile(240, 'end cap 4 safe')
show_tile(241, 'end cap 5 safe')
show_tile(242, 'end cap 6 safe')
show_tile(243, 'end cap 7 safe')
show_tile(244, 'end cap 8 safe (largest)')
show_tile(227, 'full bar danger')
show_tile(228, 'end cap 1 danger (smallest)')
show_tile(235, 'end cap 8 danger (largest)')
