#include "track_grid/Rig.h"

#include "track_grid/Constants.h"
#include "controller/LaunchpadProtocol.h"
#include "plugins/SandboxHost.h"

#include <cmath>
#include <vector>

namespace TrackGridE2E
{

void Rig::press (int pad)
{
    const int note = Launchpad::padNoteForIndex (pad);

    driver.injectMidiMessage (juce::MidiMessage::noteOn (
        1, note, static_cast<juce::uint8> (127)));
    driver.injectMidiMessage (juce::MidiMessage::noteOn (
        1, note, static_cast<juce::uint8> (0)));
}

void Rig::renderBlocks (int blocks)
{
    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
    auto& timing = sessionInstance.getEngine().getTimingMonitor();

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        const auto token = timing.beginCallback();
        sessionInstance.render (buffer, midi, kBlockSize);
        timing.endCallback (token, kBlockSize);
    }
}

void Rig::waitForActiveSandbox()
{
    const int slot = sessionInstance.getArmedSlot();
    auto* sandbox = sessionInstance.getSandboxSlotControl (slot);

    if (sandbox == nullptr)
        return;

    const auto before = sandbox->getPlayedBlockCount();
    const auto target = before + 3;

    for (int attempt = 0; attempt < 400; ++attempt)
    {
        renderBlocks (1);

        if (sandbox->getPlayedBlockCount() >= target)
            return;

        juce::Thread::sleep (2);
    }
}

double Rig::renderRms (int settleBlocks, int rmsBlocks)
{
    waitForActiveSandbox();

    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
    std::vector<float> samples;
    samples.reserve (static_cast<std::size_t> ((settleBlocks + rmsBlocks) * kBlockSize));

    auto& timing = sessionInstance.getEngine().getTimingMonitor();

    for (int block = 0; block < settleBlocks + rmsBlocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        const auto token = timing.beginCallback();
        sessionInstance.render (buffer, midi, kBlockSize);
        timing.endCallback (token, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
            samples.push_back (buffer.getSample (0, frame));

        juce::Thread::sleep (1);
    }

    const int first = juce::jlimit (0, static_cast<int> (samples.size()),
                                    settleBlocks * kBlockSize);
    const int last = static_cast<int> (samples.size());

    if (last <= first)
        return -1000.0;

    double sumSquares = 0.0;

    for (int index = first; index < last; ++index)
    {
        const double sample = static_cast<double> (samples[static_cast<std::size_t> (index)]);
        sumSquares += sample * sample;
    }

    const double rms = std::sqrt (sumSquares / static_cast<double> (last - first));
    return 20.0 * std::log10 (juce::jmax (rms, 1.0e-12));
}

void Rig::paintLeds()
{
    paintTrackLeds (*stateInstance, mapping, driver);
}

}
