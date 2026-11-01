#include "MappingModel.h"

namespace Controller
{

const InstrumentSpec* FamilySpec::findInstrument (const std::string& instrumentId) const noexcept
{
    for (const auto& instrument : instruments)
        if (instrument.id == instrumentId)
            return &instrument;

    return nullptr;
}

const PadBinding* SetMapping::padBinding (int pad) const noexcept
{
    if (pad < 0 || pad >= SetMapping::maximumPads)
        return nullptr;

    const auto& entry = pads[static_cast<std::size_t> (pad)];
    return entry.has_value() ? &*entry : nullptr;
}

const EncoderBinding* SetMapping::encoderBinding (int encoder) const noexcept
{
    if (encoder < 0 || encoder >= SetMapping::maximumEncoders)
        return nullptr;

    const auto& entry = encoders[static_cast<std::size_t> (encoder)];
    return entry.has_value() ? &*entry : nullptr;
}

const MacroBankSpec* SetMapping::bankBinding (int bank) const noexcept
{
    if (bank < 0 || bank >= SetMapping::maximumBanks)
        return nullptr;

    const auto& entry = banks[static_cast<std::size_t> (bank)];
    return entry.has_value() ? &*entry : nullptr;
}

const SceneSpec* SetMapping::sceneBinding (int scene) const noexcept
{
    if (scene < 0 || scene >= SetMapping::maximumScenes)
        return nullptr;

    const auto& entry = scenes[static_cast<std::size_t> (scene)];
    return entry.has_value() ? &*entry : nullptr;
}

const InstrumentSpec* SetMapping::instrumentFor (const PadBinding& binding) const noexcept
{
    const auto family = families.find (binding.family);

    if (family == families.end())
        return nullptr;

    return family->second.findInstrument (binding.instrument);
}

const MacroSpec* SetMapping::macroFor (const EncoderBinding& binding) const noexcept
{
    const auto macro = macros.find (binding.macro);
    return macro == macros.end() ? nullptr : &macro->second;
}

int SetMapping::colorIndexFor (const PadBinding& binding) const noexcept
{
    const auto family = families.find (binding.family);

    if (family == families.end())
        return 0;

    const auto* instrument = family->second.findInstrument (binding.instrument);
    return instrument != nullptr ? instrument->resolvedColorIndex
                                 : family->second.colorIndex;
}

int SetMapping::countPatches() const noexcept
{
    int count = 0;

    for (const auto& [familyId, family] : families)
    {
        (void) familyId;
        count += static_cast<int> (family.instruments.size());
    }

    return count;
}

int SetMapping::countPads() const noexcept
{
    int count = 0;

    for (const auto& entry : pads)
        if (entry.has_value())
            ++count;

    return count;
}

int SetMapping::countEncoders() const noexcept
{
    int count = 0;

    for (const auto& entry : encoders)
        if (entry.has_value())
            ++count;

    return count;
}

int SetMapping::countBanks() const noexcept
{
    int count = 0;

    for (const auto& entry : banks)
        if (entry.has_value())
            ++count;

    return count;
}

int SetMapping::countScenes() const noexcept
{
    int count = 0;

    for (const auto& entry : scenes)
        if (entry.has_value())
            ++count;

    return count;
}

std::vector<const InstrumentSpec*> SetMapping::allInstruments() const
{
    std::vector<const InstrumentSpec*> result;

    for (const auto& [familyId, family] : families)
        for (const auto& instrument : family.instruments)
            result.push_back (&instrument);

    return result;
}

}
