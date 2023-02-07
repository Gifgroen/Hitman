#include "debug_input_recording.h"

internal void DebugBeginRecordInput(debug_input_recording *InputRecorder, game_memory *GameMemory) 
{
    // Setup Input recording, 
    if (InputRecorder->RecordHandle == NULL)
    {
        InputRecorder->RecordHandle = fopen("../data/input.hmi", "w");
        
        u64 TotalMemorySize = InputRecorder->TotalMemorySize;
        u32 BytesToWrite = (u32)InputRecorder->TotalMemorySize;
        Assert(TotalMemorySize == BytesToWrite); // This can't be more then 4Gb, because then we cannot write it to a file at once.
        u64 Written = fwrite(GameMemory->PermanentStorage, 1, TotalMemorySize, InputRecorder->RecordHandle);
        Assert(BytesToWrite == Written);
    }
}

internal void DebugRecordInput(debug_input_recording *InputRecorder, game_input *NewInput, game_memory *GameMemory)
{
    DebugBeginRecordInput(InputRecorder, GameMemory);

    u64 InputSize = sizeof(*NewInput);
    u64 Written = fwrite(NewInput, 1, InputSize, InputRecorder->RecordHandle);
    Assert(InputSize == Written);
}

internal void DebugEndRecordInput(debug_input_recording *InputRecorder)
{
    if (InputRecorder->RecordHandle != NULL)
    {
        fclose(InputRecorder->RecordHandle);
        InputRecorder->RecordHandle = NULL;
    }
}

internal void DebugBeginPlaybackInput(debug_input_recording *InputRecorder, game_memory *GameMemory) 
{
    if (InputRecorder->PlaybackHandle == NULL)
    {
        InputRecorder->PlaybackHandle = fopen("../data/input.hmi", "r");
        u32 BytesToRead = (u32)InputRecorder->TotalMemorySize;
        Assert(InputRecorder->TotalMemorySize == BytesToRead); // This can't be more then 4Gb on Windows with Live loop, because we cannot write it to a file at once.
        fread(GameMemory->PermanentStorage, BytesToRead, 1, InputRecorder->PlaybackHandle);
    }
}

internal void DebugPlaybackInput(debug_input_recording *InputRecorder, game_input *NewInput, game_memory *GameMemory)
{
    DebugBeginPlaybackInput(InputRecorder, GameMemory);

    u64 InputSize = sizeof(game_input);
    u64 Read = fread(NewInput, 1, InputSize, InputRecorder->PlaybackHandle);
    
    if (Read == 0)
    {
        InputRecorder->PlaybackHandle = NULL;
    }
}

internal void DebugEndPlaybackInput(debug_input_recording *InputRecorder)
{
    if (InputRecorder->PlaybackHandle != NULL)
    {
        fclose(InputRecorder->PlaybackHandle);
        InputRecorder->PlaybackHandle = NULL;
    }
}