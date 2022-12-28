#include <dlfcn.h>

#include <stdio.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #include <memoryapi.h>
#else 
    #include <sys/mman.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <x86intrin.h>

#include <SDL2/SDL.h>

#include "hitman.h"
#include "platform.h"

global SDL_GameController *ControllerHandles[MAX_CONTROLLER_COUNT];

global bool Running = true;

internal void Alloc(offscreen_buffer *Buffer) 
{
    window_dimensions Dim = Buffer->Dimensions;
    int Size = Dim.Width * Dim.Height * Buffer->BytesPerPixel;
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    Buffer->Pixels = VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else 
    Buffer->Pixels = mmap(0, Size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#endif
}

internal void Dealloc(offscreen_buffer *Buffer) 
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    VirtualFree(Buffer->Pixels, 0, MEM_RELEASE);
#else 
    window_dimensions Dim = Buffer->Dimensions;
    munmap(Buffer->Pixels, Dim.Width * Dim.Height * Buffer->BytesPerPixel);
#endif

}

internal void UpdateOffscreenBufferDimensions(SDL_setup *Setup, offscreen_buffer *Buffer, window_dimensions NewDimensions)
{
    // SDL_Texture *WindowTexture = Setup->WindowTexture;
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

    Alloc(Buffer);
    Buffer->Dimensions = { Width, Height };
}

internal window_dimensions GetWindowDimensions(SDL_Window *Window) 
{
    window_dimensions Result = {};
    SDL_GetWindowSize(Window, &Result.Width, &Result.Height);
    return Result;
}

internal void UpdateWindow(SDL_Texture *WindowTexture, offscreen_buffer *Buffer, SDL_Renderer *Renderer) 
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

internal float GetSecondsElapsed(uint64 OldCounter, uint64 CurrentCounter)
{
    return ((float)(CurrentCounter - OldCounter) / (float)(SDL_GetPerformanceFrequency()));
}

internal game_controller_input *GetControllerForIndex(game_input *Input, int Index) {
    game_controller_input *Result = &(Input->Controllers[Index]);
    return Result;
}

internal void HandleWindowEvent(SDL_WindowEvent e, SDL_setup *Setup, offscreen_buffer *Buffer) 
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
        if(KeyCode == SDLK_UP)
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
            SDLProcessGameControllerButton(
                &(GetControllerForIndex(OldInput, ControllerIndex)->MoveUp),
                &(GetControllerForIndex(NewInput, ControllerIndex)->MoveUp),
                SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_UP)
            );

            SDLProcessGameControllerButton(
                &(GetControllerForIndex(OldInput, ControllerIndex)->MoveRight),
                &(GetControllerForIndex(NewInput, ControllerIndex)->MoveRight),
                SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
            );
            SDLProcessGameControllerButton(
                &(GetControllerForIndex(OldInput, ControllerIndex)->MoveDown),
                &(GetControllerForIndex(NewInput, ControllerIndex)->MoveDown),
                SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)
            );
            SDLProcessGameControllerButton(
                &(GetControllerForIndex(OldInput, ControllerIndex)->MoveLeft),
                &(GetControllerForIndex(NewInput, ControllerIndex)->MoveLeft),
                SDL_GameControllerGetButton(Controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)
            );
        }
    }
}

internal void TryWaitForNextFrame(uint64 LastCounter, double TargetSecondsPerFrame) 
{
    if (GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
    {
        int32_t TimeToSleep = ((TargetSecondsPerFrame - GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter())) * 1000) - 3;
        if (TimeToSleep > 0)
        {
            SDL_Delay(TimeToSleep);
        }

        Assert(GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame);
        while (GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
        {
            // Waiting...
        }
    }
}

internal int64_t GameCodeChanged(game_code *GameCode) 
{
    char const *filename = GameCode->LibPath;
    struct stat result;
    if (stat(filename, &result) == 0) 
    {
        return result.st_mtime;
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

internal void SetupSdl(SDL_setup *Setup, window_dimensions Dimensions) 
{
    uint32 WindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    Setup->Window = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Dimensions.Width, Dimensions.Height, WindowFlags);
    Assert(Setup->Window);
    // if (!Setup->Window) {
    //     printf("Failed to create a SDL_Window, %s\n", SDL_GetError());
    //     return;
    // }

    Setup->Renderer = SDL_CreateRenderer(Setup->Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    Assert(Setup->Renderer);
    // if (!Setup->Renderer) {
    //     printf("Failed to create an SDL_Renderer, %s\n", SDL_GetError());
    //     return;
    // }
}

internal void CloseGame(game_code *GameCode, SDL_setup *Setup) 
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

	SDL_Quit();
}

int main(int argc, char *argv[]) 
{
#if HITMAN_DEBUG
    printf("Running Hitman in DEBUG mode!\n");
#endif

    game_code GameCode = {};
    GameCode.LibPath = "../build/libhitman.so";

    // REGION - Platform using SDL
    uint32 SubSystemFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SubSystemFlags)) 
    {
        printf("Failed initialising subsystems! %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    
#if HITMAN_DEBUG
    uint64 PerfCountFrequency = SDL_GetPerformanceFrequency();
#endif

    local_persist SDL_setup SdlSetup = {};

    window_dimensions Dimensions = { 1280, 1024 };
    SetupSdl(&SdlSetup, Dimensions);

    OpenInputControllers();

    int GameUpdateHz = 30;
    double TargetSecondsPerFrame = 1.0f / (double)GameUpdateHz;
    int DetectedFrameRate = GetWindowRefreshRate(SdlSetup.Window);
    if (DetectedFrameRate != GameUpdateHz) 
    {
        printf("Device capable refresh rate is %d Hz, but Game runs in %d Hz\n", DetectedFrameRate, GameUpdateHz);
    }

    offscreen_buffer Buffer = {};
    Buffer.BytesPerPixel = sizeof(uint32);
    // Initial sizing of the game screen.
    UpdateOffscreenBufferDimensions(&SdlSetup, &Buffer, Dimensions);

    game_input Input[2] = {};
    game_input *OldInput = &Input[0];
    game_input *NewInput = &Input[1];

    uint64 LastCounter = SDL_GetPerformanceCounter(); 

#if HITMAN_DEBUG
    uint64 LastCycleCount = _rdtsc();
#endif

    SDL_Event e;

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
            switch (e.type) {
                case SDL_WINDOWEVENT: 
                {
                    HandleWindowEvent(e.window, &SdlSetup, &Buffer);
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

        if (GameCodeChanged(&GameCode) > GameCode.LastWriteTime) {
            printf("GameCode has changed, reloading!\n");
            LoadGameCode(&GameCode);
        }

        GameCode.GameUpdateAndRender(&Buffer, NewInput);

        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;
        
        *NewInput = {};
        
        TryWaitForNextFrame(LastCounter, TargetSecondsPerFrame);

#if HITMAN_DEBUG
        // Get this before UpdateWindow() so that we don't keep missing VBlanks.
        uint64 EndCounter = SDL_GetPerformanceCounter();
#endif

        UpdateWindow(SdlSetup.WindowTexture, &Buffer,  SdlSetup.Renderer);

#if HITMAN_DEBUG 
        // Calculate frame timings.
        uint64 EndCycleCount = _rdtsc();
        int64 CounterElapsed = EndCounter - LastCounter;
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;

        double MSPerFrame = (((1000.0f * (double)CounterElapsed) / (double)PerfCountFrequency));
        double FPS = (double)PerfCountFrequency / (double)CounterElapsed;
        double MCPF = ((double)CyclesElapsed / (1000.0f * 1000.0f));

        printf("%.02fms/f, %.02f/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);

        LastCycleCount = EndCycleCount;
        LastCounter = EndCounter;
#endif
    }

    // ENDREGION - Platform using SDL

    CloseGame(&GameCode, &SdlSetup);
 
    return EXIT_SUCCESS;
}
