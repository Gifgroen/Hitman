#include <stdio.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <SDL2/SDL.h>

#include "hitman.h"

#define global static
#define internal static
#define local_persist static

global SDL_Window *Window = NULL;
global SDL_Texture *WindowTexture = NULL;

global bool Running = true;

internal void UpdateBackBufferDimensions(SDL_Renderer *Renderer, offscreen_buffer *Buffer, window_dimensions NewDimensions)
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

void Render(SDL_Texture *WindowTexture, offscreen_buffer *Buffer, SDL_Renderer *Renderer) 
{
    SDL_UpdateTexture(WindowTexture, 0, Buffer->Pixels, Buffer->Dimensions.Width * Buffer->BytesPerPixel);
    SDL_RenderCopy(Renderer, WindowTexture, 0, 0);
    SDL_RenderPresent(Renderer);
}

int main(int argc, char *argv[]) 
{
    printf("Running Hitman!\n");

    // REGION - Load Game Library code!
    printf("Opening libhitman.so...\n");
    void* Handle = dlopen("../build/libhitman.so", RTLD_LAZY);
    if (!Handle) 
    {
        printf("Cannot open library: %s\n", dlerror());
        return 1;
    }

    typedef void (*GameUpdateAndRender_t)(offscreen_buffer*);

    dlerror();  // reset dl errors
    GameUpdateAndRender_t GameUpdateAndRender = (GameUpdateAndRender_t) dlsym(Handle, "GameUpdateAndRender");

    const char *DlSymError = dlerror();
    if (DlSymError) {
        printf("Cannot load symbol 'GameUpdateAndRender_t': %s \n", DlSymError);
        dlclose(Handle);
        return 1;
    }
    printf("Loaded Game service from libhitman!\n");

    // ENDREGION - Load Game Library code!

    // REGION - Platform using SDL
    if(SDL_Init(SDL_INIT_VIDEO)) 
    {
        printf("Failed initialising subsystems! %s\n", SDL_GetError());
        return 1;
    }

    int WindowWidth = 1280;
    int WindowHeight = 1024;
    u_int32_t WindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    Window = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WindowWidth, WindowHeight, WindowFlags);

    SDL_Renderer *Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);

    // REGION: setup offscreen buffer

    offscreen_buffer Buffer = {};
    Buffer.BytesPerPixel = sizeof(u_int32_t);
    // Initial sizing of the game screen.
    window_dimensions Dimensions = GetWindowDimensions(Window);
    UpdateBackBufferDimensions(Renderer, &Buffer, Dimensions);
    
    // ENDREGION: setup offscreen buffer

    SDL_Event e;

    while(Running) 
    {
        while(SDL_PollEvent(&e) != 0)
        {
            if(e.type == SDL_QUIT)
            {
                Running = false;
            }
            else if(e.type == SDL_WINDOWEVENT)
            {       
                switch(e.window.event)
                {
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        int NewWidth = e.window.data1;
                        int NewHeight = e.window.data2;
                        window_dimensions NewDimensions = { NewWidth, NewHeight };
                        UpdateBackBufferDimensions(Renderer, &Buffer, NewDimensions);
                    } break;

                    case SDL_WINDOWEVENT_EXPOSED: {
                        window_dimensions KnownDimensions = GetWindowDimensions(Window);
                        UpdateBackBufferDimensions(Renderer, &Buffer, KnownDimensions);
                    } break;

                    default: {
                        printf("WINDOWEVENT: type = %d\n", e.window.type);
                    }
                }
            }
        }

        GameUpdateAndRender(&Buffer);

        Render(WindowTexture, &Buffer,  Renderer);
    }

    // ENDREGION - Platform using SDL

    if (Handle) 
    {
        printf("Closing library\n");
        dlclose(Handle);
    }


    printf("Quiting SDL\n");
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

	SDL_Quit();
 
    return 0;
}
