#ifndef PLATFORM_H
#define PLATFORM_H

#define global static
#define internal static
#define local_persist static

#define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}

#define ArrayCount(Array) (sizeof(Array)/sizeof(*(Array)))

typedef void (*GameUpdateAndRender_t)(offscreen_buffer*, game_input*);

struct game_code
{
    char const *LibPath;
    void* LibHandle;
    int64_t LastWriteTime;

    GameUpdateAndRender_t GameUpdateAndRender;
};


#endif