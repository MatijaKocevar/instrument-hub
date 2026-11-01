#pragma once

#if defined(__linux__)

#include <juce_audio_devices/juce_audio_devices.h>

#include <alsa/asoundlib.h>

namespace AudioEngine::detail
{

/** The raw ALSA PCM names of a Loopback playback/capture pair. */
struct AlsaPcmPair
{
    juce::String playback;
    juce::String capture;
};

/** Finds the first Loopback card and its playback/capture PCM names. */
bool findAlsaLoopbackPair (AlsaPcmPair& pair);

/** Applies the probe's format to an open PCM and reports the actual values. */
bool configureAlsaPcm (snd_pcm_t* pcm,
                       double& sampleRate,
                       int& periodSize,
                       int& bufferSize);

/** Recovers a PCM from an underrun/overrun after a read or write error. */
int recoverAlsaPcm (snd_pcm_t* pcm, int error);

} // namespace AudioEngine::detail

#endif
