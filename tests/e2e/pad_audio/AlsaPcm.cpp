#include "pad_audio/AlsaPcm.h"

#if defined(__linux__)
 #include <alsa/asoundlib.h>
#endif

#include <cstdlib>
#include <utility>
#include <vector>

namespace PadAudio
{

#if defined(__linux__)
AlsaPcm::~AlsaPcm()
{
    close();
}

void AlsaPcm::close()
{
    if (playback != nullptr)
    {
        snd_pcm_drop (playback);
        snd_pcm_close (playback);
        playback = nullptr;
    }

    if (capture != nullptr)
    {
        snd_pcm_drop (capture);
        snd_pcm_close (capture);
        capture = nullptr;
    }
}

bool AlsaPcm::findPair (juce::String& playbackName, juce::String& captureName)
{
    void** hints = nullptr;

    if (snd_device_name_hint (-1, "pcm", &hints) < 0)
        return false;

    std::vector<std::pair<juce::String, juce::String>> names;

    for (char** hint = reinterpret_cast<char**> (hints); *hint != nullptr; ++hint)
    {
        char* name = snd_device_name_get_hint (*hint, "NAME");
        char* desc = snd_device_name_get_hint (*hint, "DESC");

        if (name != nullptr)
            names.emplace_back (juce::String (name),
                                desc != nullptr ? juce::String (desc) : juce::String());

        std::free (name);
        std::free (desc);
    }

    snd_device_name_free_hint (hints);

    for (const auto& entry : names)
    {
        if (! entry.second.containsIgnoreCase ("loopback")
            || ! entry.first.startsWith ("hw:CARD=")
            || ! entry.first.contains (",DEV=0"))
            continue;

        const auto capture = entry.first.replace (",DEV=0", ",DEV=1");

        for (const auto& other : names)
        {
            if (other.first != capture)
                continue;

            playbackName = entry.first;
            captureName = capture;
            return true;
        }
    }

    return false;
}

bool AlsaPcm::configure (snd_pcm_t* pcm,
                         double& sampleRate,
                         int& periodSize,
                         int& bufferSize)
{
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca (&hw);

    if (snd_pcm_hw_params_any (pcm, hw) < 0)
        return false;

    if (snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0
        || snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE) < 0
        || snd_pcm_hw_params_set_channels (pcm, hw, 1) < 0)
        return false;

    unsigned int rate = static_cast<unsigned int> (sampleRate);

    if (snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, nullptr) < 0)
        return false;

    snd_pcm_uframes_t period = static_cast<snd_pcm_uframes_t> (juce::jmax (1, periodSize));
    int dir = 0;

    if (snd_pcm_hw_params_set_period_size (pcm, hw, period, dir) < 0
        && snd_pcm_hw_params_set_period_size_near (pcm, hw, &period, &dir) < 0)
        return false;

    snd_pcm_uframes_t buffer = period * 8;

    if (snd_pcm_hw_params_set_buffer_size (pcm, hw, buffer) < 0
        && snd_pcm_hw_params_set_buffer_size_near (pcm, hw, &buffer) < 0)
        return false;

    if (snd_pcm_hw_params (pcm, hw) < 0)
        return false;

    snd_pcm_sw_params_t* sw = nullptr;
    snd_pcm_sw_params_alloca (&sw);

    if (snd_pcm_sw_params_current (pcm, sw) < 0
        || snd_pcm_sw_params_set_start_threshold (pcm, sw, 1) < 0
        || snd_pcm_sw_params (pcm, sw) < 0)
        return false;

    snd_pcm_uframes_t actualPeriod = 0;
    snd_pcm_uframes_t actualBuffer = 0;

    if (snd_pcm_hw_params_get_period_size (hw, &actualPeriod, &dir) < 0
        || snd_pcm_hw_params_get_buffer_size (hw, &actualBuffer) < 0)
        return false;

    sampleRate = static_cast<double> (rate);
    periodSize = static_cast<int> (actualPeriod);
    bufferSize = static_cast<int> (actualBuffer);
    return true;
}

bool AlsaPcm::open (juce::String& error)
{
    if (! findPair (playbackName, captureName))
    {
        error = "no ALSA Loopback playback/capture pair was found";
        return false;
    }

    if (snd_pcm_open (&playback, playbackName.toRawUTF8(),
                      SND_PCM_STREAM_PLAYBACK, 0) < 0)
    {
        error = "could not open playback device " + playbackName;
        return false;
    }

    if (snd_pcm_open (&capture, captureName.toRawUTF8(),
                      SND_PCM_STREAM_CAPTURE, 0) < 0)
    {
        error = "could not open capture device " + captureName;
        return false;
    }

    if (! configure (playback, rate, period, buffer)
        || ! configure (capture, rate, period, buffer))
    {
        error = "could not configure the Loopback devices";
        return false;
    }

    if (snd_pcm_prepare (playback) < 0 || snd_pcm_prepare (capture) < 0)
    {
        error = "could not prepare the Loopback devices";
        return false;
    }

    return true;
}

bool AlsaPcm::read (std::int16_t* destination, int frames)
{
    int done = 0;

    while (done < frames)
    {
        const auto got = snd_pcm_readi (capture,
                                        destination + done,
                                        static_cast<snd_pcm_uframes_t> (frames - done));

        if (got < 0)
        {
            if (snd_pcm_recover (capture, static_cast<int> (got), 0) < 0)
                return false;

            continue;
        }

        if (got == 0)
            continue;

        done += static_cast<int> (got);
    }

    return true;
}

bool AlsaPcm::write (const std::int16_t* source, int frames)
{
    int done = 0;

    while (done < frames)
    {
        const auto wrote = snd_pcm_writei (playback,
                                           source + done,
                                           static_cast<snd_pcm_uframes_t> (frames - done));

        if (wrote < 0)
        {
            if (snd_pcm_recover (playback, static_cast<int> (wrote), 0) < 0)
                return false;

            continue;
        }

        if (wrote == 0)
            continue;

        done += static_cast<int> (wrote);
    }

    return true;
}
#endif

} // namespace PadAudio
