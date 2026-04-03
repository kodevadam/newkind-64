/*
 * Elite - The New Kind.
 *
 * Nintendo 64 controller input routines using libdragon.
 *
 * N64 Controller mapping:
 *   Analog Stick: Roll left/right, Climb/Dive
 *   D-Pad:        Menu navigation / chart crosshair
 *   A:            Fire laser / Confirm
 *   B:            Increase speed / Back
 *   Z (trigger):  Modifier (acts as Ctrl) / Decrease speed
 *   C-Right:      Fire missile
 *   C-Up:         Target missile
 *   C-Down:       Unarm missile
 *   C-Left:       ECM
 *   L:            Cycle view left / Screen group 1 (with D-Pad)
 *   R:            Cycle view right / Screen group 2 (with D-Pad)
 *   Start:        Pause / Resume
 *   L + D-Pad:    Galactic chart, Short range, Planet data, Market
 *   R + D-Pad:    Cmdr status, Inventory, Equip ship, Options
 *   A + B:        Hyperspace
 *   Z + A:        Galactic hyperspace
 *   Z + B:        Energy bomb
 *   Z + Start:    Escape pod
 *   Z + C-Up:     Docking computer
 *   Z + C-Down:   Jump drive
 *   Z + C-Left:   Find planet
 *   Z + C-Right:  Origin
 */

#include <stdlib.h>
#include <string.h>

#include <libdragon.h>

#include "keyboard.h"

/* Analog stick deadzone */
#define STICK_DEADZONE  20

/* Virtual keyboard text input state for planet finder */
static int text_input_cursor;
static const char text_input_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int kbd_F1_pressed;
int kbd_F2_pressed;
int kbd_F3_pressed;
int kbd_F4_pressed;
int kbd_F5_pressed;
int kbd_F6_pressed;
int kbd_F7_pressed;
int kbd_F8_pressed;
int kbd_F9_pressed;
int kbd_F10_pressed;
int kbd_F11_pressed;
int kbd_F12_pressed;
int kbd_y_pressed;
int kbd_n_pressed;
int kbd_zoom_pressed;
int kbd_fire_pressed;
int kbd_ecm_pressed;
int kbd_energy_bomb_pressed;
int kbd_hyperspace_pressed;
int kbd_ctrl_pressed;
int kbd_jump_pressed;
int kbd_escape_pressed;
int kbd_dock_pressed;
int kbd_d_pressed;
int kbd_origin_pressed;
int kbd_find_pressed;
int kbd_fire_missile_pressed;
int kbd_target_missile_pressed;
int kbd_unarm_missile_pressed;
int kbd_pause_pressed;
int kbd_resume_pressed;
int kbd_inc_speed_pressed;
int kbd_dec_speed_pressed;
int kbd_up_pressed;
int kbd_down_pressed;
int kbd_left_pressed;
int kbd_right_pressed;
int kbd_enter_pressed;
int kbd_backspace_pressed;
int kbd_space_pressed;
int kbd_i_pressed;

char old_key[128]; /* compatibility with alg_main.c cheat code checks */

/* View cycling state */
static int current_view_index = 0; /* 0=front, 1=right, 2=rear, 3=left */

int kbd_keyboard_startup(void)
{
	joypad_init();
	memset(old_key, 0, sizeof(old_key));
	text_input_cursor = 0;
	return 0;
}

int kbd_keyboard_shutdown(void)
{
	return 0;
}


static void cycle_view_right(void)
{
	/* Front -> Right -> Rear -> Left -> Front (SNES R-button order) */
	current_view_index = (current_view_index + 1) & 3;
	switch (current_view_index)
	{
		case 0: kbd_F1_pressed = 1; break; /* Front */
		case 1: kbd_F4_pressed = 1; break; /* Right */
		case 2: kbd_F2_pressed = 1; break; /* Rear */
		case 3: kbd_F3_pressed = 1; break; /* Left */
	}
}

static void cycle_view_left(void)
{
	/* Front -> Left -> Rear -> Right -> Front (SNES L-button order) */
	current_view_index = (current_view_index + 3) & 3;
	switch (current_view_index)
	{
		case 0: kbd_F1_pressed = 1; break;
		case 1: kbd_F4_pressed = 1; break;
		case 2: kbd_F2_pressed = 1; break;
		case 3: kbd_F3_pressed = 1; break;
	}
}


void kbd_poll_keyboard(void)
{
	int stick_x, stick_y;
	int z_held, l_held, r_held;

	/* Read controller */
	joypad_poll();
	joypad_buttons_t btns = joypad_get_buttons(JOYPAD_PORT_1);
	joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
	joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);

	/* Read analog stick */
	stick_x = inputs.stick_x;
	stick_y = inputs.stick_y;

	/* Modifier buttons */
	z_held = btns.z;
	l_held = btns.l;
	r_held = btns.r;

	/* Clear all key states */
	kbd_F1_pressed = 0;
	kbd_F2_pressed = 0;
	kbd_F3_pressed = 0;
	kbd_F4_pressed = 0;
	kbd_F5_pressed = 0;
	kbd_F6_pressed = 0;
	kbd_F7_pressed = 0;
	kbd_F8_pressed = 0;
	kbd_F9_pressed = 0;
	kbd_F10_pressed = 0;
	kbd_F11_pressed = 0;
	kbd_F12_pressed = 0;
	kbd_y_pressed = 0;
	kbd_n_pressed = 0;
	kbd_zoom_pressed = 0;
	kbd_fire_pressed = 0;
	kbd_ecm_pressed = 0;
	kbd_energy_bomb_pressed = 0;
	kbd_hyperspace_pressed = 0;
	kbd_ctrl_pressed = 0;
	kbd_jump_pressed = 0;
	kbd_escape_pressed = 0;
	kbd_dock_pressed = 0;
	kbd_d_pressed = 0;
	kbd_origin_pressed = 0;
	kbd_find_pressed = 0;
	kbd_fire_missile_pressed = 0;
	kbd_target_missile_pressed = 0;
	kbd_unarm_missile_pressed = 0;
	kbd_pause_pressed = 0;
	kbd_resume_pressed = 0;
	kbd_inc_speed_pressed = 0;
	kbd_dec_speed_pressed = 0;
	kbd_up_pressed = 0;
	kbd_down_pressed = 0;
	kbd_left_pressed = 0;
	kbd_right_pressed = 0;
	kbd_enter_pressed = 0;
	kbd_backspace_pressed = 0;
	kbd_space_pressed = 0;
	kbd_i_pressed = 0;

	/* Z acts as a "ctrl" modifier (like Select on SNES) */
	kbd_ctrl_pressed = z_held;

	/*
	 * ==========================================
	 * L + D-Pad = Screen selection group 1
	 * ==========================================
	 */
	if (l_held)
	{
		if (pressed.d_up)    kbd_F5_pressed = 1;   /* Galactic chart */
		if (pressed.d_down)  kbd_F6_pressed = 1;   /* Short range chart */
		if (pressed.d_left)  kbd_F7_pressed = 1;   /* Planet data */
		if (pressed.d_right) kbd_F8_pressed = 1;   /* Market prices */

		/* L alone (without d-pad) = cycle view left */
		if (!btns.d_up && !btns.d_down && !btns.d_left && !btns.d_right)
		{
			if (pressed.l)
				cycle_view_left();
		}
	}
	/*
	 * ==========================================
	 * R + D-Pad = Screen selection group 2
	 * ==========================================
	 */
	else if (r_held)
	{
		if (pressed.d_up)    kbd_F9_pressed = 1;    /* Commander status */
		if (pressed.d_down)  kbd_F10_pressed = 1;   /* Inventory */
		if (pressed.d_left)  kbd_F4_pressed = 1;    /* Equip ship (when docked) */
		if (pressed.d_right) kbd_F11_pressed = 1;   /* Options */

		/* R alone = cycle view right */
		if (!btns.d_up && !btns.d_down && !btns.d_left && !btns.d_right)
		{
			if (pressed.r)
				cycle_view_right();
		}
	}
	/*
	 * ==========================================
	 * Z + buttons = special actions
	 * ==========================================
	 */
	else if (z_held)
	{
		if (pressed.a)          /* Z+A = Galactic hyperspace */
			kbd_hyperspace_pressed = 1; /* ctrl is already set */
		if (pressed.b)          /* Z+B = Energy bomb */
			kbd_energy_bomb_pressed = 1;
		if (pressed.start)      /* Z+Start = Escape pod */
			kbd_escape_pressed = 1;
		if (pressed.c_up)       /* Z+C-Up = Docking computer */
			kbd_dock_pressed = 1;
		if (pressed.c_down)     /* Z+C-Down = Jump drive */
			kbd_jump_pressed = 1;
		if (pressed.c_left)     /* Z+C-Left = Find planet */
			kbd_find_pressed = 1;
		if (pressed.c_right)    /* Z+C-Right = Origin */
			kbd_origin_pressed = 1;
		if (pressed.d_up)       /* Z+D-Up = Distance */
			kbd_d_pressed = 1;
		if (pressed.d_down)     /* Z+D-Down = Zoom */
			kbd_zoom_pressed = 1;
	}
	/*
	 * ==========================================
	 * Normal (unmodified) button mappings
	 * ==========================================
	 */
	else
	{
		/* A = Fire laser (SNES: A button) */
		if (btns.a)
		{
			/* A+B combo = Hyperspace */
			if (btns.b)
				kbd_hyperspace_pressed = 1;
			else
				kbd_fire_pressed = 1;
		}

		/* B = Increase speed (SNES: B button) */
		if (btns.b && !btns.a)
			kbd_inc_speed_pressed = 1;

		/* Z = Decrease speed (SNES: X button equivalent) */
		/* (handled above as modifier, but when tapped alone:) */

		/* C-buttons = weapon controls */
		if (pressed.c_right)
			kbd_fire_missile_pressed = 1;
		if (pressed.c_up)
			kbd_target_missile_pressed = 1;
		if (pressed.c_down)
			kbd_unarm_missile_pressed = 1;
		if (pressed.c_left)
			kbd_ecm_pressed = 1;

		/* Start = Pause */
		if (pressed.start)
			kbd_pause_pressed = 1;

		/* Start also resumes when paused */
		kbd_resume_pressed = pressed.start;

		/* D-Pad = menu navigation / chart crosshair movement */
		if (btns.d_up)    kbd_up_pressed = 1;
		if (btns.d_down)  kbd_down_pressed = 1;
		if (btns.d_left)  kbd_left_pressed = 1;
		if (btns.d_right) kbd_right_pressed = 1;

		/* A in menus = Enter/Confirm */
		if (pressed.a)
			kbd_enter_pressed = 1;

		/* B in menus = Back (acts as backspace in find, or N for no) */
		if (pressed.b)
		{
			kbd_backspace_pressed = 1;
			kbd_n_pressed = 1;
		}

		/* A also acts as Y for "yes" confirmations */
		if (pressed.a)
			kbd_y_pressed = 1;

		/* L alone = launch (F1 when docked) */
		if (pressed.l)
			kbd_F1_pressed = 1;
	}

	/*
	 * ==========================================
	 * Analog stick = flight controls
	 * ==========================================
	 * Like SNES D-pad but with analog precision.
	 * Stick left/right = roll, stick up/down = climb/dive
	 */
	if (abs(stick_x) > STICK_DEADZONE)
	{
		if (stick_x > 0)
			kbd_right_pressed = 1;
		else
			kbd_left_pressed = 1;
	}

	if (abs(stick_y) > STICK_DEADZONE)
	{
		if (stick_y > 0)
			kbd_up_pressed = 1;    /* stick up = dive (nose down) */
		else
			kbd_down_pressed = 1;  /* stick down = climb (nose up) */
	}

	/* Z tap (without other buttons) = decrease speed */
	if (pressed.z && !btns.a && !btns.b && !btns.start &&
	    !btns.c_up && !btns.c_down && !btns.c_left && !btns.c_right)
	{
		kbd_dec_speed_pressed = 1;
	}

	/* Space equivalent (for intro screen skip) = Start */
	if (pressed.start)
		kbd_space_pressed = 1;
}


/*
 * Read a single key for text input (planet finder).
 * On N64, we use a virtual keyboard navigated with D-pad.
 * C-Left/C-Right to move through alphabet, A to select, B to delete.
 */
int kbd_read_key(void)
{
	joypad_poll();
	joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

	kbd_enter_pressed = 0;
	kbd_backspace_pressed = 0;

	if (pressed.a)
	{
		kbd_enter_pressed = 1;
		return 0;
	}

	if (pressed.b)
	{
		kbd_backspace_pressed = 1;
		return 0;
	}

	/* Use d-pad left/right to select letter, c-up to type it */
	if (pressed.d_right)
	{
		text_input_cursor = (text_input_cursor + 1) % 26;
		return text_input_alphabet[text_input_cursor];
	}

	if (pressed.d_left)
	{
		text_input_cursor = (text_input_cursor + 25) % 26;
		return text_input_alphabet[text_input_cursor];
	}

	/* D-up/down for quick letter jumps */
	if (pressed.d_up)
	{
		text_input_cursor = (text_input_cursor + 5) % 26;
		return text_input_alphabet[text_input_cursor];
	}

	if (pressed.d_down)
	{
		text_input_cursor = (text_input_cursor + 21) % 26;
		return text_input_alphabet[text_input_cursor];
	}

	return 0;
}


void kbd_clear_key_buffer(void)
{
	/* No input buffer on N64 - no-op */
}
