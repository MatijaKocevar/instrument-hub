#pragma once

#include <juce_core/juce_core.h>

#include <functional>

namespace ihub::settings {

using AtomicWriter =
    std::function<bool(const juce::File&, const juce::String&)>;

bool writeFileAtomically(const juce::File& target, const juce::String& contents,
                         const AtomicWriter& writer = {});

}  // namespace ihub::settings
