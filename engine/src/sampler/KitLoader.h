#pragma once

#include "Kit.h"
#include "LoadedKit.h"
#include "SampleLoader.h"

#include <juce_core/juce_core.h>

namespace ihub::sampler {

class KitLoader final {
 public:
  KitLoader() = delete;

  static constexpr juce::int64 maximumKitBytes = 1024 * 1024;

  static bool parse(const juce::String& json, Kit& destination,
                    juce::String& error);

  static bool loadFromJson(const juce::String& json,
                           const juce::File& baseDirectory,
                           double engineSampleRate, LoadedKit& destination,
                           juce::String& error);

  static bool loadFromFile(const juce::File& file, double engineSampleRate,
                           LoadedKit& destination, juce::String& error);

 private:
  static bool buildLoadedKit(const Kit& kit, const juce::File& baseDirectory,
                             double engineSampleRate, LoadedKit& destination,
                             juce::String& error);
};

}  // namespace ihub::sampler
