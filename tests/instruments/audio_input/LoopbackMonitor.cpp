#include "LoopbackMonitor.h"
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
#include <stdexcept>
#include <thread>
#include <vector>

namespace AudioInputTest
{

void runLoopbackThroughFxChainAndDirectMonitor()
{
    std::cout << "\n-- loopback: input -> FX -> bus, then direct monitor --" << std::endl;

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
        input.prepare (kSampleRate, kBlockSize);

        GainFxProcessor fx (0.5f, 240);

        AudioEngine::Bus bus;
        bus.prepare (kSampleRate, kBlockSize, 2);

        AudioEngine::Slot slot (0);
        slot.setSource (&input);
        slot.prepare (kSampleRate, kBlockSize, 2);
        check (slot.getFxChain().add (&fx) == 0,
               "the loopback slot hosts the gain effect");
        slot.activate();

        check (slot.latencySamples() == 240,
               "the chain's 5 ms is in the slot total before monitoring");

        juce::AudioBuffer<float> capturedBlock (1, kBlockSize);
        juce::AudioBuffer<float> rendered (2, kBlockSize);
        juce::MidiBuffer midi;

        std::vector<std::int16_t> raw (kPeriodFrames, 0);

        constexpr std::size_t monitorFromFrame = kTotalFrames / 2;

        std::size_t received = 0;
        int blockIndex = 0;
        int comparedFx = 0;
        int comparedDirect = 0;
        double inputPeak = 0.0;
        double maxDifferenceFx = 0.0;
        double maxDifferenceDirect = 0.0;
        unsigned int captureXruns = 0;
        bool direct = false;
        int fxCallsAtDirectStart = 0;

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

            if (! direct && received >= monitorFromFrame)
            {
                direct = true;
                input.setDirectMonitor (true);
                fxCallsAtDirectStart = fx.getProcessCalls();
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

                        inputPeak = std::max (inputPeak, std::abs (capturedSample));

                        if (direct)
                        {
                            maxDifferenceDirect = std::max (
                                maxDifferenceDirect,
                                std::abs (busSample - capturedSample));
                            ++comparedDirect;
                        }
                        else
                        {
                            maxDifferenceFx = std::max (
                                maxDifferenceFx,
                                std::abs (busSample - 0.5 * capturedSample));
                            ++comparedFx;
                        }
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

        const int fxCallsAfter = fx.getProcessCalls();

        std::cout << "loopback: captured " << received << " frames, compared "
                  << comparedFx << " FX + " << comparedDirect << " direct samples"
                  << ", input peak " << inputPeak
                  << ", max difference " << maxDifferenceFx << " / "
                  << maxDifferenceDirect << ", direct "
                  << (direct ? "on" : "off") << ", xruns "
                  << playbackXruns.load() << "/" << captureXruns << std::endl;

        check (! failed.load(), "loopback playback completed without failure");
        check (received == kTotalFrames, "captured exactly 1 s at 48 kHz");
        check (inputPeak > 0.4,
               "loopback capture carries the injected sine (peak > 0.4)");
        check (comparedFx > 0, "part of the capture ran through the FX chain");
        check (direct && comparedDirect > 0,
               "part of the capture ran in direct-monitor mode");
        check (maxDifferenceFx < 1.0e-7,
               "input -> FX -> bus applies the chain gain on the loopback");
        check (maxDifferenceDirect <= 0.0,
               "direct monitor reproduces the loopback input bit-exactly");
        check (fxCallsAfter == fxCallsAtDirectStart,
               "the heavy chain does not run while direct monitoring");
        check (slot.latencySamples() == 0,
               "direct monitor excludes the chain's reported latency");
    }
    catch (const std::exception&)
    {
        if (playback != nullptr)
            snd_pcm_close (playback);

        if (capture != nullptr)
            snd_pcm_close (capture);

        throw;
    }
}

} // namespace AudioInputTest
