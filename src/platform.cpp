#include <stdio.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #include <memoryapi.h>
#else 
    #include <sys/types.h>
    #include <sys/mman.h>
#endif

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

#include "game_code.h"
#include "game_code.cpp"

global SDL_GameController *ControllerHandles[MAX_CONTROLLER_COUNT];
global sdl_audio_ring_buffer AudioRingBuffer;

global bool Running = true;

// BackBuffer alloc
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
internal void Alloc(game_offscreen_buffer *Buffer) 
{
    v2 Dim = Buffer->Dimensions;
    int Size = Dim.height * Buffer->Pitch;
    Buffer->Pixels = VirtualAlloc(NULL, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}
#else 
internal void Alloc(game_offscreen_buffer *Buffer) 
{
    v2 Dim = Buffer->Dimensions;
    int Size = Dim.height * Buffer->Pitch;
    Buffer->Pixels = mmap(0, Size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
internal void Dealloc(game_offscreen_buffer *Buffer) 
{
    bool Result = VirtualFree(Buffer->Pixels, 0, MEM_RELEASE);
    Assert(Result)
}
#else 
internal void Dealloc(game_offscreen_buffer *Buffer) 
{
    v2 Dim = Buffer->Dimensions;
    int Result = munmap(Buffer->Pixels, Dim.height * Buffer->Pitch);
    Assert(Result == 0);
}
#endif

// SDL Window management
internal void SdlSetupWindow(sdl_setup *Setup, v2 Dimensions) 
{
    u32 WindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    Setup->Window = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Dimensions.width, Dimensions.height, WindowFlags);
    Assert(Setup->Window);

    Setup->Renderer = SDL_CreateRenderer(Setup->Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    Assert(Setup->Renderer);
}

internal void UpdateOffscreenBufferDimensions(sdl_setup *Setup, game_offscreen_buffer *Buffer, v2 NewDimensions)
{
    if (Setup->WindowTexture) 
    {
        SDL_DestroyTexture(Setup->WindowTexture);
        Setup->WindowTexture = NULL;
    }

    if (Buffer->Pixels)
    {   
        Dealloc(Buffer);
    }

    int Width = NewDimensions.width;
    int Height = NewDimensions.height; 
    Setup->WindowTexture = SDL_CreateTexture(Setup->Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Width, Height);

    Buffer->Dimensions = V2(Width, Height);
    Buffer->Pitch = Width * Buffer->BytesPerPixel;

    Alloc(Buffer);
}

internal v2 GetWindowDimensions(SDL_Window *Window) 
{
    v2 Result = {};
    SDL_GetWindowSize(Window, &Result.width, &Result.height);
    return Result;
}

internal void UpdateWindow(SDL_Texture *WindowTexture, game_offscreen_buffer *Buffer, SDL_Renderer *Renderer) 
{
    SDL_UpdateTexture(WindowTexture, 0, Buffer->Pixels, Buffer->Pitch);
    SDL_RenderCopy(Renderer, WindowTexture, 0, 0);
    SDL_RenderPresent(Renderer);
}

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

// Input
internal void OpenInputControllers() 
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        int ControllerIndex = i + 1;
        if (ControllerIndex == ArrayCount(ControllerHandles)) 
        {
            break;
        }
        if (!SDL_IsGameController(i)) 
        { 
            continue; 
        }

        ControllerHandles[ControllerIndex] = SDL_GameControllerOpen(i);
    }
}

internal game_controller_input *GetControllerForIndex(game_input *Input, int Index) 
{
    game_controller_input *Result = &(Input->Controllers[Index]);
    return Result;
}

internal void ProcessKeyInput(game_button_state *NewState, bool IsDown)
{
    Assert(NewState->IsDown != IsDown);
    NewState->IsDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal void SDLProcessGameControllerButton(game_button_state *OldState, game_button_state *NewState, bool Value)
{
    NewState->IsDown = Value;
    NewState->HalfTransitionCount += ((NewState->IsDown == OldState->IsDown) ? 0 : 1);
}

internal real32 SDLProcessGameControllerAxisValue(s16 Value, s16 DeadZoneThreshold)
{
    real32 Result = 0;

    if(Value < -DeadZoneThreshold)
    {
        Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    }
    else if(Value > DeadZoneThreshold)
    {
        Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }

    return(Result);
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

internal void HandleWindowEvent(SDL_WindowEvent e, sdl_setup *Setup, game_offscreen_buffer *Buffer) 
{
    switch(e.event)
    {
        case SDL_WINDOWEVENT_SIZE_CHANGED: 
        {
            int NewWidth = e.data1;
            int NewHeight = e.data2;
            v2 NewDimensions = V2(NewWidth, NewHeight);
            UpdateOffscreenBufferDimensions(Setup, Buffer, NewDimensions);
        } break;

        case SDL_WINDOWEVENT_EXPOSED: 
        {
            v2 KnownDimensions = GetWindowDimensions(Setup->Window);
            UpdateOffscreenBufferDimensions(Setup, Buffer, KnownDimensions);
        } break;
    }
}    

internal void HandleKeyEvent(SDL_KeyboardEvent key, game_controller_input *KeyboardController)
{
    SDL_Keycode KeyCode = key.keysym.sym;
    bool IsDown = (key.state == SDL_PRESSED);

    // Investigate if WasDown is necessary for repeats.
    bool WasDown = false;
    if (key.state == SDL_RELEASED)
    {
        WasDown = true;
    }
    else if (key.repeat != 0)
    {
        WasDown = true;
    }

    if (key.repeat == 0)
    {
        if(KeyCode == SDLK_w)
        {
            ProcessKeyInput(&(KeyboardController->MoveUp), IsDown);
        }
        else if(KeyCode == SDLK_d)
        {
            ProcessKeyInput(&(KeyboardController->MoveRight), IsDown);
        }
        else if(KeyCode == SDLK_s)
        {
            ProcessKeyInput(&(KeyboardController->MoveDown), IsDown);
        }
        else if(KeyCode == SDLK_a)
        {
            ProcessKeyInput(&(KeyboardController->MoveLeft), IsDown);
        }
        else if(KeyCode == SDLK_UP)
        {
            ProcessKeyInput(&(KeyboardController->MoveUp), IsDown);
        }
        else if(KeyCode == SDLK_RIGHT)
        {
            ProcessKeyInput(&(KeyboardController->MoveRight), IsDown);
        }
        else if(KeyCode == SDLK_DOWN)
        {
            ProcessKeyInput(&(KeyboardController->MoveDown), IsDown);
        }
        else if(KeyCode == SDLK_LEFT)
        {
            ProcessKeyInput(&(KeyboardController->MoveLeft), IsDown);
        }
        else if(KeyCode == SDLK_ESCAPE)
        {
            printf("ESCAPE: ");
            if(IsDown)
            {
                printf("IsDown ");
            }
            if(WasDown)
            {
                printf("WasDown");
                Running= false;
            }
            printf("\n");
        }
    }
}

internal void SDLHandleEvents(SDL_Event *e, sdl_setup *SdlSetup, game_offscreen_buffer *OffscreenBuffer, game_controller_input *NewKeyboardController, debug_input_recording *InputRecording) 
{
    while(SDL_PollEvent(e) != 0)
    {
        switch (e->type) 
        {
            case SDL_WINDOWEVENT: 
            {
                HandleWindowEvent(e->window, SdlSetup, OffscreenBuffer);
            } break;

            case SDL_QUIT:
            {
                Running = false;
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: 
            {
                #if HITMAN_INTERNAL
                DebugHandleKeyEvent(e->key, SdlSetup, InputRecording, NewKeyboardController);
                #endif
                HandleKeyEvent(e->key, NewKeyboardController);
            } break;
        }
    }

}

internal void HandleControllerEvents(game_input *OldInput, game_input *NewInput) 
{
    for (int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLER_COUNT; ++ControllerIndex)
    {
        SDL_GameController *Controller = ControllerHandles[ControllerIndex];
        if(Controller != 0 && SDL_GameControllerGetAttached(Controller))
        {
            game_controller_input *OldController = GetControllerForIndex(OldInput, ControllerIndex);
            game_controller_input *NewController = GetControllerForIndex(NewInput, ControllerIndex);

            // Shoulder buttons/triggers
            SDLProcessGameControllerButton(&(OldController->LeftShoulder),
                    &(NewController->LeftShoulder),
                    SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_LEFTSHOULDER));

            SDLProcessGameControllerButton(&(OldController->RightShoulder),
                    &(NewController->RightShoulder),
                    SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));


            // Button(s) for A, B, X and Y
            SDLProcessGameControllerButton(&(OldController->ActionUp), &(NewController->ActionUp), SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_Y));
            SDLProcessGameControllerButton(&(OldController->ActionRight), &(NewController->ActionRight), SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_B));
            SDLProcessGameControllerButton(&(OldController->ActionDown), &(NewController->ActionDown), SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_A));
            SDLProcessGameControllerButton(&(OldController->ActionLeft), &(NewController->ActionLeft), SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_X));

            // Sticks
            NewController->StickAverageX = SDLProcessGameControllerAxisValue(SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTX), 1);
            NewController->StickAverageY = -SDLProcessGameControllerAxisValue(SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTY), 1);
            
            if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f))
            {
                NewController->IsAnalog = true;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_UP))
            {
                NewController->StickAverageY = -1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            {
                NewController->StickAverageX = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            {
                NewController->StickAverageY = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            {
                NewController->StickAverageX = -1.0f;
                NewController->IsAnalog = false;
            }

            // emulated Stick average for D-pad movement usage
            real32 Threshold = 0.5f;
            SDLProcessGameControllerButton(&(OldController->MoveUp), &(NewController->MoveUp), NewController->StickAverageY > Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveRight), &(NewController->MoveRight), NewController->StickAverageX > Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveDown), &(NewController->MoveDown), NewController->StickAverageY < -Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveLeft), &(NewController->MoveLeft), NewController->StickAverageX < -Threshold);
        }
    }
}

// SDL Audio
internal void SDLAudioCallback(void *UserData, u8 *AudioData, int Length)
{
    sdl_audio_ring_buffer *RingBuffer = (sdl_audio_ring_buffer *)UserData;

    int Region1Size = Length;
    int Region2Size = 0;
    if (RingBuffer->PlayCursor + Length > RingBuffer->Size)
    {
        Region1Size = RingBuffer->Size - RingBuffer->PlayCursor;
        Region2Size = Length - Region1Size;
    }
    memcpy(AudioData, (u8*)(RingBuffer->Data) + RingBuffer->PlayCursor, Region1Size);
    memcpy(&AudioData[Region1Size], RingBuffer->Data, Region2Size);
    RingBuffer->PlayCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
    RingBuffer->WriteCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
}

internal void InitAudio(s32 SamplesPerSecond, s32 BufferSize)
{
    SDL_AudioSpec AudioSettings = {0};

    AudioSettings.freq = SamplesPerSecond;
    AudioSettings.format = AUDIO_S16LSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &SDLAudioCallback;
    AudioSettings.userdata = &AudioRingBuffer;

    AudioRingBuffer.Size = BufferSize;
    AudioRingBuffer.Data = malloc(BufferSize);
    AudioRingBuffer.PlayCursor = AudioRingBuffer.WriteCursor = 0;

    SDL_OpenAudio(&AudioSettings, 0);

    printf("Initialised an Audio device at frequency %d Hz, %d Channels, buffer size %d\n", AudioSettings.freq, AudioSettings.channels, AudioSettings.samples);

    if (AudioSettings.format != AUDIO_S16LSB)
    {
        printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
        SDL_CloseAudio();
    }
}

internal sdl_audio_buffer_index  PositionAudioBuffer(sdl_sound_output *SoundOutput, int GameUpdateHz)
{
    // TODO: Check if we maybe need to check if soundIsValid and wrap if it is not Valid.

    int ByteToLock = ((SoundOutput->RunningSampleIndex * SoundOutput->BytesPerSample) % SoundOutput->SecondaryBufferSize);

    int ExpectedSoundBytesPerFrame = (SoundOutput->SamplesPerSecond * SoundOutput->BytesPerSample) / GameUpdateHz;
    int ExpectedFrameBoundaryByte = AudioRingBuffer.PlayCursor + ExpectedSoundBytesPerFrame;

    int SafeWriteCursor = AudioRingBuffer.WriteCursor;
    if (SafeWriteCursor < AudioRingBuffer.PlayCursor)
    {
        SafeWriteCursor += SoundOutput->SecondaryBufferSize;
    }
    Assert(SafeWriteCursor >= AudioRingBuffer.PlayCursor)
    SafeWriteCursor += SoundOutput->SafetyBytes;

    bool AudioCardIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;
    int TargetCursor = 0;
    if (AudioCardIsLowLatency) 
    {
        TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;                    
    }
    else 
    {
        TargetCursor = AudioRingBuffer.WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput->SafetyBytes;
    }
    TargetCursor = (TargetCursor % SoundOutput->SecondaryBufferSize);

    int BytesToWrite = 0;
    if(ByteToLock > TargetCursor)
    {
        BytesToWrite = (SoundOutput->SecondaryBufferSize - ByteToLock);
        BytesToWrite += TargetCursor;
    }
    else
    {
        BytesToWrite = TargetCursor - ByteToLock;
    }

    sdl_audio_buffer_index Result = {};
    Result.ByteToLock = ByteToLock;
    Result.TargetCursor = TargetCursor;
    Result.BytesToWrite = BytesToWrite;
    return Result;
}

internal void FillSoundBuffer(sdl_sound_output *SoundOutput, int ByteToLock, int BytesToWrite, game_sound_output_buffer *SoundBuffer)
{
    s16 *Samples = SoundBuffer->Samples;

    void *Region1 = (u8 *)AudioRingBuffer.Data + ByteToLock;
    int Region1Size = BytesToWrite;
    if (Region1Size + ByteToLock > SoundOutput->SecondaryBufferSize)
    {
        Region1Size = SoundOutput->SecondaryBufferSize - ByteToLock;
    }
    void *Region2 = AudioRingBuffer.Data;
    int Region2Size = BytesToWrite - Region1Size;
    int Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    s16 *SampleOut = (s16 *)Region1;
    for(int SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
        *SampleOut++ = *Samples++;
        *SampleOut++ = *Samples++;

        ++SoundOutput->RunningSampleIndex;
    }

    int Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    SampleOut = (s16 *)Region2;
    for(int SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
        *SampleOut++ = *Samples++;
        *SampleOut++ = *Samples++;

        ++SoundOutput->RunningSampleIndex;
    }
}

// Close
internal void CloseGame(game_code *GameCode, sdl_setup *Setup, game_memory *GameMemory) 
{
    if (GameCode->LibHandle) 
    {
        dlclose(GameCode->LibHandle);
        GameCode->LibHandle = NULL;
    }

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

    while(Running) 
    {
        game_controller_input *OldKeyboardController = GetControllerForIndex(OldInput, 0);
        game_controller_input *NewKeyboardController = GetControllerForIndex(NewInput, 0);
        *NewKeyboardController = {};
        for(u64 ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
        {
            NewKeyboardController->Buttons[ButtonIndex].IsDown = OldKeyboardController->Buttons[ButtonIndex].IsDown;
            NewKeyboardController->Buttons[ButtonIndex].HalfTransitionCount = OldKeyboardController->Buttons[ButtonIndex].HalfTransitionCount;
        }

        SDLHandleEvents(&e, &SdlSetup, &OffscreenBuffer, NewKeyboardController, &InputRecorder);

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
