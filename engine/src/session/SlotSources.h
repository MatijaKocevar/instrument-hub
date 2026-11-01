#pragma once

#include "audio/Slot.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub::session
{

/** The built-in initialisation tone and the silent source.

    Both are real `SlotSource`s so the fixed graph and the warm pool stay
    uniform: an unassigned patch still has a warm instance, its meter is zero
    and MIDI is ignored. They are created by the warm-pool factory and owned by
    the instance, never by `Session`.
*/
std::unique_ptr<AudioEngine::SlotSource> makeInitToneSource (double frequency,
                                                             float amplitude);

std::unique_ptr<AudioEngine::SlotSource> makeSilentSource();

/** Creates a sampler source that owns its loaded kit.

    The warm pool creates and destroys instruments on its own threads, so the
    kit is owned by the pooled instance; reaching a shared kit through
    `Session` would race with that lifetime.
*/
std::unique_ptr<AudioEngine::SlotSource> makeSamplerSource (const juce::File& file,
                                                            double sampleRate,
                                                            juce::String& error);

} // namespace ihub::session
