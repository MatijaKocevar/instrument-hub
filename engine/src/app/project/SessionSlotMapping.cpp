#include "SessionSlotMapping.h"

#include <utility>

namespace ihub::app {

void applyProjectSlot(const project::SlotConfig& source, SessionSlotConfig& slot) {
    slot.gain = source.gain;
    slot.pan = source.pan;
    slot.muted = source.muted;
    slot.solo = source.solo;
    slot.colorIndex = source.colorIndex;

    if (!source.name.empty()) slot.name = source.name;

    slot.instrumentKind = project::toString(source.instrument.kind);
    slot.pluginFormat = source.instrument.pluginFormat;
    slot.pluginId = source.instrument.pluginId;
    slot.pluginName = source.instrument.pluginName;
    slot.pluginManufacturer = source.instrument.pluginManufacturer;
    slot.pluginVersion = source.instrument.pluginVersion;
    slot.samplePath = source.instrument.samplePath;
    slot.pluginState.reset();

    switch (source.instrument.kind) {
        case project::SourceKind::empty:
            slot.kind = SessionSlotKind::empty;
            break;

        case project::SourceKind::audioInput:
            slot.kind = SessionSlotKind::audioInput;
            slot.inputChannel = source.instrument.inputChannel;
            break;

        case project::SourceKind::sampler:
            slot.kind = SessionSlotKind::sampler;
            slot.path = juce::String(source.instrument.samplePath);
            slot.samplePath = source.instrument.samplePath;
            break;

        case project::SourceKind::initTone:
            slot.kind = SessionSlotKind::initTone;
            break;

        case project::SourceKind::plugin:
            slot.pluginFormat =
                source.instrument.pluginFormat.empty() ? "VST3" : source.instrument.pluginFormat;
            slot.path = juce::String(source.instrument.pluginPath);

            if (slot.path.isEmpty() && juce::File(juce::String(slot.pluginId)).exists())
                slot.path = juce::String(slot.pluginId);

            slot.kind = slot.path.isEmpty() ? SessionSlotKind::empty : SessionSlotKind::plugin;
            break;
    }

    slot.fxChain.clear();

    for (const auto& sourceFx : source.fxChain) {
        AudioEngine::FxInsertConfig fx;
        fx.bypassed = sourceFx.bypassed;
        fx.id = sourceFx.id;
        fx.name = sourceFx.name;

        if (sourceFx.kind == project::FxKind::plugin) {
            fx.type = AudioEngine::FxInsertType::vst3;
            fx.path = sourceFx.id;
        } else if (!sourceFx.type.empty()) {
            if (!AudioEngine::fxInsertTypeFromString(sourceFx.type, fx.type)) continue;
        } else if (!AudioEngine::fxInsertTypeFromString(sourceFx.id, fx.type)) {
            continue;
        }

        for (const auto& [key, value] : sourceFx.parameters) {
            if (key == "wet")
                fx.wet = juce::jlimit(0.0f, 1.0f, value);
            else
                fx.parameters[key] = value;
        }

        slot.fxChain.push_back(std::move(fx));
    }
}

}  // namespace ihub::app
