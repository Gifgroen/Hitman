#include "os_window.h"

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

internal void UpdateWindow(SDL_Texture *WindowTexture, game_offscreen_buffer *Buffer, SDL_Renderer *Renderer) 
{
    SDL_UpdateTexture(WindowTexture, 0, Buffer->Pixels, Buffer->Pitch);
    SDL_RenderCopy(Renderer, WindowTexture, 0, 0);
    SDL_RenderPresent(Renderer);
}
