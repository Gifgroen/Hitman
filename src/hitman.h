#ifndef HITMAN_H
#define HITMAN_H

#include "types.h"
#include "platform/debug_io.h"

#define MAX_CONTROLLER_COUNT 5

struct game_offscreen_buffer 
{
    v2 Dimensions;
    int BytesPerPixel;
    int Pitch;

    void *Pixels;
};

struct game_state 
{
    v2 PlayerP;

    int ToneHz;

    bool Running;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    s16 *Samples;
};

typedef debug_read_file_result (* DebugReadEntireFile_t)(char const *Filename);

#pragma pack(push, 1)
struct png_chunk 
{
    u32 Length;
    char Type[4];
    void *Data;
    /* 
    According to PNG spec: https://www.w3.org/TR/2003/REC-PNG-20031110/#11Chunks

    - 4 byte (unsigned), chunk LENGTH
    - 4 byte, chunk TYPE: each byte corresponds to:
        - 65-90, [A-Z]
        - 97-122, [a-z]
    - chunk LENGTH bytes of data
    - 4 byte CRC
    */
};
struct png_stream
{
    u8 Signature[8];
    void *Chunks;
};
#pragma pack(pop)

struct game_memory 
{
    u64 PermanentStorageSize;
    void *PermanentStorage;
    
    u64 TransientStorageSize;
    void *TransientStorage;

    bool IsInitialised;

    DebugReadEntireFile_t DebugReadEntireFile;
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
