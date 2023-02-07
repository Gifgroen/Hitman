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

struct sdl_audio_buffer_index 
{
    int ByteToLock;
    int TargetCursor;
    int BytesToWrite;
};

struct sdl_debug_time_marker 
{
    int PlayCursor;
    int WriteCursor;
};

#endif
