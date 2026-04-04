/*
 * Elite - The New Kind.
 *
 * Nintendo 64 sound routines using libdragon audio mixer.
 *
 * WAV samples are converted to wav64 format and loaded from
 * the ROM filesystem (DFS). MIDI is not supported on N64.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libdragon.h>

#include "sound.h"

#define NUM_SAMPLES 14
#define NUM_CHANNELS 4

static int sound_on;

static const char *sample_filenames[NUM_SAMPLES] = {
	"rom:/launch.wav64",
	"rom:/crash.wav64",
	"rom:/dock.wav64",
	"rom:/gameover.wav64",
	"rom:/pulse.wav64",
	"rom:/hitem.wav64",
	"rom:/explode.wav64",
	"rom:/ecm.wav64",
	"rom:/missile.wav64",
	"rom:/hyper.wav64",
	"rom:/incom1.wav64",
	"rom:/incom2.wav64",
	"rom:/beep.wav64",
	"rom:/boop.wav64",
};

static int sample_runtime[NUM_SAMPLES] = {
	32, 7, 36, 24, 4, 4, 23, 23, 25, 37, 4, 5, 2, 7
};

static int sample_timeleft[NUM_SAMPLES];
static int sample_exists[NUM_SAMPLES];

/*
 * Each channel gets its own wav64_t object to avoid DFS file handle
 * conflicts when the mixer reads audio data asynchronously.
 * ch_sample[ch] tracks which sample is loaded on each channel (-1 = none).
 */
static wav64_t ch_wave[NUM_CHANNELS];
static int ch_sample[NUM_CHANNELS];
static int ch_open[NUM_CHANNELS];


void snd_sound_startup(void)
{
	int i;

	sound_on = 1;

	audio_init(16000, 4);
	mixer_init(NUM_CHANNELS);

	/* Check which sample files exist in the ROM filesystem */
	for (i = 0; i < NUM_SAMPLES; i++)
	{
		FILE *fp = fopen(sample_filenames[i], "rb");
		if (fp)
		{
			fclose(fp);
			sample_exists[i] = 1;
		}
		else
		{
			sample_exists[i] = 0;
		}
		sample_timeleft[i] = 0;
	}

	/* Initialize channel tracking */
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		ch_sample[i] = -1;
		ch_open[i] = 0;
	}
}


void snd_sound_shutdown(void)
{
	int i;

	if (!sound_on)
		return;

	for (i = 0; i < NUM_CHANNELS; i++)
	{
		mixer_ch_stop(i);
		if (ch_open[i])
		{
			wav64_close(&ch_wave[i]);
			ch_open[i] = 0;
		}
	}

	sound_on = 0;
}


void snd_play_sample(int sample_no)
{
	int ch;
	static int next_channel = 0;

	if (!sound_on)
		return;

	if (sample_no < 0 || sample_no >= NUM_SAMPLES)
		return;

	if (!sample_exists[sample_no])
		return;

	if (sample_timeleft[sample_no] != 0)
		return;

	sample_timeleft[sample_no] = sample_runtime[sample_no];

	/* Pick next channel (round-robin) */
	ch = next_channel;
	next_channel = (next_channel + 1) % NUM_CHANNELS;

	/* Stop whatever is playing on this channel */
	mixer_ch_stop(ch);

	/* Close previous wav64 on this channel if open */
	if (ch_open[ch])
	{
		wav64_close(&ch_wave[ch]);
		ch_open[ch] = 0;
		ch_sample[ch] = -1;
	}

	/* Open a fresh wav64 handle for this channel */
	wav64_open(&ch_wave[ch], sample_filenames[sample_no]);
	ch_open[ch] = 1;
	ch_sample[ch] = sample_no;

	mixer_ch_set_vol(ch, 1.0f, 1.0f);
	wav64_play(&ch_wave[ch], ch);
}


void snd_update_sound(void)
{
	int i;

	if (!sound_on)
		return;

	for (i = 0; i < NUM_SAMPLES; i++)
	{
		if (sample_timeleft[i] > 0)
			sample_timeleft[i]--;
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
	(void)midi_no;
	(void)repeat;
}


void snd_stop_midi(void)
{
}
