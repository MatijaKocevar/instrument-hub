#pragma once

#include "Kit.h"
#include "PanGains.h"
#include "SampleData.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ihub::sampler
{

    struct ResolvedHit
    {
        const SampleData* sample = nullptr;

        const KitPad* pad = nullptr;
        const VelocityLayer* layer = nullptr;

        int note = 0;
        int velocity = 0;

        int variant = 0;

        float gain = 1.0f;

        float pan = 0.0f;

        double playbackRatio = 1.0;

        bool isValid () const noexcept
        {
            return sample != nullptr && !sample->isEmpty ();
        }

        float leftGain () const noexcept
        {
            return panGainLeft (pan, gain);
        }
        float rightGain () const noexcept
        {
            return panGainRight (pan, gain);
        }
    };

    class LoadedKit final
    {
    public:
        LoadedKit () = default;
        ~LoadedKit () = default;

        LoadedKit (const LoadedKit&) = delete;
        LoadedKit& operator= (const LoadedKit&) = delete;

        LoadedKit (LoadedKit&&) noexcept = default;
        LoadedKit& operator= (LoadedKit&&) noexcept = default;

        bool isValid () const noexcept
        {
            return !kit.pads.empty ();
        }

        const Kit& getKit () const noexcept
        {
            return kit;
        }

        std::size_t getNumLoadedSamples () const noexcept
        {
            return samples.size ();
        }

        const SampleData* getLoadedSample (std::size_t index) const noexcept
        {
            return index < samples.size () ? samples[index].get () : nullptr;
        }

        ResolvedHit resolve (int note, int velocity) noexcept;

        ResolvedHit resolveWithVariant (int note, int velocity, int variantIndex) const noexcept;

    private:
        friend class KitLoader;

        struct LayerBinding
        {
            std::size_t padIndex = 0;
            std::size_t layerIndex = 0;
            std::size_t firstSample = 0;
            std::size_t sampleCount = 0;
        };

        struct PadBinding
        {
            int note = 0;
            std::size_t firstLayer = 0;
            std::size_t numLayers = 0;
        };

        int findPadBinding (int note) const noexcept;
        ResolvedHit
        makeHit (std::size_t layerBindingIndex, int variant, int note, int velocity) const noexcept;

        Kit kit;

        std::vector<std::shared_ptr<const SampleData>> samples;

        std::vector<const SampleData*> samplePointers;

        std::vector<LayerBinding> layerBindings;

        std::vector<PadBinding> padBindings;

        std::unique_ptr<std::atomic<std::uint32_t>[]> roundRobinCounters;
        std::size_t numRoundRobinCounters = 0;
    };

}
