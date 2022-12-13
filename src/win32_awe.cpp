#include <stdio.h>
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
    printf("Running AWE!\n");

    if(SDL_Init(SDL_INIT_VIDEO)) 
    {
        printf("Cannot init video\n");
        return 1;
    }

    gWindow = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

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
    }

	SDL_DestroyWindow(gWindow);
	gWindow = NULL;

	SDL_Quit();
 
    return 0;
}
