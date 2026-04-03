/*
 * Elite - The New Kind.
 *
 * Nintendo 64 version of file I/O.
 *
 * Config files are baked into ROM filesystem (read-only).
 * Commander save/load uses SRAM (256 bytes per save slot).
 *
 * This is a faithful port: same config parsing logic, same save format.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdragon.h>

#include "elite.h"
#include "config.h"
#include "file.h"

/* SRAM base address on N64 */
#define SRAM_BASE    0x08000000
#define SAVE_SIZE    256
#define SAVE_MAGIC   0x454C4954  /* "ELIT" */

/* SRAM save slot layout:
 * Offset 0x000: 4-byte magic number
 * Offset 0x004: 256-byte commander data (same format as .nkc files)
 * Offset 0x104: 4-byte checksum of above
 */


/*
 * Config file is read-only on N64 (baked into ROM).
 * Write is a no-op.
 */
void write_config_file(void)
{
	/* No-op on N64: config is in ROM, can't write back */
}


/*
 * Read a line from a config buffer in memory.
 * Adapted from the original to work with a memory buffer instead of FILE*.
 */
static int read_cfg_line_from_buf(const char *buf, int buflen, int *pos, char *str, int max_size)
{
	char *s;
	int start;

	do
	{
		if (*pos >= buflen)
			return -1;

		start = *pos;
		/* Copy until newline or end */
		int i = 0;
		while (*pos < buflen && buf[*pos] != '\n' && i < max_size - 1)
		{
			str[i++] = buf[*pos];
			(*pos)++;
		}
		str[i] = '\0';
		if (*pos < buflen && buf[*pos] == '\n')
			(*pos)++;

		/* Strip comments and trailing whitespace */
		for (s = str; *s; s++)
		{
			if (*s == '#')
			{
				*s = '\0';
				break;
			}
		}

		if (s != str)
		{
			s--;
			while (s >= str && isspace((unsigned char)*s))
			{
				*s = '\0';
				s--;
			}
		}

	} while (*str == '\0');

	return 0;
}


/*
 * Load a file from ROM filesystem into a malloc'd buffer.
 */
static char *load_rom_file(const char *filename, int *outlen)
{
	int fh;
	int len;
	char *buf;

	fh = dfs_open(filename);
	if (fh < 0)
		return NULL;

	len = dfs_size(fh);
	buf = (char *)malloc(len + 1);
	if (!buf)
	{
		dfs_close(fh);
		return NULL;
	}

	dfs_read(buf, 1, len, fh);
	buf[len] = '\0';
	dfs_close(fh);

	*outlen = len;
	return buf;
}


/*
 * Read scanner config file from ROM filesystem.
 * Faithful port of the original read_scanner_config_file().
 */
void read_scanner_config_file(char *filename)
{
	char *buf;
	int buflen;
	int pos = 0;
	char str[256];

	buf = load_rom_file(filename, &buflen);
	if (!buf)
		return;

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		strcpy(scanner_filename, str);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
	{
		sscanf(str, "%d,%d", &scanner_cx, &scanner_cy);
		scanner_cy += 385;
	}

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
	{
		sscanf(str, "%d,%d", &compass_centre_x, &compass_centre_y);
		compass_centre_y += 385;
	}

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
	{
		sscanf(str, "%d,%d,%d", &condition_x, &condition_y, &condition_r);
		condition_y += 385;
	}

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
	{
		sscanf(str, "%d,%d", &zoom_x, &zoom_y);
		zoom_y += 385;
	}

	free(buf);
}


/*
 * Read newkind.cfg from ROM filesystem.
 * Faithful port of the original read_config_file().
 */
void read_config_file(void)
{
	char *buf;
	int buflen;
	int pos = 0;
	char str[256];

	buf = load_rom_file("newkind.cfg", &buflen);
	if (!buf)
		return;

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &speed_cap);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &wireframe);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &anti_alias_gfx);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &planet_render_style);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &hoopy_casinos);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &instant_dock);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		read_scanner_config_file(str);

	/* prefer_window is meaningless on N64, but parse it for compatibility */
	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &prefer_window);

	if (read_cfg_line_from_buf(buf, buflen, &pos, str, sizeof(str)) == 0)
		sscanf(str, "%d", &remap_keys);

	free(buf);
}


/*
 * Checksum calculation - identical to original.
 */
static int checksum(unsigned char *block)
{
	int acc, carry;
	int i;

	acc = 0x49;
	carry = 0;
	for (i = 0x49; i > 0; i--)
	{
		acc += block[i - 1] + carry;
		carry = acc >> 8;
		acc &= 255;
		acc ^= block[i];
	}

	return acc;
}


/*
 * Save commander to SRAM.
 * Uses the same 256-byte binary format as the original .nkc files.
 * The path parameter is ignored on N64 (single save slot).
 */
int save_commander_file(char *path)
{
	unsigned char block[256];
	int i;
	int chk;
	uint32_t magic = SAVE_MAGIC;

	(void)path; /* Unused on N64 */

	/* Build save block - identical to original */
	memset(block, 0, 256);

	block[0]  = cmdr.mission;
	block[1]  = docked_planet.d;
	block[2]  = docked_planet.b;
	block[3]  = cmdr.galaxy.a;
	block[4]  = cmdr.galaxy.b;
	block[5]  = cmdr.galaxy.c;
	block[6]  = cmdr.galaxy.d;
	block[7]  = cmdr.galaxy.e;
	block[8]  = cmdr.galaxy.f;
	block[9]  = (cmdr.credits >> 24) & 255;
	block[10] = (cmdr.credits >> 16) & 255;
	block[11] = (cmdr.credits >> 8) & 255;
	block[12] = cmdr.credits & 255;
	block[13] = cmdr.fuel;
	block[14] = 4;
	block[15] = cmdr.galaxy_number;
	block[16] = cmdr.front_laser;
	block[17] = cmdr.rear_laser;
	block[18] = cmdr.left_laser;
	block[19] = cmdr.right_laser;
	block[20] = 0;
	block[21] = 0;
	block[22] = cmdr.cargo_capacity + 2;

	for (i = 0; i < NO_OF_STOCK_ITEMS; i++)
		block[23 + i] = cmdr.current_cargo[i];

	block[40] = cmdr.ecm ? 255 : 0;
	block[41] = cmdr.fuel_scoop ? 255 : 0;
	block[42] = cmdr.energy_bomb ? 0x7F : 0;
	block[43] = cmdr.energy_unit;
	block[44] = cmdr.docking_computer ? 255 : 0;
	block[45] = cmdr.galactic_hyperdrive ? 255 : 0;
	block[46] = cmdr.escape_pod ? 255 : 0;
	block[47] = 0;
	block[48] = 0;
	block[49] = 0;
	block[50] = 0;
	block[51] = cmdr.missiles;
	block[52] = cmdr.legal_status;

	for (i = 0; i < NO_OF_STOCK_ITEMS; i++)
		block[53 + i] = stock_market[i].current_quantity;

	block[70] = cmdr.market_rnd;
	block[71] = cmdr.score & 255;
	block[72] = cmdr.score >> 8;
	block[73] = 0x20;

	chk = checksum(block);

	block[74] = chk ^ 0xA9;
	block[75] = chk;

	/* Write to SRAM via DMA */
	/* First write magic, then the 256-byte block */
	data_cache_hit_writeback_invalidate(&magic, sizeof(magic));
	dma_write(&magic, SRAM_BASE, sizeof(magic));

	data_cache_hit_writeback_invalidate(block, sizeof(block));
	dma_write(block, SRAM_BASE + 4, sizeof(block));

	return 0;
}


/*
 * Load commander from SRAM.
 * The path parameter is ignored on N64 (single save slot).
 */
int load_commander_file(char *path)
{
	unsigned char block[256];
	uint32_t magic;
	int i;
	int chk;

	(void)path; /* Unused on N64 */

	/* Read magic number from SRAM */
	dma_read(&magic, SRAM_BASE, sizeof(magic));
	data_cache_hit_invalidate(&magic, sizeof(magic));

	if (magic != SAVE_MAGIC)
		return 1; /* No save data */

	/* Read commander block from SRAM */
	dma_read(block, SRAM_BASE + 4, sizeof(block));
	data_cache_hit_invalidate(block, sizeof(block));

	/* Verify checksum - identical to original */
	chk = checksum(block);

	if ((block[74] != (chk ^ 0xA9)) || (block[75] != chk))
		return 1;

	/* Parse block - identical to original */
	saved_cmdr.mission = block[0];

	saved_cmdr.ship_x = block[1];
	saved_cmdr.ship_y = block[2];

	saved_cmdr.galaxy.a = block[3];
	saved_cmdr.galaxy.b = block[4];
	saved_cmdr.galaxy.c = block[5];
	saved_cmdr.galaxy.d = block[6];
	saved_cmdr.galaxy.e = block[7];
	saved_cmdr.galaxy.f = block[8];

	saved_cmdr.credits = block[9] << 24;
	saved_cmdr.credits += block[10] << 16;
	saved_cmdr.credits += block[11] << 8;
	saved_cmdr.credits += block[12];

	saved_cmdr.fuel = block[13];

	saved_cmdr.galaxy_number = block[15];
	saved_cmdr.front_laser = block[16];
	saved_cmdr.rear_laser = block[17];
	saved_cmdr.left_laser = block[18];
	saved_cmdr.right_laser = block[19];

	saved_cmdr.cargo_capacity = block[22] - 2;

	for (i = 0; i < NO_OF_STOCK_ITEMS; i++)
		saved_cmdr.current_cargo[i] = block[23 + i];

	saved_cmdr.ecm = block[40];
	saved_cmdr.fuel_scoop = block[41];
	saved_cmdr.energy_bomb = block[42];
	saved_cmdr.energy_unit = block[43];
	saved_cmdr.docking_computer = block[44];
	saved_cmdr.galactic_hyperdrive = block[45];
	saved_cmdr.escape_pod = block[46];
	saved_cmdr.missiles = block[51];
	saved_cmdr.legal_status = block[52];

	for (i = 0; i < NO_OF_STOCK_ITEMS; i++)
		saved_cmdr.station_stock[i] = block[53 + i];

	saved_cmdr.market_rnd = block[70];

	saved_cmdr.score = block[71];
	saved_cmdr.score += block[72] << 8;

	return 0;
}
