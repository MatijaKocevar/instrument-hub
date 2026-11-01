#include "Session.h"

#include "audio/RtGuard.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ihub
{

namespace
{
constexpr double silenceFloor = 1.0e-12;

double toDbfs (double linear) noexcept
{
    return 20.0 * std::log10 (juce::jmax (linear, silenceFloor));
}
} // namespace

SessionSwitchReport Session::measureSwitch (int fromSlot,
                                            int toSlot,
                                            int settleBlocks,
                                            int rmsBlocks,
                                            int switchBlocks)
{
    SessionSwitchReport report;
    report.fromSlot = fromSlot;
    report.toSlot = toSlot;

    if (! prepared || engine == nullptr || switcher == nullptr)
        return report;

    // Re-preparing and re-rendering the graph is only safe with the device
    // stopped. The method is a message-thread / offline tool.
    if (isDeviceOpen())
        return report;

    const int slotCount = engine->getNumSlots();

    if (fromSlot < 0 || fromSlot >= slotCount
        || toSlot < 0 || toSlot >= slotCount
        || fromSlot == toSlot)
    {
        return report;
    }

    // This is an offline tool: make both endpoints resident and pinned before
    // the references and the real render start from the same state.
    if (warmPool.getPool() != nullptr)
    {
        if (! warmPool.pinPairForMeasurement (fromSlot, toSlot, config.neighbourCount))
            return report;
    }

    const int blockSize = juce::jmax (1, maximumBlockSize);
    const int settleCount = juce::jmax (1, settleBlocks);
    const int rmsCount = juce::jmax (1, rmsBlocks);
    const int switchCount = juce::jmax (1, switchBlocks);

    const int settleSamples = settleCount * blockSize;
    const int rmsSamples = rmsCount * blockSize;
    const int switchSamples = switchCount * blockSize;
    const int totalSamples = settleSamples + rmsSamples + switchSamples;

    // The transition the product promises: a 3 ms equal-power crossfade.
    // The hard-switch control renders with the ramp disabled, so comparing it
    // with this reference is what makes the detector fire.
    const int referenceFadeSamples = juce::jmax (1,
                                                 juce::roundToInt (0.003 * sampleRate));

    auto renderIsolatedSlot = [this, blockSize] (int slotIndex, int numSamples)
    {
        std::vector<float> result;
        result.reserve (static_cast<std::size_t> (numSamples));

        juce::MidiBuffer midiBlock;

        while (static_cast<int> (result.size()) < numSamples)
        {
            {
                const AudioEngine::RtGuard::Scope rtScope;
                engine->getSlot (slotIndex).processUnfaded (blockSize, midiBlock);
            }

            const auto& rendered = engine->getSlot (slotIndex).getOutputBuffer();
            const int frames = juce::jmin (blockSize, numSamples - static_cast<int> (result.size()));

            for (int frame = 0; frame < frames; ++frame)
                result.push_back (rendered.getSample (0, frame));
        }

        return result;
    };

    // Reference source signals from the same prepared starting state as the
    // real render. `from` runs for the whole timeline; `to` starts at the
    // switch, exactly as SlotSwitcher starts it.
    prepare (sampleRate, maximumBlockSize, numChannels);
    auto fromReference = renderIsolatedSlot (fromSlot, totalSamples);

    prepare (sampleRate, maximumBlockSize, numChannels);
    auto toReference = renderIsolatedSlot (toSlot, switchSamples);

    // The real session render.
    prepare (sampleRate, maximumBlockSize, numChannels);
    resetGraph();

    juce::AudioBuffer<float> block (numChannels, blockSize);
    juce::MidiBuffer midiBlock;

    std::vector<float> captured;
    captured.reserve (static_cast<std::size_t> (rmsSamples + switchSamples));

    armSlot (fromSlot);

    // Let the fade-in finish before measuring the armed slot.
    for (int i = 0; i < settleCount; ++i)
        render (block, midiBlock, blockSize);

    double sumSquares = 0.0;
    int measuredSamples = 0;

    for (int i = 0; i < rmsCount; ++i)
    {
        render (block, midiBlock, blockSize);

        for (int frame = 0; frame < blockSize; ++frame)
        {
            const float sample = block.getSample (0, frame);
            sumSquares += static_cast<double> (sample) * static_cast<double> (sample);
            ++measuredSamples;
            captured.push_back (sample);
        }
    }

    if (measuredSamples > 0)
    {
        report.rms = std::sqrt (sumSquares / static_cast<double> (measuredSamples));
        report.rmsDbfs = toDbfs (report.rms);
    }

    armSlot (toSlot);

    for (int i = 0; i < switchCount; ++i)
    {
        render (block, midiBlock, blockSize);

        for (int frame = 0; frame < blockSize; ++frame)
            captured.push_back (block.getSample (0, frame));
    }

    double maxDelta = 0.0;
    double maxResidual = 0.0;

    for (std::size_t index = 1; index < captured.size(); ++index)
        maxDelta = juce::jmax (maxDelta,
                               std::abs (static_cast<double> (captured[index])
                                         - static_cast<double> (captured[index - 1])));

    const int switchInCaptured = rmsSamples;

    for (std::size_t index = 0; index < captured.size(); ++index)
    {
        const int global = settleSamples + static_cast<int> (index);
        double reference = fromReference[static_cast<std::size_t> (global)];

        const int local = static_cast<int> (index) - switchInCaptured;

        if (local >= 0)
        {
            const float phase = juce::jlimit (0.0f,
                                              1.0f,
                                              static_cast<float> (local)
                                                  / static_cast<float> (referenceFadeSamples));

            const double outgoing = std::cos (phase * juce::MathConstants<double>::halfPi);
            const double incoming = std::sin (phase * juce::MathConstants<double>::halfPi);

            const auto toIndex = static_cast<std::size_t> (local);
            const double toValue = toIndex < toReference.size()
                                       ? static_cast<double> (toReference[toIndex])
                                       : 0.0;

            reference = reference * outgoing + toValue * incoming;
        }

        maxResidual = juce::jmax (maxResidual,
                                  std::abs (static_cast<double> (captured[index]) - reference));
    }

    report.maxSampleDelta = maxDelta;
    report.maxSampleDeltaDbfs = toDbfs (maxDelta);
    report.discontinuity = maxResidual;
    report.discontinuityDbfs = toDbfs (maxResidual);
    report.samplesMeasured = static_cast<int> (captured.size());
    report.rendered = true;
    return report;
}

} // namespace ihub
