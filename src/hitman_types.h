#ifndef HITMAN_TYPES_H
#define HITMAN_TYPES_H

#include <stdint.h>

#if HITMAN_INTERNAL
#define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}
#else 
#define Assert(Expression)
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float real32;
typedef double real64;

#define Pi32 3.14159265358979f

#define global static
#define internal static
#define local_persist static

#define KiloByte(Value) ((Value) * 1024LL)
#define MegaByte(Value) (KiloByte(Value) * 1024LL)
#define GigaByte(Value) (MegaByte(Value) * 1024LL)
#define TeraByte(Value) (GigaByte(Value) * 1024LL)

union v2 
{
    struct 
    {
        int x, y;
    };
    struct 
    {
        int width, height;
    };
};

v2 V2(int X, int Y) 
{
    v2 Result = {};
    Result.x = X;
    Result.y = Y;
    return Result;
}

#endif
