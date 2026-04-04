/*
 * Elite - The New Kind.
 *
 * Nintendo 64 sound routines using libdragon audio mixer.
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

/* Pre-opened wav64 handles - one per sample. Stays open for the game's lifetime. */
static wav64_t sample_wave[NUM_SAMPLES];
static int sample_loaded[NUM_SAMPLES];

/* Track which sample is playing on each channel to avoid conflicts */
static int ch_playing[NUM_CHANNELS];  /* sample index, or -1 */

void snd_sound_startup(void)
{
	int i;

	sound_on = 1;

	/* 22050 Hz is the sweet spot: good enough quality, low CPU cost.
	 * Use more buffers (8) for smoother output. */
	audio_init(22050, 8);
	mixer_init(NUM_CHANNELS);

	/* Pre-load all samples that exist */
	for (i = 0; i < NUM_SAMPLES; i++)
	{
		FILE *fp;
		sample_loaded[i] = 0;
		sample_timeleft[i] = 0;

		fp = fopen(sample_filenames[i], "rb");
		if (fp)
		{
			fclose(fp);
			wav64_open(&sample_wave[i], sample_filenames[i]);
			sample_loaded[i] = 1;
		}
	}

	for (i = 0; i < NUM_CHANNELS; i++)
		ch_playing[i] = -1;

	/* Note: we use manual mixer_poll in snd_update_sound instead of
	 * audio_set_buffer_callback because the callback approach was silent
	 * on real hardware. */
}


void snd_sound_shutdown(void)
{
	int i;

	if (!sound_on)
		return;

	for (i = 0; i < NUM_CHANNELS; i++)
		mixer_ch_stop(i);

	for (i = 0; i < NUM_SAMPLES; i++)
	{
		if (sample_loaded[i])
			wav64_close(&sample_wave[i]);
	}

	sound_on = 0;
}


void snd_play_sample(int sample_no)
{
	int ch, i;
	static int next_channel = 0;

	if (!sound_on)
		return;

	if (sample_no < 0 || sample_no >= NUM_SAMPLES)
		return;

	if (!sample_loaded[sample_no])
		return;

	if (sample_timeleft[sample_no] != 0)
		return;

	sample_timeleft[sample_no] = sample_runtime[sample_no];

	/* Check if this sample is already playing on any channel - skip if so */
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		if (ch_playing[i] == sample_no)
			return;
	}

	/* Pick next channel (round-robin) */
	ch = next_channel;
	next_channel = (next_channel + 1) % NUM_CHANNELS;

	/* Stop whatever is on this channel */
	mixer_ch_stop(ch);
	ch_playing[ch] = -1;

	/* Play the pre-opened sample */
	mixer_ch_set_vol(ch, 1.0f, 1.0f);
	wav64_play(&sample_wave[sample_no], ch);
	ch_playing[ch] = sample_no;
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

	/* Pump the audio mixer - call multiple times to fill all ready buffers */
	while (audio_can_write())
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
