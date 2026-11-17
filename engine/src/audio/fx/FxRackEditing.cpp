#include "audio/FxRack.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace AudioEngine {

bool FxRack::add(const FxInsertConfig& config, int position, double sampleRate,
                 int maximumBlockSize, int numChannels, std::string& error) {
    std::unique_ptr<InsertEffect> effect;

    if (!create(config, sampleRate, maximumBlockSize, numChannels, effect, error)) return false;

    Insert insert;
    insert.type = config.type;
    insert.id = effect->getId();
    insert.name = effect->getName().toStdString();
    insert.effect = std::move(effect);

    const int target = position < 0 ? size() : juce::jlimit(0, size(), position);

    inserts.insert(inserts.begin() + static_cast<std::ptrdiff_t>(target), std::move(insert));
    return true;
}

bool FxRack::replace(const std::vector<FxInsertConfig>& configs, double sampleRate,
                     int maximumBlockSize, int numChannels, std::string& error) {
    std::vector<Insert> replacement;
    replacement.reserve(configs.size());

    for (const auto& config : configs) {
        std::unique_ptr<InsertEffect> effect;

        if (!create(config, sampleRate, maximumBlockSize, numChannels, effect, error)) return false;

        Insert insert;
        insert.type = config.type;
        insert.id = effect->getId();
        insert.name = effect->getName().toStdString();
        insert.effect = std::move(effect);
        replacement.push_back(std::move(insert));
    }

    inserts = std::move(replacement);
    return true;
}

bool FxRack::update(int index, const FxInsertPatch& patch, double sampleRate, int maximumBlockSize,
                    int numChannels, std::string& error) {
    juce::ignoreUnused(sampleRate, maximumBlockSize, numChannels);

    if (index < 0 || index >= size()) {
        error = "fx index is out of range";
        return false;
    }

    auto& insert = inserts[static_cast<std::size_t>(index)];
    auto& effect = *insert.effect;

    if (patch.wet.has_value()) {
        const float wet = *patch.wet;

        if (!std::isfinite(wet) || wet < 0.0f || wet > 1.0f) {
            error = "wet must be between 0 and 1";
            return false;
        }

        effect.setWet(wet);
    }

    if (patch.bypassed.has_value()) effect.setBypassed(*patch.bypassed);

    if (patch.parameters.has_value() && !applyParameters(effect, *patch.parameters, true, error))
        return false;

    if (patch.name.has_value()) {
        const auto& name = *patch.name;

        if (name.empty() || name.size() > 120) {
            error = "name must be 1..120 characters";
            return false;
        }

        insert.name = name;
    }

    if (patch.position.has_value()) {
        const int position = *patch.position;

        if (position < 0 || position >= size()) {
            error = "position is out of range";
            return false;
        }

        moveInsert(index, position);
    }

    return true;
}

bool FxRack::remove(int index, std::string& error) {
    if (index < 0 || index >= size()) {
        error = "fx index is out of range";
        return false;
    }

    inserts.erase(inserts.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void FxRack::clear() noexcept { inserts.clear(); }

}  // namespace AudioEngine
