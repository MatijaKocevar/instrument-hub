#pragma once

#include <juce_core/juce_core.h>

namespace ihub::plugins::page_detail {

juce::String winePrefixForBundle(const juce::File& bundle);
void startWinePrefix(const juce::String& prefix);
void stopWinePrefixes(const juce::String& group,
                      const juce::String& knownPrefix);
bool warmWinePrefix(const juce::String& prefix, int timeoutMs);

}  // namespace ihub::plugins::page_detail
