#include "audio/device/PipeWireQuantum.h"

namespace AudioEngine::detail {

namespace {
constexpr auto metadataProgram = "pw-metadata";

juce::StringArray settingsArguments() {
  juce::StringArray arguments;
  arguments.add(metadataProgram);
  arguments.add("-n");
  arguments.add("settings");
  arguments.add("0");
  return arguments;
}

bool runMetadata(const juce::StringArray& arguments, juce::String& output) {
  juce::ChildProcess process;

  if (!process.start(arguments)) return false;

  output = process.readAllProcessOutput();

  return process.waitForProcessToFinish(2000) && process.getExitCode() == 0;
}

std::optional<int> parseForceQuantum(const juce::String& listing) {
  for (const auto& line : juce::StringArray::fromLines(listing)) {
    if (!line.contains("key:'clock.force-quantum'")) continue;

    const auto value = line.fromFirstOccurrenceOf("value:'", false, false)
                           .upToFirstOccurrenceOf("'", false, false)
                           .getIntValue();

    if (value > 0) return value;
  }

  return std::nullopt;
}
}  // namespace

PipeWireQuantum::~PipeWireQuantum() { restore(); }

bool PipeWireQuantum::apply(int frames, juce::String* errorMessage) {
  if (frames <= 0) return true;

  if (applied && appliedFrames == frames) return true;

  if (!captured) {
    juce::String listing;

    if (!runMetadata(settingsArguments(), listing)) return true;

    previous = parseForceQuantum(listing);
    captured = true;
  }

  auto arguments = settingsArguments();
  arguments.add("clock.force-quantum");
  arguments.add(juce::String(frames));

  juce::String output;

  if (!runMetadata(arguments, output)) {
    if (errorMessage != nullptr)
      *errorMessage = "could not set the JACK buffer size";

    return false;
  }

  applied = true;
  appliedFrames = frames;
  return true;
}

void PipeWireQuantum::restore() {
  if (!applied) {
    captured = false;
    previous.reset();
    return;
  }

  juce::StringArray arguments;
  arguments.add(metadataProgram);

  if (previous.has_value()) {
    arguments.add("-n");
    arguments.add("settings");
    arguments.add("0");
    arguments.add("clock.force-quantum");
    arguments.add(juce::String(*previous));
  } else {
    arguments.add("-d");
    arguments.add("-n");
    arguments.add("settings");
    arguments.add("0");
    arguments.add("clock.force-quantum");
  }

  juce::String output;
  runMetadata(arguments, output);

  applied = false;
  captured = false;
  appliedFrames = 0;
  previous.reset();
}

}  // namespace AudioEngine::detail
