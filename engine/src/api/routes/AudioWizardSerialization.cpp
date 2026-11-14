#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

    namespace
    {
        juce::var channelsToVar (const std::vector<int>& channels)
        {
            juce::Array<juce::var> result;

            for (const auto channel : channels)
                result.add (channel);

            return juce::var (result);
        }

        juce::var probeToVar (const AudioProbeState& probe)
        {
            auto root = Routes::makeObject ();
            auto* object = root.getDynamicObject ();

            object->setProperty ("sampleRate", probe.sampleRate);
            object->setProperty ("bufferSize", probe.bufferSize);
            object->setProperty ("latencyMs", probe.latencyMs);
            object->setProperty ("measuredMs", probe.measuredMs);
            object->setProperty ("expectedMs", probe.expectedMs);
            object->setProperty ("xruns", static_cast<juce::int64> (probe.xruns));
            object->setProperty ("opened", probe.opened);
            object->setProperty ("stable", probe.stable);
            object->setProperty ("recommended", probe.recommended);
            object->setProperty ("error", juce::String (probe.error));
            return root;
        }
    }

    juce::var audioProfilesToVar (const std::vector<AudioProfileState>& profiles)
    {
        juce::Array<juce::var> result;

        for (const auto& profile : profiles)
        {
            auto item = Routes::makeObject ();
            auto* object = item.getDynamicObject ();

            object->setProperty ("soundcardKey", juce::String (profile.soundcardKey));
            object->setProperty ("deviceType", juce::String (profile.deviceType));
            object->setProperty ("deviceName", juce::String (profile.deviceName));
            object->setProperty ("sampleRate", profile.sampleRate);
            object->setProperty ("bufferSize", profile.bufferSize);
            object->setProperty ("inputChannels", channelsToVar (profile.inputChannels));
            object->setProperty ("outputChannels", channelsToVar (profile.outputChannels));
            object->setProperty ("profile", juce::String (profile.profile));
            object->setProperty ("latencyMs", profile.latencyMs);
            object->setProperty ("measuredMs", profile.measuredMs);
            object->setProperty ("xruns", static_cast<juce::int64> (profile.xruns));
            result.add (item);
        }

        return juce::var (result);
    }

    juce::var audioWizardToVar (const AudioWizardState& wizard)
    {
        auto root = Routes::makeObject ();
        auto* object = root.getDynamicObject ();

        object->setProperty ("valid", wizard.valid);
        object->setProperty ("error", juce::String (wizard.error));
        object->setProperty ("deviceType", juce::String (wizard.deviceType));
        object->setProperty ("deviceName", juce::String (wizard.deviceName));
        object->setProperty ("soundcard", juce::String (wizard.soundcard));
        object->setProperty ("profile", juce::String (wizard.profile));
        object->setProperty ("sampleRate", wizard.sampleRate);
        object->setProperty ("bufferSize", wizard.bufferSize);
        object->setProperty ("inputChannels", channelsToVar (wizard.inputChannels));
        object->setProperty ("outputChannels", channelsToVar (wizard.outputChannels));
        object->setProperty ("latencyMs", wizard.latencyMs);
        object->setProperty ("measuredMs", wizard.measuredMs);
        object->setProperty ("expectedMs", wizard.expectedMs);
        object->setProperty ("xruns", static_cast<juce::int64> (wizard.xruns));
        object->setProperty ("tradeoff", juce::String (wizard.tradeoff));

        juce::Array<juce::var> candidates;

        for (const auto& probe : wizard.candidates)
            candidates.add (probeToVar (probe));

        object->setProperty ("candidates", juce::var (candidates));
        return root;
    }

}
