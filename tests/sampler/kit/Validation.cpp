#include "Validation.h"

#include "../TestSupport.h"
#include "Constants.h"
#include "Fixtures.h"

#include "sampler/KitLoader.h"

#include <string>

namespace KitTest
{

using SamplerTest::check;
using ihub::sampler::Kit;
using ihub::sampler::KitLoader;
using ihub::sampler::LoadedKit;

namespace
{

bool parseFails (const std::string& json, const std::string& expectedFragment,
                 const std::string& label)
{
    Kit kit;
    juce::String error;
    const bool parsed = KitLoader::parse (juce::String (json), kit, error);

    const bool carriedFragment = error.contains (juce::String (expectedFragment));
    check (! parsed && error.isNotEmpty() && carriedFragment,
           label + " (error: " + error.toStdString() + ")");

    return ! parsed && carriedFragment;
}

}

void runValidationChecks (const juce::File& directory, LoadedKit& kit)
{
    parseFails ("{ not json", "not valid JSON", "malformed JSON is rejected");

    parseFails (R"({ "schemaVersion": 2, "name": "K", "pads": [] })",
                "unsupported kit schema version", "unknown schema version is rejected");

    parseFails (R"({ "schemaVersion": 1, "name": "K" })",
                "missing \"pads\"", "missing pads is rejected");

    parseFails (R"({ "schemaVersion": 1, "name": "K", "pads": [], "extra": 1 })",
                "unknown property", "unknown properties are rejected");

    const auto withBothLayers = [] (const std::string& padExtra,
                                    const std::string& first,
                                    const std::string& second)
    {
        return R"({
          "schemaVersion": 1,
          "name": "K",
          "pads": [
            { "note": 36, )" + padExtra + R"("layers": [)" + first + ","
               + second + R"(] }
          ]
        })";
    };

    const std::string full = R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })";
    const std::string lowOnly = R"({ "minVelocity": 1, "maxVelocity": 50, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })";
    const std::string overlap = R"({ "minVelocity": 50, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })";
    const std::string emptyRoundRobin = R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [] })";
    const std::string traversal = R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "../escape.wav" } ] })";

    parseFails (withBothLayers ("", lowOnly, overlap),
                "overlap", "overlapping velocity layers are rejected");
    parseFails (withBothLayers ("", lowOnly,
                                R"({ "minVelocity": 80, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })"),
                "no layer covers velocity", "a velocity gap is rejected");
    parseFails (withBothLayers ("", full, emptyRoundRobin),
                "no round-robin samples", "an empty round-robin list is rejected");
    parseFails (withBothLayers ("", traversal, full),
                "unsafe or empty sample path", "path traversal is rejected");
    parseFails (withBothLayers ("", R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav", "gain": 5 } ] })", full),
                "sample gain must be between 0 and 4", "sample gain out of range is rejected");
    parseFails (withBothLayers ("", R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav", "pan": 1.5 } ] })", full),
                "sample pan must be between -1 and 1", "sample pan out of range is rejected");
    parseFails (withBothLayers ("", R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav", "tune": 49 } ] })", full),
                "sample tune must be between -48 and 48", "sample tune out of range is rejected");
    parseFails (withBothLayers ("", R"({ "minVelocity": 80, "maxVelocity": 20, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })", full),
                "minVelocity is greater than maxVelocity", "an inverted layer is rejected");
    parseFails (withBothLayers ("\"gain\": 5, ", full, full),
                "gain must be between 0 and 4", "pad gain out of range is rejected");

    const auto badNote = R"({
      "schemaVersion": 1,
      "name": "K",
      "pads": [
        { "note": 128, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] } ] }
      ]
    })";
    parseFails (badNote, "note must be between 0 and 127",
                "an out-of-range pad note is rejected");

    const auto duplicateNote = R"({
      "schemaVersion": 1,
      "name": "K",
      "pads": [
        { "note": 36, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] } ] },
        { "note": 36, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-hard.wav" } ] } ] }
      ]
    })";
    parseFails (duplicateNote, "duplicate pad note", "duplicate pad notes are rejected");

    const auto missingSample = R"({
      "schemaVersion": 1,
      "name": "K",
      "pads": [
        { "note": 36, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "no-such-file.wav" } ] } ] }
      ]
    })";

    LoadedKit missing;
    juce::String missingError;
    check (! KitLoader::loadFromJson (juce::String (missingSample), directory,
                                      engineRate, missing, missingError)
               && missingError.contains ("no-such-file.wav"),
           "a missing sample fails with its path in the error ("
               + missingError.toStdString() + ")");
    check (! missing.isValid(), "a failed load leaves an empty kit");

    const auto nameBefore = kit.getKit().name;
    LoadedKit survivor = std::move (kit);
    juce::String survivorError;

    check (! KitLoader::loadFromJson (juce::String (missingSample), directory,
                                      engineRate, survivor, survivorError),
           "loading a broken kit into a loaded one fails");
    check (survivor.getKit().name == nameBefore && survivor.isValid(),
           "a failed load leaves the existing kit untouched");

    juce::String rateError;
    check (! KitLoader::loadFromJson (juce::String (twoLayerJson()), directory,
                                      0.0, survivor, rateError),
           "an invalid engine sample rate is rejected");
    check (survivor.getKit().name == nameBefore,
           "invalid sample rate also leaves the existing kit untouched");

    kit = std::move (survivor);
}

}
