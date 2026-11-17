#pragma once

#include "dsp/drive/DriveFilters.h"
#include "dsp/drive/DriveVoicing.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>

namespace ihub::dsp {

struct DriveSettings {
  int model = 0;
  float drive = 0.35f;
  float tone = 0.55f;
  float levelDb = -8.0f;
  bool cab = true;

  bool operator==(const DriveSettings&) const = default;
};

class Drive final : public juce::AudioProcessor {
 public:
  static constexpr int numModels = static_cast<int>(DriveModel::numModels);
  static constexpr float minDrive = 0.0f;
  static constexpr float maxDrive = 1.0f;
  static constexpr float minTone = 0.0f;
  static constexpr float maxTone = 1.0f;
  static constexpr float minLevelDb = -24.0f;
  static constexpr float maxLevelDb = 12.0f;

  Drive();
  ~Drive() override = default;

  void prepareToPlay(double sampleRate,
                     int maximumExpectedSamplesPerBlock) override;
  void releaseResources() override;
  void reset() override;

  using juce::AudioProcessor::processBlock;
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi) override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  bool hasEditor() const override { return false; }
  juce::AudioProcessorEditor* createEditor() override { return nullptr; }
  const juce::String getName() const override { return "IHub Drive"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "Default"; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  void setSettings(const DriveSettings& settings);
  DriveSettings getSettings() const noexcept;

  void setModel(int model);
  void setDrive(float drive);
  void setTone(float tone);
  void setLevelDb(float levelDb);
  void setCab(bool shouldUseCab);

  bool isPrepared() const noexcept { return prepared; }

 private:
  static DriveSettings clampSettings(const DriveSettings& settings) noexcept;

  static float driveDbFor(const DriveSettings& settings) noexcept;
  static float gainForDecibels(float decibels) noexcept;

  void updateTargets(const DriveSettings& settings) noexcept;
  void finishModelSwitch(const DriveSettings& settings) noexcept;

  std::atomic<int> model{0};
  std::atomic<float> drive{0.35f};
  std::atomic<float> tone{0.55f};
  std::atomic<float> levelDb{-8.0f};
  std::atomic<bool> cab{true};

  double currentSampleRate = 48000.0;
  int maximumBlockSize = 0;
  int numChannels = 2;
  bool prepared = false;

  DriveFilters filters;
  std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveGain;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> levelGain;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cabMix;
  juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modelFade;

  int appliedModel = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Drive)
};

}  // namespace ihub::dsp
