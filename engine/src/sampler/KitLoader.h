#pragma once

#include "Kit.h"
#include "LoadedKit.h"
#include "SampleLoader.h"

#include <juce_core/juce_core.h>

namespace ihub::sampler
{

/**
    Reads, validates and decodes a JSON kit.

    ## Document

        {
          "schemaVersion": 1,
          "name": "Studio Kit",
          "pads": [
            {
              "note": 36,
              "name": "Kick",
              "gain": 1.0,
              "pan": 0.0,
              "tune": 0.0,
              "layers": [
                {
                  "minVelocity": 1,
                  "maxVelocity": 63,
                  "roundRobin": [
                    { "path": "kick-soft-1.wav" },
                    { "path": "kick-soft-2.wav" }
                  ]
                },
                {
                  "minVelocity": 64,
                  "maxVelocity": 127,
                  "roundRobin": [ { "path": "kick-hard.wav" } ]
                }
              ]
            }
          ]
        }

    Sample paths are relative to the kit file's directory unless absolute.
    The schema lives at `shared/schema/kit.schema.json`.

    ## Threading

    All loading functions decode synchronously on the calling thread and are
    for control threads only. `LoadedKit` is published to the audio thread
    only after it is fully built. A failed load leaves `destination`
    untouched.
*/
class KitLoader final
{
public:
    KitLoader() = delete;

    /** Largest kit document accepted, before parsing. */
    static constexpr juce::int64 maximumKitBytes = 1024 * 1024;

    /** Parses and validates `json` without reading any sample file. */
    static bool parse (const juce::String& json, Kit& destination, juce::String& error);

    /** Parses, validates and decodes every referenced sample.

        `baseDirectory` resolves relative sample paths. Returns false and
        fills `error` on a malformed document, a validation failure, or a
        missing/unreadable sample.
    */
    static bool loadFromJson (const juce::String& json,
                              const juce::File& baseDirectory,
                              double engineSampleRate,
                              LoadedKit& destination,
                              juce::String& error);

    /** Reads a kit file and decodes its samples relative to its directory. */
    static bool loadFromFile (const juce::File& file,
                              double engineSampleRate,
                              LoadedKit& destination,
                              juce::String& error);

private:
    static bool buildLoadedKit (const Kit& kit,
                                const juce::File& baseDirectory,
                                double engineSampleRate,
                                LoadedKit& destination,
                                juce::String& error);
};

} // namespace ihub::sampler
