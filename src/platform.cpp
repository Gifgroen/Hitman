#include <stdio.h>

#include <sys/stat.h>

#include <x86intrin.h>

#include "hitman.h"
#include "platform.h"

#if HITMAN_INTERNAL
#include "platform/debug_io.h"
#include "platform/debug_io.cpp"

#include "platform/debug_sync_display.h"
#include "platform/debug_sync_display.cpp"
#endif

// TODO: this should be wrapped in #if HITMAN_INTERNAL, but structs from .h currently used in None Internal handlers.
#include "platform/debug_input_recording.h"
#include "platform/debug_input_recording.cpp"

#include "platform/os_memory.h"
#include "platform/os_memory.cpp"
#include "platform/os_window.h"
#include "platform/os_window.cpp"

#include "platform/audio.h"
#include "platform/audio.cpp"
#include "platform/input.h"
#include "platform/input.cpp"

#include "game_code.h"
#include "game_code.cpp"

// Enforce Framerate
internal int GetWindowRefreshRate(SDL_Window *Window)
{
    SDL_DisplayMode Mode;
    int DisplayIndex = SDL_GetWindowDisplayIndex(Window);

    int DefaultRefreshRate = 60;
    if (SDL_GetDesktopDisplayMode(DisplayIndex, &Mode) != 0)
    {
        return DefaultRefreshRate;
    }
    if (Mode.refresh_rate == 0)
    {
        return DefaultRefreshRate;
    }
    return Mode.refresh_rate;
}

internal real32 GetSecondsElapsed(u64 OldCounter, u64 CurrentCounter)
{
    return ((real32)(CurrentCounter - OldCounter) / (real32)(SDL_GetPerformanceFrequency()));
}

internal void TryWaitForNextFrame(u64 LastCounter, real64 TargetSecondsPerFrame) 
{
    if (GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
    {
        s32 TimeToSleep = ((TargetSecondsPerFrame - GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter())) * 1000) - 1;
        if (TimeToSleep > 0)
        {
            SDL_Delay(TimeToSleep);
        }

        if (GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) >= TargetSecondsPerFrame) 
        {
            printf("Frame time %02f was more then our target\n", GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()));
        }
        while (GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
        {
            // Waiting...
        }
    }
}

// Close
internal void CloseGame(game_code *GameCode, sdl_setup *Setup, game_memory *GameMemory) 
{
    UnloadGameCode(GameCode);

    if (Setup->WindowTexture) 
    {
        SDL_DestroyTexture(Setup->WindowTexture);
        Setup->WindowTexture = NULL;
    }
    if (Setup->Renderer) 
    {
        SDL_DestroyRenderer(Setup->Renderer);
        Setup->Renderer = NULL;
    }
    if (Setup->Window) 
    {
        SDL_DestroyWindow(Setup->Window);
	    Setup->Window = NULL;
    }
    for (u64 i = 0; i < ArrayCount(ControllerHandles); ++i) 
    {
        if (ControllerHandles[i]) 
        {
            SDL_GameControllerClose(ControllerHandles[i]);
            ControllerHandles[i] = NULL;
        }
    }

    SDL_CloseAudio();

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    bool Result = VirtualFree(GameMemory->PermanentStorage, 0, MEM_RELEASE);
    Assert(Result);
#else
    u64 TotalStorageSize = GameMemory->PermanentStorageSize + GameMemory->TransientStorageSize;
    int Result = munmap(GameMemory->PermanentStorage, TotalStorageSize);
    Assert(Result == 0);
#endif

    SDL_Quit();
}

// Main
int main(int argc, char *argv[]) 
{
#if HITMAN_DEBUG
    printf("Running Hitman in DEBUG mode!\n");
#endif

    game_code GameCode = {};
    GameCode.LibPath = "../build/libhitman.so";
    LoadGameCode(&GameCode);

    // REGION - Platform using SDL
    u32 SubSystemFlags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SubSystemFlags)) 
    {
        printf("Failed initialising subsystems! %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    
#if HITMAN_DEBUG
    u64 PerfCountFrequency = SDL_GetPerformanceFrequency();
#endif

    local_persist sdl_setup SdlSetup = {};

    v2 Dimensions = V2(1280, 1024);
    SdlSetupWindow(&SdlSetup, Dimensions);

    int const GameUpdateHz = 30;
    real64 TargetSecondsPerFrame = 1.0f / (real64)GameUpdateHz;
    
    int DetectedFrameRate = GetWindowRefreshRate(SdlSetup.Window);
    if (DetectedFrameRate != GameUpdateHz) 
    {
        printf("Device capable refresh rate is %d Hz, but Game runs in %d Hz\n", DetectedFrameRate, GameUpdateHz);
    }

    sdl_sound_output SoundOutput = {};
    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.ToneHz = 256;
    SoundOutput.ToneVolume = 3000;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
    SoundOutput.BytesPerSample = sizeof(s16) * 2;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    // TODO: compute variance in the frame time so we can choose what the lowest reasonable value is.
    SoundOutput.SafetyBytes = 0.25 * ((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);

    InitAudio(SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
    s16 *Samples = (s16 *)calloc(SoundOutput.SamplesPerSecond, SoundOutput.BytesPerSample);
    bool SoundIsPlaying = false;

    OpenInputControllers();

    game_offscreen_buffer OffscreenBuffer = {};
    OffscreenBuffer.BytesPerPixel = sizeof(u32);
    // Initial sizing of the game screen.
    UpdateOffscreenBufferDimensions(&SdlSetup, &OffscreenBuffer, Dimensions);

    game_input Input[2] = {};
    game_input *OldInput = &Input[0];
    game_input *NewInput = &Input[1];

    game_memory GameMemory = {};
    GameMemory.PermanentStorageSize = MegaByte(64);
    GameMemory.TransientStorageSize = GigaByte(1);
    GameMemory.IsInitialised = false;
#if HITMAN_INTERNAL
    GameMemory.DebugReadEntireFile = DebugReadEntireFile;
#endif

    debug_input_recording InputRecorder = {};

#if HITMAN_INTERNAL
    void *BaseAddress = (void *)TeraByte(2);
#else
    void *BaseAddress = (void *)(0);
#endif

    u64 TotalStorageSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
    InputRecorder.TotalMemorySize = TotalStorageSize;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalStorageSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else 
    GameMemory.PermanentStorage = mmap(BaseAddress, TotalStorageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#endif    
    GameMemory.TransientStorage = (u8*)(GameMemory.PermanentStorage) + GameMemory.PermanentStorageSize;
    Assert(GameMemory.PermanentStorage);
    Assert(GameMemory.TransientStorage);
    
    game_state *State = (game_state*)GameMemory.PermanentStorage;
    *State = {};
    State->Running = true;
    Assert(State);

#if HITMAN_INTERNAL
    sdl_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {};
    u64 DebugLastPlayCursorIndex = 0;
#endif
    u64 LastCounter = SDL_GetPerformanceCounter(); 

#if HITMAN_DEBUG
    u64 LastCycleCount = _rdtsc();
#endif

    SDL_Event e;

    while(State->Running) 
    {
        game_controller_input *OldKeyboardController = GetControllerForIndex(OldInput, 0);
        game_controller_input *NewKeyboardController = GetControllerForIndex(NewInput, 0);
        *NewKeyboardController = {};
        for(u64 ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
        {
            NewKeyboardController->Buttons[ButtonIndex].IsDown = OldKeyboardController->Buttons[ButtonIndex].IsDown;
            NewKeyboardController->Buttons[ButtonIndex].HalfTransitionCount = OldKeyboardController->Buttons[ButtonIndex].HalfTransitionCount;
        }

        SDLHandleEvents(State, &e, &SdlSetup, &OffscreenBuffer, NewKeyboardController, &InputRecorder);

        HandleControllerEvents(OldInput, NewInput);

#if HITMAN_INTERNAL
        if (InputRecorder.Action == record_action::Recording) 
        {
            DebugRecordInput(&InputRecorder, NewInput, &GameMemory);
        } 
        if (InputRecorder.Action == record_action::Playing)
        {
            DebugEndRecordInput(&InputRecorder);   
            DebugPlaybackInput(&InputRecorder, NewInput, &GameMemory);
        }
#endif

        if (GameCodeChanged(&GameCode) > GameCode.LastWriteTime) 
        {
            printf("GameCode has changed, reloading!\n");
            LoadGameCode(&GameCode);
        }

        game_offscreen_buffer Buffer = {};
        Buffer.Pixels = OffscreenBuffer.Pixels;
        Buffer.Dimensions = OffscreenBuffer.Dimensions;
        Buffer.BytesPerPixel = OffscreenBuffer.BytesPerPixel;
        Buffer.Pitch = OffscreenBuffer.Pitch;

        GameCode.GameUpdateAndRender(&Buffer, &GameMemory, NewInput);

        // REGION: Write Audio to Ringbuffer

        SDL_LockAudio();
        
        sdl_audio_buffer_index AudioBufferIndex = PositionAudioBuffer(&SoundOutput, GameUpdateHz);

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = AudioBufferIndex.BytesToWrite / SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;

        GameCode.GameGetSoundSamples(&GameMemory, &SoundBuffer);

        SDL_UnlockAudio();

        FillSoundBuffer(&SoundOutput, AudioBufferIndex.ByteToLock, AudioBufferIndex.BytesToWrite, &SoundBuffer); 

#if HITMAN_DEBUG // SOUND SYNC DEBUG
        int UnwrappedWriteCursor = AudioRingBuffer.WriteCursor;
        if (UnwrappedWriteCursor < AudioRingBuffer.PlayCursor)
        {
            UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
        }

        int AudioLatencyBytes = UnwrappedWriteCursor - AudioRingBuffer.PlayCursor;
        real32 AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);
        printf(
            "BTL: %d, TC: %d, BTW: %d - PC: %d, WC: %d, DELTA: %d (%fs)\n", 
            AudioBufferIndex.ByteToLock, 
            AudioBufferIndex.TargetCursor,
            AudioBufferIndex.BytesToWrite, 
            AudioRingBuffer.PlayCursor,
            AudioRingBuffer.WriteCursor,
            AudioLatencyBytes,
            AudioLatencySeconds
        );
#endif
        if(!SoundIsPlaying)
        {
            SDL_PauseAudio(0);
            SoundIsPlaying = true;
        }
        // END REGION: Write Audio to Ringbuffer

        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
        
        *NewInput = {};
        
        TryWaitForNextFrame(LastCounter, TargetSecondsPerFrame);

#if HITMAN_DEBUG
        // Get this before UpdateWindow() so that we don't keep missing VBlanks.
        u64 EndCounter = SDL_GetPerformanceCounter();
#endif

#if HITMAN_INTERNAL
        SDLDebugSyncDisplay(&OffscreenBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, &SoundOutput, TargetSecondsPerFrame);
#endif

        UpdateWindow(SdlSetup.WindowTexture, &OffscreenBuffer,  SdlSetup.Renderer);

#if HITMAN_INTERNAL
    {
        sdl_debug_time_marker *marker = &DebugTimeMarkers[DebugLastPlayCursorIndex++];
        if (DebugLastPlayCursorIndex >= ArrayCount(DebugTimeMarkers))
        {
            DebugLastPlayCursorIndex = 0;
        }
        marker->PlayCursor = AudioRingBuffer.PlayCursor;
        marker->WriteCursor = AudioRingBuffer.WriteCursor;
    }
#endif

#if HITMAN_DEBUG 
        // Calculate frame timings.
        u64 EndCycleCount = _rdtsc();
        s64 CounterElapsed = EndCounter - LastCounter;
        u64 CyclesElapsed = EndCycleCount - LastCycleCount;

        real64 MSPerFrame = (((1000.0f * (real64)CounterElapsed) / (real64)PerfCountFrequency));
        real64 FPS = (real64)PerfCountFrequency / (real64)CounterElapsed;
        real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

        printf("%.02fms/f, %.02f/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);

        LastCycleCount = EndCycleCount;
        LastCounter = EndCounter;
#endif
    }

    // ENDREGION - Platform using SDL

    CloseGame(&GameCode, &SdlSetup, &GameMemory);
 
    return EXIT_SUCCESS;
}
