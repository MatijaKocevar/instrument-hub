#include "Alsa.h"
#include "Fixtures.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace AudioInputTest
{

std::string alsaError (const char* what, int err)
{
    return std::string (what) + ": " + snd_strerror (err);
}

void checkAlsa (int err, const char* what)
{
    if (err < 0)
        throw std::runtime_error (alsaError (what, err));
}

snd_pcm_t* openPcm (const char* device, snd_pcm_stream_t stream)
{
    snd_pcm_t* pcm = nullptr;
    const int err = snd_pcm_open (&pcm, device, stream, 0);

    if (err < 0)
        throw std::runtime_error (alsaError (std::string ("open ").append (device).c_str(),
                                             err));

    return pcm;
}

void configurePcm (snd_pcm_t* pcm, const char* device)
{
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca (&hw);

    checkAlsa (snd_pcm_hw_params_any (pcm, hw),
               (std::string ("no configurations for ").append (device)).c_str());
    checkAlsa (snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED),
               (std::string ("interleaved access for ").append (device)).c_str());
    checkAlsa (snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE),
               (std::string ("S16_LE for ").append (device)).c_str());
    checkAlsa (snd_pcm_hw_params_set_channels (pcm, hw, kChannels),
               (std::string ("channel count for ").append (device)).c_str());

    unsigned int rate = static_cast<unsigned int> (kSampleRate);
    checkAlsa (snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, nullptr),
               (std::string ("sample rate for ").append (device)).c_str());

    if (rate != static_cast<unsigned int> (kSampleRate))
        throw std::runtime_error (std::string ("device ").append (device)
                                  + " selected " + std::to_string (rate)
                                  + " Hz, not 48000 Hz");

    snd_pcm_uframes_t period = kPeriodFrames;
    checkAlsa (snd_pcm_hw_params_set_period_size_near (pcm, hw, &period, nullptr),
               (std::string ("period size for ").append (device)).c_str());

    snd_pcm_uframes_t buffer = kBufferFrames;
    checkAlsa (snd_pcm_hw_params_set_buffer_size_near (pcm, hw, &buffer),
               (std::string ("buffer size for ").append (device)).c_str());

    checkAlsa (snd_pcm_hw_params (pcm, hw),
               (std::string ("hardware parameters for ").append (device)).c_str());

    snd_pcm_sw_params_t* sw = nullptr;
    snd_pcm_sw_params_alloca (&sw);
    checkAlsa (snd_pcm_sw_params_current (pcm, sw),
               (std::string ("software parameters for ").append (device)).c_str());
    checkAlsa (snd_pcm_sw_params_set_start_threshold (pcm, sw, 1),
               (std::string ("start threshold for ").append (device)).c_str());
    checkAlsa (snd_pcm_sw_params (pcm, sw),
               (std::string ("software parameters for ").append (device)).c_str());
}

std::vector<std::int16_t> makeSine()
{
    std::vector<std::int16_t> samples (kTotalFrames);

    for (unsigned int i = 0; i < kTotalFrames; ++i)
    {
        const double phase = 2.0 * std::numbers::pi * kFrequency
                             * static_cast<double> (i) / kSampleRate;
        const double value = kAmplitude * std::sin (phase) * 32767.0;
        samples[i] = static_cast<std::int16_t> (std::lround (value));
    }

    return samples;
}

void playbackLoop (snd_pcm_t* pcm,
                   const std::vector<std::int16_t>& source,
                   std::atomic<unsigned int>& xruns,
                   std::atomic<bool>& failed,
                   std::atomic<std::size_t>& writtenFrames)
{
    std::size_t offset = 0;

    while (offset < source.size())
    {
        const auto remaining = source.size() - offset;
        const auto want = static_cast<snd_pcm_uframes_t> (
            std::min<std::size_t> (remaining, kPeriodFrames));

        const snd_pcm_sframes_t written = snd_pcm_writei (pcm, source.data() + offset, want);

        if (written < 0)
        {
            ++xruns;
            const int recovered = snd_pcm_recover (pcm, static_cast<int> (written), 0);

            if (recovered < 0)
            {
                std::cerr << "alsa: playback write failed: "
                          << snd_strerror (static_cast<int> (written)) << std::endl;
                failed = true;
                return;
            }

            continue;
        }

        offset += static_cast<std::size_t> (written);
        writtenFrames.store (offset, std::memory_order_release);
    }
}

}
