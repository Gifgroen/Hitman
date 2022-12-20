#include <stdio.h>
#include <dlfcn.h>
#include <SDL2/SDL.h>

#include "hitman.h"

#define global static
#define internal static
#define local_persist static

global bool Running = true;

global SDL_Window *Window = NULL;

int main(int argc, char *argv[]) 
{
    printf("Running Hitman!\n");

    // REGION - Load Game Library code!
    printf("Opening libhitman.so...\n");
    void* handle = dlopen("../build/libhitman.so", RTLD_LAZY);
    if (!handle) 
    {
        printf("Cannot open library: %s\n", dlerror());
        return 1;
    }

    typedef void (*GameUpdateAndRender_t)(back_buffer*);

    // reset dl errors
    dlerror();
    GameUpdateAndRender_t GameUpdateAndRender = (GameUpdateAndRender_t) dlsym(handle, "GameUpdateAndRender");

    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        printf("Cannot load symbol 'GameUpdateAndRender_t': %s \n", dlsym_error);
        dlclose(handle);
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

    back_buffer Buffer = {};
    buffer_dimensions Dimensions = {};
    Dimensions.Width = 1280;
    Dimensions.Height = 1024;
    Buffer.Dimensions = Dimensions;

    int32_t Pixels[Dimensions.Height][Dimensions.Width];

    Buffer.Pixels = Pixels;
    Buffer.BytesPerPixel = sizeof(int32_t);


    Window = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Dimensions.Width, Dimensions.Height, SDL_WINDOW_SHOWN);

    SDL_Renderer *Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_TARGETTEXTURE);


    SDL_Texture *Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Dimensions.Width, Dimensions.Height);

    SDL_Event e;

    while(Running) 
    {
        while(SDL_PollEvent(&e) != 0)
        {
            if(e.type == SDL_QUIT)
            {
                Running = false;
            }
        }

        GameUpdateAndRender(&Buffer);

        SDL_UpdateTexture(Texture, 0, Buffer.Pixels, Buffer.Dimensions.Width * Buffer.BytesPerPixel);
        SDL_RenderCopy(Renderer, Texture, 0, 0);
        SDL_RenderPresent(Renderer);
    }

    // ENDREGION - Platform using SDL

    printf("Closing library\n");
    dlclose(handle);

    printf("Quiting SDL\n");
	SDL_DestroyWindow(Window);
	Window = NULL;
	SDL_Quit();
 
    return 0;
}
