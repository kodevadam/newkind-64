/**
 *
 * Elite - The New Kind.
 *
 * Nintendo 64 version of Graphics routines using libdragon.
 *
 * N64 port: Software-rendered framebuffer at 640x480 (interlaced).
 * Based on the Allegro backend (alg_gfx.c) by C.J.Pinder.
 *
 **/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <libdragon.h>

#include "config.h"
#include "gfx.h"
#include "elite.h"
#include "n64_assets.h"

/* The 256-color palette, stored as RGBA5551 for direct write */
static uint16_t palette_rgba16[256];

/* Private framebuffer - persistent between frames for screens that draw once.
 * Flight views clear and redraw every frame; info screens persist.
 * Aligned to 16 bytes for optimal DMA/cache line operations. */
static uint16_t framebuf[N64_SCREEN_W * N64_SCREEN_H] __attribute__((aligned(16)));

/* Pointer used by all rendering functions. Points to framebuf. */
static uint16_t *active_fb = framebuf;

volatile int frame_count;
static timer_link_t *frame_timer_handle;

/* Clipping region */
static int clip_tx, clip_ty, clip_bx, clip_by;

/* Scanner image data - loaded from ROM filesystem (BMP converted to raw) */
static uint8_t *scanner_pixels;  /* indexed color pixel data */
static int scanner_w, scanner_h;

/* Palette loaded from scanner BMP */
static uint8_t bmp_palette[256][4]; /* RGBX */

#define MAX_POLYS	100

static int start_poly;
static int total_polys;

struct poly_data
{
	int z;
	int no_points;
	int face_colour;
	int point_list[16];
	int next;
};

static struct poly_data poly_chain[MAX_POLYS];


/*
 * Load the scanner BMP from ROM filesystem and extract its palette.
 * The BMP is an 8-bit indexed color image (256 colors).
 * This gives us the authentic Elite palette used by all game graphics.
 */
static int load_scanner_bmp(const char *filename)
{
	FILE *fp;
	uint8_t header[54];
	int width, height, bpp, offset;
	int row;
	int padding;
	char path[64];

	/* In libdragon, files in the DFS are accessed via "rom:/" prefix */
	snprintf(path, sizeof(path), "rom:/%s", filename);
	fp = fopen(path, "rb");
	if (!fp)
		return -1;

	/* Read BMP header */
	if (fread(header, 1, 54, fp) != 54)
	{
		fclose(fp);
		return -1;
	}

	/* Parse BMP header */
	offset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
	width = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
	height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
	bpp = header[28] | (header[29] << 8);

	if (bpp != 8)
	{
		fclose(fp);
		return -1;
	}

	/* Read palette (256 entries, 4 bytes each: BGRA) */
	fread(bmp_palette, 4, 256, fp);

	/* Convert palette to RGBA5551 */
	for (int i = 0; i < 256; i++)
	{
		int b = bmp_palette[i][0];
		int g = bmp_palette[i][1];
		int r = bmp_palette[i][2];
		int r5 = (r >> 3) & 0x1F;
		int g5 = (g >> 3) & 0x1F;
		int b5 = (b >> 3) & 0x1F;
		/* RGBA5551: RRRRR GGGGG BBBBB A */
		palette_rgba16[i] = (r5 << 11) | (g5 << 6) | (b5 << 1) | (i ? 1 : 0);
	}

	/* Allocate pixel data */
	scanner_w = width;
	scanner_h = abs(height);
	scanner_pixels = (uint8_t *)malloc(scanner_w * scanner_h);
	if (!scanner_pixels)
	{
		fclose(fp);
		return -1;
	}

	/* Seek to pixel data */
	fseek(fp, offset, SEEK_SET);

	/* BMP rows are padded to 4-byte boundaries */
	padding = (4 - (width % 4)) % 4;

	/* BMP is stored bottom-up */
	for (row = scanner_h - 1; row >= 0; row--)
	{
		fread(&scanner_pixels[row * scanner_w], 1, scanner_w, fp);
		if (padding > 0)
		{
			uint8_t pad[4];
			fread(pad, 1, padding, fp);
		}
	}

	fclose(fp);
	return 0;
}


static void init_default_palette(void)
{
	/* Fallback palette if BMP load fails - matches Elite's color scheme */
	memset(palette_rgba16, 0, sizeof(palette_rgba16));

	palette_rgba16[0]   = 0x0001;  /* Black (with alpha) - actually use 0 for transparent */
	palette_rgba16[0]   = 0x0000;  /* Black */
	palette_rgba16[1]   = (0x15 << 11) | (0x00 << 6) | (0x00 << 1) | 1; /* dark red */
	palette_rgba16[2]   = (0x00 << 11) | (0x1F << 6) | (0x00 << 1) | 1; /* green */
	palette_rgba16[4]   = (0x00 << 11) | (0x00 << 6) | (0x1F << 1) | 1; /* blue */
	palette_rgba16[11]  = (0x00 << 11) | (0x1F << 6) | (0x1F << 1) | 1; /* cyan */
	palette_rgba16[28]  = (0x18 << 11) | (0x00 << 6) | (0x00 << 1) | 1; /* dark red */
	palette_rgba16[39]  = (0x1F << 11) | (0x18 << 6) | (0x00 << 1) | 1; /* gold */
	palette_rgba16[49]  = (0x1F << 11) | (0x00 << 6) | (0x00 << 1) | 1; /* red */
	palette_rgba16[234] = (0x0D << 11) | (0x0D << 6) | (0x0D << 1) | 1; /* grey */
	palette_rgba16[235] = (0x10 << 11) | (0x10 << 6) | (0x10 << 1) | 1; /* grey */
	palette_rgba16[237] = (0x13 << 11) | (0x13 << 6) | (0x13 << 1) | 1; /* grey */
	palette_rgba16[248] = (0x09 << 11) | (0x09 << 6) | (0x09 << 1) | 1; /* grey */
	palette_rgba16[255] = (0x1F << 11) | (0x1F << 6) | (0x1F << 1) | 1; /* white */

	/* AA grey ramp */
	for (int i = 0; i < 8; i++)
	{
		int v = (i * 31) / 7;
		palette_rgba16[235 + i] = (v << 11) | (v << 6) | (v << 1) | 1;
	}
}


static void frame_timer_callback(int ovfl)
{
	frame_count++;
}


/* Inline pixel set with bounds checking and clipping.
 * Converts palette index to RGBA5551 on write. */
static inline void fb_putpixel(int x, int y, int col)
{
	if (x >= clip_tx && x <= clip_bx && y >= clip_ty && y <= clip_by)
		active_fb[y * N64_SCREEN_W + x] = palette_rgba16[col];
}

/* Fast pixel set without clipping (for internal use) */
static inline void fb_putpixel_fast(int x, int y, int col)
{
	if (x >= 0 && x < N64_SCREEN_W && y >= 0 && y < N64_SCREEN_H)
		active_fb[y * N64_SCREEN_W + x] = palette_rgba16[col];
}


/* Fast 64-bit framebuffer copy. */
static void fast_framebuf_copy(void *dst, const void *src, int nbytes)
{
	const uint64_t *s = (const uint64_t *)src;
	uint64_t *d = (uint64_t *)dst;
	int n = nbytes / 64;
	int i;
	for (i = 0; i < n; i++)
	{
		uint64_t a = s[0], b = s[1], c = s[2], d0 = s[3];
		uint64_t e = s[4], f = s[5], g = s[6], h = s[7];
		d[0] = a; d[1] = b; d[2] = c; d[3] = d0;
		d[4] = e; d[5] = f; d[6] = g; d[7] = h;
		s += 8;
		d += 8;
	}
}

/* Widescreen flag from boot menu */
extern int n64_widescreen;

int gfx_graphics_startup(void)
{
	/* Initialize N64 display at 640x480 interlaced, 16-bit color, 3 buffers.
	 * Use ANTIALIAS_OFF for crisp pixel rendering.
	 *
	 * In widescreen mode, we still render at 640x480 but the N64 VI
	 * stretches this to fill a 16:9 display. The game content is
	 * pre-squished horizontally so it looks correct on widescreen TVs.
	 * Since the game's internal coordinates are 512x384 centered in
	 * 640x480, on a 16:9 TV the image will appear wider and shorter
	 * which actually helps with the vertical clipping issue. */
	display_init(RESOLUTION_640x480, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);

	/* Try to load palette from the scanner BMP (the authentic Elite palette) */
	if (load_scanner_bmp(scanner_filename) != 0)
	{
		if (load_scanner_bmp("scanner.bmp") != 0)
		{
			init_default_palette();
		}
	}

	/* Clear framebuffer */
	memset(framebuf, 0, sizeof(framebuf));
	active_fb = framebuf;

	/* Set default clip region to full screen */
	clip_tx = 0;
	clip_ty = 0;
	clip_bx = N64_SCREEN_W - 1;
	clip_by = N64_SCREEN_H - 1;

	/* Don't draw scanner at startup - it will be drawn by update_console()
	 * when the game starts. Drawing it here makes it appear on intro screens. */

	/* Setup frame timer for game speed regulation */
	frame_count = 0;
	frame_timer_handle = new_timer(TIMER_TICKS(speed_cap * 1000), TF_CONTINUOUS, frame_timer_callback);

	return 0;
}


void gfx_graphics_shutdown(void)
{
	if (scanner_pixels)
	{
		free(scanner_pixels);
		scanner_pixels = NULL;
	}
	display_close();
}


void gfx_acquire_screen(void)
{
	/* No-op - we always render to the private framebuf */
}

void gfx_update_screen(void)
{
	surface_t *disp;

	/* display_get() blocks until a buffer is free (~vsync locked). */
	disp = display_get();

	/* Flush framebuf from CPU cache, then fast-copy to display surface. */
	data_cache_hit_writeback(framebuf, sizeof(framebuf));
	fast_framebuf_copy(disp->buffer, framebuf, N64_SCREEN_W * N64_SCREEN_H * 2);

	display_show(disp);
}


void gfx_release_screen(void)
{
	/* No-op on N64 - release is handled by gfx_update_screen */
}


void gfx_fast_plot_pixel(int x, int y, int col)
{
	/* No bounds check - caller is responsible. Used by planet renderer. */
	active_fb[y * N64_SCREEN_W + x] = palette_rgba16[col];
}


void gfx_plot_pixel(int x, int y, int col)
{
	fb_putpixel(x + GFX_X_OFFSET, y + GFX_Y_OFFSET, col);
}


void gfx_draw_filled_circle(int cx, int cy, int radius, int circle_colour)
{
	int y, r2;

	cx += GFX_X_OFFSET;
	cy += GFX_Y_OFFSET;

	if (radius <= 0) return;
	r2 = radius * radius;

	/* Scanline fill: for each row, compute the horizontal span */
	for (y = -radius; y <= radius; y++)
	{
		/* x range where x*x + y*y <= r*r  =>  x = sqrt(r2 - y*y) */
		int y2 = y * y;
		int xspan = 0;
		while (xspan * xspan + y2 <= r2) xspan++;
		xspan--;

		/* Draw horizontal line from cx-xspan to cx+xspan */
		{
			int py = cy + y;
			int sx = cx - xspan;
			int ex = cx + xspan;
			if (py >= clip_ty && py <= clip_by)
			{
				if (sx < clip_tx) sx = clip_tx;
				if (ex > clip_bx) ex = clip_bx;
				if (sx <= ex)
				{
					uint16_t c16 = palette_rgba16[circle_colour];
					uint16_t *row = &active_fb[py * N64_SCREEN_W + sx];
					int len = ex - sx + 1;
					if ((((uintptr_t)row) & 2) && len > 0) { *row++ = c16; len--; }
					{
						uint32_t c32 = ((uint32_t)c16 << 16) | c16;
						uint32_t *r32 = (uint32_t *)row;
						int pairs = len >> 1;
						int j;
						for (j = 0; j < pairs; j++) r32[j] = c32;
						row += pairs * 2;
						len -= pairs * 2;
					}
					if (len > 0) *row = c16;
				}
			}
		}
	}
}


/* Bresenham circle */
void gfx_draw_circle(int cx, int cy, int radius, int circle_colour)
{
	int x = 0, y = radius;
	int d = 3 - 2 * radius;

	cx += GFX_X_OFFSET;
	cy += GFX_Y_OFFSET;

	while (y >= x)
	{
		fb_putpixel(cx + x, cy + y, circle_colour);
		fb_putpixel(cx - x, cy + y, circle_colour);
		fb_putpixel(cx + x, cy - y, circle_colour);
		fb_putpixel(cx - x, cy - y, circle_colour);
		fb_putpixel(cx + y, cy + x, circle_colour);
		fb_putpixel(cx - y, cy + x, circle_colour);
		fb_putpixel(cx + y, cy - x, circle_colour);
		fb_putpixel(cx - y, cy - x, circle_colour);

		x++;
		if (d > 0)
		{
			y--;
			d = d + 4 * (x - y) + 10;
		}
		else
		{
			d = d + 4 * x + 6;
		}
	}
}


/* Bresenham line drawing */
static void draw_line_clipped(int x1, int y1, int x2, int y2, int col)
{
	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int sx = (x1 < x2) ? 1 : -1;
	int sy = (y1 < y2) ? 1 : -1;
	int err = dx - dy;
	int e2;

	for (;;)
	{
		fb_putpixel(x1, y1, col);
		if (x1 == x2 && y1 == y2)
			break;
		e2 = 2 * err;
		if (e2 > -dy)
		{
			err -= dy;
			x1 += sx;
		}
		if (e2 < dx)
		{
			err += dx;
			y1 += sy;
		}
	}
}


/*
 * Optimized horizontal line - fills with 32-bit writes (2 pixels at a time).
 * This is the single hottest function in the renderer.
 */
static void draw_hline(int x1, int x2, int y, int col)
{
	int tmp;
	uint16_t c16;
	uint16_t *row;
	if (x1 > x2) { tmp = x1; x1 = x2; x2 = tmp; }
	if (y < clip_ty || y > clip_by) return;
	if (x1 < clip_tx) x1 = clip_tx;
	if (x2 > clip_bx) x2 = clip_bx;
	if (x1 > x2) return;

	c16 = palette_rgba16[col];
	row = &active_fb[y * N64_SCREEN_W];

	/* Align to 32-bit boundary */
	if ((x1 & 1) && x1 <= x2)
		row[x1++] = c16;

	/* Fill 2 pixels at a time with 32-bit writes */
	{
		uint32_t c32 = ((uint32_t)c16 << 16) | c16;
		uint32_t *row32 = (uint32_t *)&row[x1];
		int pairs = (x2 - x1 + 1) >> 1;
		int i;
		for (i = 0; i < pairs; i++)
			row32[i] = c32;
		x1 += pairs * 2;
	}

	/* Handle remaining pixel */
	if (x1 <= x2)
		row[x1] = c16;
}


/* Vertical line */
static void draw_vline(int x, int y1, int y2, int col)
{
	int tmp;
	uint16_t c16;
	if (y1 > y2) { tmp = y1; y1 = y2; y2 = tmp; }
	if (x < clip_tx || x > clip_bx) return;
	if (y1 < clip_ty) y1 = clip_ty;
	if (y2 > clip_by) y2 = clip_by;
	c16 = palette_rgba16[col];
	for (; y1 <= y2; y1++)
		active_fb[y1 * N64_SCREEN_W + x] = c16;
}


void gfx_draw_line(int x1, int y1, int x2, int y2)
{
	x1 += GFX_X_OFFSET;
	y1 += GFX_Y_OFFSET;
	x2 += GFX_X_OFFSET;
	y2 += GFX_Y_OFFSET;

	if (y1 == y2)
	{
		draw_hline(x1, x2, y1, GFX_COL_WHITE);
		return;
	}
	if (x1 == x2)
	{
		draw_vline(x1, y1, y2, GFX_COL_WHITE);
		return;
	}

	draw_line_clipped(x1, y1, x2, y2, GFX_COL_WHITE);
}


void gfx_draw_colour_line(int x1, int y1, int x2, int y2, int line_colour)
{
	x1 += GFX_X_OFFSET;
	y1 += GFX_Y_OFFSET;
	x2 += GFX_X_OFFSET;
	y2 += GFX_Y_OFFSET;

	if (y1 == y2)
	{
		draw_hline(x1, x2, y1, line_colour);
		return;
	}
	if (x1 == x2)
	{
		draw_vline(x1, y1, y2, line_colour);
		return;
	}

	draw_line_clipped(x1, y1, x2, y2, line_colour);
}


/* Software triangle fill using scanline */
void gfx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int col)
{
	int tmp;
	int y;

	x1 += GFX_X_OFFSET; y1 += GFX_Y_OFFSET;
	x2 += GFX_X_OFFSET; y2 += GFX_Y_OFFSET;
	x3 += GFX_X_OFFSET; y3 += GFX_Y_OFFSET;

	/* Sort vertices by y */
	if (y1 > y2) { tmp=x1; x1=x2; x2=tmp; tmp=y1; y1=y2; y2=tmp; }
	if (y1 > y3) { tmp=x1; x1=x3; x3=tmp; tmp=y1; y1=y3; y3=tmp; }
	if (y2 > y3) { tmp=x2; x2=x3; x3=tmp; tmp=y2; y2=y3; y3=tmp; }

	if (y3 == y1) {
		int minx = x1 < x2 ? (x1 < x3 ? x1 : x3) : (x2 < x3 ? x2 : x3);
		int maxx = x1 > x2 ? (x1 > x3 ? x1 : x3) : (x2 > x3 ? x2 : x3);
		draw_hline(minx, maxx, y1, col);
		return;
	}

	for (y = y1; y <= y3; y++)
	{
		int xa, xb;
		/* Edge from (x1,y1) to (x3,y3) is always active */
		xa = x1 + (x3 - x1) * (y - y1) / (y3 - y1);

		if (y < y2)
		{
			if (y2 == y1) xb = x1;
			else xb = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
		}
		else
		{
			if (y3 == y2) xb = x2;
			else xb = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
		}

		draw_hline(xa, xb, y, col);
	}
}


void gfx_draw_rectangle(int tx, int ty, int bx, int by, int col)
{
	int y;
	tx += GFX_X_OFFSET;
	ty += GFX_Y_OFFSET;
	bx += GFX_X_OFFSET;
	by += GFX_Y_OFFSET;

	for (y = ty; y <= by; y++)
		draw_hline(tx, bx, y, col);
}


/*
 * Embedded bitmap font - 8x8 pixel, ASCII 32-127.
 * Used for both ELITE_1 (normal 8px text) and ELITE_2 (title text).
 * At 640x480 the 8x8 font is a faithful representation for both sizes.
 */
extern const uint8_t n64_font_8x8[];

static void draw_char(int x, int y, char ch, int col)
{
	int cx, cy;
	const uint8_t *glyph;
	int idx = (unsigned char)ch;

	if (idx < 32 || idx > 127) return;
	glyph = &n64_font_8x8[(idx - 32) * 8];

	for (cy = 0; cy < 8; cy++)
	{
		uint8_t row = glyph[cy];
		for (cx = 0; cx < 8; cx++)
		{
			if (row & (0x80 >> cx))
				fb_putpixel(x + cx, y + cy, col);
		}
	}
}


void gfx_display_text(int x, int y, char *txt)
{
	int px = (x / (2 / GFX_SCALE)) + GFX_X_OFFSET;
	int py = (y / (2 / GFX_SCALE)) + GFX_Y_OFFSET;

	while (*txt)
	{
		draw_char(px, py, *txt, GFX_COL_WHITE);
		px += 8;
		txt++;
	}
}


void gfx_display_colour_text(int x, int y, char *txt, int col)
{
	int px = (x / (2 / GFX_SCALE)) + GFX_X_OFFSET;
	int py = (y / (2 / GFX_SCALE)) + GFX_Y_OFFSET;

	while (*txt)
	{
		draw_char(px, py, *txt, col);
		px += 8;
		txt++;
	}
}


void gfx_display_centre_text(int y, char *str, int psize, int col)
{
	int len = strlen(str);
	int txt_colour = col;
	int py = (y / (2 / GFX_SCALE)) + GFX_Y_OFFSET;
	int px;

	/* psize 140 = large title font; use same font but different color */
	if (psize == 140)
		txt_colour = GFX_COL_GOLD;

	px = (128 * GFX_SCALE) + GFX_X_OFFSET - (len * 4);

	while (*str)
	{
		draw_char(px, py, *str, txt_colour);
		px += 8;
		str++;
	}
}


/* Fast zero-fill a region of the framebuffer. */
static void fast_clear_region(int x1, int y1, int x2, int y2)
{
	int y;
	int w_bytes = (x2 - x1 + 1) * 2;
	for (y = y1; y <= y2; y++)
		memset(&active_fb[y * N64_SCREEN_W + x1], 0, w_bytes);
}

void gfx_clear_display(void)
{
	fast_clear_region(GFX_X_OFFSET + 1, GFX_Y_OFFSET + 1,
	                  GFX_X_OFFSET + 510, GFX_Y_OFFSET + GFX_VIEW_BY);
}

void gfx_clear_text_area(void)
{
	fast_clear_region(GFX_X_OFFSET + 1, GFX_Y_OFFSET + GFX_VIEW_BY - 40,
	                  GFX_X_OFFSET + 510, GFX_Y_OFFSET + GFX_VIEW_BY);
}

void gfx_clear_area(int tx, int ty, int bx, int by)
{
	tx += GFX_X_OFFSET;
	ty += GFX_Y_OFFSET;
	bx += GFX_X_OFFSET;
	by += GFX_Y_OFFSET;

	if (tx < 0) tx = 0;
	if (ty < 0) ty = 0;
	if (bx >= N64_SCREEN_W) bx = N64_SCREEN_W - 1;
	if (by >= N64_SCREEN_H) by = N64_SCREEN_H - 1;

	fast_clear_region(tx, ty, bx, by);
}


void gfx_display_pretty_text(int tx, int ty, int bx, int by, char *txt)
{
	char strbuf[100];
	char *str;
	char *bptr;
	int len;
	int pos;
	int maxlen;

	maxlen = (bx - tx) / 8;

	str = txt;
	len = strlen(txt);

	while (len > 0)
	{
		pos = maxlen;
		if (pos > len)
			pos = len;

		while ((str[pos] != ' ') && (str[pos] != ',') &&
		       (str[pos] != '.') && (str[pos] != '\0'))
		{
			pos--;
		}

		len = len - pos - 1;

		for (bptr = strbuf; pos >= 0; pos--)
			*bptr++ = *str++;

		*bptr = '\0';

		gfx_display_text(tx, ty, strbuf);
		ty += (8 * GFX_SCALE);
	}
}


void gfx_draw_scanner(void)
{
	int x, y;
	int dst_x, dst_y;

	if (!scanner_pixels)
		return;

	/* Set clip region to the scanner area */
	int old_ctx = clip_tx, old_cty = clip_ty, old_cbx = clip_bx, old_cby = clip_by;

	clip_tx = GFX_X_OFFSET;
	clip_ty = SCANNER_Y + GFX_Y_OFFSET;
	clip_bx = GFX_X_OFFSET + scanner_w - 1;
	if (clip_bx >= N64_SCREEN_W) clip_bx = N64_SCREEN_W - 1;
	clip_by = GFX_Y_OFFSET + SCANNER_Y + scanner_h - 1;
	if (clip_by >= N64_SCREEN_H) clip_by = N64_SCREEN_H - 1;

	/* Blit the scanner bitmap */
	for (y = 0; y < scanner_h; y++)
	{
		dst_y = SCANNER_Y + GFX_Y_OFFSET + y;
		if (dst_y >= N64_SCREEN_H) break;
		for (x = 0; x < scanner_w; x++)
		{
			dst_x = GFX_X_OFFSET + x;
			if (dst_x >= N64_SCREEN_W) break;
			active_fb[dst_y * N64_SCREEN_W + dst_x] = palette_rgba16[scanner_pixels[y * scanner_w + x]];
		}
	}

	/* Restore clip region */
	clip_tx = old_ctx; clip_ty = old_cty;
	clip_bx = old_cbx; clip_by = old_cby;
}


void gfx_set_clip_region(int tx, int ty, int bx, int by)
{
	clip_tx = tx + GFX_X_OFFSET;
	clip_ty = ty + GFX_Y_OFFSET;
	clip_bx = bx + GFX_X_OFFSET;
	clip_by = by + GFX_Y_OFFSET;

	/* Clamp */
	if (clip_tx < 0) clip_tx = 0;
	if (clip_ty < 0) clip_ty = 0;
	if (clip_bx >= N64_SCREEN_W) clip_bx = N64_SCREEN_W - 1;
	if (clip_by >= N64_SCREEN_H) clip_by = N64_SCREEN_H - 1;
}


void gfx_draw_borders(void)
{
	/* Border lines around the view area - must be redrawn each frame
	 * since there's no persistent framebuffer. */
	gfx_draw_line(0, 0, 0, GFX_VIEW_BY + 1);
	gfx_draw_line(0, 0, 511, 0);
	gfx_draw_line(511, 0, 511, GFX_VIEW_BY + 1);
}


void gfx_start_render(void)
{
	start_poly = 0;
	total_polys = 0;
}


void gfx_render_polygon(int num_points, int *point_list, int face_colour, int zavg)
{
	int i;
	int x;
	int nx;

	if (total_polys == MAX_POLYS)
		return;

	x = total_polys;
	total_polys++;

	poly_chain[x].no_points = num_points;
	poly_chain[x].face_colour = face_colour;
	poly_chain[x].z = zavg;
	poly_chain[x].next = -1;

	for (i = 0; i < 16; i++)
		poly_chain[x].point_list[i] = point_list[i];

	if (x == 0)
		return;

	if (zavg > poly_chain[start_poly].z)
	{
		poly_chain[x].next = start_poly;
		start_poly = x;
		return;
	}

	for (i = start_poly; poly_chain[i].next != -1; i = poly_chain[i].next)
	{
		nx = poly_chain[i].next;

		if (zavg > poly_chain[nx].z)
		{
			poly_chain[i].next = x;
			poly_chain[x].next = nx;
			return;
		}
	}

	poly_chain[i].next = x;
}


void gfx_render_line(int x1, int y1, int x2, int y2, int dist, int col)
{
	int point_list[4];

	point_list[0] = x1;
	point_list[1] = y1;
	point_list[2] = x2;
	point_list[3] = y2;

	gfx_render_polygon(2, point_list, col, dist);
}


void gfx_finish_render(void)
{
	int num_points;
	int *pl;
	int i;
	int col;

	if (total_polys == 0)
		return;

	for (i = start_poly; i != -1; i = poly_chain[i].next)
	{
		num_points = poly_chain[i].no_points;
		pl = poly_chain[i].point_list;
		col = poly_chain[i].face_colour;

		if (num_points == 2)
		{
			gfx_draw_colour_line(pl[0], pl[1], pl[2], pl[3], col);
			continue;
		}

		gfx_polygon(num_points, pl, col);
	};
}


/* Software polygon fill using scanline rasterization */
void gfx_polygon(int num_points, int *poly_list, int face_colour)
{
	int i;
	int x, y;
	int min_y, max_y;
	int scanline_min[N64_SCREEN_H];
	int scanline_max[N64_SCREEN_H];
	int j;

	x = 0;
	y = 1;
	for (i = 0; i < num_points; i++)
	{
		poly_list[x] += GFX_X_OFFSET;
		poly_list[y] += GFX_Y_OFFSET;
		x += 2;
		y += 2;
	}

	/* Find Y extents */
	min_y = N64_SCREEN_H;
	max_y = 0;
	for (i = 0; i < num_points; i++)
	{
		int py = poly_list[i * 2 + 1];
		if (py < min_y) min_y = py;
		if (py > max_y) max_y = py;
	}

	if (min_y < clip_ty) min_y = clip_ty;
	if (max_y > clip_by) max_y = clip_by;
	if (min_y > max_y) return;

	/* Init scanline bounds */
	for (i = min_y; i <= max_y; i++)
	{
		scanline_min[i] = N64_SCREEN_W;
		scanline_max[i] = 0;
	}

	/* Trace all edges */
	for (i = 0; i < num_points; i++)
	{
		int x1 = poly_list[i * 2];
		int y1 = poly_list[i * 2 + 1];
		j = (i + 1) % num_points;
		int x2 = poly_list[j * 2];
		int y2 = poly_list[j * 2 + 1];

		int dy = abs(y2 - y1);
		int dx = abs(x2 - x1);
		int sx = (x1 < x2) ? 1 : -1;
		int sy = (y1 < y2) ? 1 : -1;
		int err = dx - dy;
		int cx = x1, cy = y1;

		for (;;)
		{
			if (cy >= min_y && cy <= max_y)
			{
				if (cx < scanline_min[cy]) scanline_min[cy] = cx;
				if (cx > scanline_max[cy]) scanline_max[cy] = cx;
			}
			if (cx == x2 && cy == y2) break;
			int e2 = 2 * err;
			if (e2 > -dy) { err -= dy; cx += sx; }
			if (e2 < dx) { err += dx; cy += sy; }
		}
	}

	/* Fill scanlines */
	for (i = min_y; i <= max_y; i++)
	{
		if (scanline_min[i] <= scanline_max[i])
		{
			int sx = scanline_min[i];
			int ex = scanline_max[i];
			if (sx < clip_tx) sx = clip_tx;
			if (ex > clip_bx) ex = clip_bx;
			if (sx <= ex)
				{
					uint16_t c16 = palette_rgba16[face_colour];
					uint16_t *row = &active_fb[i * N64_SCREEN_W + sx];
					int len = ex - sx + 1;
					/* 32-bit fill for speed */
					if ((((uintptr_t)row) & 2) && len > 0) { *row++ = c16; len--; }
					{
						uint32_t c32 = ((uint32_t)c16 << 16) | c16;
						uint32_t *r32 = (uint32_t *)row;
						int pairs = len >> 1;
						int j;
						for (j = 0; j < pairs; j++) r32[j] = c32;
						row += pairs * 2;
						len -= pairs * 2;
					}
					if (len > 0) *row = c16;
				}
		}
	}
}


/*
 * Sprite data lookup: n64_sprite_data[], n64_sprite_widths[],
 * n64_sprite_heights[] are provided by n64_spritedata.c (generated
 * from .sprite files by tools/convert_assets.py) and declared in
 * n64_assets.h.  Arrays are indexed by alg_data.h constants
 * (BLAKE=0, ECM=2, ELITETXT=5, etc.)
 */

/* Map IMG_* constants (from gfx.h) to alg_data.h sprite indices */
static int img_to_sprite(int sprite_no)
{
	switch (sprite_no)
	{
		case IMG_GREEN_DOT:      return 7;   /* GRNDOT */
		case IMG_RED_DOT:        return 11;  /* REDDOT */
		case IMG_BIG_S:          return 12;  /* SAFE */
		case IMG_ELITE_TXT:      return 5;   /* ELITETXT */
		case IMG_BIG_E:          return 2;   /* ECM */
		case IMG_DICE:           return 6;   /* FRONTV */
		case IMG_BLAKE:          return 0;   /* BLAKE */
		case IMG_MISSILE_GREEN:  return 8;   /* MISSILE_G */
		case IMG_MISSILE_YELLOW: return 10;  /* MISSILE_Y */
		case IMG_MISSILE_RED:    return 9;   /* MISSILE_R */
		default: return -1;
	}
}


void gfx_draw_sprite(int sprite_no, int x, int y)
{
	int idx;
	int sx, sy;
	int w, h;
	const uint8_t *pixels;

	idx = img_to_sprite(sprite_no);
	if (idx < 0 || !n64_sprite_data[idx])
		return;

	pixels = n64_sprite_data[idx];
	w = n64_sprite_widths[idx];
	h = n64_sprite_heights[idx];

	if (x == -1)
		x = ((256 * GFX_SCALE) - w) / 2;

	x += GFX_X_OFFSET;
	y += GFX_Y_OFFSET;

	for (sy = 0; sy < h; sy++)
	{
		for (sx = 0; sx < w; sx++)
		{
			uint8_t pixel = pixels[sy * w + sx];
			if (pixel != 0)  /* 0 = transparent */
				fb_putpixel(x + sx, y + sy, pixel);
		}
	}
}


int gfx_request_file(char *title, char *path, char *ext)
{
	/* No file dialog on N64 - use fixed save slots */
	/* Return 1 to indicate "ok" with the default path */
	return 1;
}
