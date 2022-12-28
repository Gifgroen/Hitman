#include "hitman.h"

extern "C" void GameUpdateAndRender(offscreen_buffer *Buffer, game_input *Input) 
{
    window_dimensions Dim = Buffer->Dimensions;

    int Pitch = Dim.Width * Buffer->BytesPerPixel;

    for (int i = 0; i < MAX_CONTROLLER_COUNT; ++i) 
    {
        game_controller_input *Controller = &(Input->Controllers[i]);
        int cHalfTransitionCount = Controller->MoveLeft.HalfTransitionCount;
        bool cIsDown = Controller->MoveLeft.IsDown; 
        printf("[Controller %d][Left Pressed]  HalfTransitionCount = %d, IsDown = %d\n", i, cHalfTransitionCount, cIsDown);
    }
    
    uint8 *Row = (uint8 *)Buffer->Pixels;
    for(int Y = 0; Y < Dim.Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0; X < Dim.Width; ++X)
        {
            uint8 Red = 0;
            uint8 Blue = X;
            uint8 Green = Y;
            
            *Pixel++ = ((Red << 16) | (Green << 8)) | Blue;
        }
        Row += Pitch;
    }
}
