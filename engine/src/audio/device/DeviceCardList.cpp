#include "audio/DeviceManager.h"

#include "audio/device/DeviceCards.h"
#include "audio/device/DevicePcmHints.h"

#include <utility>

namespace AudioEngine
{

namespace
{
bool isLoopbackCard (const detail::AlsaCard& card)
{
    return card.id.containsIgnoreCase ("Loopback")
           || card.name.containsIgnoreCase ("Loopback")
           || card.pcmName.containsIgnoreCase ("Loopback");
}

bool isMidiCard (const detail::AlsaCard& card)
{
    return card.id.containsIgnoreCase ("VirMIDI")
           || card.name.containsIgnoreCase ("VirMIDI")
           || card.name.containsIgnoreCase ("Launchpad")
           || card.name.containsIgnoreCase ("Keystation");
}
}

std::vector<DeviceInfo> DeviceManager::getCardDevices()
{
    std::vector<DeviceInfo> result;
    auto* type = findDeviceType ("ALSA");

    if (type == nullptr)
        return result;

    const auto outputNames = type->getDeviceNames (false);
    const auto hints = detail::readAlsaPcmHints();
    const auto cards = detail::enumerateAlsaCards();

    juce::StringArray seenNames;

    for (const auto& card : cards)
    {
        if (isMidiCard (card))
            continue;

        if (! listOptions.showTestDevices && isLoopbackCard (card))
            continue;

        auto friendlyName = detail::friendlyCardName (card);

        if (seenNames.contains (friendlyName))
            friendlyName << " (" << card.id << ")";

        seenNames.addIfNotAlreadyThere (friendlyName);

        bool busy = false;
        bool opened = false;
        juce::String firstJuceName;
        juce::String firstPcmName;
        const auto description = detail::cardDescription (card);

        for (const auto& candidate : detail::cardPcmCandidates (card))
        {
            const auto juceName = detail::juceDeviceNameForPcm (hints, candidate);

            if (juceName.isEmpty() || ! outputNames.contains (juceName))
                continue;

            if (firstJuceName.isEmpty())
            {
                firstJuceName = juceName;
                firstPcmName = candidate;
            }

            DeviceInfo info;

            if (probeListedDevice (friendlyName,
                                   juceName,
                                   candidate,
                                   card.hasInput,
                                   info,
                                   busy))
            {
                info.description = description;
                result.push_back (std::move (info));
                opened = true;
                break;
            }
        }

        if (opened || ! busy)
            continue;

        DeviceInfo info;
        info.typeName = "ALSA";
        info.name = friendlyName;
        info.description = description;
        info.juceName = firstJuceName;
        info.pcmName = firstPcmName;
        info.hasOutput = true;
        info.hasInput = card.hasInput;
        info.available = false;
        info.unavailableReason = "in use by another process";
        result.push_back (std::move (info));
    }

    return result;
}

std::vector<DeviceInfo> DeviceManager::getSystemDevice()
{
    std::vector<DeviceInfo> result;
    auto* type = findDeviceType ("ALSA");

    if (type == nullptr)
        return result;

    const auto outputNames = type->getDeviceNames (false);
    const auto hints = detail::readAlsaPcmHints();

    const auto pulseName = detail::juceDeviceNameForPcm (hints, "pulse");
    const std::vector<std::pair<juce::String, juce::String>> candidates {
        { "pulse", pulseName },
        { "default", outputNames.contains ("Default ALSA Output")
                         ? juce::String ("Default ALSA Output")
                         : juce::String() }
    };

    for (const auto& [pcmName, juceName] : candidates)
    {
        if (juceName.isEmpty() || ! outputNames.contains (juceName))
            continue;

        DeviceInfo info;
        bool busy = false;

        if (probeListedDevice ("System (PipeWire)",
                               juceName,
                               pcmName,
                               true,
                               info,
                               busy))
        {
            info.description = "System mixer";
            info.isDefaultOutput = true;
            result.push_back (std::move (info));
            return result;
        }

        if (busy)
        {
            DeviceInfo unavailable;
            unavailable.typeName = "ALSA";
            unavailable.name = "System (PipeWire)";
            unavailable.description = "System mixer";
            unavailable.juceName = juceName;
            unavailable.pcmName = pcmName;
            unavailable.hasOutput = true;
            unavailable.hasInput = true;
            unavailable.available = false;
            unavailable.unavailableReason = "in use by another process";
            result.push_back (std::move (unavailable));
            return result;
        }
    }

    return result;
}

} // namespace AudioEngine
