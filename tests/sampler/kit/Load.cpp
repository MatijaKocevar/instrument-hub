#include "Load.h"

#include "../TestSupport.h"
#include "Constants.h"

#include "sampler/KitLoader.h"

#include <string>

namespace KitTest
{

using SamplerTest::check;
using ihub::sampler::KitLoader;
using ihub::sampler::LoadedKit;

void runLoadChecks (const juce::File& directory)
{
    const auto file = directory.getChildFile ("dedup.ihkit.json");
    const auto json = R"({
      "schemaVersion": 1,
      "name": "Dedup Kit",
      "pads": [
        {
          "note": 40,
          "layers": [
            { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] }
          ]
        },
        {
          "note": 41,
          "layers": [
            { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] }
          ]
        }
      ]
    })";
    file.replaceWithText (juce::String (json));

    LoadedKit fromFile;
    juce::String fileError;
    check (KitLoader::loadFromFile (file, engineRate, fromFile, fileError),
           "kit file loads relative to its own directory ("
               + fileError.toStdString() + ")");
    check (fromFile.getNumLoadedSamples() == 1,
           "a shared sample path is decoded once (got "
               + std::to_string (fromFile.getNumLoadedSamples()) + ")");
    check (fromFile.getLoadedSample (0) != nullptr
               && fromFile.getKit().findPad (40) != nullptr
               && fromFile.getKit().findPad (41) != nullptr,
           "both pads resolve from the shared sample");

    check (! KitLoader::loadFromFile (directory.getChildFile ("missing.ihkit.json"),
                                      engineRate, fromFile, fileError),
           "a missing kit file fails");
    check (fileError.isNotEmpty(), "missing kit file fills an error");
}

} // namespace KitTest
