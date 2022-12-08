#include "audio.h"
#include "debug.h"
#include "heap.h"
#include "vec3f.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

typedef struct audio_t
{
	heap_t* heap;
	ma_engine* engine;
} audio_t;

typedef enum audio_clip_type_t
{
	k_sync,
	k_async,
	k_stream
} audio_clip_type_t;

typedef struct audio_clip_t
{
	ma_sound* sound;
	audio_clip_type_t type;
} audio_clip_t;

audio_t* audio_init(heap_t* heap)
{
	audio_t* audio = heap_alloc(heap, sizeof(audio_t), 8);
	audio->heap = heap;
	audio->engine = heap_alloc(heap, sizeof(ma_engine), 8);
	ma_result result = ma_engine_init(NULL, audio->engine);
	if (result != MA_SUCCESS)
	{
		debug_print(k_print_error, "Audio: failed to initialize sound engine\n");
		return NULL;
	}
	return audio;
}

void audio_clip_set_position(audio_t* audio, audio_clip_t* clip, vec3f_t pos)
{
	ma_sound_set_position(clip->sound, pos.x, pos.y, pos.z);
}

void audio_clip_set_gain(audio_t* audio, audio_clip_t* clip, float gain)
{
	ma_sound_set_max_gain(clip->sound, gain);
}

void audio_destroy(audio_t* audio)
{
	ma_engine_uninit(audio->engine);
	heap_free(audio->heap, audio->engine);
}

void audio_clip_destroy(audio_t* audio, audio_clip_t* clip)
{
	ma_sound_uninit(clip->sound);
	heap_free(audio->heap, clip->sound);
}

audio_clip_t* audio_clip_load(audio_t* audio, char* path, int streamed, int spatialization)
{
	audio_clip_t* clip = heap_alloc(audio->heap, sizeof(audio_clip_t), 8);
	int type_flag;
	if (streamed)
		clip->type = k_stream;
	else
		clip->type = k_sync;

	switch (clip->type)
	{
		case k_sync: //decode audio immediately
			type_flag = MA_SOUND_FLAG_DECODE;
			break;
		case k_async: //decode audio asynchronously (may not play immediately)
			type_flag = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
			break;
		case k_stream: //stream audio from given file
			type_flag = MA_SOUND_FLAG_STREAM;
			break;
		default:
			type_flag = MA_SOUND_FLAG_DECODE;
			break;
	}

	clip->sound = heap_alloc(audio->heap, sizeof(ma_sound), 8);
	ma_result result = ma_sound_init_from_file(audio->engine, path, type_flag, NULL, NULL, clip->sound);
	if (result != MA_SUCCESS) {
		return NULL;
	}

	if (!spatialization)
	{
		ma_sound_set_spatialization_enabled(clip->sound, false);
	}

	return clip;
}

void audio_clip_play(audio_t* audio, audio_clip_t* clip)
{
	ma_sound_start(clip->sound);
}