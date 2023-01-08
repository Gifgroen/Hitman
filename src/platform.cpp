#include <dlfcn.h>

#include <stdio.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #include <memoryapi.h>
#else 
    #include <sys/types.h>
    #include <sys/mman.h>
#endif

#include <sys/stat.h>

#include <x86intrin.h>

#include <SDL2/SDL.h>

#include "hitman.h"
#include "platform.h"

global SDL_GameController *ControllerHandles[MAX_CONTROLLER_COUNT];
global sdl_audio_ring_buffer AudioRingBuffer;

global bool Running = true;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    internal void Alloc(game_offscreen_buffer *Buffer) 
    {
        window_dimensions Dim = Buffer->Dimensions;
        int Size = Dim.Width * Dim.Height * Buffer->BytesPerPixel;
        Buffer->Pixels = VirtualAlloc(NULL, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    }
#else 
    internal void Alloc(game_offscreen_buffer *Buffer) 
    {
        window_dimensions Dim = Buffer->Dimensions;
        int Size = Dim.Width * Dim.Height * Buffer->BytesPerPixel;
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
    window_dimensions Dim = Buffer->Dimensions;
    int Result = munmap(Buffer->Pixels, Dim.Width * Dim.Height * Buffer->BytesPerPixel);
    Assert(Result == 0);
}
#endif

internal void UpdateOffscreenBufferDimensions(sdl_setup *Setup, game_offscreen_buffer *Buffer, window_dimensions NewDimensions)
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

    int Width = NewDimensions.Width;
    int Height = NewDimensions.Height; 
    Setup->WindowTexture = SDL_CreateTexture(Setup->Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Width, Height);

    Buffer->Dimensions = { Width, Height };
    Alloc(Buffer);
}

internal window_dimensions GetWindowDimensions(SDL_Window *Window) 
{
    window_dimensions Result = {};
    SDL_GetWindowSize(Window, &Result.Width, &Result.Height);
    return Result;
}

internal void SDLDebugDrawVertical(game_offscreen_buffer *Buffer, int Value, int Top, int Bottom, uint32 Color)
{
    int Pitch = Buffer->BytesPerPixel * Buffer->Dimensions.Width;
    uint8 *Pixel = ((uint8 *)Buffer->Pixels + Value * Buffer->BytesPerPixel + Top * Pitch);
    for(int Y = Top; Y < Bottom; ++Y)
    {
        *(uint32 *)Pixel = Color;
        Pixel += Pitch;
    }
}

inline void SDLDrawSoundBufferMarker(
    game_offscreen_buffer *Buffer,
    sdl_sound_output *SoundOutput,
    real32 C, 
    int PadX, 
    int Top, 
    int Bottom,
    int Value, 
    uint32 Color
) {
    Assert(Value < SoundOutput->SecondaryBufferSize);
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;
    SDLDebugDrawVertical(Buffer, X, Top, Bottom, Color);
}

internal void SDLDebugSyncDisplay(
    game_offscreen_buffer *Buffer, 
    int DebugTimeMarkerCount, 
    sdl_debug_time_marker *DebugTimeMarkers, 
    sdl_sound_output *SoundOutput, 
    real64 TargetSecondsPerFrame
) {
    int PadX = 16;
    int PadY = 16;

    window_dimensions Dimensions = Buffer->Dimensions;
    int Width = Dimensions.Width;
    real32 PixelsPerByte = (real32)(Width - (2 * PadX)) / (real32)SoundOutput->SecondaryBufferSize;

    int Top = PadY;
    int Bottom = Dimensions.Height - PadY;

    for (int MarkerIndex = 0; MarkerIndex < DebugTimeMarkerCount; ++MarkerIndex)
    {
        sdl_debug_time_marker *Marker = &DebugTimeMarkers[MarkerIndex];
        SDLDrawSoundBufferMarker(Buffer, SoundOutput, PixelsPerByte, PadX, Top, Bottom, Marker->PlayCursor, 0xFFFFFFFF);
        SDLDrawSoundBufferMarker(Buffer, SoundOutput, PixelsPerByte, PadX, Top, Bottom, Marker->WriteCursor, 0xFFFF0000);
    }
}

internal void UpdateWindow(SDL_Texture *WindowTexture, game_offscreen_buffer *Buffer, SDL_Renderer *Renderer) 
{
    SDL_UpdateTexture(WindowTexture, 0, Buffer->Pixels, Buffer->Dimensions.Width * Buffer->BytesPerPixel);
    SDL_RenderCopy(Renderer, WindowTexture, 0, 0);
    SDL_RenderPresent(Renderer);
}

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

internal real32 GetSecondsElapsed(uint64 OldCounter, uint64 CurrentCounter)
{
    return ((real32)(CurrentCounter - OldCounter) / (real32)(SDL_GetPerformanceFrequency()));
}

internal game_controller_input *GetControllerForIndex(game_input *Input, int Index) 
{
    game_controller_input *Result = &(Input->Controllers[Index]);
    return Result;
}

internal void HandleWindowEvent(SDL_WindowEvent e, sdl_setup *Setup, game_offscreen_buffer *Buffer) 
{
    switch(e.event)
    {
        case SDL_WINDOWEVENT_SIZE_CHANGED: 
        {
            int NewWidth = e.data1;
            int NewHeight = e.data2;
            window_dimensions NewDimensions = { NewWidth, NewHeight };
            UpdateOffscreenBufferDimensions(Setup, Buffer, NewDimensions);
        } break;

        case SDL_WINDOWEVENT_EXPOSED: 
        {
            window_dimensions KnownDimensions = GetWindowDimensions(Setup->Window);
            UpdateOffscreenBufferDimensions(Setup, Buffer, KnownDimensions);
        } break;
    }
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

internal real32 SDLProcessGameControllerAxisValue(int16 Value, int16 DeadZoneThreshold)
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
                NewController->StickAverageY = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            {
                NewController->StickAverageX = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            {
                NewController->StickAverageY = -1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            {
                NewController->StickAverageX = -1.0f;
                NewController->IsAnalog = false;
            }

            // emulated Stick average for D-pad movement usage
            real32 Threshold = 0.5f;
            SDLProcessGameControllerButton(&(OldController->MoveUp), &(NewController->MoveUp), NewController->StickAverageY < -Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveRight), &(NewController->MoveRight), NewController->StickAverageX > Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveDown), &(NewController->MoveDown), NewController->StickAverageY > Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveLeft), &(NewController->MoveLeft), NewController->StickAverageX < -Threshold);
        }
    }
}

internal void TryWaitForNextFrame(uint64 LastCounter, real64 TargetSecondsPerFrame) 
{
    if (GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
    {
        int32 TimeToSleep = ((TargetSecondsPerFrame - GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter())) * 1000) - 1;
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

internal int64 GameCodeChanged(game_code *GameCode) 
{
    char const *Filename = GameCode->LibPath;
    struct stat Result;
    if (stat(Filename, &Result) == 0) 
    {
        return Result.st_mtime;
    }
    return 0;
}

internal int LoadGameCode(game_code *GameCode)
{
    if (GameCode->LibHandle) 
    {
        dlclose(GameCode->LibHandle);
        GameCode->LibHandle = NULL;
    }

    GameCode->LibHandle = dlopen(GameCode->LibPath, RTLD_LAZY);
    if (!GameCode->LibHandle) 
    {
        printf("Cannot open library: %s\n", dlerror());
        return -1;
    }

    dlerror();  // reset dl errors
    GameCode->GameUpdateAndRender = (GameUpdateAndRender_t)dlsym(GameCode->LibHandle, "GameUpdateAndRender");
    GameCode->GameGetSoundSamples = (GameGetSoundSamples_t)dlsym(GameCode->LibHandle, "GameGetSoundSamples");

    char const *DlSymError = dlerror();
    if (DlSymError) 
    {
        printf("Cannot load symbol(s) 'GameUpdateAndRender_t': %s \n", DlSymError);
        dlclose(GameCode->LibHandle);
        return 1;
    }

    GameCode->LastWriteTime = GameCodeChanged(GameCode);

    return 0;
}

internal void SdlSetupWindow(sdl_setup *Setup, window_dimensions Dimensions) 
{
    uint32 WindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    Setup->Window = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Dimensions.Width, Dimensions.Height, WindowFlags);
    Assert(Setup->Window);

    Setup->Renderer = SDL_CreateRenderer(Setup->Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    Assert(Setup->Renderer);
}

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
    for (uint64 i = 0; i < ArrayCount(ControllerHandles); ++i) 
    {
        if (ControllerHandles[i]) 
        {
            SDL_GameControllerClose(ControllerHandles[i]);
            ControllerHandles[i] = NULL;
        }
    }

    SDL_CloseAudio();

    uint64 TotalStorageSize = GameMemory->PermanentStorageSize + GameMemory->TransientStorageSize;
    int Result = munmap(GameMemory->PermanentStorage, TotalStorageSize);
    Assert(Result == 0);

    SDL_Quit();
}

internal void SDLAudioCallback(void *UserData, Uint8 *AudioData, int Length)
{
    sdl_audio_ring_buffer *RingBuffer = (sdl_audio_ring_buffer *)UserData;

    int Region1Size = Length;
    int Region2Size = 0;
    if (RingBuffer->PlayCursor + Length > RingBuffer->Size)
    {
        Region1Size = RingBuffer->Size - RingBuffer->PlayCursor;
        Region2Size = Length - Region1Size;
    }
    memcpy(AudioData, (uint8*)(RingBuffer->Data) + RingBuffer->PlayCursor, Region1Size);
    memcpy(&AudioData[Region1Size], RingBuffer->Data, Region2Size);
    RingBuffer->PlayCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
    RingBuffer->WriteCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
}

internal void InitAudio(int32 SamplesPerSecond, int32 BufferSize)
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

internal void FillSoundBuffer(sdl_sound_output *SoundOutput, int ByteToLock, int BytesToWrite, game_sound_output_buffer *SoundBuffer)
{
    int16 *Samples = SoundBuffer->Samples;

    void *Region1 = (uint8 *)AudioRingBuffer.Data + ByteToLock;
    int Region1Size = BytesToWrite;
    if (Region1Size + ByteToLock > SoundOutput->SecondaryBufferSize)
    {
        Region1Size = SoundOutput->SecondaryBufferSize - ByteToLock;
    }
    void *Region2 = AudioRingBuffer.Data;
    int Region2Size = BytesToWrite - Region1Size;
    int Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    int16 *SampleOut = (int16 *)Region1;
    for(int SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
        *SampleOut++ = *Samples++;
        *SampleOut++ = *Samples++;

        ++SoundOutput->RunningSampleIndex;
    }

    int Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    SampleOut = (int16 *)Region2;
    for(int SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
        *SampleOut++ = *Samples++;
        *SampleOut++ = *Samples++;

        ++SoundOutput->RunningSampleIndex;
    }
}

#if HITMAN_DEBUG
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
            int64 Size = Stat.st_size;
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

internal bool DebugWriteEntireFile(char const *Filename, char const *Content, int Length) 
{
    FILE * File = fopen (Filename, "w");
    if (File == NULL) 
    {
        return false;
    }

    uint64 Written = fwrite(Content, 1, Length, File);
    fclose(File);
    return Length == Written;
}
#endif

int main(int argc, char *argv[]) 
{
#if HITMAN_DEBUG
    printf("Running Hitman in DEBUG mode!\n");
#endif

    game_code GameCode = {};
    GameCode.LibPath = "../build/libhitman.so";
    LoadGameCode(&GameCode);

    // REGION - Platform using SDL
    uint32 SubSystemFlags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SubSystemFlags)) 
    {
        printf("Failed initialising subsystems! %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    
#if HITMAN_DEBUG
    uint64 PerfCountFrequency = SDL_GetPerformanceFrequency();
#endif

    local_persist sdl_setup SdlSetup = {};

    window_dimensions Dimensions = { 1280, 1024 };
    SdlSetupWindow(&SdlSetup, Dimensions);

    int const GameUpdateHz = 30;
    int const FramesOfAudioLatency = 3;
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
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    SoundOutput.tSine = 0.0f;
    SoundOutput.LatencySampleCount = FramesOfAudioLatency * (SoundOutput.SamplesPerSecond / GameUpdateHz);
    // TODO: compute variance in the frame time so we can choose what the lowest reasonable value is.
    SoundOutput.SafetyBytes = 0.5 * ((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);

    InitAudio(SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
    int16 *Samples = (int16 *)calloc(SoundOutput.SamplesPerSecond, SoundOutput.BytesPerSample);
    bool SoundIsPlaying = false;

    OpenInputControllers();

    game_offscreen_buffer OffscreenBuffer = {};
    OffscreenBuffer.BytesPerPixel = sizeof(uint32);
    // Initial sizing of the game screen.
    UpdateOffscreenBufferDimensions(&SdlSetup, &OffscreenBuffer, Dimensions);

    game_input Input[2] = {};
    game_input *OldInput = &Input[0];
    game_input *NewInput = &Input[1];

    game_memory GameMemory = {};
    GameMemory.PermanentStorageSize = MegaByte(64);
    GameMemory.TransientStorageSize = GigaByte(4);

#if HITMAN_DEBUG
    void *BaseAddress = (void *)TeraByte(2);
#else
    void *BaseAddress = (void *)(0);
#endif

    uint64 TotalStorageSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
    GameMemory.PermanentStorage = mmap(BaseAddress, TotalStorageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    GameMemory.TransientStorage = (uint8*)(GameMemory.PermanentStorage) + GameMemory.PermanentStorageSize;
    Assert(GameMemory.PermanentStorage);
    Assert(GameMemory.TransientStorage);
    
    game_state *State = (game_state*)GameMemory.PermanentStorage;
    Assert(State);

#if HITMAN_DEBUG
    char const *Path = "../data/read.txt";
    debug_read_file_result ReadResult = DebugReadEntireFile(Path);
    printf("Read file (length = %d)\n", ReadResult.ContentSize);
    printf("%s", (char const *)ReadResult.Content);
    DebugFreeFileMemory(ReadResult.Content);

    char const *WritePath = "../data/write.txt";
    char const *WriteContent = "Written to a file!\n\nWith multi line String\n";
    printf("Writing %zu bytes to %s\n", strlen(WriteContent), WritePath);
    DebugWriteEntireFile(WritePath, WriteContent, strlen(WriteContent));
#endif

#if HITMAN_DEBUG
    sdl_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {};
    int DebugLastPlayCursorIndex = 0;
#endif
    uint64 LastCounter = SDL_GetPerformanceCounter(); 

#if HITMAN_DEBUG
    uint64 LastCycleCount = _rdtsc();
#endif

    SDL_Event e;

    int AudioLatencyBytes;
    real32 AudioLatencySeconds;

    while(Running) 
    {
        game_controller_input *OldKeyboardController = GetControllerForIndex(OldInput, 0);
        game_controller_input *NewKeyboardController = GetControllerForIndex(NewInput, 0);
        *NewKeyboardController = {};
        for(uint64 ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
        {
            NewKeyboardController->Buttons[ButtonIndex].IsDown =
            OldKeyboardController->Buttons[ButtonIndex].IsDown;
        }

        while(SDL_PollEvent(&e) != 0)
        {
            switch (e.type) 
            {
                case SDL_WINDOWEVENT: 
                {
                    HandleWindowEvent(e.window, &SdlSetup, &OffscreenBuffer);
                } break;

                case SDL_QUIT:
                {
                    Running = false;
                } break;

                case SDL_KEYDOWN:
                case SDL_KEYUP: 
                {
                    HandleKeyEvent(e.key, NewKeyboardController);
                } break;
            }
        }

        HandleControllerEvents(OldInput, NewInput);

        if (GameCodeChanged(&GameCode) > GameCode.LastWriteTime) 
        {
            printf("GameCode has changed, reloading!\n");
            LoadGameCode(&GameCode);
        }

        game_offscreen_buffer Buffer = {};
        Buffer.Pixels = OffscreenBuffer.Pixels;
        Buffer.Dimensions = OffscreenBuffer.Dimensions;
        Buffer.BytesPerPixel = OffscreenBuffer.BytesPerPixel;

        GameCode.GameUpdateAndRender(&Buffer, &GameMemory, NewInput, SoundOutput.ToneHz);

        // REGION: Write Audio to Ringbuffer

        SDL_LockAudio();
        
        // TODO: Check if we maybe need to check if soundIsValid and wrap if it is not Valid.

        int ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % 
            SoundOutput.SecondaryBufferSize);

        int ExpectedSoundBytesPerFrame = 
            (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
        int ExpectedFrameBoundaryByte = AudioRingBuffer.PlayCursor + ExpectedSoundBytesPerFrame;

        int SafeWriteCursor = AudioRingBuffer.WriteCursor;
        if (SafeWriteCursor < AudioRingBuffer.PlayCursor)
        {
            SafeWriteCursor += SoundOutput.SecondaryBufferSize;
        }
        Assert(SafeWriteCursor >= AudioRingBuffer.PlayCursor)
        SafeWriteCursor += SoundOutput.SafetyBytes;

        bool AudioCardIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;

        int TargetCursor = 0;
        if (AudioCardIsLowLatency) 
        {
            TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;                    
        }
        else 
        {
            TargetCursor = AudioRingBuffer.WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
        }
        TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

        int BytesToWrite = 0;
        if(ByteToLock > TargetCursor)
        {
            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
            BytesToWrite += TargetCursor;
        }
        else
        {
            BytesToWrite = TargetCursor - ByteToLock;
        }

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;

        GameCode.GameGetSoundSamples(&GameMemory, &SoundBuffer);

        SDL_UnlockAudio();

        FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer); 

        uint64 UnwrappedWriteCursor = AudioRingBuffer.WriteCursor;
        if (UnwrappedWriteCursor < AudioRingBuffer.PlayCursor)
        {
            UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
        }
        
        AudioLatencyBytes = UnwrappedWriteCursor - AudioRingBuffer.PlayCursor;
        AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / 
            (real32)SoundOutput.SamplesPerSecond);
        printf(
            "BTL: %d, TC: %d, BTW: %d - PC: %d, WC: %d, DELTA: %d (%fs)\n", 
            ByteToLock, 
            TargetCursor,
            BytesToWrite, 
            AudioRingBuffer.PlayCursor,
            AudioRingBuffer.WriteCursor,
            AudioLatencyBytes,
            AudioLatencySeconds
        );

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
        uint64 EndCounter = SDL_GetPerformanceCounter();
#endif

#if HITMAN_DEBUG
        SDLDebugSyncDisplay(&OffscreenBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, &SoundOutput, TargetSecondsPerFrame);
#endif

        UpdateWindow(SdlSetup.WindowTexture, &OffscreenBuffer,  SdlSetup.Renderer);

#ifdef HITMAN_DEBUG
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
        uint64 EndCycleCount = _rdtsc();
        int64 CounterElapsed = EndCounter - LastCounter;
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;

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
