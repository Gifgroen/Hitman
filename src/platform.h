#ifndef PLATFORM_H
#define PLATFORM_H

#include "hitman_defines.h"

#define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}

#define ArrayCount(Array) (sizeof(Array)/sizeof(*(Array)))

struct SDL_setup {
    SDL_Window *Window;
    SDL_Renderer *Renderer;
    SDL_Texture *WindowTexture = NULL;
};

typedef void (*GameUpdateAndRender_t)(offscreen_buffer*, game_input*);

struct game_code
{
    char const *LibPath;
    void* LibHandle;
    int64 LastWriteTime;

    GameUpdateAndRender_t GameUpdateAndRender;
};

#endif
