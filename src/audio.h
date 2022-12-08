#pragma once

// Audio playback system.

typedef struct audio_t audio_t;
typedef struct audio_clip_t audio_clip_t;
typedef struct heap_t heap_t;
typedef struct vec3f_t vec3f_t;

// Initialize the audio system.
audio_t* audio_init(heap_t* heap);

// Create a reference to the audio clip stored at the given file path.
audio_clip_t* audio_clip_load(audio_t* audio, char* path, int streamed, int spatialization);

// Destroy the given audio system.
void audio_destroy(audio_t* audio);

// Destroy the given audio clip.
void audio_clip_destroy(audio_t* audio, audio_clip_t* clip);

// Play the given audio clip.
void audio_clip_play(audio_t* audio, audio_clip_t* clip);

// Set the max spacialized gain of the given audio clip to the given float.
void audio_clip_set_gain(audio_t* audio, audio_clip_t* clip, float gain);

// Set the position in space that the sound is being played from.
void audio_clip_set_position(audio_t* audio, audio_clip_t* clip, vec3f_t pos);