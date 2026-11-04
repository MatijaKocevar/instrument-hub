#include "track_grid/RigConfig.h"

#include "track_grid/Constants.h"

#include "app/project/ProjectImport.h"
#include "app/project/ProjectSession.h"
#include "project/Serializer.h"

namespace TrackGridE2E
{

namespace
{

bool fail (std::string& errorText, const juce::String& message)
{
    errorText = message.toStdString();
    return false;
}

AudioEngine::FxInsertConfig makeEq()
{
    AudioEngine::FxInsertConfig insert;
    insert.type = AudioEngine::FxInsertType::equalizer;
    insert.id = "eq";
    insert.name = "EQ";
    return insert;
}

AudioEngine::FxInsertConfig makeCompressor()
{
    AudioEngine::FxInsertConfig insert;
    insert.type = AudioEngine::FxInsertType::compressor;
    insert.id = "comp";
    insert.name = "Compressor";
    return insert;
}

}

bool makeTrackProject (const juce::File& tracksYaml,
                       const juce::File& projectFile,
                       std::string& errorText)
{
    if (! tracksYaml.existsAsFile())
        return fail (errorText, "the track template is missing: "
                                    + tracksYaml.getFullPathName());

    ihub::project::SetFile set;
    juce::String error;

    if (! ihub::app::importBundledSet (tracksYaml, {}, {}, set, error))
        return fail (errorText, "the track template imports: " + error);

    if (! ihub::project::Serializer::save (set, projectFile, error))
        return fail (errorText, "the track project saves: " + error);

    return true;
}

bool loadTrackConfig (const juce::File& projectFile,
                      const juce::String& pluginPath,
                      ihub::SessionConfig& config,
                      std::string& errorText)
{
    if (pluginPath.isEmpty() || ! juce::File (pluginPath).exists())
        return fail (errorText, "TestPlugin.vst3 is not built: " + pluginPath);

    juce::String error;

    if (! ihub::app::loadProjectSession (projectFile, config, error))
        return fail (errorText, "the track project loads: " + error);

    if (config.layout != ihub::SessionLayout::tracks)
        return fail (errorText, "the new project is not in the track layout");

    if (config.tracks.size() != kTrackCount)
        return fail (errorText, "the new project does not have eight tracks");

    if (config.slots.size() != kTrackCount)
        return fail (errorText, "the new project does not have eight track slots");

    config.initialSlot = 0;

    for (int index = 0; index < kTrackCount; ++index)
    {
        auto& slot = config.slots[static_cast<std::size_t> (index)];

        slot.kind = ihub::SessionSlotKind::plugin;
        slot.path = pluginPath;
        slot.pluginFormat = "VST3";
        slot.pluginId = "com.instrumenthub.TestPlugin";
        slot.pluginName = "TestPlugin";
        slot.pluginManufacturer = "Instrument Hub";
        slot.pluginVersion = "1.0.0";
        slot.instrumentKind = "plugin";
        slot.colorIndex = index;
        slot.gain = 1.0f;
        slot.fxChain = { makeEq(), makeCompressor() };

        config.tracks[static_cast<std::size_t> (index)].hotFx = { "eq", "comp" };
    }

    return true;
}

}
