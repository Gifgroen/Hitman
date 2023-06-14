#include "os_memory.h"

// BackBuffer alloc
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

#include <memoryapi.h>

internal void Alloc(game_offscreen_buffer *Buffer) 
{
    v2 Dim = Buffer->Dimensions;
    int Size = Dim.height * Buffer->Pitch;
    Buffer->Pixels = VirtualAlloc(NULL, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

internal void Dealloc(game_offscreen_buffer *Buffer) 
{
    bool Result = VirtualFree(Buffer->Pixels, 0, MEM_RELEASE);
    Assert(Result)
}

#else 

#include <sys/types.h>
#include <sys/mman.h>

internal void Alloc(game_offscreen_buffer *Buffer) 
{
    v2 Dim = Buffer->Dimensions;
    int Size = Dim.height * Buffer->Pitch;
    Buffer->Pixels = mmap(0, Size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

internal void Dealloc(game_offscreen_buffer *Buffer) 
{
    v2 Dim = Buffer->Dimensions;
    int Result = munmap(Buffer->Pixels, Dim.height * Buffer->Pitch);
    Assert(Result == 0);
}
#endif