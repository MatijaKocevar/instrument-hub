#include "ProjectFixtures.h"

#include "project/Serializer.h"

namespace ApiTest::ProjectFixture
{

std::string mappingYaml (const std::string& name,
                         const std::string& familyId,
                         const std::string& familyName,
                         const std::string& instrumentId,
                         const std::string& instrumentName,
                         int slot)
{
    return "schemaVersion: 1\n"
           "name: " + name + "\n"
           "families:\n"
           "  " + familyId + ":\n"
           "    name: " + familyName + "\n"
           "    colorIndex: 2\n"
           "    instruments:\n"
           "      - id: " + instrumentId + "\n"
           "        name: " + instrumentName + "\n"
           "        kind: empty\n"
           "        slot: " + std::to_string (slot) + "\n"
           "macros: {}\n"
           "encoders: []\n"
           "pads:\n"
           "  - pad: " + std::to_string (slot) + "\n"
           "    family: " + familyId + "\n"
           "    instrument: " + instrumentId + "\n";
}

std::string mappingYamlFromTemplate (const juce::File& templateFile,
                                     const std::string& familyId,
                                     const std::string& familyName)
{
    auto text = templateFile.loadFileAsString().toStdString();

    const auto replaceAll = [] (std::string& value, const std::string& from,
                                const std::string& to)
    {
        std::size_t position = 0;

        while ((position = value.find (from, position)) != std::string::npos)
        {
            value.replace (position, from.size(), to);
            position += to.size();
        }
    };

    replaceAll (text, "keys", familyId);
    replaceAll (text, "Keys", familyName);
    return text;
}

ihub::project::SetFile makeSet (const std::string& name,
                                const std::string& yaml,
                                int slot,
                                const std::string& toneName,
                                float gain,
                                float pan,
                                int colorIndex)
{
    ihub::project::SetFile set;
    set.name = name;
    set.mappingYaml = yaml;

    ihub::project::SlotConfig config;
    config.index = slot;
    config.name = toneName;
    config.gain = gain;
    config.pan = pan;
    config.colorIndex = colorIndex;
    config.instrument.kind = ihub::project::SourceKind::initTone;
    config.instrument.id = toneName;
    config.instrument.name = toneName;
    config.instrument.gain = gain;
    set.slots.push_back (std::move (config));
    return set;
}

bool writeSet (const juce::File& file,
               const ihub::project::SetFile& set,
               juce::String& error)
{
    return ihub::project::Serializer::save (set, file, error);
}

juce::MemoryBlock readBytes (const juce::File& file)
{
    juce::MemoryBlock data;
    (void) file.loadFileAsData (data);
    return data;
}

}
