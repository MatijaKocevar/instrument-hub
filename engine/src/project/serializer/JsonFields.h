#pragma once

#include <juce_core/juce_core.h>

#include <string>

namespace ihub::project::serializer
{

/** Creates an empty JSON object. */
juce::var makeObject();

/** Builds a dotted field path for an error message. */
juce::String fieldName (const juce::String& context, const char* key);

/** Typed readers shared by the `set.json` document parsers.

    Each reader validates the field's JSON type and range, leaves `destination`
    untouched when the field is optional and absent, and fills `error` with a
    path-qualified message on failure.
*/
bool readString (const juce::var& parent,
                 const char* key,
                 const juce::String& context,
                 std::string& destination,
                 juce::String& error,
                 bool required = true);

bool readNumber (const juce::var& parent,
                 const char* key,
                 const juce::String& context,
                 double& destination,
                 juce::String& error,
                 bool required = true);

bool readInteger (const juce::var& parent,
                  const char* key,
                  const juce::String& context,
                  int& destination,
                  int minimum,
                  int maximum,
                  juce::String& error,
                  bool required = true);

bool readInteger64 (const juce::var& parent,
                    const char* key,
                    const juce::String& context,
                    juce::int64& destination,
                    juce::String& error,
                    bool required = true);

bool readBool (const juce::var& parent,
               const char* key,
               const juce::String& context,
               bool& destination,
               juce::String& error,
               bool required = true);

bool readObject (const juce::var& parent,
                 const char* key,
                 const juce::String& context,
                 juce::var& destination,
                 juce::String& error,
                 bool required = true);

bool readArray (const juce::var& parent,
                const char* key,
                const juce::String& context,
                const juce::Array<juce::var>*& destination,
                juce::String& error,
                bool required = true);

} // namespace ihub::project::serializer
