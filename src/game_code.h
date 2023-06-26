#ifndef GAME_CODE_H
#define GAME_CODE_H

#include "hitman.h"

extern "C" 
{
    typedef void (*GameUpdateAndRender_t)(game_offscreen_buffer*, game_memory *, game_input*);
    typedef void (*GameGetSoundSamples_t)(game_memory *, game_sound_output_buffer*);
}

struct game_code
{
    char const *LibPath;
    void* LibHandle;
    s64 LastWriteTime;

    GameUpdateAndRender_t GameUpdateAndRender;
    GameGetSoundSamples_t GameGetSoundSamples;
};

 
internal s64 GameCodeChanged(game_code *GameCode);

internal int LoadGameCode(game_code *GameCode);

internal void UnloadGameCode(game_code *GameCode);

#endif // GAME_CODE_H