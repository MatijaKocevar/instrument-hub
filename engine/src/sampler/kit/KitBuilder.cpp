#include "../KitLoader.h"

#include <map>
#include <utility>

namespace ihub::sampler
{

    bool KitLoader::buildLoadedKit (const Kit& kit,
                                    const juce::File& baseDirectory,
                                    double engineSampleRate,
                                    LoadedKit& destination,
                                    juce::String& error)
    {
        LoadedKit result;
        result.kit = kit;

        std::map<juce::String, std::shared_ptr<const SampleData>> cache;

        result.padBindings.reserve (kit.pads.size ());

        for (std::size_t padIndex = 0; padIndex < kit.pads.size (); ++padIndex)
        {
            const auto& pad = kit.pads[padIndex];

            LoadedKit::PadBinding padBinding;
            padBinding.note = pad.note;
            padBinding.firstLayer = result.layerBindings.size ();

            for (std::size_t layerIndex = 0; layerIndex < pad.layers.size (); ++layerIndex)
            {
                const auto& layer = pad.layers[layerIndex];

                LoadedKit::LayerBinding layerBinding;
                layerBinding.padIndex = padIndex;
                layerBinding.layerIndex = layerIndex;
                layerBinding.firstSample = result.samplePointers.size ();
                layerBinding.sampleCount = layer.roundRobin.size ();

                for (const auto& ref : layer.roundRobin)
                {
                    const auto file = baseDirectory.getChildFile (juce::String (ref.path));
                    const auto key = file.getFullPathName ();

                    auto found = cache.find (key);

                    if (found == cache.end ())
                    {
                        juce::String loadError;
                        auto sample =
                            SampleLoader::loadBlocking (file, engineSampleRate, loadError);

                        if (sample == nullptr)
                        {
                            error = "pad " + juce::String (pad.note) + ": sample \"" +
                                    juce::String (ref.path) + "\": " +
                                    (loadError.isNotEmpty () ? loadError
                                                             : juce::String ("load failed"));
                            return false;
                        }

                        found = cache.emplace (key, std::move (sample)).first;
                        result.samples.push_back (found->second);
                    }

                    result.samplePointers.push_back (found->second.get ());
                }

                result.layerBindings.push_back (layerBinding);
            }

            padBinding.numLayers = result.layerBindings.size () - padBinding.firstLayer;
            result.padBindings.push_back (padBinding);
        }

        result.numRoundRobinCounters = result.layerBindings.size ();

        if (result.numRoundRobinCounters > 0)
        {
            result.roundRobinCounters =
                std::make_unique<std::atomic<std::uint32_t>[]> (result.numRoundRobinCounters);

            for (std::size_t i = 0; i < result.numRoundRobinCounters; ++i)
                result.roundRobinCounters[i].store (0, std::memory_order_relaxed);
        }

        destination = std::move (result);
        return true;
    }

}
