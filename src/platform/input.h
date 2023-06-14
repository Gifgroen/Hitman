#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>

// #include "../platform.h"

#include "../hitman.h"
#include "../types.h"
#include "os_window.h"
#include "debug_input_recording.h"

// input
internal void HandleKeyEvent(game_state *State, SDL_KeyboardEvent key, game_controller_input *KeyboardController);

internal void SDLHandleEvents(game_state *State, SDL_Event *e, sdl_setup *SdlSetup, game_offscreen_buffer *OffscreenBuffer, game_controller_input *NewKeyboardController, debug_input_recording *InputRecording);

internal void HandleControllerEvents(game_input *OldInput, game_input *NewInput);

internal void OpenInputControllers();

internal game_controller_input *GetControllerForIndex(game_input *Input, int Index);

internal void ProcessKeyInput(game_button_state *NewState, bool IsDown);

internal void SDLProcessGameControllerButton(game_button_state *OldState, game_button_state *NewState, bool Value);

internal real32 SDLProcessGameControllerAxisValue(s16 Value, s16 DeadZoneThreshold);

#endif