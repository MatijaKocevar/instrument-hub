#pragma once

#include "pad_audio/Constants.h"

#include <juce_core/juce_core.h>

#include <cstdint>

typedef struct _snd_pcm snd_pcm_t;

namespace PadAudio
{

/** One ALSA Loopback playback/capture pair in S16 mono. */
struct AlsaPcm
{
    snd_pcm_t* playback = nullptr;
    snd_pcm_t* capture = nullptr;

    double rate = kSampleRate;
    int period = kBlockSize;
    int buffer = kBlockSize * 8;

    juce::String playbackName;
    juce::String captureName;

    ~AlsaPcm();

    void close();

    bool open (juce::String& error);

    bool read (std::int16_t* destination, int frames);

    bool write (const std::int16_t* source, int frames);

private:
    static bool findPair (juce::String& playbackName, juce::String& captureName);

    static bool configure (snd_pcm_t* pcm,
                           double& sampleRate,
                           int& periodSize,
                           int& bufferSize);
};

} // namespace PadAudio
