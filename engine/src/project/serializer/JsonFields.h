#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace ihub::project::serializer {

juce::var makeObject();

juce::String fieldName(const juce::String& context, const char* key);

bool readString(const juce::var& parent, const char* key,
                const juce::String& context, std::string& destination,
                juce::String& error, bool required = true);

bool readNumber(const juce::var& parent, const char* key,
                const juce::String& context, double& destination,
                juce::String& error, bool required = true);

bool readInteger(const juce::var& parent, const char* key,
                 const juce::String& context, int& destination, int minimum,
                 int maximum, juce::String& error, bool required = true);

bool readInteger64(const juce::var& parent, const char* key,
                   const juce::String& context, juce::int64& destination,
                   juce::String& error, bool required = true);

bool readBool(const juce::var& parent, const char* key,
              const juce::String& context, bool& destination,
              juce::String& error, bool required = true);

bool readObject(const juce::var& parent, const char* key,
                const juce::String& context, juce::var& destination,
                juce::String& error, bool required = true);

bool readArray(const juce::var& parent, const char* key,
               const juce::String& context,
               const juce::Array<juce::var>*& destination, juce::String& error,
               bool required = true);

}  // namespace ihub::project::serializer
