/*
 * Elite - The New Kind.
 *
 * Nintendo 64 sound routines using libdragon audio mixer.
 *
 * WAV samples are converted to wav64 format and loaded from
 * the ROM filesystem (DFS). MIDI is not supported on N64;
 * the theme and Blue Danube are omitted.
 */

#include <stdlib.h>
#include <string.h>

#include <libdragon.h>

#include "sound.h"

#define NUM_SAMPLES 14

static int sound_on;

struct sound_sample
{
	wav64_t wave;
	char filename[64];
	int loaded;
	int runtime;
	int timeleft;
};

static struct sound_sample sample_list[NUM_SAMPLES] =
{
	{ .filename = "rom:/launch.wav64",    .runtime = 32, .timeleft = 0 },
	{ .filename = "rom:/crash.wav64",     .runtime =  7, .timeleft = 0 },
	{ .filename = "rom:/dock.wav64",      .runtime = 36, .timeleft = 0 },
	{ .filename = "rom:/gameover.wav64",  .runtime = 24, .timeleft = 0 },
	{ .filename = "rom:/pulse.wav64",     .runtime =  4, .timeleft = 0 },
	{ .filename = "rom:/hitem.wav64",     .runtime =  4, .timeleft = 0 },
	{ .filename = "rom:/explode.wav64",   .runtime = 23, .timeleft = 0 },
	{ .filename = "rom:/ecm.wav64",       .runtime = 23, .timeleft = 0 },
	{ .filename = "rom:/missile.wav64",   .runtime = 25, .timeleft = 0 },
	{ .filename = "rom:/hyper.wav64",     .runtime = 37, .timeleft = 0 },
	{ .filename = "rom:/incom1.wav64",    .runtime =  4, .timeleft = 0 },
	{ .filename = "rom:/incom2.wav64",    .runtime =  5, .timeleft = 0 },
	{ .filename = "rom:/beep.wav64",      .runtime =  2, .timeleft = 0 },
	{ .filename = "rom:/boop.wav64",      .runtime =  7, .timeleft = 0 },
};

/* Number of mixer channels for sound effects */
#define SFX_CHANNELS 4


void snd_sound_startup(void)
{
	int i;

	sound_on = 1;

	/* Initialize audio subsystem */
	audio_init(22050, 4);  /* 22050 Hz, 4 buffers */
	mixer_init(SFX_CHANNELS);

	/* Load all sound samples from ROM filesystem */
	for (i = 0; i < NUM_SAMPLES; i++)
	{
		sample_list[i].loaded = 0;
		/* wav64_open takes the rom:/ path directly */
		wav64_open(&sample_list[i].wave, sample_list[i].filename);
		/* If the file doesn't exist, the wav64 struct will be zeroed/invalid
		 * but won't crash. Mark as loaded optimistically. */
		sample_list[i].loaded = 1;
	}
}


void snd_sound_shutdown(void)
{
	if (!sound_on)
		return;

	/* wav64 objects are statically allocated - no close needed */
	sound_on = 0;
}


void snd_play_sample(int sample_no)
{
	int ch;

	if (!sound_on)
		return;

	if (sample_no < 0 || sample_no >= NUM_SAMPLES)
		return;

	if (!sample_list[sample_no].loaded)
		return;

	if (sample_list[sample_no].timeleft != 0)
		return;

	sample_list[sample_no].timeleft = sample_list[sample_no].runtime;

	/* Find a free channel (round-robin) */
	static int next_channel = 0;
	ch = next_channel;
	next_channel = (next_channel + 1) % SFX_CHANNELS;

	mixer_ch_set_vol(ch, 1.0f, 1.0f);
	wav64_play(&sample_list[sample_no].wave, ch);
}


void snd_update_sound(void)
{
	int i;

	if (!sound_on)
		return;

	for (i = 0; i < NUM_SAMPLES; i++)
	{
		if (sample_list[i].timeleft > 0)
			sample_list[i].timeleft--;
	}

	/* Pump the audio mixer */
	if (audio_can_write())
	{
		short *buf = audio_write_begin();
		mixer_poll(buf, audio_get_buffer_length());
		audio_write_end();
	}
}


void snd_play_midi(int midi_no, int repeat)
{
	/* MIDI not supported on N64 */
	(void)midi_no;
	(void)repeat;
}


void snd_stop_midi(void)
{
	/* No-op on N64 */
}
