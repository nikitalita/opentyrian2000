/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "nortsong.h"

#include "file.h"
#include "joystick.h"
#include "keyboard.h"
#include "loudness.h"
#include "musmast.h"
#include "opentyr.h"
#include "params.h"
#include "sndmast.h"
#include "vga256d.h"

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#if defined(__APPLE__) & defined(__MACH__)
#include "macos-bundle.h"

#define fseek fseeko
#define ftell ftello
#elif (defined(_WIN32) || defined(WIN32)) && !defined(_MSC_VER)
#define fseek fseeko64
#define ftell ftello64
#define fopen fopen64
#endif

JE_word frameCountMax;

Sint16 *soundSamples[SOUND_COUNT] = { NULL }; /* [1..soundnum + 9] */  // FKA digiFx
size_t soundSampleCount[SOUND_COUNT] = { 0 }; /* [1..soundnum + 9] */  // FKA fxSize

JE_word tyrMusicVolume, fxVolume;
const JE_word fxPlayVol = 4;
JE_word tempVolume;

// The period of the x86 programmable interval timer in milliseconds.
static const float pitPeriod = (12.0f / 14318180.0f) * 1000.0f;

static Uint16 delaySpeed = 0x4300;
static float delayPeriod = 0x4300 * ((12.0f / 14318180.0f) * 1000.0f);

static Uint32 target = 0;
static Uint32 target2 = 0;

void setDelay(int delay)  // FKA NortSong.frameCount
{
	target = SDL_GetTicks() + delay * delayPeriod;
}

void setDelay2(int delay)  // FKA NortSong.frameCount2
{
	target2 = SDL_GetTicks() + delay * delayPeriod;
}

Uint32 getDelayTicks(void)  // FKA NortSong.frameCount
{
	Sint32 delay = target - SDL_GetTicks();
	return MAX(0, delay);
}

Uint32 getDelayTicks2(void)  // FKA NortSong.frameCount2
{
	Sint32 delay = target2 - SDL_GetTicks();
	return MAX(0, delay);
}

void wait_delay(void)
{
	Sint32 delay = target - SDL_GetTicks();
	if (delay > 0)
		SDL_Delay(delay);
}

void service_wait_delay(void)
{
	for (; ; )
	{
		service_SDL_events(false);

		Sint32 delay = target - SDL_GetTicks();
		if (delay <= 0)
			return;

		SDL_Delay(MIN(delay, SDL_POLL_INTERVAL));
	}
}

void wait_delayorinput(void)
{
	for (; ; )
	{
		service_SDL_events(false);
		poll_joysticks();

		if (newkey || mousedown || joydown)
		{
			newkey = false;
			return;
		}

		Sint32 delay = target - SDL_GetTicks();
		if (delay <= 0)
			return;

		SDL_Delay(MIN(delay, SDL_POLL_INTERVAL));
	}
}

void loadSndFile(bool xmas)
{
	FILE *f;

	f = dir_fopen_die(data_dir(), "tyrian.snd", "rb");

	Uint16 sfxCount;
	Uint32 sfxPositions[SFX_COUNT + 1];

	// Read number of sounds.
	fread_u16_die(&sfxCount, 1, f);
	/*if (sfxCount != SFX_COUNT)
		goto die;*/

	// Read positions of sounds.
	fread_u32_die(sfxPositions, sfxCount, f);

	// Determine end of last sound.
	fseek(f, 0, SEEK_END);
	sfxPositions[sfxCount] = ftell(f);

	// Read samples.
	for (size_t i = 0; i < sfxCount; ++i)
	{
		soundSampleCount[i] = sfxPositions[i + 1] - sfxPositions[i];

		// Sound size cannot exceed 64 KiB.
		if (soundSampleCount[i] > UINT16_MAX)
			goto die;

		free(soundSamples[i]);
		soundSamples[i] = malloc(soundSampleCount[i]);

		fseek(f, sfxPositions[i], SEEK_SET);
		fread_u8_die((Uint8 *)soundSamples[i], soundSampleCount[i], f);
	}

	fclose(f);

	f = dir_fopen_die(data_dir(), xmas ? "voicesc.snd" : "voices.snd", "rb");

	Uint16 voiceCount;
	Uint32 voicePositions[VOICE_COUNT + 1];

	// Read number of sounds.
	fread_u16_die(&voiceCount, 1, f);
	if (voiceCount != VOICE_COUNT)
		goto die;

	// Read positions of sounds.
	fread_u32_die(voicePositions, voiceCount, f);

	// Determine end of last sound.
	fseek(f, 0, SEEK_END);
	voicePositions[voiceCount] = ftell(f);

	for (size_t vi = 0; vi < voiceCount; ++vi)
	{
		size_t i = SFX_COUNT + vi;

		soundSampleCount[i] = voicePositions[vi + 1] - voicePositions[vi];

		// Voice sounds have some bad data at the end.
		soundSampleCount[i] = soundSampleCount[i] >= 100
			? soundSampleCount[i] - 100
			: 0;

		// Sound size cannot exceed 64 KiB.
		if (soundSampleCount[i] > UINT16_MAX)
			goto die;

		free(soundSamples[i]);
		soundSamples[i] = malloc(soundSampleCount[i]);

		fseek(f, voicePositions[vi], SEEK_SET);
		fread_u8_die((Uint8 *)soundSamples[i], soundSampleCount[i], f);
	}

	fclose(f);

	// Convert samples to output sample format and rate.

#ifdef WITH_SDL3
    SDL_AudioStream *cvtstream = NULL;
    SDL_AudioSpec cvtinspec;
    SDL_AudioSpec cvtoutspec;
    int src_samplesize = 0;
    int src_len = 0;
    int dst_len = 0;
    int real_dst_len = 0;
    int len_mult = 1;
    void *cvtdata = NULL;
    size_t maxSampleSize = 0;

    for (size_t i = 0; i < SOUND_COUNT; ++i)
        

    cvtinspec.format = SDL_AUDIO_S8;
    cvtinspec.channels = 1;
    cvtinspec.freq = 11025;

    cvtoutspec.format = SDL_AUDIO_S16;
    cvtoutspec.channels = 1;
    cvtoutspec.freq = audioSampleRate;

    if (11025 < audioSampleRate) {
        const double mult = ((double)audioSampleRate / (double)11025);
        len_mult *= (int)SDL_ceil(mult);
    }

    src_samplesize = (SDL_AUDIO_BITSIZE(cvtinspec.format) / 8) * cvtinspec.channels;

    for (size_t i = 0; i < SOUND_COUNT; ++i)
    {
        src_len = soundSampleCount[i] & ~(src_samplesize - 1);
        dst_len = soundSampleCount[i] * len_mult;
        
        cvtstream = SDL_CreateAudioStream(&cvtinspec, &cvtoutspec);

        if (cvtstream == NULL)
        {
            fprintf(stderr, "error: Failed to make audio converter stream: %s\n", SDL_GetError());

            for (int i = 0; i < SOUND_COUNT; ++i)
                soundSampleCount[i] = 0;
            
            return;
        }

        maxSampleSize = MAX(maxSampleSize, soundSampleCount[i]);
        
        cvtdata = malloc(maxSampleSize * len_mult);
        
        if (cvtdata == NULL)
        {
            fprintf(stderr, "error: Failed to allocate memory for audio converter\n");
            
            for (int i = 0; i < SOUND_COUNT; ++i)
                soundSampleCount[i] = 0;
            
            return;
        }
        
        if ((SDL_PutAudioStreamData(cvtstream, soundSamples[i], src_len) == false) ||
            (SDL_FlushAudioStream(cvtstream) == false))
        {
            fprintf(stderr, "error: Failed to load sound samples: %s\n", SDL_GetError());

            for (int i = 0; i < SOUND_COUNT; ++i)
                soundSampleCount[i] = 0;
            
            return;
        }
        
        real_dst_len = SDL_GetAudioStreamData(cvtstream, cvtdata, dst_len);
        
        if (real_dst_len < 0)
        {
            fprintf(stderr, "error: Failed to save sound samples: %s\n", SDL_GetError());
            
            for (int i = 0; i < SOUND_COUNT; ++i)
                soundSampleCount[i] = 0;
            
            return;
        }
        
        SDL_DestroyAudioStream(cvtstream);
        
        if (soundSamples[i] != NULL)
        {
            free(soundSamples[i]);
        }
        
        soundSamples[i] = malloc(real_dst_len);
        memcpy(soundSamples[i], cvtdata, real_dst_len);
        soundSampleCount[i] = real_dst_len / sizeof (Sint16);

        if (cvtdata != NULL)
        {
            free(cvtdata);
        }
    }
#else
	SDL_AudioCVT cvt;
	if (SDL_BuildAudioCVT(&cvt, AUDIO_S8, 1, 11025, AUDIO_S16SYS, 1, audioSampleRate) < 0)
	{
		fprintf(stderr, "error: Failed to build audio converter: %s\n", SDL_GetError());

		for (int i = 0; i < SOUND_COUNT; ++i)
			soundSampleCount[i] = 0;

		return;
	}

	size_t maxSampleSize = 0;
	for (size_t i = 0; i < SOUND_COUNT; ++i)
		maxSampleSize = MAX(maxSampleSize, soundSampleCount[i]);

	cvt.buf = malloc(maxSampleSize * cvt.len_mult);

	for (size_t i = 0; i < SOUND_COUNT; ++i)
	{
		cvt.len = soundSampleCount[i];
		memcpy(cvt.buf, soundSamples[i], cvt.len);

		if (SDL_ConvertAudio(&cvt))
		{
			fprintf(stderr, "error: Failed to convert audio: %s\n", SDL_GetError());

			soundSampleCount[i] = 0;

			continue;
		}

		free(soundSamples[i]);
		soundSamples[i] = malloc(cvt.len_cvt);

		memcpy(soundSamples[i], cvt.buf, cvt.len_cvt);
		soundSampleCount[i] = cvt.len_cvt / sizeof (Sint16);
	}

	free(cvt.buf);
#endif

	return;

die:
	fprintf(stderr, "error: Unexpected data was read from a file.\n");
	SDL_Quit();
	exit(EXIT_FAILURE);
}

void JE_playSampleNum(JE_byte samplenum)
{
	multiSamplePlay(soundSamples[samplenum-1], soundSampleCount[samplenum-1], 0, fxPlayVol);
}

void setDelaySpeed(Uint16 speed)  // FKA NortSong.speed and NortSong.setTimerInt
{
	delaySpeed = speed;
	delayPeriod = speed * pitPeriod;
}

void JE_changeVolume(JE_word *music, int music_delta, JE_word *sample, int sample_delta)
{
	int music_temp = *music + music_delta,
	    sample_temp = *sample + sample_delta;

	if (music_delta)
	{
		if (music_temp > 255)
		{
			music_temp = 255;
			JE_playSampleNum(S_CLINK);
		}
		else if (music_temp < 0)
		{
			music_temp = 0;
			JE_playSampleNum(S_CLINK);
		}
	}

	if (sample_delta)
	{
		if (sample_temp > 255)
		{
			sample_temp = 255;
			JE_playSampleNum(S_CLINK);
		}
		else if (sample_temp < 0)
		{
			sample_temp = 0;
			JE_playSampleNum(S_CLINK);
		}
	}

	*music = music_temp;
	*sample = sample_temp;

	set_volume(*music, *sample);
}
