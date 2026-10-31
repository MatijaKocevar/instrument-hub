// ihub-instrument-test: acceptance checks for T030 and T070.
//
// 1. Proves `AudioInputInstrument` copies one interface channel to every
//    output channel at bit-exact unity gain, applies its optional gain, selects
//    the requested input channel, clears with no input, and reports its own
//    cached latency and AudioInput kind.
// 2. T070: an audio-input slot runs the instrument -> FX chain -> bus path, a
//    direct-monitor request makes the slot skip the chain (no processBlock
//    call, no chain latency, bit-exact input), and the instrument enforces its
//    own latency budget. AudioEngine::audioDeviceIOCallbackWithContext feeds
//    the device input to the slot without a manual hand-off.
// 3. Plays a 440 Hz sine into the ALSA Loopback playback side, captures it
//    from the matching capture side, feeds every captured block through the
//    instrument and the fixed slot/bus graph, and requires the master bus to
//    reproduce the captured input at unity gain.
// 4. T070 acceptance: the same loopback runs the first half through a gain
//    effect (input -> FX -> bus) and the second half in direct-monitor mode,
//    where the chain is bypassed and the input is reproduced bit-exactly.
//
// The loopback pair is the one documented in plan/TESTING.md:
// hw:Loopback,0,0 -> hw:Loopback,1,0. Device names are used deliberately;
// card indices are not stable across boots.

#include "audio/AudioEngine.h"
#include "audio/Bus.h"
#include "audio/Slot.h"
#include "instruments/AudioInputInstrument.h"

#include <alsa/asoundlib.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using ihub::AudioInputInstrument;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr unsigned int kChannels = 1;
constexpr unsigned int kPeriodFrames = 128;
constexpr unsigned int kBufferFrames = 16384;
constexpr unsigned int kTotalFrames = 48000; // 1 second
constexpr double kFrequency = 440.0;
constexpr double kAmplitude = 0.5;
constexpr int kFadeBlocks = 8; // 1024 frames > the slot's 3 ms (144 frame) fade

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

double dbfs (double linear)
{
    if (linear <= 0.0)
        return -200.0;

    return 20.0 * std::log10 (linear);
}

void fillSine (juce::AudioBuffer<float>& buffer, int channel)
{
    auto* samples = buffer.getWritePointer (channel);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double phase = 2.0 * std::numbers::pi * kFrequency
                             * static_cast<double> (i) / kSampleRate;
        samples[i] = static_cast<float> (kAmplitude * std::sin (phase));
    }
}

/** A deterministic stereo effect used as the slot's heavy chain.

    It scales by `gain` and reports (without actually inserting) `latency`
    samples, so the test can prove both that the signal went through the chain
    and that direct monitoring removed the chain's latency. Every processBlock
    call is counted: a bypassed chain must not run at all, even if its output
    would otherwise look unchanged.
*/
class GainFxProcessor final : public juce::AudioProcessor
{
public:
    explicit GainFxProcessor (float gainToUse = 1.0f, int latencyToReport = 0)
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          gain (gainToUse),
          latency (latencyToReport)
    {
    }

    const juce::String getName() const override { return "GainFxProcessor"; }

    void prepareToPlay (double, int) override
    {
        // Reporting latency here (rather than delaying samples) keeps the
        // loopback comparison aligned while still exercising the accounting.
        setLatencySamples (latency);
    }

    void releaseResources() override {}

    using juce::AudioProcessor::processBlock;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        processCalls.fetch_add (1, std::memory_order_relaxed);

        if (std::abs (gain - 1.0f) > 0.0f)
            buffer.applyGain (gain);
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    int getProcessCalls() const noexcept
    {
        return processCalls.load (std::memory_order_relaxed);
    }

private:
    float gain = 1.0f;
    int latency = 0;
    std::atomic<int> processCalls { 0 };
};

//==============================================================================
void testUnityGainIntoBus()
{
    std::cout << "\n-- unity gain through slot and master bus --" << std::endl;

    AudioInputInstrument input;
    input.setInputChannel (0);
    input.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> source (1, kBlockSize);
    fillSine (source, 0);

    AudioEngine::Bus bus;
    bus.prepare (kSampleRate, kBlockSize, 2);

    AudioEngine::Slot slot (0);
    slot.setSource (&input);
    slot.prepare (kSampleRate, kBlockSize, 2);
    slot.activate();

    juce::AudioBuffer<float> rendered (2, kBlockSize);
    juce::MidiBuffer midi;

    double maxDifference = 0.0;
    int processedBlocks = 0;

    for (int blockIndex = 0; blockIndex < kFadeBlocks; ++blockIndex)
    {
        const float* channels[] = { source.getReadPointer (0) };
        input.setInputBlock (channels, 1);

        if (slot.process (kBlockSize, midi))
            ++processedBlocks;

        bus.beginBlock (kBlockSize);
        bus.add (slot.getOutputBuffer(), kBlockSize);
        bus.finish (rendered, kBlockSize);

        if (blockIndex == kFadeBlocks - 1)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < kBlockSize; ++i)
                    maxDifference = std::max (maxDifference,
                                              std::abs (static_cast<double> (
                                                            rendered.getSample (channel, i))
                                                        - static_cast<double> (
                                                            source.getSample (0, i))));
        }
    }

    check (processedBlocks == kFadeBlocks,
           "slot processes the audio input through the fade");
    check (slot.getState() == AudioEngine::SlotState::active,
           "slot is active after its fade ramp");
    check (maxDifference <= 0.0,
           "master bus output is bit-identical to the input at unity gain");
}

void testGainAndLatency()
{
    std::cout << "\n-- optional gain and own latency accounting --" << std::endl;

    AudioInputInstrument input;
    input.setGain (0.25f);
    input.setLatencySamples (96);
    input.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> source (1, kBlockSize);
    fillSine (source, 0);

    const float* channels[] = { source.getReadPointer (0) };
    input.setInputBlock (channels, 1);

    juce::AudioBuffer<float> rendered (2, kBlockSize);
    juce::MidiBuffer midi;
    input.process (rendered, midi);

    double maxDifference = 0.0;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            maxDifference = std::max (
                maxDifference,
                std::abs (static_cast<double> (rendered.getSample (channel, i))
                          - 0.25 * static_cast<double> (source.getSample (0, i))));

    check (maxDifference < 1.0e-7, "gain 0.25 is applied to both output channels");
    check (std::abs (input.getGain() - 0.25f) <= 0.0f,
           "getGain() reports the linear gain");
    check (input.latencySamples() == 96,
           "latencySamples() reports the cached input-path latency");
    check (input.kind() == ihub::Instrument::Kind::AudioInput,
           "kind() reports Instrument::Kind::AudioInput");
    check (std::string (ihub::toString (input.kind())) == "audio-input",
           "toString() is stable for logs and tests");

    input.noteOn (1, 60, 1.0f);
    input.noteOff (1, 60);
}

void testChannelSelectionAndMissingInput()
{
    std::cout << "\n-- channel selection and missing input --" << std::endl;

    AudioInputInstrument input;
    input.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> twoChannels (2, kBlockSize);
    twoChannels.clear();
    juce::FloatVectorOperations::fill (twoChannels.getWritePointer (0), 0.1f, kBlockSize);
    juce::FloatVectorOperations::fill (twoChannels.getWritePointer (1), 0.2f, kBlockSize);

    const float* channels[] = { twoChannels.getReadPointer (0),
                                twoChannels.getReadPointer (1) };

    juce::AudioBuffer<float> rendered (2, kBlockSize);
    juce::MidiBuffer midi;

    input.setInputChannel (1);
    input.setInputBlock (channels, 2);
    input.process (rendered, midi);

    check (std::abs (rendered.getSample (0, 0) - 0.2f) <= 0.0f
               && std::abs (rendered.getSample (1, 0) - 0.2f) <= 0.0f,
           "the selected interface channel is copied to the output");

    input.setInputChannel (5);
    input.setInputBlock (channels, 2);
    input.process (rendered, midi);

    check (rendered.getMagnitude (0, kBlockSize) <= 0.0f,
           "an out-of-range channel renders silence");

    rendered.clear();
    rendered.getWritePointer (0)[0] = 0.5f;
    input.process (rendered, midi);

    check (rendered.getMagnitude (0, kBlockSize) <= 0.0f,
           "no input block renders silence (the hand-off is one block only)");
}

//==============================================================================
void testDirectMonitorBypassesFxChain()
{
    std::cout << "\n-- direct monitoring bypasses the FX chain --" << std::endl;

    AudioInputInstrument input;
    input.prepare (kSampleRate, kBlockSize);
    input.setLatencySamples (96);

    GainFxProcessor fx (0.5f, 240); // half gain, reports 5 ms

    AudioEngine::Slot slot (0);
    slot.setSource (&input);
    slot.prepare (kSampleRate, kBlockSize, 2);
    check (slot.getFxChain().add (&fx) == 0, "the gain effect is added to the slot chain");
    slot.activate();

    juce::AudioBuffer<float> source (1, kBlockSize);
    fillSine (source, 0);

    juce::MidiBuffer midi;

    const float* channels[] = { source.getReadPointer (0) };

    // Fade in with the chain in the audible path.
    for (int block = 0; block < kFadeBlocks; ++block)
    {
        input.setInputBlock (channels, 1);
        slot.process (kBlockSize, midi);
    }

    const int callsAfterFade = fx.getProcessCalls();

    check (callsAfterFade >= kFadeBlocks, "the chain runs every block in normal mode");
    check (slot.latencySamples() == 96 + 240,
           "normal mode reports input plus chain latency");

    double fxDifference = 0.0;
    const auto& processed = slot.getOutputBuffer();

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            fxDifference = std::max (
                fxDifference,
                std::abs (static_cast<double> (processed.getSample (channel, i))
                          - 0.5 * static_cast<double> (source.getSample (0, i))));

    check (fxDifference < 1.0e-7, "input -> FX -> bus applies the chain gain");

    // Direct monitoring: the very next block must skip the chain.
    input.setDirectMonitor (true);
    check (input.isDirectMonitor(), "direct monitoring is latched");
    check (input.bypassesFxChain(), "direct monitoring asks the slot to bypass the chain");

    input.setInputBlock (channels, 1);
    check (slot.process (kBlockSize, midi), "the slot still renders the input while monitoring");

    check (fx.getProcessCalls() == callsAfterFade,
           "the bypassed chain's processBlock is never called");
    check (slot.latencySamples() == 96,
           "direct mode reports only the input path's own latency");

    bool bitIdentical = true;
    const auto& monitored = slot.getOutputBuffer();

    for (int channel = 0; channel < 2 && bitIdentical; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            if (! juce::exactlyEqual (monitored.getSample (channel, i),
                                      source.getSample (0, i)))
            {
                bitIdentical = false;
                break;
            }

    check (bitIdentical, "direct-monitor output is the input, bit-exact");

    // Leaving direct monitoring puts the loaded chain back in the path.
    input.setDirectMonitor (false);
    input.setInputBlock (channels, 1);
    slot.process (kBlockSize, midi);

    check (fx.getProcessCalls() == callsAfterFade + 1,
           "leaving direct mode runs the chain again on the next block");
    check (slot.latencySamples() == 96 + 240,
           "latency returns to input plus chain");
}

void testLatencyBudget()
{
    std::cout << "\n-- the input's own latency budget --" << std::endl;

    AudioInputInstrument input;
    input.prepare (kSampleRate, kBlockSize);

    check (input.latencyBudgetSamples() == -1, "no latency budget is configured by default");
    check (input.isWithinLatencyBudget(), "an unset budget always passes");

    input.setLatencySamples (200);
    input.setLatencyBudgetSamples (240);

    check (input.latencyBudgetSamples() == 240, "the budget is reported back");
    check (input.isWithinLatencyBudget(), "200 samples fits a 240-sample budget");

    input.setLatencyBudgetSamples (200);
    check (input.isWithinLatencyBudget(), "the budget is inclusive");

    input.setLatencyBudgetSamples (199);
    check (! input.isWithinLatencyBudget(), "200 samples exceeds a 199-sample budget");

    input.setLatencyBudgetSamples (-1);
    check (input.latencyBudgetSamples() == -1 && input.isWithinLatencyBudget(),
           "a negative budget clears the check");
}

void testEngineFeedsDeviceInput()
{
    std::cout << "\n-- the engine feeds the device input to an audio-input slot --" << std::endl;

    using Engine = AudioEngine::AudioEngine;

    Engine engine (2, 2);
    engine.prepare (kSampleRate, kBlockSize, 2);

    AudioInputInstrument input;
    input.setGain (0.25f); // prove the engine used the instrument, not a copy

    AudioEngine::Slot& slot = engine.getSlot (0);
    slot.setSource (&input);
    slot.activate();

    juce::AudioBuffer<float> deviceInput (1, kBlockSize);
    fillSine (deviceInput, 0);

    juce::AudioBuffer<float> deviceOutput (2, kBlockSize);
    float* outputChannels[] = { deviceOutput.getWritePointer (0),
                                deviceOutput.getWritePointer (1) };
    const float* inputChannels[] = { deviceInput.getReadPointer (0) };

    for (int block = 0; block < kFadeBlocks; ++block)
        engine.audioDeviceIOCallbackWithContext (inputChannels, 1,
                                                 outputChannels, 2,
                                                 kBlockSize, {});

    check (slot.getState() == AudioEngine::SlotState::active,
           "the engine-driven audio-input slot reaches active");

    double maxDifference = 0.0;

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < kBlockSize; ++i)
            maxDifference = std::max (
                maxDifference,
                std::abs (static_cast<double> (deviceOutput.getSample (channel, i))
                          - 0.25 * static_cast<double> (deviceInput.getSample (0, i))));

    check (maxDifference < 1.0e-7,
           "the device callback path renders the instrument's gained input");
}

//==============================================================================
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

//==============================================================================
void testLoopbackThroughMasterBus()
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

        // Prime the loopback ring before capture starts, exactly as the audio
        // smoke test does, so the beginning of the sine is not lost.
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

//==============================================================================
void testLoopbackThroughFxChainAndDirectMonitor()
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

        // Half gain and a reported 5 ms: the signal proves the chain ran, the
        // latency number proves direct mode takes it out of the total.
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

            // Flip the monitor switch once, between blocks, halfway through
            // the capture.
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

} // namespace

int main()
{
    try
    {
        testUnityGainIntoBus();
        testGainAndLatency();
        testChannelSelectionAndMissingInput();
        testDirectMonitorBypassesFxChain();
        testLatencyBudget();
        testEngineFeedsDeviceInput();
        testLoopbackThroughMasterBus();
        testLoopbackThroughFxChainAndDirectMonitor();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << ": ihub-instrument audio input" << std::endl;

    return failures == 0 ? 0 : 1;
}
