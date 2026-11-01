#include "LoopbackMaster.h"
#include "Alsa.h"
#include "Fixtures.h"
#include "TestSupport.h"

#include "audio/AudioEngine.h"
#include "audio/Bus.h"
#include "audio/Slot.h"
#include "instruments/AudioInputInstrument.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace AudioInputTest
{

void runLoopbackThroughMasterBus()
{
    std::cout << "\n-- loopback sine on the master bus --" << std::endl;

    constexpr const char* playbackDevice = "hw:Loopback,0,0";
    constexpr const char* captureDevice = "hw:Loopback,1,0";

    snd_pcm_t* playback = nullptr;
    snd_pcm_t* capture = nullptr;

    try
    {
        const auto source = makeSine();

        playback = openPcm (playbackDevice, SND_PCM_STREAM_PLAYBACK);
        capture = openPcm (captureDevice, SND_PCM_STREAM_CAPTURE);

        configurePcm (playback, playbackDevice);
        configurePcm (capture, captureDevice);

        checkAlsa (snd_pcm_prepare (playback), "prepare playback");
        checkAlsa (snd_pcm_prepare (capture), "prepare capture");

        std::atomic<unsigned int> playbackXruns { 0 };
        std::atomic<bool> failed { false };
        std::atomic<std::size_t> writtenFrames { 0 };

        std::thread playbackThread (playbackLoop, playback, std::cref (source),
                                    std::ref (playbackXruns), std::ref (failed),
                                    std::ref (writtenFrames));

        while (writtenFrames.load (std::memory_order_acquire) < kPeriodFrames
               && ! failed.load (std::memory_order_acquire))
            std::this_thread::yield();

        if (failed.load (std::memory_order_acquire))
            throw std::runtime_error ("playback failed before capture started");

        AudioInputInstrument input;
        input.setInputChannel (0);
        input.setLatencySamples (0);
        input.prepare (kSampleRate, kBlockSize);

        AudioEngine::Bus bus;
        bus.prepare (kSampleRate, kBlockSize, 2);

        AudioEngine::Slot slot (0);
        slot.setSource (&input);
        slot.prepare (kSampleRate, kBlockSize, 2);
        slot.activate();

        juce::AudioBuffer<float> capturedBlock (1, kBlockSize);
        juce::AudioBuffer<float> rendered (2, kBlockSize);
        juce::MidiBuffer midi;

        std::vector<std::int16_t> raw (kPeriodFrames, 0);

        std::size_t received = 0;
        int blockIndex = 0;
        int compared = 0;
        double inputPeak = 0.0;
        double maxDifference = 0.0;
        double sumSquaredDifference = 0.0;
        unsigned int captureXruns = 0;

        while (received < kTotalFrames)
        {
            const auto remaining = kTotalFrames - received;
            const auto want = static_cast<snd_pcm_uframes_t> (
                std::min<std::size_t> (remaining, kPeriodFrames));

            const snd_pcm_sframes_t read = snd_pcm_readi (capture, raw.data(), want);

            if (read < 0)
            {
                ++captureXruns;
                const int recovered = snd_pcm_recover (capture, static_cast<int> (read), 0);

                if (recovered < 0)
                    throw std::runtime_error (
                        alsaError ("capture read", static_cast<int> (read)));

                continue;
            }

            const int samples = static_cast<int> (read);
            capturedBlock.setSize (1, samples, false, true, true);
            auto* destination = capturedBlock.getWritePointer (0);

            for (int i = 0; i < samples; ++i)
                destination[i] = static_cast<float> (raw[static_cast<std::size_t> (i)])
                                 / 32768.0f;

            const float* channels[] = { capturedBlock.getReadPointer (0) };
            input.setInputBlock (channels, 1);

            const bool produced = slot.process (samples, midi);
            bus.beginBlock (samples);

            if (produced)
                bus.add (slot.getOutputBuffer(), samples);

            bus.finish (rendered, samples);

            if (blockIndex >= kFadeBlocks)
            {
                for (int channel = 0; channel < 2; ++channel)
                {
                    for (int i = 0; i < samples; ++i)
                    {
                        const double capturedSample = static_cast<double> (destination[i]);
                        const double busSample = static_cast<double> (
                            rendered.getSample (channel, i));
                        const double difference = busSample - capturedSample;

                        inputPeak = std::max (inputPeak, std::abs (capturedSample));
                        maxDifference = std::max (maxDifference, std::abs (difference));
                        sumSquaredDifference += difference * difference;
                        ++compared;
                    }
                }
            }

            received += static_cast<std::size_t> (read);
            ++blockIndex;
        }

        playbackThread.join();

        snd_pcm_drop (playback);
        snd_pcm_drop (capture);
        snd_pcm_close (playback);
        snd_pcm_close (capture);
        playback = nullptr;
        capture = nullptr;

        const double rmsDifference = compared > 0
                                         ? std::sqrt (sumSquaredDifference
                                                      / static_cast<double> (compared))
                                         : std::numeric_limits<double>::infinity();

        std::cout << "loopback: captured " << received << " frames, compared "
                  << compared << " samples, input peak " << inputPeak
                  << ", max difference " << maxDifference
                  << ", RMS difference " << dbfs (rmsDifference) << " dBFS, xruns "
                  << playbackXruns.load() << "/" << captureXruns << std::endl;

        check (! failed.load(), "loopback playback completed without failure");
        check (received == kTotalFrames, "captured exactly 1 s at 48 kHz");
        check (inputPeak > 0.4,
               "loopback capture carries the injected sine (peak > 0.4)");
        check (slot.getState() == AudioEngine::SlotState::active,
               "audio input slot is active through the master bus");
        check (compared > 0, "master bus was compared against the captured input");
        check (maxDifference <= 0.0,
               "master bus reproduces the loopback sine at bit-exact unity gain");
        check (dbfs (rmsDifference) < -120.0,
               "RMS difference from the loopback input is below -120 dBFS");
    }
    catch (const std::exception& e)
    {
        if (playback != nullptr)
            snd_pcm_close (playback);

        if (capture != nullptr)
            snd_pcm_close (capture);

        throw;
    }
}

} // namespace AudioInputTest
