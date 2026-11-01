#include "Alsa.h"
#include "Analysis.h"
#include "Constants.h"
#include "Options.h"
#include "Wav.h"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main (int argc, char** argv)
{
    SelfTest::Options options;

    try
    {
        if (! SelfTest::parseOptions (argc, argv, options))
            return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ihub-selftest: " << e.what() << std::endl;
        SelfTest::printUsage (argv[0]);
        return 2;
    }

    snd_pcm_t* playback = nullptr;
    snd_pcm_t* capture = nullptr;

    try
    {
        const auto source = SelfTest::makeSine();

        playback = SelfTest::openPcm (options.playback, SND_PCM_STREAM_PLAYBACK);
        capture = SelfTest::openPcm (options.capture, SND_PCM_STREAM_CAPTURE);

        SelfTest::configurePcm (playback, options.playback);
        SelfTest::configurePcm (capture, options.capture);

        SelfTest::checkAlsa (snd_pcm_prepare (playback), "prepare playback");
        SelfTest::checkAlsa (snd_pcm_prepare (capture), "prepare capture");

        std::vector<std::int16_t> captured (SelfTest::kTotalFrames, 0);
        std::atomic<unsigned int> playbackXruns { 0 };
        std::atomic<unsigned int> captureXruns { 0 };
        std::atomic<std::size_t> playbackWritten { 0 };
        std::atomic<bool> failed { false };

        std::thread playbackThread (SelfTest::playbackLoop, playback, std::cref (source),
                                    std::ref (playbackXruns), std::ref (failed),
                                    std::ref (playbackWritten));

        while (playbackWritten.load (std::memory_order_acquire) < SelfTest::kPeriodFrames
               && ! failed.load (std::memory_order_acquire))
            std::this_thread::yield();

        if (failed.load (std::memory_order_acquire))
        {
            playbackThread.join();
            throw std::runtime_error ("playback failed before capture started");
        }

        std::thread captureThread (SelfTest::captureLoop, capture, std::ref (captured),
                                   std::ref (captureXruns), std::ref (failed));

        playbackThread.join();
        captureThread.join();

        snd_pcm_drop (playback);
        snd_pcm_drop (capture);
        snd_pcm_close (playback);
        snd_pcm_close (capture);
        playback = nullptr;
        capture = nullptr;

        std::cout << "alsa: playback_xruns=" << playbackXruns.load()
                  << " capture_xruns=" << captureXruns.load() << std::endl;

        SelfTest::writeWav (options.output, captured);
        std::cout << "wav: wrote " << options.output << " ("
                  << (captured.size() / SelfTest::kChannels) << " frames, "
                  << SelfTest::kSampleRate << " Hz, " << SelfTest::kChannels
                  << " ch, 16-bit)" << std::endl;

        if (failed)
            throw std::runtime_error ("an ALSA stream failed during the transfer");

        const SelfTest::Report report = SelfTest::analyse (source, captured);

        std::cout << "check: frames=" << (captured.size() / SelfTest::kChannels)
                  << " expected=" << SelfTest::kTotalFrames
                  << " " << (report.framesOk ? "ok" : "FAIL") << std::endl;
        std::cout << "check: rms source=" << report.sourceRms
                  << " capture=" << report.captureRms
                  << " delta=" << report.rmsDb << " dB "
                  << (report.rmsOk ? "ok" : "FAIL") << std::endl;
        std::cout << "check: silent_blocks=" << report.silentBlocks
                  << " longest_run=" << report.longestSilentRun;

        if (! report.noGap)
            std::cout << " first_block=" << report.firstSilentBlock;

        std::cout << " " << (report.noGap ? "ok" : "FAIL") << std::endl;

        const bool passed = report.framesOk && report.rmsOk && report.noGap;

        std::cout << (passed ? "PASS" : "FAIL") << ": ihub-selftest audio smoke"
                  << std::endl;

        return passed ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;

        if (playback != nullptr)
            snd_pcm_close (playback);

        if (capture != nullptr)
            snd_pcm_close (capture);

        return 1;
    }
}
