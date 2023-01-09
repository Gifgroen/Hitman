#ifndef HITMAN_H
#define HITMAN_H

#include <stdio.h>

#include "hitman_defines.h"

#define MAX_CONTROLLER_COUNT 5

struct window_dimensions 
{
    int Width;
    int Height;
};

struct game_offscreen_buffer 
{
    window_dimensions Dimensions;
    int BytesPerPixel;
    int Pitch;

    void *Pixels;
};

struct game_state 
{
    int XOffset;
    int YOffset;

    int ToneHz;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

struct game_memory 
{
    uint64 PermanentStorageSize;
    void *PermanentStorage;
    
    uint64 TransientStorageSize;
    void *TransientStorage;
};

struct game_button_state 
{
    bool IsDown;
    int HalfTransitionCount;
};

struct game_controller_input 
{
    bool IsConnected;
    bool IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    union 
    {
        game_button_state Buttons[10];
        struct  
        {
            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state MoveUp;
            game_button_state MoveRight;
            game_button_state MoveDown;
            game_button_state MoveLeft;

            game_button_state ActionUp;
            game_button_state ActionRight;
            game_button_state ActionDown;
            game_button_state ActionLeft;
        };
    };
};

struct game_input 
{
    /**
     * List of Keyboard and four controller states. 
     * - Index 0 is the Keyboard
     * - Index 1..3 are controllers
     */
    game_controller_input Controllers[MAX_CONTROLLER_COUNT];
};

#endif
