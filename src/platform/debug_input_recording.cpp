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

// Platform Event handling
#if HITMAN_INTERNAL // Debug Input handling
internal void DebugHandleKeyEvent(SDL_KeyboardEvent Event, sdl_setup *Setup, debug_input_recording *Recording, game_controller_input *KeyboardController)
{
    SDL_Keycode KeyCode = Event.keysym.sym;

    bool IsDown = (Event.state == SDL_PRESSED);

    if (Event.repeat == 0) 
    {
        u32 mod = Event.keysym.mod;
        if (IsDown && (KeyCode == SDLK_RETURN && (mod & KMOD_CTRL)))
        {
            u32 FullscreenFlag = SDL_WINDOW_FULLSCREEN;
            bool IsFullscreen = SDL_GetWindowFlags(Setup->Window) & FullscreenFlag;
            SDL_SetWindowFullscreen(Setup->Window, IsFullscreen ? 0 : FullscreenFlag);
        }
        else if (IsDown && KeyCode == SDLK_l)
        {   
            switch (Recording->Action)
            {
                case record_action::Idle: 
                {
                    Recording->Action = record_action::Recording;
                } break;
                case record_action::Recording: 
                {
                    Recording->Action = record_action::Playing;
                } break;
                case record_action::Playing: 
                {
                    Recording->Action = record_action::Idle;
                } break;
            }
            if (Recording->Action == Idle) 
            {
                DebugEndRecordInput(Recording);
                DebugEndPlaybackInput(Recording);

                // Note(Karsten): Need a more structured way to detect reset of looped input, so we can reset keyboard.
                for (int ButtonIndex = 0; ButtonIndex < ArrayCount(KeyboardController->Buttons); ++ButtonIndex)
                {
                    game_button_state *Btn = &(KeyboardController->Buttons[ButtonIndex]);
                    Btn->IsDown = false;
                    Btn->HalfTransitionCount = 0;
                }
            }
        }
    }
}
#endif
