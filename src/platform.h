#ifndef PLATFORM_H
#define PLATFORM_H

#include "hitman_types.h"

#include <SDL2/SDL.h>

#define ArrayCount(Array) (sizeof(Array)/sizeof(*(Array)))

struct sdl_setup 
{
    SDL_Window *Window;
    SDL_Renderer *Renderer;
    SDL_Texture *WindowTexture = NULL;
};

struct sdl_sound_output
{
    int SamplesPerSecond;
    int ToneHz;
    s16 ToneVolume;
    u32 RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;

    int SafetyBytes;
};

struct sdl_audio_ring_buffer
{
    int Size;
    int WriteCursor;
    int PlayCursor;
    void *Data;
};

struct sdl_debug_time_marker 
{
    int PlayCursor;
    int WriteCursor;
};

typedef void (*GameUpdateAndRender_t)(game_offscreen_buffer*, game_memory *, game_input*, int);
typedef void (*GameGetSoundSamples_t)(game_memory *, game_sound_output_buffer*);

struct game_code
{
    char const *LibPath;
    void* LibHandle;
    s64 LastWriteTime;

    GameUpdateAndRender_t GameUpdateAndRender;
    GameGetSoundSamples_t GameGetSoundSamples;
};

#endif
