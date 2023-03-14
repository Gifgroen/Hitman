#include "hitman.h"

#include <math.h> // Used for sinf, will be removed in the future  with Intrinsics.
#include <string.h> // Used for memcmp, will be removed in the future  with Intrinsics

void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{
    local_persist real32 tSine;
    s16 ToneVolume = 3000;
#if !HITMAN_INTERNAL
    int ToneHz = GameState->ToneHz > 0 ? GameState->ToneHz : 256;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
#endif
    s16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        s16 SampleValue = (s16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
    
#if HITMAN_INTERNAL
        tSine = 0;
#else
        tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
        if (tSine > 2.0f * Pi32) 
        {
            tSine -= 2.0f * Pi32;
        }
#endif
    }
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, v2 Origin, v2 Destination, u32 TileValue)
{
    Assert(Origin.x < Destination.x);
    Assert(Origin.y < Destination.y);

    int Width = Destination.x - Origin.x;
    int Height = Destination.y - Origin.y;

    v2 Dim = Buffer->Dimensions;
    u32 *Pixels = (u32 *)Buffer->Pixels + Origin.x + (Origin.y * Dim.width);
    for (int Y = 0; Y < Height; ++Y) 
    {
        for (int X = 0; X < Width; ++X)
        {
            *Pixels++ = TileValue;
        }
        Pixels += (Dim.width - Width);
    }
}

global int const XSize = 17;
global int const YSize = 9;

v2 GetTileMapPosition(int TileMap[YSize][XSize], v2 Point, int TileSideInPixels)
{
    v2 Result = V2(
        Point.x / TileSideInPixels,
        Point.y / TileSideInPixels
    );
    return Result;
}

u32 GetTileValue(int TileMap[YSize][XSize], int X, int Y)
{
    u32 Result;
    Result = TileMap[Y][X];
    return Result;
}

bool CheckTileWalkable(int TileMap[YSize][XSize], v2 Point, int TileSideInPixels)
{   
    v2 MapPosition = GetTileMapPosition(TileMap, Point, TileSideInPixels);
    bool Result = Point.x > 0
        && MapPosition.x < XSize
        && Point.y > 0
        && MapPosition.y < YSize
        && GetTileValue(TileMap, MapPosition.x, MapPosition.y) == 0;
    return Result;
}

global int TileMap[YSize][XSize] = 
{
    { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 }
};

struct loaded_texture 
{
    v2 Size;
    void *Pixels;
};

uint32_t reverse_bytes(uint32_t bytes)
{
    uint32_t aux = 0;
    uint8_t byte;
    int i;

    for(i = 0; i < 32; i += 8)
    {
        byte = (bytes >> i) & 0xff;
        aux |= byte << (32 - 8 - i);
    }
    return aux;
}

u8 const SignatureSize = 8;
unsigned char const MagicSignature[SignatureSize] = {137, 80, 78, 71, 13, 10, 26, 10};

#pragma pack(push, 1)
struct png_header 
{
    u32 Width;
    u32 Height;

    u8 BitDepth;	
    u8 ColourType;
    u8 CompressionMethod;
    u8 FilterMethod;
    u8 InterlaceMethod;
};
#pragma pack(pop)

#define ArrayCount(Array) (sizeof(Array)/sizeof(*(Array)))

internal loaded_texture DebugLoadTextureFromPNG(game_memory *GameMemory, char const *Path)
{
    // Read the file contents into a png_stream.
    debug_read_file_result FileResult = GameMemory->DebugReadEntireFile(Path);
    png_stream *Stream = (png_stream *)FileResult.Content;
    
    if (memcmp(Stream->Signature, MagicSignature, SignatureSize) != 0)
    {
        // File has no PNG signature.
        loaded_texture Result = {};
        Result.Size = V2(0, 0);
        Result.Pixels = NULL;
        return Result;
    }

    // Process the chunk Stream
    u8 *Chunks = (u8 *)Stream + SignatureSize;

    u32 Diff = 0;
    u32 ContentSize = FileResult.ContentSize - SignatureSize;

    png_chunk *ExtractedChunks[8] = {};
    u8 Count = 0;
    do 
    {
        Assert(Count < ArrayCount(ExtractedChunks));

        png_chunk *Chunk = (png_chunk *)(Chunks + Diff);       
        u32 Length = reverse_bytes(Chunk->Length);

        ExtractedChunks[Count++] = Chunk;

        Diff += (4 + 4 + Length + 4);
    } while(Diff < ContentSize);

    // convert to Result
    loaded_texture Result = {};

    for (u8 ChunkCount = 0; ChunkCount < Count; ++ChunkCount)
    {
        png_chunk *Chunk = ExtractedChunks[ChunkCount];
        // TODO: process the chunks into a loaded_texture
        printf("Found chunks: type = %.*s, Length = %d\n", 4, Chunk->Type, reverse_bytes(Chunk->Length));
    }

    return Result;
}

extern "C" void GameUpdateAndRender(game_offscreen_buffer *Buffer, game_memory *GameMemory, game_input *Input) 
{
    game_state *GameState = (game_state *)GameMemory->PermanentStorage;

    if (!GameMemory->IsInitialised) 
    {
        // TODO: further setup of GameState.
        GameState->PlayerP = V2(64, 64);

        printf("=== Floor ===\n");
        char const *FloorPath = "../data/floor.png";
        DebugLoadTextureFromPNG(GameMemory, FloorPath);

        printf("=== Spiderman ===\n");

        char const *SpidermanPath = "../data/spiderman.png";
        DebugLoadTextureFromPNG(GameMemory, SpidermanPath);

        GameMemory->IsInitialised = true;
    }
    
    int TileSideInPixels = 64;
    v2 PlayerSize = V2(0.5 * (real32)TileSideInPixels, 0.75 * (real32)TileSideInPixels);

    for (int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLER_COUNT; ++ControllerIndex) 
    {
        game_controller_input *Controller = &(Input->Controllers[ControllerIndex]);

        int HorizontalDirection = 0;
        if (Controller->MoveLeft.IsDown) 
        {
            HorizontalDirection = -1;
        }
        if (Controller->MoveRight.IsDown)
        {
            HorizontalDirection = 1;
        }
        int VerticalDirection = 0;
        if (Controller->MoveUp.IsDown) 
        {
            VerticalDirection = -1;
        }
        if (Controller->MoveDown.IsDown) 
        {
            VerticalDirection = 1;
        }
        v2 NewPlayerP = GameState->PlayerP;
        u8 Speed = 5;
        NewPlayerP.x += Speed * HorizontalDirection;
        NewPlayerP.y += Speed * VerticalDirection;

        int HalfWidth = (0.5f * PlayerSize.width);
        v2 PlayerFeet = V2(
            NewPlayerP.x + HalfWidth,
            NewPlayerP.y + PlayerSize.height
        );
        if (
            CheckTileWalkable(TileMap, V2(PlayerFeet.x - HalfWidth, PlayerFeet.y), TileSideInPixels)
            && CheckTileWalkable(TileMap, PlayerFeet, TileSideInPixels)
            && CheckTileWalkable(TileMap, V2(PlayerFeet.x + HalfWidth, PlayerFeet.y), TileSideInPixels)
        ) 
        {
            GameState->PlayerP = NewPlayerP;
        }

        real32 AverageY = Controller->StickAverageY;
        if (AverageY != 0)
        {
            GameState->ToneHz = 256 + (int)(128.0f * AverageY);
        }
    }

    v2 Origin = V2(0, 0);
    v2 ScreenSize = Buffer->Dimensions;
    DrawRectangle(Buffer, Origin, ScreenSize, 0xFF000FF); // Clear the Buffer to weird magenta

    real32 DrawXOffset = (ScreenSize.width - (XSize * TileSideInPixels)) * 0.5f;
    real32 DrawYOffset = (ScreenSize.height - (YSize * TileSideInPixels)) * 0.5f;

    v2 PlayerP = GameState->PlayerP;
    int HalfWidth = (0.5f * PlayerSize.width);
    v2 Point = V2(PlayerP.x + HalfWidth, PlayerP.y + PlayerSize.height);
    v2 PlayerFeet = GetTileMapPosition(TileMap, Point, TileSideInPixels);

    for (int Y = 0; Y < YSize; ++Y) 
    {
        for (int X = 0; X < XSize; ++X) 
        {
            u32 TileColor = 0;
            if (X == PlayerFeet.x && Y == PlayerFeet.y) 
            {
                TileColor = 0xFFFF0000;
            } 
            else 
            {
                TileColor = GetTileValue(TileMap, X, Y) == 1 ? 0xFFFFFFFF : 0xFF008335;
            }

            int OriginX = DrawXOffset + X * TileSideInPixels;
            int OriginY = DrawYOffset + Y * TileSideInPixels;
            v2 Origin = V2(OriginX, OriginY);
            v2 Destination = V2(OriginX + TileSideInPixels, OriginY + TileSideInPixels);
            DrawRectangle(Buffer, Origin, Destination, TileColor);
        }
    }

    v2 PlayerOrigin = V2(DrawXOffset + GameState->PlayerP.x, DrawYOffset + GameState->PlayerP.y);
    v2 PlayerDest = V2(PlayerOrigin.x + PlayerSize.width, PlayerOrigin.y + PlayerSize.height);
    DrawRectangle(Buffer, PlayerOrigin, PlayerDest, 0xFF0000FF);
}

extern "C" void GameGetSoundSamples(game_memory *GameMemory, game_sound_output_buffer *SoundBuffer) 
{
    game_state *GameState = (game_state *)GameMemory->PermanentStorage;
    GameOutputSound(SoundBuffer, GameState);
}
