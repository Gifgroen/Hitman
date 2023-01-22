#include "hitman.h"

#include <math.h> // Userd for sinf, will be removed in the future.

void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
#if HITMAN_INTERNAL
    int WavePeriod = 1; 
#else
    int ToneHz = GameState->ToneHz > 0 ? GameState->ToneHz : 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
#endif
    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
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

internal void DrawRectangle(game_offscreen_buffer *Buffer, int originX, int32 originY, int32 destinationX, int destinationY, uint32 TileValue)
{
    Assert(originX < destinationX);
    Assert(originY < destinationY);

    window_dimensions Dim = Buffer->Dimensions;

    int Width = destinationX - originX;
    int Height = destinationY - originY;
    Assert(Width > 0);
    Assert(Height > 0);

    uint32 *Pixels = (uint32 *)Buffer->Pixels + originX + (originY * Dim.Width);
    
    for (int Y = 0; Y < Height; ++Y) 
    {
        for (int X = 0; X < Width; ++X)
        {
            *Pixels++ = TileValue;
        }
        Pixels += (Dim.Width - Width);
    }
}

extern "C" void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_memory *GameMemory, game_input *Input, int ToneHz) 
{
    int Speed = 5;
    game_state *GameState = (game_state *)GameMemory->PermanentStorage;
    for (int i = 0; i < MAX_CONTROLLER_COUNT; ++i) 
    {
        game_controller_input *Controller = &(Input->Controllers[i]);
        
        if (Controller->MoveLeft.IsDown) 
        {
            GameState->PlayerX -= 1 * Speed;
        }
        if (Controller->MoveRight.IsDown)
        {
            GameState->PlayerX += 1 * Speed;
        }
        if (Controller->MoveUp.IsDown) 
        {
            GameState->PlayerY -= 1 * Speed;
        }
        if (Controller->MoveDown.IsDown) 
        {
            GameState->PlayerY += 1 * Speed;
        }
        real32 AverageY = Controller->StickAverageY;
        if (AverageY != 0)
        {
            GameState->ToneHz = 256 + (int)(128.0f * AverageY);
        }
    }

    window_dimensions Dim = Buffer->Dimensions;
    DrawRectangle(Buffer, 0, 0, Dim.Width, Dim.Height, 0xFF000FF); // Clear the Buffer to weird magenta

    int const XSize = 16;
    int const YSize = 9;

    local_persist int TileMap [YSize][XSize] = {
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
            uint32 TileValue = TileMap[Y][X] == 1 ? 0xFFFFFFFF : 0xFF008335;

            int OriginX = X * TileWidth;
            int OriginY = Y * TileHeight;
            DrawRectangle(Buffer, OriginX, OriginY, OriginX + TileWidth, OriginY + TileHeight, TileValue);
        }
    }

    int PlayerX = GameState->PlayerX;
    int PlayerY = GameState->PlayerY;
    printf("player: (%d, %d)\n", PlayerX, PlayerY);
    DrawRectangle(Buffer, PlayerX, PlayerY, PlayerX + 32, PlayerY + 50, 0xFF0000FF);
}

extern "C" void GameGetSoundSamples(game_memory *GameMemory, game_sound_output_buffer *SoundBuffer) 
{
    game_state *GameState = (game_state *)GameMemory->PermanentStorage;
    GameOutputSound(SoundBuffer, GameState);
}
