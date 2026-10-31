// ihub-selftest: ALSA loopback audio smoke test.
//
// Plays one second of a 440 Hz sine at 48 kHz through the Loopback card's
// playback side, captures it from the matching capture side, writes out.wav,
// and verifies the capture:
//
//   * exactly 48,000 frames (1 s at 48 kHz)
//   * RMS within 1 dB of the generated source
//   * no silent gap (every 128-frame block carries signal)
//
// The playback/capture pair is the one documented in plan/TESTING.md:
// hw:Loopback,0,0 -> hw:Loopback,1,0. Device names are used deliberately;
// card indices are not stable across boots.

#include <alsa/asoundlib.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr unsigned int kSampleRate   = 48000;
constexpr unsigned int kChannels     = 1;
constexpr unsigned int kPeriodFrames = 128;
constexpr unsigned int kBufferFrames = 16384;
constexpr unsigned int kTotalFrames  = kSampleRate; // 1 second
constexpr double       kFrequency    = 440.0;
constexpr double       kAmplitude    = 0.5; // -6 dBFS sine

constexpr const char* kDefaultPlayback = "hw:Loopback,0,0";
constexpr const char* kDefaultCapture  = "hw:Loopback,1,0";
constexpr const char* kDefaultOutput   = "out.wav";

struct Options
{
    std::string playback = kDefaultPlayback;
    std::string capture  = kDefaultCapture;
    std::string output   = kDefaultOutput;
};

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

void putLe16 (std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back (static_cast<std::uint8_t> (value & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
}

void putLe32 (std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back (static_cast<std::uint8_t> (value & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 16) & 0xff));
    bytes.push_back (static_cast<std::uint8_t> ((value >> 24) & 0xff));
}

void appendFourCC (std::vector<std::uint8_t>& bytes, const char* text)
{
    bytes.insert (bytes.end(), text, text + 4);
}

void writeWav (const std::string& path, const std::vector<std::int16_t>& samples)
{
    const std::uint32_t dataBytes = static_cast<std::uint32_t> (samples.size() * sizeof (std::int16_t));
    std::vector<std::uint8_t> bytes;
    bytes.reserve (44 + dataBytes);

    appendFourCC (bytes, "RIFF");
    putLe32 (bytes, 36 + dataBytes);
    appendFourCC (bytes, "WAVE");
    appendFourCC (bytes, "fmt ");
    putLe32 (bytes, 16);
    putLe16 (bytes, 1); // PCM
    putLe16 (bytes, kChannels);
    putLe32 (bytes, kSampleRate);
    putLe32 (bytes, kSampleRate * kChannels * sizeof (std::int16_t));
    putLe16 (bytes, static_cast<std::uint16_t> (kChannels * sizeof (std::int16_t)));
    putLe16 (bytes, 16);
    appendFourCC (bytes, "data");
    putLe32 (bytes, dataBytes);

    for (const auto sample : samples)
    {
        const auto value = static_cast<std::uint16_t> (sample);
        bytes.push_back (static_cast<std::uint8_t> (value & 0xff));
        bytes.push_back (static_cast<std::uint8_t> ((value >> 8) & 0xff));
    }

    std::ofstream file (path, std::ios::binary | std::ios::trunc);

    if (! file)
        throw std::runtime_error ("cannot open " + path + " for writing");

    file.write (reinterpret_cast<const char*> (bytes.data()),
                static_cast<std::streamsize> (bytes.size()));

    if (! file)
        throw std::runtime_error ("failed while writing " + path);
}

double rms (const std::int16_t* samples, std::size_t count)
{
    if (count == 0)
        return 0.0;

    double sum = 0.0;

    for (std::size_t i = 0; i < count; ++i)
    {
        const double value = static_cast<double> (samples[i]);
        sum += value * value;
    }

    return std::sqrt (sum / static_cast<double> (count));
}

struct Report
{
    bool framesOk = false;
    bool rmsOk = false;
    bool noGap = false;
    double sourceRms = 0.0;
    double captureRms = 0.0;
    double rmsDb = 0.0;
    unsigned int silentBlocks = 0;
    unsigned int longestSilentRun = 0;
    unsigned int firstSilentBlock = std::numeric_limits<unsigned int>::max();
};

Report analyse (const std::vector<std::int16_t>& source,
                const std::vector<std::int16_t>& capture)
{
    Report report;
    report.framesOk = capture.size() == kTotalFrames;
    report.sourceRms = rms (source.data(), source.size());
    report.captureRms = rms (capture.data(), capture.size());

    if (report.sourceRms > 0.0 && report.captureRms > 0.0)
        report.rmsDb = 20.0 * std::log10 (report.captureRms / report.sourceRms);
    else
        report.rmsDb = -std::numeric_limits<double>::infinity();

    report.rmsOk = std::abs (report.rmsDb) <= 1.0;

    // A gap is any 128-frame block whose RMS collapses below -40 dB of the
    // source. A 440 Hz sine spans more than a full cycle per block, so a
    // healthy block never gets close to this threshold.
    const double gapThreshold = std::max (report.sourceRms * 0.01, 1.0);
    unsigned int currentRun = 0;
    const auto blocks = capture.size() / kPeriodFrames;

    for (std::size_t block = 0; block < blocks; ++block)
    {
        const double blockRms = rms (capture.data() + block * kPeriodFrames, kPeriodFrames);

        if (blockRms < gapThreshold)
        {
            ++report.silentBlocks;

            if (report.firstSilentBlock == std::numeric_limits<unsigned int>::max())
                report.firstSilentBlock = static_cast<unsigned int> (block);

            ++currentRun;
            report.longestSilentRun = std::max (report.longestSilentRun, currentRun);
        }
        else
        {
            currentRun = 0;
        }
    }

    report.noGap = report.silentBlocks == 0;
    return report;
}

void printUsage (const char* argv0)
{
    std::cout << "usage: " << argv0 << " [--out FILE] [--playback DEVICE] [--capture DEVICE]\n"
              << "  defaults: --out out.wav --playback " << kDefaultPlayback
              << " --capture " << kDefaultCapture << std::endl;
}

bool parseOptions (int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            printUsage (argv[0]);
            return false;
        }

        if (arg == "--out" && i + 1 < argc)
            options.output = argv[++i];
        else if (arg == "--playback" && i + 1 < argc)
            options.playback = argv[++i];
        else if (arg == "--capture" && i + 1 < argc)
            options.capture = argv[++i];
        else
            throw std::runtime_error ("unknown or incomplete argument: " + arg);
    }

    return true;
}
} // namespace

int main (int argc, char** argv)
{
    Options options;

    try
    {
        if (! parseOptions (argc, argv, options))
            return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ihub-selftest: " << e.what() << std::endl;
        printUsage (argv[0]);
        return 2;
    }

    snd_pcm_t* playback = nullptr;
    snd_pcm_t* capture = nullptr;

    try
    {
        const auto source = makeSine();

        playback = openPcm (options.playback, SND_PCM_STREAM_PLAYBACK);
        capture = openPcm (options.capture, SND_PCM_STREAM_CAPTURE);

        configurePcm (playback, options.playback);
        configurePcm (capture, options.capture);

        checkAlsa (snd_pcm_prepare (playback), "prepare playback");
        checkAlsa (snd_pcm_prepare (capture), "prepare capture");

        std::vector<std::int16_t> captured (kTotalFrames, 0);
        std::atomic<unsigned int> playbackXruns { 0 };
        std::atomic<unsigned int> captureXruns { 0 };
        std::atomic<std::size_t> playbackWritten { 0 };
        std::atomic<bool> failed { false };

        // Prime the loopback ring before starting capture. Both streams
        // auto-start on their first read/write; starting capture with no data
        // queued would immediately raise EPIPE on the loopback's capture side.
        // The 16k-frame ring gives several hundred milliseconds of slack, so
        // waiting for the first period cannot wrap or drop the beginning.
        std::thread playbackThread (playbackLoop, playback, std::cref (source),
                                    std::ref (playbackXruns), std::ref (failed),
                                    std::ref (playbackWritten));

        while (playbackWritten.load (std::memory_order_acquire) < kPeriodFrames
               && ! failed.load (std::memory_order_acquire))
            std::this_thread::yield();

        if (failed.load (std::memory_order_acquire))
        {
            playbackThread.join();
            throw std::runtime_error ("playback failed before capture started");
        }

        std::thread captureThread (captureLoop, capture, std::ref (captured),
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

        writeWav (options.output, captured);
        std::cout << "wav: wrote " << options.output << " ("
                  << (captured.size() / kChannels) << " frames, "
                  << kSampleRate << " Hz, " << kChannels << " ch, 16-bit)" << std::endl;

        if (failed)
            throw std::runtime_error ("an ALSA stream failed during the transfer");

        const Report report = analyse (source, captured);

        std::cout << "check: frames=" << (captured.size() / kChannels)
                  << " expected=" << kTotalFrames
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
