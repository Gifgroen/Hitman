#ifndef DEBUG_SYNC_DISPLAY_H
#define DEBUG_SYNC_DISPLAY_H

#include "../hitman.h"
#include "../platform.h"
#include "../types.h"

#include "audio.h"

internal void SDLDebugDrawVertical(game_offscreen_buffer *Buffer, int Value, int Top, int Bottom, u32 Color);

inline void SDLDrawSoundBufferMarker(
    game_offscreen_buffer *Buffer,
    sdl_sound_output *SoundOutput,
    real32 C, 
    int PadX, 
    int Top, 
    int Bottom,
    int Value, 
    u32 Color
);

internal void SDLDebugSyncDisplay(
    game_offscreen_buffer *Buffer, 
    int DebugTimeMarkerCount, 
    sdl_debug_time_marker *DebugTimeMarkers, 
    sdl_sound_output *SoundOutput, 
    real64 TargetSecondsPerFrame
);

#endif // DEBUG_SYNC_DISPLAY_H