#include "SandboxClientArguments.h"

#include "../SandboxClient.h"

#include <algorithm>

namespace ihub::plugins::sandbox_client {
namespace {
bool takeString(const juce::StringArray& commandLine, int& index,
                juce::String& target) {
  if (index + 1 >= commandLine.size()) return false;

  target = commandLine[++index];
  return true;
}

bool parseIntegerArgument(const juce::StringArray& commandLine, int& index,
                          int minimum, int maximum, int& target,
                          juce::String& error, const juce::String& option) {
  juce::String value;

  if (!takeString(commandLine, index, value)) {
    error = "missing value after " + option;
    return false;
  }

  target = value.getIntValue();

  if (!value.containsOnly("0123456789") || target < minimum ||
      target > maximum) {
    error = "invalid value for " + option + ": " + value;
    return false;
  }

  return true;
}

bool parseParameter(const juce::String& value, ParameterAssignment& assignment,
                    juce::String& error) {
  const auto separator = value.indexOfChar('=');

  if (separator <= 0 || separator + 1 >= value.length()) {
    error = "invalid parameter assignment (expected name=value): " + value;
    return false;
  }

  assignment.name = value.substring(0, separator).trim();
  assignment.value = value.substring(separator + 1).trim().getFloatValue();
  return true;
}

PageSlotArguments& slotFor(ChildArguments& arguments, int slotIndex) {
  const auto found =
      std::find_if(arguments.pageSlots.begin(), arguments.pageSlots.end(),
                   [slotIndex](const PageSlotArguments& slot) {
                     return slot.slotIndex == slotIndex;
                   });

  if (found != arguments.pageSlots.end()) return *found;

  PageSlotArguments slot;
  slot.slotIndex = slotIndex;
  arguments.pageSlots.push_back(std::move(slot));
  return arguments.pageSlots.back();
}
}  // namespace

ChildArguments parseArguments(const juce::StringArray& commandLine) {
  ChildArguments arguments;

  for (int i = 1; i < commandLine.size(); ++i) {
    const auto& argument = commandLine[i];

    if (argument == sandbox::cli::mapFlag) {
      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing value after " + argument;
        return arguments;
      }

      arguments.mapFile = juce::File(value).getFullPathName();
    } else if (argument == sandbox::cli::pluginFlag) {
      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing value after " + argument;
        return arguments;
      }

      arguments.pluginBundle = juce::File(value).getFullPathName();
    } else if (argument == sandbox::cli::fakeToneFlag) {
      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing value after " + argument;
        return arguments;
      }

      arguments.fakeToneFrequency = value.getDoubleValue();

      if (arguments.fakeToneFrequency <= 0.0) {
        arguments.error = "invalid fake tone frequency: " + value;
        return arguments;
      }
    } else if (argument == sandbox::cli::abortAfterFlag) {
      int blocks = 0;

      if (!parseIntegerArgument(commandLine, i, 0, 1000000, blocks,
                                arguments.error, argument))
        return arguments;

      arguments.abortAfterBlocks = blocks;
    } else if (argument == sandbox::cli::paramFlag) {
      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing value after " + argument;
        return arguments;
      }

      ParameterAssignment assignment;

      if (!parseParameter(value, assignment, arguments.error)) return arguments;

      arguments.parameters.push_back(assignment);
    } else if (argument == sandbox::cli::pageMapFlag) {
      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing value after " + argument;
        return arguments;
      }

      arguments.pageMapFile = juce::File(value).getFullPathName();
    } else if (argument == sandbox::cli::pageIdFlag) {
      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing value after " + argument;
        return arguments;
      }

      arguments.pageId = value;
    } else if (argument == sandbox::cli::pageSlotFlag) {
      int slotIndex = -1;

      if (!parseIntegerArgument(commandLine, i, 0,
                                sandbox::maximumPageSlots - 1, slotIndex,
                                arguments.error, argument))
        return arguments;

      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing plugin path after " + argument;
        return arguments;
      }

      slotFor(arguments, slotIndex).pluginBundle =
          juce::File(value).getFullPathName();
    } else if (argument == sandbox::cli::pageFakeToneFlag) {
      int slotIndex = -1;

      if (!parseIntegerArgument(commandLine, i, 0,
                                sandbox::maximumPageSlots - 1, slotIndex,
                                arguments.error, argument))
        return arguments;

      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing frequency after " + argument;
        return arguments;
      }

      const auto frequency = value.getDoubleValue();

      if (frequency <= 0.0) {
        arguments.error = "invalid fake tone frequency: " + value;
        return arguments;
      }

      slotFor(arguments, slotIndex).fakeToneFrequency = frequency;
    } else if (argument == sandbox::cli::pageParamFlag) {
      int slotIndex = -1;

      if (!parseIntegerArgument(commandLine, i, 0,
                                sandbox::maximumPageSlots - 1, slotIndex,
                                arguments.error, argument))
        return arguments;

      juce::String value;

      if (!takeString(commandLine, i, value)) {
        arguments.error = "missing parameter after " + argument;
        return arguments;
      }

      ParameterAssignment assignment;

      if (!parseParameter(value, assignment, arguments.error)) return arguments;

      slotFor(arguments, slotIndex).parameters.push_back(assignment);
    } else if (argument == sandbox::cli::pageAbortAfterFlag) {
      int slotIndex = -1;

      if (!parseIntegerArgument(commandLine, i, 0,
                                sandbox::maximumPageSlots - 1, slotIndex,
                                arguments.error, argument))
        return arguments;

      int blocks = 0;

      if (!parseIntegerArgument(commandLine, i, 0, 1000000, blocks,
                                arguments.error, argument))
        return arguments;

      slotFor(arguments, slotIndex).abortAfterBlocks = blocks;
    } else {
      arguments.error = "unknown argument: " + argument;
      return arguments;
    }
  }

  arguments.pageMode = arguments.pageMapFile != juce::File();

  if (arguments.pageMode) {
    if (arguments.mapFile != juce::File())
      arguments.error =
          "page mode cannot also use " + juce::String(sandbox::cli::mapFlag);
    else if (arguments.pageSlots.empty())
      arguments.error = "no page slots given";
    else if (arguments.pageId.isEmpty())
      arguments.error =
          "no " + juce::String(sandbox::cli::pageIdFlag) + " given";
  } else if (arguments.mapFile == juce::File()) {
    arguments.error = "no " + juce::String(sandbox::cli::mapFlag) + " or " +
                      juce::String(sandbox::cli::pageMapFlag) + " path given";
  } else if (arguments.pluginBundle == juce::File() &&
             arguments.fakeToneFrequency <= 0.0) {
    arguments.error =
        "no " + juce::String(sandbox::cli::pluginFlag) + " bundle and no " +
        juce::String(sandbox::cli::fakeToneFlag) + " frequency given";
  }

  arguments.valid = arguments.error.isEmpty();
  return arguments;
}

}  // namespace ihub::plugins::sandbox_client
