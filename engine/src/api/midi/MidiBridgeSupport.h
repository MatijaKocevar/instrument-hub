#pragma once

#include "api/MidiBridge.h"

#include <algorithm>
#include <cctype>

namespace Api::Detail {

inline std::string toStd(const juce::String& value) {
  return value.toStdString();
}

inline bool containsIgnoringCase(const juce::String& name,
                                 const char* fragment) {
  return name.toLowerCase().contains(fragment);
}

inline std::string lastToken(const juce::String& name) {
  const auto text = name.trim().toStdString();
  const auto space = text.find_last_of(' ');

  std::string token =
      space == std::string::npos ? text : text.substr(space + 1);

  std::transform(token.begin(), token.end(), token.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::toupper(character));
                 });

  return token;
}

inline MidiInputState baseStateFor(const MidiInputDevice& device) {
  MidiInputState state;
  state.id = device.id;
  state.name = device.name;
  state.role = MidiBridge::roleForName(juce::String(device.name));
  state.port = MidiBridge::portForName(juce::String(device.name));
  return state;
}

}  // namespace Api::Detail
