#include "KitLoader.h"

#include <cmath>

namespace ihub::sampler {

namespace {

bool readEngineSampleRate(double engineSampleRate, juce::String& error) {
  if (!std::isfinite(engineSampleRate) || engineSampleRate < 8000.0 ||
      engineSampleRate > 384000.0) {
    error = "unsupported engine sample rate";
    return false;
  }

  return true;
}

}  // namespace

bool KitLoader::loadFromJson(const juce::String& json,
                             const juce::File& baseDirectory,
                             double engineSampleRate, LoadedKit& destination,
                             juce::String& error) {
  error.clear();

  if (!readEngineSampleRate(engineSampleRate, error)) return false;

  Kit parsed;

  if (!parse(json, parsed, error)) return false;

  const auto base = baseDirectory == juce::File()
                        ? juce::File::getCurrentWorkingDirectory()
                        : baseDirectory;

  return buildLoadedKit(parsed, base, engineSampleRate, destination, error);
}

bool KitLoader::loadFromFile(const juce::File& file, double engineSampleRate,
                             LoadedKit& destination, juce::String& error) {
  error.clear();

  if (!file.existsAsFile()) {
    error = "Kit file not found: " + file.getFullPathName();
    return false;
  }

  if (file.getSize() > maximumKitBytes) {
    error = "Kit file is larger than the " + juce::String(maximumKitBytes) +
            " byte limit";
    return false;
  }

  juce::String json;

  {
    juce::FileInputStream stream(file);

    if (!stream.openedOk()) {
      error = "Cannot read kit file: " + file.getFullPathName();
      return false;
    }

    json = stream.readEntireStreamAsString();
  }

  return loadFromJson(json, file.getParentDirectory(), engineSampleRate,
                      destination, error);
}

}  // namespace ihub::sampler
