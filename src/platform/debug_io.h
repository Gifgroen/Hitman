#ifndef PLATFORM_DEBUG_IO_H
#define PLATFORM_DEBUG_IO_H

#include "../hitman_types.h"

struct debug_read_file_result
{
    u32 ContentSize;
    void *Content;
};

internal void DebugFreeFileMemory(void *Memory);

internal debug_read_file_result DebugReadEntireFile(char const *Filename);

internal bool DebugWriteEntireFile(char const *Filename, char const *Content, u64 Length);

#endif