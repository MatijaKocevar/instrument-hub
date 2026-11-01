#include "JsonSupport.h"

namespace ApiTest
{

juce::var parseJson (const std::string& text)
{
    return juce::JSON::parse (juce::String (text));
}

juce::var property (const juce::var& object, const char* name)
{
    if (! object.isObject() || object.getDynamicObject() == nullptr)
        return {};

    return object.getDynamicObject()->getProperty (name);
}

juce::var property (const juce::var& object, const juce::String& name)
{
    return property (object, name.toRawUTF8());
}

const juce::Array<juce::var>* arrayProperty (const juce::var& object, const char* name)
{
    return property (object, name).getArray();
}

double numberProperty (const juce::var& object, const char* name)
{
    return static_cast<double> (property (object, name));
}

int intProperty (const juce::var& object, const char* name)
{
    return static_cast<int> (property (object, name));
}

bool boolProperty (const juce::var& object, const char* name)
{
    return static_cast<bool> (property (object, name));
}

std::string stringProperty (const juce::var& object, const char* name)
{
    return property (object, name).toString().toStdString();
}

juce::var patchForSlot (const juce::var& state, int slot)
{
    const auto* families = arrayProperty (state, "families");

    if (families == nullptr)
        return {};

    for (const auto& family : *families)
    {
        const auto* patches = arrayProperty (family, "patches");

        if (patches == nullptr)
            continue;

        for (const auto& patch : *patches)
        {
            if (intProperty (patch, "slot") == slot)
                return patch;
        }
    }

    return {};
}

}
