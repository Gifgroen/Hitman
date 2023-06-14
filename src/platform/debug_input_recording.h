#ifndef DEBUG_INPUT_RECORDING_H
#define DEBUG_INPUT_RECORDING_H

#include "../hitman.h"
#include "os_window.h"

enum record_action {
    Idle,
    Recording,
    Playing
};

struct debug_input_recording
{
    record_action Action;

    FILE *RecordHandle;
    FILE *PlaybackHandle;

    u64 TotalMemorySize;
};

// Record Input
internal void DebugBeginRecordInput(debug_input_recording *InputRecorder, game_memory *GameMemory);

internal void DebugRecordInput(debug_input_recording *InputRecorder, game_input *NewInput, game_memory *GameMemory);

internal void DebugEndRecordInput(debug_input_recording *InputRecorder);

// Playback Input
internal void DebugBeginPlaybackInput(debug_input_recording *InputRecorder, game_memory *GameMemory) ;

internal void DebugPlaybackInput(debug_input_recording *InputRecorder, game_input *NewInput, game_memory *GameMemory);

internal void DebugEndPlaybackInput(debug_input_recording *InputRecorder);

#if HITMAN_INTERNAL // Debug Input handling
internal void DebugHandleKeyEvent(SDL_KeyboardEvent Event, sdl_setup *Setup, debug_input_recording *Recording, game_controller_input *KeyboardController);
#endif

#endif