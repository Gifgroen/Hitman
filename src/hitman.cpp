#include "hitman.h"

#include <math.h> // Userd for sinf, will be removed in the future.

void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{
    local_persist real32 tSine;
    s16 ToneVolume = 3000;
#if !HITMAN_INTERNAL
    int ToneHz = GameState->ToneHz > 0 ? GameState->ToneHz : 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
#endif
    s16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        s16 SampleValue = (s16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
    
#if HITMAN_INTERNAL
        tSine = 0;
#else
        tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
        if (tSine > 2.0f * Pi32) 
        {
            tSine -= 2.0f * Pi32;
        }
#endif
    }
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, v2 Origin, v2 Destination, u32 TileValue)
{
    Assert(Origin.x < Destination.x);
    Assert(Origin.y < Destination.y);

    int Width = Destination.x - Origin.x;
    int Height = Destination.y - Origin.y;
    Assert(Width > 0);
    Assert(Height > 0);

    v2 Dim = Buffer->Dimensions;

    u32 *Pixels = (u32 *)Buffer->Pixels + Origin.x + (Origin.y * Dim.width);
    for (int Y = 0; Y < Height; ++Y) 
    {
        for (int X = 0; X < Width; ++X)
        {
            *Pixels++ = TileValue;
        }
        Pixels += (Dim.width - Width);
    }
}

global int const XSize = 16;
global int const YSize = 9;

u32 GetTileValue(int TileMap[YSize][XSize], int X, int Y)
{
    u32 Result;
    Result = TileMap[Y][X] == 1 ? 0xFFFFFFFF : 0xFF008335;
    return Result;
}

extern "C" void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_memory *GameMemory, game_input *Input, int ToneHz) 
{
    game_state *GameState = (game_state *)GameMemory->PermanentStorage;

    if (!GameMemory->IsInitialised) 
    {
        // TODO: further setup of GameState.
        GameState->PlayerP = V2(64, 64);

        GameMemory->IsInitialised = true;
    }
    
    for (int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLER_COUNT; ++ControllerIndex) 
    {
        game_controller_input *Controller = &(Input->Controllers[ControllerIndex]);

        int HorizontalDirection = 0;
        if (Controller->MoveLeft.IsDown) 
        {
            HorizontalDirection = -1;
        }
        if (Controller->MoveRight.IsDown)
        {
            HorizontalDirection = 1;
        }
        int VerticalDirection = 0;
        if (Controller->MoveUp.IsDown) 
        {
            VerticalDirection = -1;
        }
        if (Controller->MoveDown.IsDown) 
        {
            VerticalDirection = 1;
        }
        u8 Speed = 5;
        v2 NewPlayerP = V2(
            GameState->PlayerP.x + Speed * HorizontalDirection, 
            GameState->PlayerP.y + Speed * VerticalDirection
        );
        GameState->PlayerP = NewPlayerP;

        real32 AverageY = Controller->StickAverageY;
        if (AverageY != 0)
        {
            GameState->ToneHz = 256 + (int)(128.0f * AverageY);
        }
    }

    v2 Origin = V2(0, 0);
    v2 ScreenSize = Buffer->Dimensions;
    DrawRectangle(Buffer, Origin, ScreenSize, 0xFF000FF); // Clear the Buffer to weird magenta

    local_persist int TileMap[YSize][XSize] = 
    {
        { 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
        { 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
        { 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
        { 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
        { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
        { 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
        { 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 }
    };

    int TileWidth = 64;
    int TileHeight = 64;
    for (int Y = 0; Y < YSize; ++Y) 
    {
        for (int X = 0; X < XSize; ++X) 
        {
            u32 TileValue = GetTileValue(TileMap, X, Y);

            int OriginX = X * TileWidth;
            int OriginY = Y * TileHeight;
            v2 Origin = V2(OriginX, OriginY);
            v2 Destination = V2(OriginX + TileWidth, OriginY + TileHeight);
            DrawRectangle(Buffer, Origin, Destination, TileValue);
        }
    }

    v2 PlayerP = GameState->PlayerP;
    v2 PlayerDest = V2(PlayerP.x + 32, PlayerP.y + 50);
    DrawRectangle(Buffer, PlayerP, PlayerDest, 0xFF0000FF);
}

extern "C" void GameGetSoundSamples(game_memory *GameMemory, game_sound_output_buffer *SoundBuffer) 
{
    game_state *GameState = (game_state *)GameMemory->PermanentStorage;
    GameOutputSound(SoundBuffer, GameState);
}
