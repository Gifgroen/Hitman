#include <stdio.h>
#include <dlfcn.h>
#include <SDL2/SDL.h>

#define global static
#define internal static
#define local_persist static

global bool Running = true;

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 1024

global SDL_Window *gWindow = NULL;

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

    typedef void (*GameUpdateAndRender_t)();

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

    gWindow = SDL_CreateWindow("Hitman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

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

        GameUpdateAndRender();
    }

    // ENDREGION - Platform using SDL

    printf("Closing library\n");
    dlclose(handle);

	SDL_DestroyWindow(gWindow);
	gWindow = NULL;

	SDL_Quit();
 
    return 0;
}
