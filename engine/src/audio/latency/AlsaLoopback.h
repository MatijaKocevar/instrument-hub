#pragma once

#if defined(__linux__)

#include <juce_audio_devices/juce_audio_devices.h>

#include <alsa/asoundlib.h>

namespace AudioEngine::detail
{

struct AlsaPcmPair
{
    juce::String playback;
    juce::String capture;
};

bool findAlsaLoopbackPair (AlsaPcmPair& pair);

bool configureAlsaPcm (snd_pcm_t* pcm,
                       double& sampleRate,
                       int& periodSize,
                       int& bufferSize);

int recoverAlsaPcm (snd_pcm_t* pcm, int error);

}

#endif
