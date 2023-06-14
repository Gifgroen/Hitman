#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL2/SDL.h>

#include "types.h"

#define ArrayCount(Array) (sizeof(Array)/sizeof(*(Array)))

struct sdl_debug_time_marker 
{
    int PlayCursor;
    int WriteCursor;
};

#endif
