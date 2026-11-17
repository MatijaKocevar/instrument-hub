#pragma once

#include "FxChain.h"
#include "fx/FxInsertTypes.h"
#include "fx/InsertEffect.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace AudioEngine {

class FxRack {
 public:
  explicit FxRack(juce::AudioPluginFormatManager* formatManagerToUse);
  ~FxRack() = default;

  bool add(const FxInsertConfig& config, int position, double sampleRate,
           int maximumBlockSize, int numChannels, std::string& error);

  bool replace(const std::vector<FxInsertConfig>& configs, double sampleRate,
               int maximumBlockSize, int numChannels, std::string& error);

  bool update(int index, const FxInsertPatch& patch, double sampleRate,
              int maximumBlockSize, int numChannels, std::string& error);

  bool remove(int index, std::string& error);

  bool reorder(const std::vector<int>& order, std::string& error);

  void clear() noexcept;

  void rebuild(FxChain& chain, double sampleRate, int maximumBlockSize,
               int numChannels);

  int size() const noexcept { return static_cast<int>(inserts.size()); }
  int latencySamples() const noexcept;

  bool getTunerReading(int index, FxTunerReading& reading) const;

  FxInsertSnapshot getInsert(int index) const;
  std::vector<FxInsertSnapshot> snapshots() const;

  void setTransport(const FxTransport& transport) noexcept;
  const FxTransport& getTransport() const noexcept { return transport; }

 private:
  struct Insert {
    std::unique_ptr<InsertEffect> effect;
    FxInsertType type = FxInsertType::equalizer;
    std::string id;
    std::string name;
  };

  bool create(const FxInsertConfig& config, double sampleRate,
              int maximumBlockSize, int numChannels,
              std::unique_ptr<InsertEffect>& destination,
              std::string& error) const;

  bool applyParameters(InsertEffect& effect,
                       const std::map<std::string, float>& parameters,
                       bool replaceAll, std::string& error) const;

  std::map<std::string, float> readParameters(const InsertEffect& effect) const;

  void moveInsert(int from, int to) noexcept;

  void applyTransportTo(InsertEffect& effect) const;

  juce::AudioPluginFormatManager* formatManager = nullptr;
  std::vector<Insert> inserts;
  FxTransport transport;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxRack)
};

}  // namespace AudioEngine
