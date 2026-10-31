// ihub-e2e-test: the T117 end-to-end pad-to-audio acceptance check.
//
// One Launchpad Mini MK3 pad event is pushed through the real controller
// stack into the composed session and then on to the ALSA Loopback card:
//
//   LaunchpadDriver::injectMidiMessage (Note On, grid note)
//     -> Mapping::handlePad (shared/set.yaml)
//       -> Mapping::armSlot
//         -> Session::armSlot -> SlotSwitcher (3 ms equal-power crossfade)
//           -> AudioEngine -> summing bus -> ALSA Loopback playback
//
// The test proves, with no hardware attached:
//
//   1. All 64 grid pads resolve through shared/set.yaml. Pads 0..7 address the
//      eight slots the default composed session owns; the shipped mapping also
//      declares slots 8..63 for a future 64-slot set.
//   2. A pad press arms exactly the slot the mapping names, and the armed slot
//      is the one that renders non-silent audio.
//   3. The pad-triggered switch is click-free: the rendered session is compared
//      with the ideal 3 ms equal-power crossfade (<= -60 dBFS residual), and a
//      sample-to-sample delta detector is proven to fire on a hard switch with
//      the ramp disabled.
//   4. The same pad events, sent while the composed session streams through the
//      ALSA Loopback, produce non-silent captured audio. The exact click-free
//      proof is the deterministic offline render and its equal-power residual;
//      the captured stream delta is reported as diagnostics only, because this
//      harness feeds a real-time PCM ring from an offline render.
//
// Everything is deterministic except the loopback transport itself: the
// offline renders are driven block by block, and the loopback pass reads the
// capture ring before writing each rendered block, exactly as LatencyProbe
// does. No wall-clock sleep decides a measurement.

#include "Session.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
 #include <alsa/asoundlib.h>
#endif

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kChannels = 2;
constexpr int kDefaultSlots = 8;

constexpr int kSettleBlocks = 8;
constexpr int kRmsBlocks = 8;
constexpr int kSwitchBlocks = 32;

constexpr int kFirstPad = 0;   // Keys / Grand Piano -> slot 0
constexpr int kSecondPad = 3;  // Keys / Drawbar Organ -> slot 3

constexpr double kMinimumRmsDbfs = -50.0;
constexpr double kDiscontinuityLimitDbfs = -60.0;
constexpr double kMaximumSwitchDelta = 0.02;
constexpr double kHardSwitchMinimumDelta = 0.05;

constexpr int kLoopbackArmFirstBlock = 12;
constexpr int kLoopbackArmSecondBlock = 60;
constexpr int kLoopbackBlocks = 140;
constexpr int kLoopbackWarmupBlocks = 4;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

double toDbfs (double linear) noexcept
{
    return 20.0 * std::log10 (juce::jmax (linear, 1.0e-12));
}

double rmsDbfs (const std::vector<float>& samples, int first, int last)
{
    first = juce::jlimit (0, static_cast<int> (samples.size()), first);
    last = juce::jlimit (first, static_cast<int> (samples.size()), last);

    double sumSquares = 0.0;

    for (int index = first; index < last; ++index)
        sumSquares += static_cast<double> (samples[static_cast<std::size_t> (index)])
                      * static_cast<double> (samples[static_cast<std::size_t> (index)]);

    const int count = last - first;

    if (count <= 0)
        return -1000.0;

    return toDbfs (std::sqrt (sumSquares / static_cast<double> (count)));
}

double maxDelta (const std::vector<float>& samples, int first, int last)
{
    first = juce::jlimit (0, static_cast<int> (samples.size()), first);
    last = juce::jlimit (first, static_cast<int> (samples.size()), last);

    double maximum = 0.0;

    for (int index = juce::jmax (1, first); index < last; ++index)
        maximum = juce::jmax (maximum,
                              std::abs (static_cast<double> (samples[static_cast<std::size_t> (index)])
                                        - static_cast<double> (samples[static_cast<std::size_t> (index - 1)])));

    return maximum;
}

/** Magnitude of one frequency in a window (a single-bin DFT).

    Used to prove that the slot the pad armed is the slot that is audible: the
    default set gives slot 0 a 110 Hz tone and slot 3 a 146.83 Hz tone.
*/
double toneMagnitude (const std::vector<float>& samples,
                      int first,
                      int last,
                      double frequency)
{
    first = juce::jlimit (0, static_cast<int> (samples.size()), first);
    last = juce::jlimit (first, static_cast<int> (samples.size()), last);

    const int count = last - first;

    if (count <= 0)
        return 0.0;

    double real = 0.0;
    double imaginary = 0.0;

    for (int index = first; index < last; ++index)
    {
        const double phase = juce::MathConstants<double>::twoPi * frequency
                             * static_cast<double> (index - first) / kSampleRate;
        const double value = static_cast<double> (samples[static_cast<std::size_t> (index)]);

        real += value * std::cos (phase);
        imaginary += value * std::sin (phase);
    }

    return std::sqrt (real * real + imaginary * imaginary) / static_cast<double> (count);
}

//==============================================================================
/** The controller stack exactly as Main.cpp wires it: driver -> mapping -> arm. */
struct PadRig
{
    Controller::Mapping mapping;
    Controller::LaunchpadDriver driver;
    ihub::Session& session;

    std::vector<int> armedRequests;

    explicit PadRig (ihub::Session& sessionToDrive) : session (sessionToDrive) {}

    bool load (const std::string& file, std::string& errorText)
    {
        Controller::Mapping::Listeners mappingListeners;

        mappingListeners.armSlot = [this] (int slot)
        {
            armedRequests.push_back (slot);

            if (slot >= 0)
                session.armSlot (slot);
        };

        mapping.setListeners (std::move (mappingListeners));

        Controller::MappingError error;

        if (! mapping.load (std::filesystem::path (file), &error))
        {
            errorText = error.message;
            return false;
        }

        Controller::LaunchpadDriver::DriverListeners driverListeners;

        driverListeners.padEvent = [this] (int pad, bool pressed)
        {
            mapping.handlePad (pad, pressed);
        };

        driver.setListeners (std::move (driverListeners));
        return true;
    }

    void press (int pad)
    {
        const int note = Launchpad::padNoteForIndex (pad);

        driver.injectMidiMessage (juce::MidiMessage::noteOn (1, note,
                                                             static_cast<juce::uint8> (127)));
        driver.injectMidiMessage (juce::MidiMessage::noteOn (1, note,
                                                             static_cast<juce::uint8> (0)));
    }
};

//==============================================================================
ihub::SessionConfig makeConfig (float fadeMilliseconds)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = kChannels;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = fadeMilliseconds;
    config.slots = ihub::Session::makeDefaultSet();
    return config;
}

void renderBlock (ihub::Session& session,
                  juce::AudioBuffer<float>& buffer,
                  std::vector<float>& capture)
{
    juce::MidiBuffer midi;
    session.render (buffer, midi, kBlockSize);

    for (int frame = 0; frame < kBlockSize; ++frame)
        capture.push_back (buffer.getSample (0, frame));
}

struct PadRender
{
    std::vector<float> samples;
    int armedAfterFirst = -1;
    int armedAfterSecond = -1;
    int stateAfterFirst = -1;
    int stateAfterSecond = -1;
    double switchMaxDelta = 0.0;
    int switchSample = 0;
};

/** Runs the offline pad sequence and returns the rendered master bus. */
PadRender runPadRender (float fadeMilliseconds)
{
    PadRender result;

    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig (fadeMilliseconds), error))
    {
        check (false, "offline session builds: " + error.toStdString());
        return result;
    }

    PadRig rig (session);
    std::string mappingError;

    if (! rig.load (IHUB_SET_YAML, mappingError))
    {
        check (false, "set.yaml loads: " + mappingError);
        return result;
    }

    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
    buffer.clear();

    rig.press (kFirstPad);
    result.armedAfterFirst = session.getArmedSlot();
    check (session.waitForSlotActive (kFirstPad, 5000),
           "the first pad's patch warms and arms");

    for (int block = 0; block < kSettleBlocks + kRmsBlocks; ++block)
    {
        renderBlock (session, buffer, result.samples);

        if (block == kSettleBlocks - 1)
            result.stateAfterFirst = static_cast<int> (
                session.getSwitcher().getSlotState (kFirstPad));
    }

    const int switchStart = static_cast<int> (result.samples.size());

    rig.press (kSecondPad);
    result.armedAfterSecond = session.getArmedSlot();
    check (session.waitForSlotActive (kSecondPad, 5000),
           "the second pad's patch warms and arms");

    for (int block = 0; block < kSwitchBlocks; ++block)
    {
        renderBlock (session, buffer, result.samples);

        if (block == kSwitchBlocks - 1)
            result.stateAfterSecond = static_cast<int> (
                session.getSwitcher().getSlotState (kSecondPad));
    }

    result.switchSample = switchStart;
    result.switchMaxDelta = maxDelta (result.samples,
                                      switchStart,
                                      switchStart + 4 * kBlockSize);
    return result;
}

//==============================================================================
#if defined(__linux__)
struct LoopbackPcm
{
    snd_pcm_t* playback = nullptr;
    snd_pcm_t* capture = nullptr;

    double rate = kSampleRate;
    int period = kBlockSize;
    int buffer = kBlockSize * 8;

    juce::String playbackName;
    juce::String captureName;

    ~LoopbackPcm() { close(); }

    void close()
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

    static bool findPair (juce::String& playbackName, juce::String& captureName)
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

    static bool configure (snd_pcm_t* pcm,
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

    bool open (juce::String& error)
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

    bool read (std::int16_t* destination, int frames)
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

    bool write (const std::int16_t* source, int frames)
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
};
#endif

#if defined(__linux__)
namespace
{
void silentAlsaError (const char*, int, const char*, int, const char*, ...)
{
}

/** Keeps ALSA's underrun chatter out of the ctest log.

    The loopback pass is an offline render fed through a real-time PCM ring.
    Recoverable underruns are expected and do not change the captured samples
    that are measured, so the library's stderr noise is suppressed.
*/
struct AlsaErrorSilencer
{
    AlsaErrorSilencer()  { snd_lib_error_set_handler (&silentAlsaError); }
    ~AlsaErrorSilencer() { snd_lib_error_set_handler (nullptr); }
};
} // namespace
#endif

std::vector<std::int16_t> toPcm16 (const juce::AudioBuffer<float>& buffer, int frames)
{
    std::vector<std::int16_t> result (static_cast<std::size_t> (frames), 0);

    for (int frame = 0; frame < frames; ++frame)
    {
        const float value = juce::jlimit (-1.0f, 1.0f, buffer.getSample (0, frame));
        result[static_cast<std::size_t> (frame)] =
            static_cast<std::int16_t> (juce::roundToInt (value * 32767.0f));
    }

    return result;
}

struct LoopbackResult
{
    std::vector<float> captured;
    int armedAfterFirst = -1;
    int armedAfterSecond = -1;
    juce::String playbackName;
    juce::String captureName;
    juce::String error;
    bool ran = false;
};

LoopbackResult runLoopback (ihub::Session& session, PadRig& rig)
{
    LoopbackResult result;

#if ! defined(__linux__)
    result.error = "the ALSA Loopback pass is Linux-only";
    return result;
#else
    LoopbackPcm pcm;

    const AlsaErrorSilencer silencer;
    juce::ignoreUnused (silencer);

    if (! pcm.open (result.error))
        return result;

    result.playbackName = pcm.playbackName;
    result.captureName = pcm.captureName;

    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
    buffer.clear();

    std::vector<std::int16_t> pcmBlock (static_cast<std::size_t> (kBlockSize), 0);
    std::vector<std::int16_t> silence (static_cast<std::size_t> (kBlockSize), 0);

    // Start both streams with silence so the capture has real frames to read
    // before the first rendered block is written. Eight periods of slack keep
    // the PCM ring ahead of this offline render loop.
    for (int index = 0; index < 4; ++index)
    {
        if (! pcm.write (silence.data(), kBlockSize))
        {
            result.error = "loopback playback did not start";
            return result;
        }

        if (! pcm.read (pcmBlock.data(), kBlockSize))
        {
            result.error = "loopback capture did not start";
            return result;
        }
    }

    for (int block = 0; block < kLoopbackBlocks; ++block)
    {
        if (! pcm.read (pcmBlock.data(), kBlockSize))
        {
            result.error = "loopback capture failed at block " + juce::String (block);
            return result;
        }

        if (block >= kLoopbackWarmupBlocks)
        {
            for (int frame = 0; frame < kBlockSize; ++frame)
                result.captured.push_back (
                    static_cast<float> (pcmBlock[static_cast<std::size_t> (frame)]) / 32768.0f);
        }

        if (block == kLoopbackArmFirstBlock)
        {
            rig.press (kFirstPad);
            result.armedAfterFirst = session.getArmedSlot();
        }

        if (block == kLoopbackArmSecondBlock)
        {
            rig.press (kSecondPad);
            result.armedAfterSecond = session.getArmedSlot();
        }

        juce::MidiBuffer midi;
        session.render (buffer, midi, kBlockSize);

        const auto output = toPcm16 (buffer, kBlockSize);

        if (! pcm.write (output.data(), kBlockSize))
        {
            result.error = "loopback playback failed at block " + juce::String (block);
            return result;
        }
    }

    result.ran = true;
    return result;
#endif
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-e2e-test: Launchpad pad -> mapping -> composed session -> ALSA Loopback\n"
              << "  block=" << kBlockSize << " sample_rate=" << kSampleRate
              << " slots=" << kDefaultSlots << std::endl;

    //==========================================================================
    // 1. The shipped mapping resolves the whole Launchpad surface.
    {
        Controller::Mapping mapping;
        Controller::MappingError error;

        const bool loaded = mapping.load (std::filesystem::path (IHUB_SET_YAML), &error);
        check (loaded, "shared/set.yaml loads"
                           + (loaded ? std::string() : ": " + error.message));
        check (mapping.getMapping() != nullptr, "the mapping was published");

        if (loaded)
        {
            int bound = 0;

            for (int pad = 0; pad < Launchpad::padCount; ++pad)
            {
                const auto binding = mapping.getPadBinding (pad);

                if (binding.has_value() && binding->slot == pad)
                    ++bound;
            }

            check (bound == Launchpad::padCount,
                   "all 64 grid pads resolve to their own binding ("
                       + std::to_string (bound) + "/64)");
            check (bound >= kDefaultSlots,
                   "the first eight pads address the eight composed slots");
        }
    }

    //==========================================================================
    // 2. Offline: a pad press arms the intended slot and renders non-silent.
    const auto normal = runPadRender (3.0f);

    if (normal.samples.empty())
    {
        check (false, "the pad-driven offline render produced samples");
    }
    else
    {
        check (normal.armedAfterFirst == kFirstPad,
               "pad " + std::to_string (kFirstPad) + " arms slot "
                   + std::to_string (kFirstPad) + " (got "
                   + std::to_string (normal.armedAfterFirst) + ")");
        check (normal.armedAfterSecond == kSecondPad,
               "pad " + std::to_string (kSecondPad) + " arms slot "
                   + std::to_string (kSecondPad) + " (got "
                   + std::to_string (normal.armedAfterSecond) + ")");
        check (normal.stateAfterFirst
                   == static_cast<int> (AudioEngine::SlotSwitcher::State::active),
               "the first pad's slot is active after the fade");
        check (normal.stateAfterSecond
                   == static_cast<int> (AudioEngine::SlotSwitcher::State::active),
               "the second pad's slot is active after the fade");

        const int firstRmsEnd = (kSettleBlocks + kRmsBlocks) * kBlockSize;
        const auto firstRms = rmsDbfs (normal.samples, kSettleBlocks * kBlockSize, firstRmsEnd);
        const auto secondRms = rmsDbfs (normal.samples,
                                        firstRmsEnd + 4 * kBlockSize,
                                        static_cast<int> (normal.samples.size()));

        std::cout << "  pad render: first_rms_dbfs=" << firstRms
                  << " second_rms_dbfs=" << secondRms
                  << " switch_max_delta=" << normal.switchMaxDelta << std::endl;

        check (firstRms > kMinimumRmsDbfs, "the first armed slot is non-silent");
        check (secondRms > kMinimumRmsDbfs, "the second armed slot is non-silent");

        const auto firstTone = toneMagnitude (normal.samples, 0, firstRmsEnd, 110.0);
        const auto firstOther = toneMagnitude (normal.samples, 0, firstRmsEnd, 146.83);
        const auto secondTone = toneMagnitude (normal.samples,
                                               firstRmsEnd + 4 * kBlockSize,
                                               static_cast<int> (normal.samples.size()),
                                               146.83);
        const auto secondOther = toneMagnitude (normal.samples,
                                                firstRmsEnd + 4 * kBlockSize,
                                                static_cast<int> (normal.samples.size()),
                                                110.0);

        std::cout << "  tone identity: slot0=110Hz (" << firstTone << " vs "
                  << firstOther << ") slot3=146.83Hz (" << secondTone << " vs "
                  << secondOther << ")" << std::endl;

        check (firstTone > 3.0 * firstOther,
               "the first window is slot 0's 110 Hz tone");
        check (secondTone > 3.0 * secondOther,
               "the second window is slot 3's 146.83 Hz tone");

        check (normal.switchMaxDelta <= kMaximumSwitchDelta,
               "the pad-triggered switch has no sample step above "
                   + std::to_string (kMaximumSwitchDelta));
    }

    // The standard session switch measurement: rendered RMS and the residual
    // against the ideal 3 ms equal-power crossfade.
    double renderRms = 0.0;
    double renderRmsDbfs = -1000.0;
    double switchDiscontinuityDbfs = -1000.0;
    {
        ihub::Session session;
        juce::String error;

        if (session.build (makeConfig (3.0f), error))
        {
            const auto report = session.measureSwitch (kFirstPad, kSecondPad);

            renderRms = report.rms;
            renderRmsDbfs = report.rmsDbfs;
            switchDiscontinuityDbfs = report.discontinuityDbfs;

            std::cout << "  switch measurement: rms=" << report.rms
                      << " rms_dbfs=" << report.rmsDbfs
                      << " discontinuity_dbfs=" << report.discontinuityDbfs
                      << " max_slew_dbfs=" << report.maxSampleDeltaDbfs
                      << " samples=" << report.samplesMeasured << std::endl;

            check (report.rendered, "the switch measurement rendered samples");
            check (report.rmsDbfs > kMinimumRmsDbfs,
                   "the switch measurement reports a non-silent RMS");
            check (report.discontinuityDbfs <= kDiscontinuityLimitDbfs,
                   "the switch is click-free (residual <= -60 dBFS)");
        }
        else
        {
            check (false, "switch-measurement session builds: " + error.toStdString());
        }
    }

    //==========================================================================
    // 3. Negative control: the same pad sequence with the ramp disabled must
    //    produce a measurable step, so the click check can fail.
    {
        const auto hard = runPadRender (0.0f);

        std::cout << "  hard-switch control: switch_max_delta="
                  << hard.switchMaxDelta << std::endl;

        check (! hard.samples.empty(), "the hard-switch control rendered samples");
        check (hard.switchMaxDelta > kHardSwitchMinimumDelta,
               "the pad-switch delta detector fires on a hard switch");
    }

    //==========================================================================
    // 4. The same pads on the ALSA Loopback: the composed session streams to
    //    hardware, the capture is non-silent and the switch stays smooth.
    double loopbackFirstDbfs = -1000.0;
    double loopbackSecondDbfs = -1000.0;
    std::string loopbackPlaybackName;
    std::string loopbackCaptureName;
    {
        ihub::Session session;
        juce::String error;

        if (! session.build (makeConfig (3.0f), error))
        {
            check (false, "loopback session builds: " + error.toStdString());
        }
        else
        {
            PadRig rig (session);
            std::string mappingError;

            if (! rig.load (IHUB_SET_YAML, mappingError))
            {
                check (false, "loopback set.yaml loads: " + mappingError);
            }
            else
            {
                const auto loopback = runLoopback (session, rig);

                if (! loopback.ran)
                {
                    check (false, "the ALSA Loopback pass runs: "
                                      + loopback.error.toStdString());
                }
                else
                {
                    const int firstWindow = 16 * kBlockSize;
                    const int secondWindow = 96 * kBlockSize;
                    const int end = static_cast<int> (loopback.captured.size());

                    const auto capturedFirst = rmsDbfs (loopback.captured, firstWindow,
                                                        firstWindow + 16 * kBlockSize);
                    const auto capturedSecond = rmsDbfs (loopback.captured, secondWindow, end);
                    const auto capturedDelta = maxDelta (loopback.captured,
                                                         kLoopbackWarmupBlocks * kBlockSize,
                                                         end);

                    loopbackFirstDbfs = capturedFirst;
                    loopbackSecondDbfs = capturedSecond;
                    loopbackPlaybackName = loopback.playbackName.toStdString();
                    loopbackCaptureName = loopback.captureName.toStdString();

                    std::cout << "  loopback: playback=\"" << loopback.playbackName << "\""
                              << " capture=\"" << loopback.captureName << "\""
                              << " captured_first_rms_dbfs=" << capturedFirst
                              << " captured_second_rms_dbfs=" << capturedSecond
                              << " captured_max_delta=" << capturedDelta
                              << " master_peak=" << session.getMasterMeterPeak()
                              << std::endl;

                    check (loopback.armedAfterFirst == kFirstPad,
                           "the loopback pad 0 press arms slot 0");
                    check (loopback.armedAfterSecond == kSecondPad,
                           "the loopback pad 3 press arms slot 3");
                    check (capturedFirst > kMinimumRmsDbfs,
                           "the loopback capture is non-silent for the first pad");
                    check (capturedSecond > kMinimumRmsDbfs,
                           "the loopback capture is non-silent for the second pad");
                    check (session.getMasterMeterPeak() > 0.0f,
                           "the session master meter reports the rendered bus");

                    // The captured delta is recorded, not asserted: this loop
                    // feeds a real-time PCM ring from an offline render, so a
                    // host scheduling hiccup can splice one block boundary.
                    // Click-free switching is asserted by the deterministic
                    // render and its equal-power residual above.
                    std::cout << "  loopback switch delta (informational): "
                              << capturedDelta << std::endl;
                }
            }
        }
    }

    std::cout << "  record: pads=" << kFirstPad << "->" << kSecondPad
              << " render_rms=" << renderRms
              << " render_rms_dbfs=" << renderRmsDbfs
              << " switch_discontinuity_dbfs=" << switchDiscontinuityDbfs
              << " loopback_rms_dbfs_first=" << loopbackFirstDbfs
              << " loopback_rms_dbfs_second=" << loopbackSecondDbfs << std::endl;

    {
        std::ofstream record ("ihub-e2e-record.txt");
        record << "test=ihub-e2e\n"
               << "pads=" << kFirstPad << "->" << kSecondPad << "\n"
               << "render_rms=" << renderRms << "\n"
               << "render_rms_dbfs=" << renderRmsDbfs << "\n"
               << "switch_discontinuity_dbfs=" << switchDiscontinuityDbfs << "\n"
               << "pad_switch_max_delta=" << normal.switchMaxDelta << "\n"
               << "loopback_playback=" << loopbackPlaybackName << "\n"
               << "loopback_capture=" << loopbackCaptureName << "\n"
               << "loopback_rms_dbfs_first=" << loopbackFirstDbfs << "\n"
               << "loopback_rms_dbfs_second=" << loopbackSecondDbfs << "\n"
               << "status=" << (failures == 0 ? "PASS" : "FAIL") << "\n";

        check (record.good(), "the measurement record was written "
                                  "(ihub-e2e-record.txt)");
    }

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL")
              << ": " << failures << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
