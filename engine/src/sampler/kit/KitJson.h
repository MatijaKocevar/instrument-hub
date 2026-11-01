#pragma once

#include <juce_core/juce_core.h>

#include <initializer_list>
#include <string>

namespace ihub::sampler::kit_detail
{

const juce::var* findProperty (const juce::DynamicObject& object, const char* name) noexcept;

bool hasOnlyProperties (const juce::DynamicObject& object,
                        std::initializer_list<const char*> allowed,
                        const juce::String& context,
                        juce::String& error);

bool getRequired (const juce::DynamicObject& object,
                  const char* name,
                  const juce::String& context,
                  const juce::var*& value,
                  juce::String& error);

bool readString (const juce::var& value,
                 const juce::String& context,
                 bool allowEmpty,
                 juce::String& destination,
                 juce::String& error);

bool readNumber (const juce::var& value,
                 const juce::String& context,
                 double& destination,
                 juce::String& error);

bool readInteger (const juce::var& value,
                  const juce::String& context,
                  int& destination,
                  juce::String& error);

bool readOptionalNumber (const juce::DynamicObject& object,
                         const char* name,
                         const juce::String& context,
                         double& destination,
                         juce::String& error);

bool readOptionalString (const juce::DynamicObject& object,
                         const char* name,
                         const juce::String& context,
                         bool allowEmpty,
                         std::string& destination,
                         juce::String& error);

}
