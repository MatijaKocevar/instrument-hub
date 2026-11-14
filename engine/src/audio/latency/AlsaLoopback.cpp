#include "audio/latency/AlsaLoopback.h"

#if defined(__linux__)

#include <cstdlib>
#include <utility>
#include <vector>

namespace AudioEngine::detail
{

    bool findAlsaLoopbackPair (AlsaPcmPair& pair)
    {
        void** hints = nullptr;

        if (snd_device_name_hint (-1, "pcm", &hints) < 0)
            return false;

        std::vector<std::pair<juce::String, juce::String>> names;

        for (char** h = reinterpret_cast<char**> (hints); *h != nullptr; ++h)
        {
            char* name = snd_device_name_get_hint (*h, "NAME");
            char* desc = snd_device_name_get_hint (*h, "DESC");

            if (name != nullptr)
                names.emplace_back (juce::String (name),
                                    desc != nullptr ? juce::String (desc) : juce::String ());

            std::free (name);
            std::free (desc);
        }

        snd_device_name_free_hint (hints);

        for (const auto& entry : names)
        {
            const auto& name = entry.first;

            if (!entry.second.containsIgnoreCase ("loopback") || !name.startsWith ("hw:CARD=") ||
                !name.contains (",DEV=0"))
                continue;

            const auto capture = name.replace (",DEV=0", ",DEV=1");

            for (const auto& other : names)
            {
                if (other.first == capture)
                {
                    pair.playback = name;
                    pair.capture = capture;
                    return true;
                }
            }
        }

        return false;
    }

    bool configureAlsaPcm (snd_pcm_t* pcm, double& sampleRate, int& periodSize, int& bufferSize)
    {
        snd_pcm_hw_params_t* hw = nullptr;
        snd_pcm_hw_params_alloca (&hw);

        if (snd_pcm_hw_params_any (pcm, hw) < 0)
            return false;

        if (snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE) < 0 ||
            snd_pcm_hw_params_set_channels (pcm, hw, 1) < 0)
            return false;

        unsigned int rate = static_cast<unsigned int> (sampleRate);

        if (snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, nullptr) < 0)
            return false;

        snd_pcm_uframes_t period = static_cast<snd_pcm_uframes_t> (juce::jmax (1, periodSize));
        snd_pcm_uframes_t buffer = static_cast<snd_pcm_uframes_t> (juce::jmax (period * 2, period));

        int dir = 0;

        if (snd_pcm_hw_params_set_period_size (pcm, hw, period, dir) < 0 &&
            snd_pcm_hw_params_set_period_size_near (pcm, hw, &period, &dir) < 0)
            return false;

        buffer = period * 2;

        if (snd_pcm_hw_params_set_buffer_size (pcm, hw, buffer) < 0 &&
            snd_pcm_hw_params_set_buffer_size_near (pcm, hw, &buffer) < 0)
            return false;

        if (snd_pcm_hw_params (pcm, hw) < 0)
            return false;

        snd_pcm_sw_params_t* sw = nullptr;
        snd_pcm_sw_params_alloca (&sw);

        if (snd_pcm_sw_params_current (pcm, sw) < 0 ||
            snd_pcm_sw_params_set_start_threshold (pcm, sw, 1) < 0 ||
            snd_pcm_sw_params (pcm, sw) < 0)
            return false;

        snd_pcm_uframes_t actualPeriod = 0;
        snd_pcm_uframes_t actualBuffer = 0;

        if (snd_pcm_hw_params_get_period_size (hw, &actualPeriod, &dir) < 0 ||
            snd_pcm_hw_params_get_buffer_size (hw, &actualBuffer) < 0)
            return false;

        sampleRate = static_cast<double> (rate);
        periodSize = static_cast<int> (actualPeriod);
        bufferSize = static_cast<int> (actualBuffer);
        return true;
    }

    int recoverAlsaPcm (snd_pcm_t* pcm, int error)
    {
        return snd_pcm_recover (pcm, error, 0);
    }

}

#endif
