#ifndef PLATFORM_H
#define PLATFORM_H

#include "hitman_defines.h"

#if HITMAN_DEBUG
#define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}
#else 
#define Assert(Expression)
#endif

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
    int16 ToneVolume;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;

    real32 tSine;
    int LatencySampleCount;
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

typedef void (*GameUpdateAndRender_t)(game_offscreen_buffer*, game_state *, game_sound_output_buffer*, game_input*, int);

struct game_code
{
    char const *LibPath;
    void* LibHandle;
    int64 LastWriteTime;

    GameUpdateAndRender_t GameUpdateAndRender;
};

struct debug_read_file_result
{
    uint32 ContentSize;
    void *Content;
};

#endif
