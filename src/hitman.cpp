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

    u_int8_t *Row = (u_int8_t *)Buffer->Pixels;
    for(int Y = 0; Y < Dim.Height; ++Y)
    {
        u_int32_t *Pixel = (u_int32_t *)Row;
        for(int X = 0; X < Dim.Width; ++X)
        {
            u_int8_t Red = 0;
            u_int8_t Blue = X;
            u_int8_t Green = Y;
            
            *Pixel++ = ((Red << 16) | (Green << 8)) | Blue;
        }
        Row += Pitch;
    }
}
