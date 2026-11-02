#include "Session.h"
#include "api/MidiBridge.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef IHUB_NOTETONE_VST3
 #define IHUB_NOTETONE_VST3 "NoteTonePlugin.vst3"
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kChannels = 2;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

float renderBlockPeak (ihub::Session& session, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    buffer.clear();
    session.render (buffer, midi, kBlockSize);
    juce::Thread::sleep (2);

    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int frame = 0; frame < kBlockSize; ++frame)
            peak = juce::jmax (peak, std::abs (buffer.getSample (channel, frame)));

    return peak;
}

float renderBlocksPeak (ihub::Session& session,
                        juce::AudioBuffer<float>& buffer,
                        int blocks)
{
    float peak = 0.0f;

    for (int block = 0; block < blocks; ++block)
        peak = juce::jmax (peak, renderBlockPeak (session, buffer));

    return peak;
}

bool waitForArmed (ihub::Session& session, int slot)
{
    session.armSlot (slot);
    return session.waitForSlotActive (slot, 5000);
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-session-midi-test" << std::endl;

    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = kChannels;
    config.device.numInputChannels = 0;
    config.fadeMilliseconds = 3.0f;
    config.slots.resize (2);

    config.slots[0].name = "Unassigned";
    config.slots[0].kind = ihub::SessionSlotKind::empty;
    config.slots[0].instrumentKind = "empty";

    config.slots[1].name = "Note Tone";
    config.slots[1].kind = ihub::SessionSlotKind::plugin;
    config.slots[1].path = juce::String (IHUB_NOTETONE_VST3);
    config.slots[1].instrumentKind = "plugin";
    config.slots[1].pluginName = "NoteTonePlugin";

    ihub::Session session;
    juce::String error;

    const bool built = session.build (config, error);
    check (built, "the session builds" + (built ? std::string() : ": " + error.toStdString()));

    if (! built)
        return 1;

    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);

    check (waitForArmed (session, 0), "the empty patch arms");

    const auto emptyPeak = renderBlocksPeak (session, buffer, 8);
    check (emptyPeak == 0.0f, "an unassigned patch renders digital silence");

    check (session.pushMidi (juce::MidiMessage::noteOn (1, 60,
                                                        static_cast<juce::uint8> (100))),
           "a note is accepted for the empty patch");
    check (session.getQueuedMidiCount() == 1, "the note waits in the lock-free queue");

    const auto emptyWithNotePeak = renderBlocksPeak (session, buffer, 8);
    check (emptyWithNotePeak == 0.0f,
           "a note on an unassigned patch stays silent");
    check (session.getQueuedMidiCount() == 0,
           "the audio render drained the queue exactly once");

    check (waitForArmed (session, 1), "the plugin patch warms and arms");

    const auto idlePeak = renderBlocksPeak (session, buffer, 8);
    check (idlePeak == 0.0f, "an armed note-gated plugin is silent before a note");

    check (session.pushMidi (juce::MidiMessage::noteOn (1, 60,
                                                        static_cast<juce::uint8> (110))),
           "note-on enters the SPSC queue");

    const auto noteOnPeak = renderBlocksPeak (session, buffer, 8);
    check (noteOnPeak > 0.05f,
           "the injected note renders non-silent audio (peak "
               + std::to_string (noteOnPeak) + ")");

    check (session.pushMidi (juce::MidiMessage::noteOff (1, 60, 0.0f)),
           "note-off enters the SPSC queue");

    (void) renderBlocksPeak (session, buffer, 8);
    const auto noteOffPeak = renderBlocksPeak (session, buffer, 8);
    check (noteOffPeak == 0.0f,
           "the note-off stops the patch (peak "
               + std::to_string (noteOffPeak) + ")");

    Api::MidiBridge bridge;
    bridge.setMidiSink ([&session] (const juce::MidiMessage& message)
    {
        return session.pushMidi (message);
    });

    check (Api::MidiBridge::roleForName ("Launchpad Mini MK3 MI") == "launchpad",
           "the Launchpad MI port is classified as a controller");
    check (Api::MidiBridge::roleForName ("Keystation Mini 32 USB") == "keystation",
           "the Keystation is classified as a keyboard input");

    check (! bridge.forwardMidiMessage ("Launchpad Mini MK3 MI",
                                        juce::MidiMessage::noteOn (1, 60,
                                                                   static_cast<juce::uint8> (127))),
           "a Launchpad MI pad note is not forwarded to the instrument");
    check (session.getQueuedMidiCount() == 0,
           "the Launchpad pad note never enters the MIDI queue");
    check (renderBlocksPeak (session, buffer, 4) == 0.0f,
           "a Launchpad pad note does not play the armed patch");

    check (! bridge.forwardMidiMessage ("Launchpad Mini MK3 DA",
                                        juce::MidiMessage::noteOn (1, 60,
                                                                   static_cast<juce::uint8> (127))),
           "the Launchpad DA port is not forwarded either");

    check (bridge.forwardMidiMessage ("Keystation Mini 32 USB",
                                      juce::MidiMessage::noteOn (1, 64,
                                                                 static_cast<juce::uint8> (110))),
           "a Keystation note is forwarded into the session queue");
    check (renderBlocksPeak (session, buffer, 8) > 0.05f,
           "the forwarded keyboard note plays the armed patch");

    check (bridge.forwardMidiMessage ("Keystation Mini 32 USB",
                                      juce::MidiMessage::noteOff (1, 64, 0.0f)),
           "the forwarded note-off enters the session queue");
    (void) renderBlocksPeak (session, buffer, 8);
    check (renderBlocksPeak (session, buffer, 8) == 0.0f,
           "the forwarded note-off stops the patch");

    bridge.setMidiSink (nullptr);

    bool assignmentObserved = false;
    std::string assignedKind;

    session.setSourceAssignedHook ([&assignmentObserved, &assignedKind] (
        int slot, const ihub::SessionSlotConfig& source)
    {
        assignmentObserved = slot == 0;
        assignedKind = source.instrumentKind;
    });

    auto toneSource = config.slots[0];
    toneSource.kind = ihub::SessionSlotKind::initTone;
    toneSource.instrumentKind = "init-tone";

    std::string sourceError;
    const bool assigned = session.setSlotSource (0, toneSource, sourceError);

    check (assigned,
           "a source assignment is accepted"
               + (assigned ? std::string() : ": " + sourceError));
    check (assignmentObserved && assignedKind == "init-tone",
           "the assignment hook fires so the picker can persist it");

    std::cout << (failures == 0 ? "PASS" : "FAIL") << " ("
              << failures << " failure(s))" << std::endl;

    return failures == 0 ? 0 : 1;
}
