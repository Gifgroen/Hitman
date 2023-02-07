#include "debug_sync_display.h"

internal void SDLDebugDrawVertical(game_offscreen_buffer *Buffer, int Value, int Top, int Bottom, u32 Color)
{
    int Pitch = Buffer->Pitch;
    u8 *Pixel = ((u8 *)Buffer->Pixels + Value * Buffer->BytesPerPixel + Top * Pitch);
    for(int Y = Top; Y < Bottom; ++Y)
    {
        *(u32 *)Pixel = Color;
        Pixel += Pitch;
    }
}

inline void SDLDrawSoundBufferMarker(
    game_offscreen_buffer *Buffer,
    sdl_sound_output *SoundOutput,
    real32 C, 
    int PadX, 
    int Top, 
    int Bottom,
    int Value, 
    u32 Color
) {
    Assert(Value < SoundOutput->SecondaryBufferSize);
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;
    SDLDebugDrawVertical(Buffer, X, Top, Bottom, Color);
}

internal void SDLDebugSyncDisplay(
    game_offscreen_buffer *Buffer, 
    int DebugTimeMarkerCount, 
    sdl_debug_time_marker *DebugTimeMarkers, 
    sdl_sound_output *SoundOutput, 
    real64 TargetSecondsPerFrame
) {
    int PadX = 16;
    int PadY = 16;

    v2 Dimensions = Buffer->Dimensions;
    int Width = Dimensions.width;
    real32 PixelsPerByte = (real32)(Width - (2 * PadX)) / (real32)SoundOutput->SecondaryBufferSize;

    int Top = PadY;
    int Bottom = Dimensions.height - PadY;

    for (int MarkerIndex = 0; MarkerIndex < DebugTimeMarkerCount; ++MarkerIndex)
    {
        sdl_debug_time_marker *Marker = &DebugTimeMarkers[MarkerIndex];
        SDLDrawSoundBufferMarker(Buffer, SoundOutput, PixelsPerByte, PadX, Top, Bottom, Marker->PlayCursor, 0xFFFFFFFF);
        SDLDrawSoundBufferMarker(Buffer, SoundOutput, PixelsPerByte, PadX, Top, Bottom, Marker->WriteCursor, 0xFFFF0000);
    }
}