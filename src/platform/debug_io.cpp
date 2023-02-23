#include "debug_io.h"

internal void DebugFreeFileMemory(void *Memory)
{
    if (Memory)
    {
        free(Memory);
    }
}

internal debug_read_file_result DebugReadEntireFile(char const *Filename) 
{
    debug_read_file_result Result = {};
    struct stat Stat;
    if (stat(Filename, &Stat) == 0) 
    {
        FILE *File = fopen(Filename, "r");
        if (File != NULL) 
        {
            s64 Size = Stat.st_size;
            Result.ContentSize = Size;
            Result.Content = malloc(Size);
            if(Result.Content)
            {
                fread(Result.Content, Size, 1, File);
                fclose(File);
            } 
            else 
            {
                DebugFreeFileMemory(Result.Content);
            }
        }
    }
    return Result;
}

internal bool DebugWriteEntireFile(char const *Filename, char const *Content, u64 Length) 
{
    FILE * File = fopen(Filename, "w");
    if (File == NULL) 
    {
        return false;
    }

    u64 Written = fwrite(Content, 1, Length, File);
    fclose(File);
    return Length == Written;
}
