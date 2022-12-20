#ifndef HITMAN_H
#define HITMAN_H

#include <stdio.h>

struct buffer_dimensions {
    int Width;
    int Height;
};

struct back_buffer {
    buffer_dimensions Dimensions;
    void *Pixels;
    int BytesPerPixel;
};

#endif
