// ihub-driver-integration-test: the T044 acceptance check.
//
// This is the whole controller path, from the wire to the audio graph:
//
//   ALSA `Launchkey Mini MK3 SIM`
//     -> LaunchkeyDriver (device matching, session handshake, pad decode)
//     -> Mapping (set.yaml pad -> slot resolution)
//     -> SlotSwitcher (arm and crossfade)
//     -> LaunchkeyDriver LED feedback
//     -> simulator recording
//
// The simulator replays tests/midi/fixtures/pads-16.json one pad at a time.
// All 16 pads are mapped by the shipped shared/set.yaml; pad N arms slot N.
//
// Every outbound LED message is canonicalised to one hex line and compared,
// exactly and in order, against tests/midi/fixtures/driver-integration-leds.golden.
// Message timestamps are intentionally not part of the golden stream: they are
// wall-clock dependent, the LED byte sequence is not.
//
// The handshake is checked byte for byte against session-handshake.json before
// any pad is pressed. Removing `sendHandshake()` from `LaunchkeyDriver::connect()`
// therefore fails this test at the handshake check (verified as a negative
// control while writing it).

#include "controller/LaunchkeyDriver.h"
#include "controller/Mapping.h"

#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"

#include "LaunchkeySim.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef IHUB_MIDI_FIXTURE_DIR
 #define IHUB_MIDI_FIXTURE_DIR "tests/midi/fixtures"
#endif

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr float kFadeMilliseconds = 3.0f;
constexpr int kMaxFadeBlocks = 64;

/** The shipped shared/set.yaml binds pads 0..15 to these slots, in order. */
constexpr std::array<int, 16> kMappedSlots { 0, 1, 2, 3, 4, 5, 6, 7,
                                             8, 9, 10, 11, 12, 13, 14, 15 };

/** 16 startup LEDs + 16 pads: 2 messages for the first press and 3 for each
    later press (previous idle, new warming, new active). */
constexpr std::size_t kExpectedLedMessages = 16 + 2 + 15 * 3;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string fixturePath (const std::string& name)
{
    return std::string (IHUB_MIDI_FIXTURE_DIR) + "/" + name;
}

std::string hex (const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill ('0');

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
            out << ' ';

        out << std::setw (2) << static_cast<unsigned int> (bytes[i]);
    }

    return out.str();
}

template <std::size_t N>
std::string hex (const std::array<std::uint8_t, N>& bytes)
{
    return hex (std::vector<std::uint8_t> (bytes.begin(), bytes.end()));
}

bool waitFor (const std::function<bool()>& predicate, int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);

    while (! predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;

        std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }

    return true;
}

bool isNoteOn (const LaunchkeySim::RecordedMessage& message,
               int channel, int note, int velocity)
{
    return message.type == "noteon" && message.channel == channel
           && message.data1 == note && message.data2 == velocity;
}

/** True for the vendor's session-layout LED notes (96..103 and 112..119). */
bool isPadLedMessage (const LaunchkeySim::RecordedMessage& message)
{
    if (message.type != "noteon" || message.channel != 1)
        return false;

    const int note = message.data1;
    return (note >= 96 && note <= 103) || (note >= 112 && note <= 119);
}

std::vector<std::string> canonicalLedStream (
    const std::vector<LaunchkeySim::RecordedMessage>& messages)
{
    std::vector<std::string> lines;

    for (const auto& message : messages)
        if (isPadLedMessage (message))
            lines.push_back (hex (message.bytes));

    return lines;
}

std::vector<std::string> readGolden (const std::string& path)
{
    std::ifstream file (path);

    if (! file)
        throw std::runtime_error ("cannot open golden file " + path);

    std::vector<std::string> lines;
    std::string line;

    while (std::getline (file, line))
    {
        while (! line.empty()
               && (line.back() == '\r' || line.back() == ' '
                   || line.back() == '\t'))
            line.pop_back();

        std::size_t first = 0;

        while (first < line.size()
               && (line[first] == ' ' || line[first] == '\t'))
            ++first;

        line.erase (0, first);

        if (line.empty() || line.front() == '#')
            continue;

        lines.push_back (line);
    }

    return lines;
}

std::string expectedLedLine (int pad, Controller::PadState state)
{
    return hex (Launchkey::padColourMessage (pad, Controller::colourForState (state)));
}

/** The LED sequence the application is documented to produce. */
std::vector<std::string> policyLedStream()
{
    std::vector<std::string> lines;

    for (int pad = 0; pad < Launchkey::padCount; ++pad)
        lines.push_back (expectedLedLine (pad, Controller::PadState::idle));

    int previousPad = -1;

    for (int pad = 0; pad < static_cast<int> (kMappedSlots.size()); ++pad)
    {
        if (previousPad >= 0)
            lines.push_back (expectedLedLine (previousPad, Controller::PadState::idle));

        lines.push_back (expectedLedLine (pad, Controller::PadState::warming));
        lines.push_back (expectedLedLine (pad, Controller::PadState::active));
        previousPad = pad;
    }

    return lines;
}

/** A real source so the switcher renders audio, not just a fake gain. */
struct SineSource final : public AudioEngine::SlotSource
{
    SineSource (double frequencyToUse, double phaseOffset)
        : frequency (frequencyToUse), initialPhase (phaseOffset)
    {
    }

    void prepare (double newSampleRate, int) override
    {
        phase = initialPhase;
        increment = juce::MathConstants<double>::twoPi * frequency / newSampleRate;
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            const auto value = static_cast<float> (0.1 * std::sin (phase));
            phase += increment;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.getWritePointer (channel)[frame] = value;
        }
    }

private:
    double frequency;
    double initialPhase;
    double phase = 0.0;
    double increment = 0.0;
};

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    try
    {
        // ---------------------------------------------------------------------
        // The app: a set mapping, sixteen fixed slots and the switcher.
        // ---------------------------------------------------------------------
        Controller::Mapping mapping;
        Controller::MappingError mappingError;

        const bool mappingLoaded = mapping.load (IHUB_SET_YAML, &mappingError);

        check (mappingLoaded, std::string ("shared/set.yaml loads")
                                   + (mappingLoaded ? std::string()
                                                    : " (" + mappingError.message + ")"));

        if (! mappingLoaded)
            return 1;

        std::vector<std::unique_ptr<SineSource>> sources;
        std::vector<std::unique_ptr<AudioEngine::Slot>> slots;
        std::vector<AudioEngine::Slot*> slotPointers;

        for (int slot = 0; slot < 16; ++slot)
        {
            sources.push_back (std::make_unique<SineSource> (220.0 + 55.0 * slot,
                                                              0.37 * slot));
            slots.push_back (std::make_unique<AudioEngine::Slot> (slot));
            slots.back()->setSource (sources.back().get());
            slotPointers.push_back (slots.back().get());
        }

        AudioEngine::SlotSwitcher switcher (kFadeMilliseconds);
        switcher.setSlots (slotPointers);
        switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

        Controller::LaunchkeyDriver driver;

        std::atomic<int> activePad { -1 };
        std::atomic<int> presses { 0 };
        std::atomic<bool> lastPadHandled { false };

        // The mapping decides the slot. LED feedback mirrors the same binding.
        Controller::Mapping::Listeners mappingListeners;

        mappingListeners.armSlot = [&] (int slot)
        {
            if (slot >= 0)
                switcher.armSlot (slot);
            else
                switcher.disarm();
        };

        mappingListeners.padEvent = [&] (int pad,
                                         const Controller::PadBinding* binding,
                                         bool pressed)
        {
            if (! pressed || binding == nullptr)
                return;

            const int previous = activePad.exchange (pad);

            if (previous >= 0 && previous != pad)
                driver.setPadState (previous, Controller::PadState::idle);

            driver.setPadState (pad, Controller::PadState::warming);
        };

        mapping.setListeners (mappingListeners);

        // The driver only decodes. Routing and LED policy are the app's job.
        Controller::DriverListeners driverListeners;

        driverListeners.padEvent = [&] (int pad, bool pressed)
        {
            if (! pressed)
            {
                mapping.handlePad (pad, false);
                return;
            }

            const bool handled = mapping.handlePad (pad, true);
            lastPadHandled.store (handled, std::memory_order_release);
            presses.fetch_add (1, std::memory_order_release);
        };

        driverListeners.encoderEvent = [&] (int encoder, int value)
        {
            mapping.handleEncoder (encoder, value);
        };

        driver.setListeners (driverListeners);

        // ---------------------------------------------------------------------
        // Simulator and driver connection
        // ---------------------------------------------------------------------
        LaunchkeySim::Simulator simulator;
        simulator.open();

        std::cout << "ihub-driver-integration-test\n"
                  << "  block: " << kBlockSize << ", sample rate: " << kSampleRate
                  << ", golden: " << fixturePath ("driver-integration-leds.golden")
                  << std::endl;

        const auto selection = Controller::LaunchkeyDriver::findDevice();
        check (selection.has_value(), "Launchkey Mini MK3 detected by name");

        if (! selection.has_value())
            return 1;

        const bool connected = driver.connect();

        check (connected, "connect() opens the simulator"
                              + (connected ? std::string()
                                           : " (" + driver.getLastError().toStdString() + ")"));

        if (! connected)
            return 1;

        simulator.drain (300);

        const auto handshakeFixture = LaunchkeySim::loadFixture (
            fixturePath ("session-handshake.json"));
        const auto& handshake = simulator.recorded();

        check (handshake.size() == 3,
               "connect emitted exactly the 3 handshake messages (got "
                   + std::to_string (handshake.size()) + ")");

        if (handshake.size() >= 3)
        {
            check (handshake[0].type == "sysex"
                       && handshake[0].bytes == handshakeFixture.events[0].bytes,
                   "handshake SysEx matches the fixture: " + hex (handshake[0].bytes));

            check (isNoteOn (handshake[1], 16, 12, 127),
                   "handshake DAW-mode note matches the fixture: "
                       + hex (handshake[1].bytes));

            check (handshake[2].type == "cc" && handshake[2].channel == 16
                       && handshake[2].data1 == 3 && handshake[2].data2 == 2,
                   "handshake session-layout CC matches the fixture: "
                       + hex (handshake[2].bytes));
        }

        check (! driver.hasReceivedHandshakeEcho(),
               "no handshake echo before the device answers");

        LaunchkeySim::Fixture echoFixture;
        echoFixture.events.push_back (handshakeFixture.events[0]);
        simulator.replay (echoFixture);

        check (waitFor ([&]
                        {
                            simulator.pump();
                            return driver.hasReceivedHandshakeEcho();
                        }, 1000),
               "driver recognises the session-mode SysEx echo");

        // ---------------------------------------------------------------------
        // Startup LED state: every pad idle, then pads 0..15 as they arrive.
        // ---------------------------------------------------------------------
        simulator.clearRecording();
        driver.setAllPadStates (Controller::PadState::idle);

        check (waitFor ([&]
                        {
                            simulator.pump();
                            return canonicalLedStream (simulator.recorded()).size() >= 16;
                        }, 2000),
               "all 16 startup LEDs were recorded");

        simulator.drain (100);

        const auto startupLeds = canonicalLedStream (simulator.recorded());

        check (startupLeds.size() == 16,
               "startup wrote exactly 16 LED messages (got "
                   + std::to_string (startupLeds.size()) + ")");

        bool startupOrderCorrect = startupLeds.size() == 16;

        for (int pad = 0; pad < Launchkey::padCount && startupOrderCorrect; ++pad)
            startupOrderCorrect = startupLeds[static_cast<std::size_t> (pad)]
                                  == expectedLedLine (pad, Controller::PadState::idle);

        check (startupOrderCorrect,
               "startup LEDs are pad 0..15, all idle (DarkGrey)");

        // ---------------------------------------------------------------------
        // Replay the 16-pad fixture one press at a time.
        // ---------------------------------------------------------------------
        const auto padFixture = LaunchkeySim::loadFixture (
            fixturePath ("pads-16.json"));

        check (padFixture.events.size() == 32,
               "pads-16.json contains 16 press/release pairs (got "
                   + std::to_string (padFixture.events.size()) + " events)");

        std::vector<int> observedArmedSlots;
        int expectedSlot = -1;
        int expectedPresses = 0;

        for (std::size_t index = 0; index + 1 < padFixture.events.size(); index += 2)
        {
            const auto& noteOn = padFixture.events[index];
            const auto& noteOff = padFixture.events[index + 1];

            const auto padIndex = Launchkey::padIndexForNote (noteOn.note);
            check (padIndex.has_value()
                       && *padIndex == static_cast<int> (index / 2)
                       && noteOff.note == noteOn.note,
                   "fixture pair " + std::to_string (index / 2)
                       + " is pad " + std::to_string (index / 2)
                       + " (note " + std::to_string (noteOn.note) + ")");

            if (! padIndex.has_value())
                continue;

            const int pad = *padIndex;

            LaunchkeySim::Fixture onePress;
            auto press = noteOn;
            auto release = noteOff;
            press.t = 0;
            release.t = std::max<std::int64_t> (noteOff.t - noteOn.t, 1);
            onePress.events = { press, release };

            ++expectedPresses;
            simulator.replay (onePress);

            check (waitFor ([&] { return presses.load() >= expectedPresses; }, 2000),
                   "pad " + std::to_string (pad) + " press reached the driver");

            const bool mapped = pad < static_cast<int> (kMappedSlots.size());

            if (mapped)
            {
                expectedSlot = kMappedSlots[static_cast<std::size_t> (pad)];

                check (lastPadHandled.load(),
                       "mapping handled pad " + std::to_string (pad)
                           + " as a slot selection");

                check (waitFor ([&] { return switcher.getArmedSlot() == expectedSlot; },
                                1000),
                       "pad " + std::to_string (pad) + " arms slot "
                           + std::to_string (expectedSlot));

                juce::MidiBuffer midi;
                bool active = false;

                for (int block = 0; block < kMaxFadeBlocks && ! active; ++block)
                {
                    switcher.process (kBlockSize, midi);
                    active = switcher.getSlotState (expectedSlot)
                                 == AudioEngine::SlotSwitcher::State::active
                             && switcher.getSlotGain (expectedSlot) > 0.99f;
                }

                check (active, "slot " + std::to_string (expectedSlot)
                                   + " reached active after the 3 ms crossfade");
                check (mapping.getArmedSlot() == expectedSlot,
                       "mapping reports slot " + std::to_string (expectedSlot)
                           + " as armed");
                check (mapping.getActivePad() == pad,
                       "mapping reports pad " + std::to_string (pad)
                           + " as the active selection");

                observedArmedSlots.push_back (switcher.getArmedSlot());

                driver.setPadState (pad, Controller::PadState::active);
                check (driver.getPadState (pad) == Controller::PadState::active,
                       "pad " + std::to_string (pad) + " LED is active");
            }
            else
            {
                check (! lastPadHandled.load(),
                       "unmapped pad " + std::to_string (pad) + " is ignored");
                check (switcher.getArmedSlot() == expectedSlot,
                       "unmapped pad " + std::to_string (pad)
                           + " keeps slot " + std::to_string (expectedSlot)
                           + " armed");
            }
        }

        const std::vector<int> expectedObserved (kMappedSlots.begin(),
                                                  kMappedSlots.end());
        check (observedArmedSlots == expectedObserved,
               "the app switched through slots 0..15");

        check (driver.getPadState (15) == Controller::PadState::active,
               "pad 15 is the last active LED");
        check (driver.getPadState (0) == Controller::PadState::idle
                   && driver.getPadState (14) == Controller::PadState::idle,
               "previously selected pads returned to idle");

        // ---------------------------------------------------------------------
        // Golden LED stream
        // ---------------------------------------------------------------------
        simulator.drain (300);

        const auto actual = canonicalLedStream (simulator.recorded());
        const auto golden = readGolden (
            fixturePath ("driver-integration-leds.golden"));
        const auto policy = policyLedStream();

        check (golden.size() == kExpectedLedMessages,
               "golden contains " + std::to_string (kExpectedLedMessages)
                   + " LED messages (got " + std::to_string (golden.size()) + ")");
        check (policy == golden,
               "golden matches the documented idle/warming/active policy");
        check (actual.size() == kExpectedLedMessages,
               "the simulator recorded " + std::to_string (kExpectedLedMessages)
                   + " LED messages (got " + std::to_string (actual.size()) + ")");

        if (actual != golden)
        {
            const auto common = std::min (actual.size(), golden.size());

            for (std::size_t i = 0; i < common; ++i)
            {
                if (actual[i] != golden[i])
                {
                    std::cout << "  first LED mismatch at message " << i
                              << ": actual '" << actual[i]
                              << "', golden '" << golden[i] << "'" << std::endl;
                    break;
                }
            }

            if (actual.size() != golden.size())
                std::cout << "  LED count mismatch: actual "
                          << actual.size() << ", golden " << golden.size()
                          << std::endl;
        }

        check (actual == golden,
               "golden LED stream matches exactly (" + std::to_string (actual.size())
                   + " messages)");

        driver.disconnect();
        check (! driver.isConnected(), "disconnect() closes the simulator");

        std::cout << (failures == 0 ? "PASS" : "FAIL")
                  << ": driver integration acceptance (" << failures
                  << " failure(s))" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
