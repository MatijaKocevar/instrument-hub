#include "Check.h"
#include "pad_audio/Render.h"
#include "pad_audio/Constants.h"
#include "pad_audio/Metrics.h"
#include "pad_audio/Rig.h"

#ifndef IHUB_SET_YAML
 #define IHUB_SET_YAML "shared/set.yaml"
#endif

namespace PadAudio
{
using E2E::check;


ihub::SessionConfig makeConfig (float fadeMilliseconds)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = kChannels;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = fadeMilliseconds;
    config.slots = ihub::Session::makeDefaultSet();

    for (auto& slot : config.slots)
    {
        slot.kind = ihub::SessionSlotKind::initTone;
        slot.instrumentKind = "init-tone";
    }

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

double renderRms (ihub::Session& session, int settleBlocks, int rmsBlocks)
{
    juce::AudioBuffer<float> buffer (kChannels, kBlockSize);
    buffer.clear();

    std::vector<float> samples;
    samples.reserve (static_cast<std::size_t> ((settleBlocks + rmsBlocks) * kBlockSize));

    for (int block = 0; block < settleBlocks + rmsBlocks; ++block)
        renderBlock (session, buffer, samples);

    return rmsDbfs (samples,
                    settleBlocks * kBlockSize,
                    (settleBlocks + rmsBlocks) * kBlockSize);
}

}
