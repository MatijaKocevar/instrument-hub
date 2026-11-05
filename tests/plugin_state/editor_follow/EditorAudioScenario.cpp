#include "EditorAudioScenario.h"

#include "Session.h"
#include "plugins/PluginInstrument.h"
#include "plugins/SandboxHost.h"

#include <cmath>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

double renderPitch (ihub::Session& session, double& rms)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 48; ++block)
    {
        buffer.clear();
        midi.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);
    }

    double sum = 0.0;
    int frames = 0;
    int crossings = 0;
    float previous = 0.0f;

    for (int block = 0; block < 192; ++block)
    {
        buffer.clear();
        midi.clear();
        session.render (buffer, midi, kBlockSize);
        juce::Thread::sleep (1);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const float sample = buffer.getSample (0, frame);

            if (previous <= 0.0f && sample > 0.0f)
                ++crossings;

            previous = sample;
            sum += static_cast<double> (sample) * sample;
            ++frames;
        }
    }

    rms = frames > 0 ? std::sqrt (sum / static_cast<double> (frames)) : 0.0;
    return frames > 0 ? static_cast<double> (crossings) * kSampleRate
                            / static_cast<double> (frames)
                      : 0.0;
}

void setFrequency (ihub::plugins::PluginInstrument& instrument, double hz)
{
    (void) instrument.setParameterValue ("Frequency", normaliseFrequency (hz));
}

void waitForSync()
{
    juce::Thread::sleep (220);
}
}

namespace EditorAudioScenario
{

void run (ihub::Session& session, int slot, const Check& check)
{
    auto* instrument = session.getPluginInstrument (slot);

    if (instrument == nullptr)
    {
        check (false, "the edited patch has a live mirror");
        return;
    }

    const auto sandboxPid = session.getSandboxProcessId (slot);
    auto* sandbox = session.getSandboxSlotControl (slot);
    const auto playedBefore = sandbox != nullptr ? sandbox->getPlayedBlockCount() : 0;

    (void) instrument->setParameterValue ("Sine Level", 1.0f);
    (void) instrument->setParameterValue ("Gain", 0.0f);
    setFrequency (*instrument, 440.0);

    double beforeRms = 0.0;
    const auto beforePitch = renderPitch (session, beforeRms);
    check (beforeRms > 0.01,
           "the editor-open slot renders above -40 dBFS ("
               + std::to_string (beforeRms) + ")");
    check (std::abs (beforePitch - 440.0) < 20.0,
           "the editor-open slot renders its current value ("
               + std::to_string (beforePitch) + " Hz)");

    setFrequency (*instrument, 880.0);

    double afterRms = 0.0;
    const auto afterPitch = renderPitch (session, afterRms);
    check (afterRms > 0.01,
           "the slot still renders above -40 dBFS after an editor change ("
               + std::to_string (afterRms) + ")");
    check (std::abs (afterPitch - 880.0) < 20.0,
           "the next render uses the new editor value ("
               + std::to_string (afterPitch) + " Hz)");

    const auto state880 = instrument->getStateInformation();
    check (state880.getSize() > 0, "the mirror produced a preset state");

    setFrequency (*instrument, 440.0);
    waitForSync();

    double resetRms = 0.0;
    const auto resetPitch = renderPitch (session, resetRms);
    check (std::abs (resetPitch - 440.0) < 20.0,
           "a second editor change reaches the sandbox");

    const auto burstStart = juce::Time::getMillisecondCounterHiRes();

    for (int change = 0; change < 64; ++change)
        setFrequency (*instrument, change % 2 == 0 ? 440.0 : 880.0);

    const auto burstMs = juce::Time::getMillisecondCounterHiRes() - burstStart;

    check (burstMs < 100.0,
           "64 editor changes do not block the message thread ("
               + std::to_string (burstMs) + " ms)");

    double burstRms = 0.0;
    const auto burstPitch = renderPitch (session, burstRms);
    check (burstRms > 0.01,
           "the slot renders above -40 dBFS after an edit burst ("
               + std::to_string (burstRms) + ")");
    check (std::abs (burstPitch - 880.0) < 20.0,
           "the last value of an edit burst is audible ("
               + std::to_string (burstPitch) + " Hz)");

    (void) instrument->setStateInformation (state880);
    waitForSync();

    double stateRms = 0.0;
    const auto statePitch = renderPitch (session, stateRms);
    check (stateRms > 0.01,
           "the slot renders above -40 dBFS after a preset state change ("
               + std::to_string (stateRms) + ")");
    check (std::abs (statePitch - 880.0) < 20.0,
           "the preset state reaches the sandbox ("
               + std::to_string (statePitch) + " Hz)");

    check (sandboxPid > 0 && session.getSandboxProcessId (slot) == sandboxPid,
           "the sandbox process was not replaced");
    check (! session.isSlotPluginFailed (slot),
           "the slot did not fail while the editor stayed open");

    if (sandbox != nullptr)
        check (sandbox->getPlayedBlockCount() > playedBefore,
               "the sandbox kept rendering blocks");
}

}
