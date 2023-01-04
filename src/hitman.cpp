#include "hitman.h"

void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
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

    for (int i = 0; i < MAX_CONTROLLER_COUNT; ++i) 
    {
        game_controller_input *Controller = &(Input->Controllers[i]);
        
        if (Controller->MoveLeft.IsDown) 
        {
            State->XOffset += 10;
        }
        if (Controller->MoveRight.IsDown)
        {
            State->XOffset -= 10;
        }
        
    }
    
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

extern "C" void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_state *State, game_sound_output_buffer *SoundBuffer, game_input *Input, int ToneHz) 
{
    RenderWeirdGradient(Buffer, State, Input);
    GameOutputSound(SoundBuffer, ToneHz);
}
