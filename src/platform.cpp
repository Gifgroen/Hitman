#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <x86intrin.h>

#include <SDL2/SDL.h>

#include <sys/types.h>
#include <sys/stat.h>
// #ifndef WIN32
// #include <unistd.h>
// #endif
// #ifdef WIN32
// #define stat _stat
// #endif

#include "hitman.h"

#define global static
#define internal static
#define local_persist static

#define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}

#define ArrayCount(Array) (sizeof(Array)/sizeof(*(Array)))

global SDL_GameController *ControllerHandles[MAX_CONTROLLER_COUNT];

global SDL_Window *Window = NULL;
global SDL_Texture *WindowTexture = NULL;

typedef void (*GameUpdateAndRender_t)(offscreen_buffer*, game_input*);

struct game_code
{
    char const *LibPath;
    void* LibHandle;
    int64_t LastWriteTime;

    GameUpdateAndRender_t GameUpdateAndRender;
};

global bool Running = true;

internal void UpdateOffscreenBufferDimensions(SDL_Renderer *Renderer, offscreen_buffer *Buffer, window_dimensions NewDimensions)
{
    if (WindowTexture) 
    {
        SDL_DestroyTexture(WindowTexture);
        WindowTexture = NULL;
    }

    if (Buffer->Pixels) 
    {   
        window_dimensions Dim = Buffer->Dimensions;
        munmap(Buffer->Pixels, Dim.Width * Dim.Height * Buffer->BytesPerPixel);
    }

    int Width = NewDimensions.Width;
    int Height = NewDimensions.Height; 
    WindowTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Width, Height);

    Buffer->Pixels = mmap(0, Width * Height * Buffer->BytesPerPixel, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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

internal float GetSecondsElapsed(u_int64_t OldCounter, u_int64_t CurrentCounter)
{
    return ((float)(CurrentCounter - OldCounter) / (float)(SDL_GetPerformanceFrequency()));
}

internal game_controller_input *GetControllerForIndex(game_input *Input, int Index) {
    game_controller_input *Result = &(Input->Controllers[Index]);
    return Result;
}

internal void HandleWindowEvent(SDL_WindowEvent e, SDL_Renderer *Renderer, offscreen_buffer *Buffer) 
{
    switch(e.event)
    {
        case SDL_WINDOWEVENT_SIZE_CHANGED: 
        {
            int NewWidth = e.data1;
            int NewHeight = e.data2;
            window_dimensions NewDimensions = { NewWidth, NewHeight };
            UpdateOffscreenBufferDimensions(Renderer, Buffer, NewDimensions);
        } break;

        case SDL_WINDOWEVENT_EXPOSED: 
        {
            window_dimensions KnownDimensions = GetWindowDimensions(Window);
            UpdateOffscreenBufferDimensions(Renderer, Buffer, KnownDimensions);
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

internal void TryWaitForNextFrame(u_int64_t LastCounter, double TargetSecondsPerFrame) 
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

int64_t GameCodeChanged(game_code *GameCode) 
{
    char const *filename = GameCode->LibPath;
    struct stat result;
    if (stat(filename, &result) == 0) {
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

int main(int argc, char *argv[]) 
{
#if HITMAN_DEBUG
    printf("Running Hitman in DEBUG mode!\n");
#endif

    game_code GameCode = {};
    GameCode.LibPath = "../build/libhitman.so";
    // LoadGameCode(&GameCode);

    // REGION - Platform using SDL
    u_int32_t SubSystemFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SubSystemFlags)) 
    {
        printf("Failed initialising subsystems! %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    
#if HITMAN_DEBUG
    u_int64_t PerfCountFrequency = SDL_GetPerformanceFrequency();
#endif

    int WindowWidth = 1280;
    int WindowHeight = 1024;
    u_int32_t WindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    Window = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WindowWidth, WindowHeight, WindowFlags);

    SDL_Renderer *Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);

    OpenInputControllers();

    int GameUpdateHz = 30;
    double TargetSecondsPerFrame = 1.0f / (double)GameUpdateHz;
    if (GetWindowRefreshRate(Window) !=  GameUpdateHz) 
    {
        printf("Device capable refresh rate is %d Hz, but Game runs in %d Hz\n", GetWindowRefreshRate(Window), GameUpdateHz);
    }

    offscreen_buffer Buffer = {};
    Buffer.BytesPerPixel = sizeof(u_int32_t);
    // Initial sizing of the game screen.
    window_dimensions Dimensions = GetWindowDimensions(Window);
    UpdateOffscreenBufferDimensions(Renderer, &Buffer, Dimensions);
    

    game_input Input[2] = {};
    game_input *OldInput = &Input[0];
    game_input *NewInput = &Input[1];

    u_int64_t LastCounter = SDL_GetPerformanceCounter(); 

#if HITMAN_DEBUG
    u_int64_t LastCycleCount = _rdtsc();
#endif

    SDL_Event e;

    while(Running) 
    {
        game_controller_input *OldKeyboardController = GetControllerForIndex(OldInput, 0);
        game_controller_input *NewKeyboardController = GetControllerForIndex(NewInput, 0);
        *NewKeyboardController = {};
        for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
        {
            NewKeyboardController->Buttons[ButtonIndex].IsDown =
            OldKeyboardController->Buttons[ButtonIndex].IsDown;
        }

        while(SDL_PollEvent(&e) != 0)
        {
            switch (e.type) {
                case SDL_WINDOWEVENT: 
                {
                    HandleWindowEvent(e.window, Renderer, &Buffer);
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

        // REGION: Process Controller Input
        HandleControllerEvents(OldInput, NewInput);
        // ENDREGION: Process Controller Input

        /**
         * TODO: check refresh guard; (last write time) of file at GameCode.LibPath has changed recently 
         */

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
        u_int64_t EndCounter = SDL_GetPerformanceCounter();
#endif

        UpdateWindow(WindowTexture, &Buffer,  Renderer);

#if HITMAN_DEBUG 
        // Calculate frame timings.
        u_int64_t EndCycleCount = _rdtsc();
        u_int64_t CounterElapsed = EndCounter - LastCounter;
        u_int64_t CyclesElapsed = EndCycleCount - LastCycleCount;

        double MSPerFrame = (((1000.0f * (double)CounterElapsed) / (double)PerfCountFrequency));
        double FPS = (double)PerfCountFrequency / (double)CounterElapsed;
        double MCPF = ((double)CyclesElapsed / (1000.0f * 1000.0f));

        printf("%.02fms/f, %.02f/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);

        LastCycleCount = EndCycleCount;
        LastCounter = EndCounter;
#endif
    }

    // ENDREGION - Platform using SDL

    if (GameCode.LibHandle) 
    {
        dlclose(GameCode.LibHandle);
        GameCode.LibHandle = NULL;
    }

    if (WindowTexture) 
    {
        SDL_DestroyTexture(WindowTexture);
        WindowTexture = NULL;
    }
    if (Renderer) 
    {
        SDL_DestroyRenderer(Renderer);
        Renderer = NULL;
    }
    if (Window) 
    {
        SDL_DestroyWindow(Window);
	    Window = NULL;
    }
    for (int i = 0; i < ArrayCount(ControllerHandles); ++i) 
    {
        if (ControllerHandles[i]) 
        {
            SDL_GameControllerClose(ControllerHandles[i]);
            ControllerHandles[i] = NULL;
        }
    }

	SDL_Quit();
 
    return EXIT_SUCCESS;
}
