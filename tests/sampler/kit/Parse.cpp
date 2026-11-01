#include "Parse.h"

#include "../TestSupport.h"
#include "Fixtures.h"

#include "sampler/KitLoader.h"

#include <fstream>
#include <iterator>
#include <string>

#ifndef IHUB_KIT_SCHEMA
 #define IHUB_KIT_SCHEMA "shared/schema/kit.schema.json"
#endif

namespace KitTest
{

using SamplerTest::check;
using ihub::sampler::Kit;
using ihub::sampler::KitLoader;

void runParseChecks()
{
    std::ifstream schemaFile (IHUB_KIT_SCHEMA);
    const std::string schema ((std::istreambuf_iterator<char> (schemaFile)),
                              std::istreambuf_iterator<char>());

    check (! schema.empty()
               && schema.find ("\"schemaVersion\"") != std::string::npos
               && schema.find ("\"roundRobin\"") != std::string::npos
               && schema.find ("\"minVelocity\"") != std::string::npos,
           "shared/schema/kit.schema.json describes schemaVersion, layers and roundRobin");

    const auto json = twoLayerJson();

    Kit parsedKit;
    juce::String error;
    check (KitLoader::parse (juce::String (json), parsedKit, error),
           "two-layer kit parses (" + error.toStdString() + ")");
    check (parsedKit.name == "Test Kit", "kit name is read");
    check (parsedKit.pads.size() == 1, "one pad is read");
    check (parsedKit.findPad (36) != nullptr, "findPad(36) finds the pad");
    check (parsedKit.findPad (37) == nullptr, "findPad(37) returns nullptr");
    check (parsedKit.pads[0].layers.size() == 2, "two layers are read");
    check (parsedKit.pads[0].layerForVelocity (1) != nullptr
               && parsedKit.pads[0].layerForVelocity (63) != nullptr
               && parsedKit.pads[0].layerForVelocity (64) != nullptr
               && parsedKit.pads[0].layerForVelocity (127) != nullptr,
           "velocity lookup covers both layers");
}

} // namespace KitTest
