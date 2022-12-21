#ifndef HITMAN_H
#define HITMAN_H

#include <stdio.h>

struct window_dimensions {
    int Width;
    int Height;
};

struct offscreen_buffer {
    window_dimensions Dimensions;
    void *Pixels;
    int BytesPerPixel;
};

#endif
