#include "Alsa.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace SelfTest
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

snd_pcm_t* openPcm (const std::string& device, snd_pcm_stream_t stream)
{
    snd_pcm_t* pcm = nullptr;
    const int err = snd_pcm_open (&pcm, device.c_str(), stream, 0);

    if (err < 0)
        throw std::runtime_error (alsaError (("open " + device).c_str(), err));

    return pcm;
}

void configurePcm (snd_pcm_t* pcm, const std::string& device)
{
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca (&hw);

    checkAlsa (snd_pcm_hw_params_any (pcm, hw),
               ("no configurations for " + device).c_str());
    checkAlsa (snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED),
               ("interleaved access for " + device).c_str());
    checkAlsa (snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE),
               ("S16_LE for " + device).c_str());
    checkAlsa (snd_pcm_hw_params_set_channels (pcm, hw, kChannels),
               ("channel count for " + device).c_str());

    unsigned int rate = kSampleRate;
    checkAlsa (snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, nullptr),
               ("sample rate for " + device).c_str());

    if (rate != kSampleRate)
        throw std::runtime_error ("device " + device + " selected "
                                  + std::to_string (rate) + " Hz, not 48000 Hz");

    snd_pcm_uframes_t period = kPeriodFrames;
    checkAlsa (snd_pcm_hw_params_set_period_size_near (pcm, hw, &period, nullptr),
               ("period size for " + device).c_str());

    snd_pcm_uframes_t buffer = kBufferFrames;
    checkAlsa (snd_pcm_hw_params_set_buffer_size_near (pcm, hw, &buffer),
               ("buffer size for " + device).c_str());

    checkAlsa (snd_pcm_hw_params (pcm, hw),
               ("hardware parameters for " + device).c_str());

    snd_pcm_sw_params_t* sw = nullptr;
    snd_pcm_sw_params_alloca (&sw);
    checkAlsa (snd_pcm_sw_params_current (pcm, sw),
               ("software parameters for " + device).c_str());
    checkAlsa (snd_pcm_sw_params_set_start_threshold (pcm, sw, 1),
               ("start threshold for " + device).c_str());
    checkAlsa (snd_pcm_sw_params (pcm, sw),
               ("software parameters for " + device).c_str());

    std::cout << "alsa: " << device << " rate=" << rate
              << " period=" << period << " buffer=" << buffer
              << " channels=" << kChannels << " format=S16_LE" << std::endl;
}

std::vector<std::int16_t> makeSine()
{
    std::vector<std::int16_t> samples (kTotalFrames);

    for (unsigned int i = 0; i < kTotalFrames; ++i)
    {
        const double phase = 2.0 * std::numbers::pi * kFrequency * static_cast<double> (i)
                             / static_cast<double> (kSampleRate);
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
        const auto want = static_cast<snd_pcm_sframes_t> (
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

void captureLoop (snd_pcm_t* pcm,
                  std::vector<std::int16_t>& destination,
                  std::atomic<unsigned int>& xruns,
                  std::atomic<bool>& failed)
{
    std::size_t offset = 0;

    while (offset < destination.size())
    {
        const auto remaining = destination.size() - offset;
        const auto want = static_cast<snd_pcm_sframes_t> (
            std::min<std::size_t> (remaining, kPeriodFrames));

        const snd_pcm_sframes_t read = snd_pcm_readi (pcm, destination.data() + offset, want);

        if (read < 0)
        {
            ++xruns;
            const int recovered = snd_pcm_recover (pcm, static_cast<int> (read), 0);

            if (recovered < 0)
            {
                std::cerr << "alsa: capture read failed: "
                          << snd_strerror (static_cast<int> (read)) << std::endl;
                failed = true;
                return;
            }

            continue;
        }

        offset += static_cast<std::size_t> (read);
    }
}

} // namespace SelfTest
