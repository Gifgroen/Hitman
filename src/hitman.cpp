#include "hitman.h"

#include <math.h> // Userd for sinf, will be removed in the future.

void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;

    int ToneHz = GameState->ToneHz > 0 ? GameState->ToneHz : 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
    
        tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
        if (tSine > 2.0f * Pi32) 
        {
            tSine -= 2.0f * Pi32;
        }
    }
}

void RenderWeirdGradient(game_offscreen_buffer *Buffer, game_state *State, game_input *Input) 
{
    window_dimensions Dim = Buffer->Dimensions;

    int Pitch = Dim.Width * Buffer->BytesPerPixel;
    
    uint8 *Row = (uint8 *)Buffer->Pixels;
    for(int Y = 0; Y < Dim.Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0; X < Dim.Width; ++X)
        {
            uint8 Red = 0;
            uint8 Blue = X + State->XOffset;
            uint8 Green = Y;
            
            *Pixel++ = ((Red << 16) | (Green << 8)) | Blue;
        }
        Row += Pitch;
    }
}

extern "C" void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_state *GameState, game_sound_output_buffer *SoundBuffer, game_input *Input, int ToneHz) 
{
    for (int i = 0; i < MAX_CONTROLLER_COUNT; ++i) 
    {
        game_controller_input *Controller = &(Input->Controllers[i]);
        
        if (Controller->MoveLeft.IsDown) 
        {
            GameState->XOffset += 10;
        }
        if (Controller->MoveRight.IsDown)
        {
            GameState->XOffset -= 10;
        }
        real32 AverageY = Controller->StickAverageY;
        if (AverageY != 0)
        {
            GameState->ToneHz = 256 + (int)(128.0f * AverageY);
        }
    }

    RenderWeirdGradient(Buffer, GameState, Input);
    GameOutputSound(SoundBuffer, GameState);
}
