#include "audio.h"

global sdl_audio_ring_buffer AudioRingBuffer;

// SDL Audio
internal void SDLAudioCallback(void *UserData, u8 *AudioData, int Length)
{
    sdl_audio_ring_buffer *RingBuffer = (sdl_audio_ring_buffer *)UserData;

    int Region1Size = Length;
    int Region2Size = 0;
    if (RingBuffer->PlayCursor + Length > RingBuffer->Size)
    {
        Region1Size = RingBuffer->Size - RingBuffer->PlayCursor;
        Region2Size = Length - Region1Size;
    }
    memcpy(AudioData, (u8*)(RingBuffer->Data) + RingBuffer->PlayCursor, Region1Size);
    memcpy(&AudioData[Region1Size], RingBuffer->Data, Region2Size);
    RingBuffer->PlayCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
    RingBuffer->WriteCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
}

internal void InitAudio(s32 SamplesPerSecond, s32 BufferSize)
{
    SDL_AudioSpec AudioSettings = {0};

    AudioSettings.freq = SamplesPerSecond;
    AudioSettings.format = AUDIO_S16LSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &SDLAudioCallback;
    AudioSettings.userdata = &AudioRingBuffer;

    AudioRingBuffer.Size = BufferSize;
    AudioRingBuffer.Data = malloc(BufferSize);
    AudioRingBuffer.PlayCursor = AudioRingBuffer.WriteCursor = 0;

    SDL_OpenAudio(&AudioSettings, 0);

    printf("Initialised an Audio device at frequency %d Hz, %d Channels, buffer size %d\n", AudioSettings.freq, AudioSettings.channels, AudioSettings.samples);

    if (AudioSettings.format != AUDIO_S16LSB)
    {
        printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
        SDL_CloseAudio();
    }
}

internal sdl_audio_buffer_index  PositionAudioBuffer(sdl_sound_output *SoundOutput, int GameUpdateHz)
{
    // TODO: Check if we maybe need to check if soundIsValid and wrap if it is not Valid.

    int ByteToLock = ((SoundOutput->RunningSampleIndex * SoundOutput->BytesPerSample) % SoundOutput->SecondaryBufferSize);

    int ExpectedSoundBytesPerFrame = (SoundOutput->SamplesPerSecond * SoundOutput->BytesPerSample) / GameUpdateHz;
    int ExpectedFrameBoundaryByte = AudioRingBuffer.PlayCursor + ExpectedSoundBytesPerFrame;

    int SafeWriteCursor = AudioRingBuffer.WriteCursor;
    if (SafeWriteCursor < AudioRingBuffer.PlayCursor)
    {
        SafeWriteCursor += SoundOutput->SecondaryBufferSize;
    }
    Assert(SafeWriteCursor >= AudioRingBuffer.PlayCursor)
    SafeWriteCursor += SoundOutput->SafetyBytes;

    bool AudioCardIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;
    int TargetCursor = 0;
    if (AudioCardIsLowLatency) 
    {
        TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;                    
    }
    else 
    {
        TargetCursor = AudioRingBuffer.WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput->SafetyBytes;
    }
    TargetCursor = (TargetCursor % SoundOutput->SecondaryBufferSize);

    int BytesToWrite = 0;
    if(ByteToLock > TargetCursor)
    {
        BytesToWrite = (SoundOutput->SecondaryBufferSize - ByteToLock);
        BytesToWrite += TargetCursor;
    }
    else
    {
        BytesToWrite = TargetCursor - ByteToLock;
    }

    sdl_audio_buffer_index Result = {};
    Result.ByteToLock = ByteToLock;
    Result.TargetCursor = TargetCursor;
    Result.BytesToWrite = BytesToWrite;
    return Result;
}

internal void FillSoundBuffer(sdl_sound_output *SoundOutput, int ByteToLock, int BytesToWrite, game_sound_output_buffer *SoundBuffer)
{
    s16 *Samples = SoundBuffer->Samples;

    void *Region1 = (u8 *)AudioRingBuffer.Data + ByteToLock;
    int Region1Size = BytesToWrite;
    if (Region1Size + ByteToLock > SoundOutput->SecondaryBufferSize)
    {
        Region1Size = SoundOutput->SecondaryBufferSize - ByteToLock;
    }
    void *Region2 = AudioRingBuffer.Data;
    int Region2Size = BytesToWrite - Region1Size;
    int Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    s16 *SampleOut = (s16 *)Region1;
    for(int SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
    {
        *SampleOut++ = *Samples++;
        *SampleOut++ = *Samples++;

        ++SoundOutput->RunningSampleIndex;
    }

    int Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    SampleOut = (s16 *)Region2;
    for(int SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
    {
        *SampleOut++ = *Samples++;
        *SampleOut++ = *Samples++;

        ++SoundOutput->RunningSampleIndex;
    }
}
