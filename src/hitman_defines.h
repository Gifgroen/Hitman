
#include <stdint.h>

#if HITMAN_INTERNAL
#define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}
#else 
#define Assert(Expression)
#endif

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

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
