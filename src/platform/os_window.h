#ifndef OS_WINDOW_H
#define OS_WINDOW_H

#include <SDL2/SDL.h>

#include "../hitman.h"
#include "../types.h"

struct sdl_setup 
{
    SDL_Window *Window;
    SDL_Renderer *Renderer;
    SDL_Texture *WindowTexture = NULL;
};

// SDL Window management
internal void SdlSetupWindow(sdl_setup *Setup, v2 Dimensions);

internal void UpdateOffscreenBufferDimensions(sdl_setup *Setup, game_offscreen_buffer *Buffer, v2 NewDimensions);

internal v2 GetWindowDimensions(SDL_Window *Window);

internal void HandleWindowEvent(SDL_WindowEvent e, sdl_setup *Setup, game_offscreen_buffer *Buffer);

internal void UpdateWindow(SDL_Texture *WindowTexture, game_offscreen_buffer *Buffer, SDL_Renderer *Renderer);

#endif // OS_WINDOW_H
