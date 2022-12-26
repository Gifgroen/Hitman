#ifndef HITMAN_H
#define HITMAN_H

#include <stdio.h>

#define MAX_CONTROLLER_COUNT 5

struct window_dimensions 
{
    int Width;
    int Height;
};

struct offscreen_buffer 
{
    window_dimensions Dimensions;
    void *Pixels;
    int BytesPerPixel;
};

struct game_button_state 
{
    bool IsDown;
    int HalfTransitionCount;
};

struct game_controller_input 
{
    union 
    {
        game_button_state Buttons[4];
        struct  
        {
            game_button_state MoveUp;
            game_button_state MoveRight;
            game_button_state MoveDown;
            game_button_state MoveLeft;
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
