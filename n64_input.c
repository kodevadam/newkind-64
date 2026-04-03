/*
 * Elite - The New Kind.
 *
 * Nintendo 64 controller input routines using libdragon.
 *
 * N64 Controller Layout:
 *
 *   Analog Stick:  Roll / Climb-Dive (flight), Navigate (menus/charts)
 *   D-Pad:         Also Roll / Climb-Dive / Navigate (digital)
 *   A:             Fire laser / Confirm / Yes
 *   B:             Speed up / Cancel / No
 *   Z:             Speed down
 *   Start:         Pause / Resume / Skip intro
 *   L:             Cycle view left  / Launch (when docked)
 *   R:             Cycle view right
 *   C-Up:          Target missile
 *   C-Down:        Unarm missile
 *   C-Left:        ECM
 *   C-Right:       Fire missile
 *
 *   L+R+A:         Hyperspace
 *   L+R+B:         Galactic hyperspace
 *   L+R+C-Up:      Docking computer
 *   L+R+C-Down:    Jump drive
 *   L+R+C-Left:    Energy bomb
 *   L+R+C-Right:   Escape pod
 *   L+R+Start:     Find planet
 *   L+R+D-Up:      Distance
 *   L+R+D-Down:    Zoom
 *   L+R+D-Left:    Origin
 *
 *   R+D-Up:        Galactic chart
 *   R+D-Down:      Short range chart
 *   R+D-Left:      Planet data
 *   R+D-Right:     Market prices
 *   L+D-Up:        Commander status
 *   L+D-Down:      Inventory
 *   L+D-Left:      Equip ship
 *   L+D-Right:     Options
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

char old_key[128];

/* Track previous L/R state for edge detection */
static int prev_l = 0, prev_r = 0;

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


void kbd_poll_keyboard(void)
{
	int stick_x, stick_y;
	int l_held, r_held, lr_held;
	int l_just, r_just;

	/* Read controller */
	joypad_poll();
	joypad_buttons_t btns = joypad_get_buttons(JOYPAD_PORT_1);
	joypad_buttons_t pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
	joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);

	/* Read analog stick */
	stick_x = inputs.stick_x;
	stick_y = inputs.stick_y;

	/* Shoulder button state */
	l_held = btns.l;
	r_held = btns.r;
	lr_held = l_held && r_held;
	l_just = l_held && !prev_l;
	r_just = r_held && !prev_r;
	prev_l = l_held;
	prev_r = r_held;

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

	/*
	 * ==========================================
	 * L+R combos (highest priority)
	 * ==========================================
	 */
	if (lr_held)
	{
		kbd_ctrl_pressed = 1;
		if (pressed.a)       kbd_hyperspace_pressed = 1;
		if (pressed.b)       { kbd_hyperspace_pressed = 1; kbd_ctrl_pressed = 1; } /* galactic */
		if (pressed.c_up)    kbd_dock_pressed = 1;
		if (pressed.c_down)  kbd_jump_pressed = 1;
		if (pressed.c_left)  kbd_energy_bomb_pressed = 1;
		if (pressed.c_right) kbd_escape_pressed = 1;
		if (pressed.start)   kbd_find_pressed = 1;
		if (pressed.d_up)    kbd_d_pressed = 1;
		if (pressed.d_down)  kbd_zoom_pressed = 1;
		if (pressed.d_left)  kbd_origin_pressed = 1;
		return; /* Don't process anything else */
	}

	/*
	 * ==========================================
	 * R + D-Pad = Screen selection group 1
	 * ==========================================
	 */
	if (r_held && (pressed.d_up || pressed.d_down || pressed.d_left || pressed.d_right))
	{
		if (pressed.d_up)    kbd_F5_pressed = 1;    /* Galactic chart */
		if (pressed.d_down)  kbd_F6_pressed = 1;    /* Short range chart */
		if (pressed.d_left)  kbd_F7_pressed = 1;    /* Planet data */
		if (pressed.d_right) kbd_F8_pressed = 1;    /* Market prices */
		return;
	}

	/*
	 * ==========================================
	 * L + D-Pad = Screen selection group 2
	 * ==========================================
	 */
	if (l_held && (pressed.d_up || pressed.d_down || pressed.d_left || pressed.d_right))
	{
		if (pressed.d_up)    kbd_F9_pressed = 1;    /* Commander status */
		if (pressed.d_down)  kbd_F10_pressed = 1;   /* Inventory */
		if (pressed.d_left)  kbd_F4_pressed = 1;    /* Equip ship */
		if (pressed.d_right) kbd_F11_pressed = 1;   /* Options */
		return;
	}

	/*
	 * ==========================================
	 * L alone = cycle view left / launch
	 * R alone = cycle view right
	 * ==========================================
	 */
	if (l_just && !r_held)
		kbd_F1_pressed = 1;  /* Launch when docked, Front view when flying */

	if (r_just && !l_held)
	{
		/* Cycle: front -> right -> rear -> left */
		static int view_idx = 0;
		view_idx = (view_idx + 1) & 3;
		switch (view_idx)
		{
			case 0: kbd_F1_pressed = 1; break;
			case 1: kbd_F4_pressed = 1; break;
			case 2: kbd_F2_pressed = 1; break;
			case 3: kbd_F3_pressed = 1; break;
		}
	}

	/*
	 * ==========================================
	 * A = Fire / Confirm / Yes
	 * ==========================================
	 */
	if (btns.a)
		kbd_fire_pressed = 1;
	if (pressed.a)
	{
		kbd_enter_pressed = 1;
		kbd_y_pressed = 1;
	}

	/*
	 * ==========================================
	 * B = Speed up / Cancel / No
	 * ==========================================
	 */
	if (btns.b)
		kbd_inc_speed_pressed = 1;
	if (pressed.b)
	{
		kbd_n_pressed = 1;
		kbd_backspace_pressed = 1;
	}

	/*
	 * ==========================================
	 * Z = Speed down
	 * ==========================================
	 */
	if (btns.z)
		kbd_dec_speed_pressed = 1;

	/*
	 * ==========================================
	 * C-buttons = weapon management
	 * ==========================================
	 */
	if (pressed.c_right)  kbd_fire_missile_pressed = 1;
	if (pressed.c_up)     kbd_target_missile_pressed = 1;
	if (pressed.c_down)   kbd_unarm_missile_pressed = 1;
	if (pressed.c_left)   kbd_ecm_pressed = 1;

	/*
	 * ==========================================
	 * Start = Pause/Resume/Skip
	 * ==========================================
	 */
	if (pressed.start)
	{
		kbd_pause_pressed = 1;
		kbd_resume_pressed = 1;
		kbd_space_pressed = 1;
	}

	/*
	 * ==========================================
	 * D-Pad = Navigation (menus, charts, flight)
	 * ==========================================
	 */
	if (btns.d_up)    kbd_up_pressed = 1;
	if (btns.d_down)  kbd_down_pressed = 1;
	if (btns.d_left)  kbd_left_pressed = 1;
	if (btns.d_right) kbd_right_pressed = 1;

	/*
	 * ==========================================
	 * Analog stick = Flight / Navigation
	 * ==========================================
	 */
	if (stick_x > STICK_DEADZONE)   kbd_right_pressed = 1;
	if (stick_x < -STICK_DEADZONE)  kbd_left_pressed = 1;
	if (stick_y > STICK_DEADZONE)   kbd_up_pressed = 1;
	if (stick_y < -STICK_DEADZONE)  kbd_down_pressed = 1;
}


/*
 * Read a single key for text input (planet finder).
 * On N64: D-pad left/right to select letter, A to confirm, B to delete.
 *
 * NOTE: This must NOT call joypad_poll() because it's called from
 * handle_flight_keys() before kbd_poll_keyboard(), and a second poll
 * would consume the pressed state before the main poll reads it.
 * Instead, this is a no-op that returns 0 - the actual button handling
 * is done through kbd_enter_pressed/kbd_backspace_pressed/kbd_up/down/etc
 * which are set by kbd_poll_keyboard() on the next frame.
 */
int kbd_read_key(void)
{
	/* Return 0 - actual input handled via kbd_* flags set by poll */
	return 0;
}


void kbd_clear_key_buffer(void)
{
	/* No-op on N64 */
}
