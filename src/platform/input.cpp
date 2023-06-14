#include "input.h"

global SDL_GameController *ControllerHandles[MAX_CONTROLLER_COUNT];

internal void HandleKeyEvent(game_state *State, SDL_KeyboardEvent key, game_controller_input *KeyboardController)
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
                State->Running= false;
            }
            printf("\n");
        }
    }
}

internal void SDLHandleEvents(game_state *State, SDL_Event *e, sdl_setup *SdlSetup, game_offscreen_buffer *OffscreenBuffer, game_controller_input *NewKeyboardController, debug_input_recording *InputRecording) 
{
    while(SDL_PollEvent(e) != 0)
    {
        switch (e->type) 
        {
            case SDL_WINDOWEVENT: 
            {
                HandleWindowEvent(e->window, SdlSetup, OffscreenBuffer);
            } break;

            case SDL_QUIT:
            {
                State->Running = false;
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: 
            {
                #if HITMAN_INTERNAL
                DebugHandleKeyEvent(e->key, SdlSetup, InputRecording, NewKeyboardController);
                #endif
                HandleKeyEvent(State, e->key, NewKeyboardController);
            } break;
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
                NewController->StickAverageY = -1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            {
                NewController->StickAverageX = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            {
                NewController->StickAverageY = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            {
                NewController->StickAverageX = -1.0f;
                NewController->IsAnalog = false;
            }

            // emulated Stick average for D-pad movement usage
            real32 Threshold = 0.5f;
            SDLProcessGameControllerButton(&(OldController->MoveUp), &(NewController->MoveUp), NewController->StickAverageY > Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveRight), &(NewController->MoveRight), NewController->StickAverageX > Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveDown), &(NewController->MoveDown), NewController->StickAverageY < -Threshold);
            SDLProcessGameControllerButton(&(OldController->MoveLeft), &(NewController->MoveLeft), NewController->StickAverageX < -Threshold);
        }
    }
}

// Input
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

internal game_controller_input *GetControllerForIndex(game_input *Input, int Index) 
{
    game_controller_input *Result = &(Input->Controllers[Index]);
    return Result;
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

internal real32 SDLProcessGameControllerAxisValue(s16 Value, s16 DeadZoneThreshold)
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
