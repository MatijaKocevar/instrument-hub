#include "Layers.h"

#include "../Fixtures.h"
#include "../Measurements.h"
#include "../TestSupport.h"
#include "Constants.h"
#include "Fixtures.h"
#include "RenderHit.h"

#include "sampler/KitLoader.h"

#include <cmath>
#include <string>

namespace KitTest
{

using SamplerTest::check;
using SamplerTest::estimateFrequency;
using SamplerTest::number;
using ihub::sampler::KitLoader;
using ihub::sampler::LoadedKit;
using ihub::sampler::VelocityLayer;

namespace
{

std::string layerLabel (const VelocityLayer* layer)
{
    if (layer == nullptr)
        return "null";

    return std::to_string (layer->minVelocity) + ".." + std::to_string (layer->maxVelocity);
}

} // namespace

void runLayersChecks (const juce::File& directory, LoadedKit& kit)
{
    const auto json = twoLayerJson();
    juce::String error;

    check (KitLoader::loadFromJson (juce::String (json), directory,
                                    engineRate, kit, error),
           "two-layer kit loads with its samples (" + error.toStdString() + ")");
    check (kit.isValid(), "loaded kit is valid");
    check (kit.getNumLoadedSamples() == 3,
           "three unique samples decoded (got "
               + std::to_string (kit.getNumLoadedSamples()) + ")");

    {
        const auto low = kit.resolve (36, 40);
        check (low.isValid(), "velocity 40 resolves to a sample");
        check (low.layer != nullptr && low.layer->minVelocity == 1
                   && low.layer->maxVelocity == 63,
               "velocity 40 selects layer " + layerLabel (low.layer));
        check (low.sample != nullptr && low.sample->path.contains ("kick-soft-a.wav"),
               "velocity 40 selects the soft sample");
        check (low.pad != nullptr && low.pad->note == 36 && low.note == 36
                   && low.velocity == 40,
               "resolved hit carries pad, note and velocity");
        check (std::abs (low.gain - 1.0f) < 1.0e-6f, "default layer gain is 1");
        check (std::abs (low.pan) < 1.0e-6f, "default pan is centre");
        check (std::abs (low.playbackRatio - 1.0) < 1.0e-9, "default tune leaves pitch alone");

        const auto renderedLow = renderHit (low);
        const auto lowPitch = estimateFrequency (renderedLow, engineRate);
        check (std::abs (lowPitch - 220.0) <= 3.0,
               "layer 1 plays at 220 Hz (measured " + number (lowPitch, 1) + " Hz)");

        const auto high = kit.resolve (36, 100);
        check (high.isValid(), "velocity 100 resolves to a sample");
        check (high.layer != nullptr && high.layer->minVelocity == 64
                   && high.layer->maxVelocity == 127,
               "velocity 100 selects layer " + layerLabel (high.layer));
        check (high.sample != nullptr && high.sample->path.contains ("kick-hard.wav"),
               "velocity 100 selects the hard sample");

        const auto renderedHigh = renderHit (high);
        const auto highPitch = estimateFrequency (renderedHigh, engineRate);
        check (std::abs (highPitch - 880.0) <= 5.0,
               "layer 2 plays at 880 Hz (measured " + number (highPitch, 1) + " Hz)");

        const auto boundaryLow = kit.resolveWithVariant (36, 63, 0);
        const auto boundaryHigh = kit.resolveWithVariant (36, 64, 0);

        check (boundaryLow.layer != nullptr && boundaryLow.layer->minVelocity == 1,
               "velocity 63 is still layer 1");
        check (boundaryHigh.layer != nullptr && boundaryHigh.layer->minVelocity == 64,
               "velocity 64 is already layer 2");
        check (! kit.resolve (37, 100).isValid(), "an unassigned note resolves to nothing");
        check (! kit.resolve (36, 0).isValid(), "velocity 0 resolves to nothing");
    }

    {
        const auto first = kit.resolve (36, 40);
        const auto second = kit.resolve (36, 40);
        const auto third = kit.resolve (36, 40);

        check (first.sample != second.sample, "consecutive hits use different variants");
        check (first.sample == third.sample, "round-robin wraps after two variants");
        check (first.variant == (second.variant + 1) % 2,
               "round-robin advances one variant per hit");

        const auto variantB = kit.resolveWithVariant (36, 40, 1);
        check (variantB.variant == 1 && second.sample != nullptr
                   && variantB.sample != nullptr,
               "resolveWithVariant picks the requested variant");
        check (kit.resolveWithVariant (36, 40, 0).sample
                   != kit.resolveWithVariant (36, 40, 1).sample,
               "the two soft variants are different decoded files");
    }

    {
        const auto tunedJson = R"({
          "schemaVersion": 1,
          "name": "Tune Kit",
          "pads": [
            {
              "note": 38,
              "name": "Snare",
              "gain": 0.5,
              "pan": -1.0,
              "tune": 12.0,
              "layers": [
                {
                  "minVelocity": 1,
                  "maxVelocity": 127,
                  "roundRobin": [
                    { "path": "kick-soft-a.wav", "gain": 0.5, "pan": 1.0, "tune": 0.0 }
                  ]
                }
              ]
            }
          ]
        })";

        LoadedKit tuned;
        juce::String tuneError;
        check (KitLoader::loadFromJson (juce::String (tunedJson), directory,
                                        engineRate, tuned, tuneError),
               "gain/pan/tune kit loads (" + tuneError.toStdString() + ")");

        const auto hit = tuned.resolve (38, 100);
        check (hit.isValid(), "tuned pad resolves");
        check (std::abs (hit.gain - 0.25f) < 1.0e-6f,
               "pad gain 0.5 x sample gain 0.5 = 0.25 (got " + number (hit.gain, 4) + ")");
        check (std::abs (hit.pan) < 1.0e-6f,
               "pad pan -1 + sample pan +1 = centre (got " + number (hit.pan, 4) + ")");
        check (std::abs (hit.playbackRatio - 2.0) < 1.0e-9,
               "12 semitones double the playback rate");

        const auto rendered = renderHit (hit);
        const auto pitch = estimateFrequency (rendered, engineRate);
        check (std::abs (pitch - 440.0) <= 5.0,
               "tuned sample plays an octave higher (measured " + number (pitch, 1) + " Hz)");
        check (std::abs (rendered.getRMSLevel (0, 0, rendered.getNumSamples())
                         - rendered.getRMSLevel (1, 0, rendered.getNumSamples())) < 1.0e-3f,
               "centre pan gives equal left and right level");

        LoadedKit hardLeft;
        juce::String leftError;
        const auto leftJson = R"({
          "schemaVersion": 1,
          "name": "Left Kit",
          "pads": [
            {
              "note": 39,
              "pan": -1.0,
              "layers": [
                { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] }
              ]
            }
          ]
        })";
        check (KitLoader::loadFromJson (juce::String (leftJson), directory,
                                        engineRate, hardLeft, leftError),
               "hard-left kit loads");
        const auto leftHit = hardLeft.resolve (39, 100);
        check (leftHit.leftGain() > 0.9f && std::abs (leftHit.rightGain()) < 1.0e-6f,
               "hard-left pan puts the hit only on the left");
    }
}

} // namespace KitTest
