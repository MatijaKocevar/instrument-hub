#include "KitJson.h"

#include <cmath>
#include <limits>

namespace ihub::sampler::kit_detail
{

const juce::var* findProperty (const juce::DynamicObject& object, const char* name) noexcept
{
    return object.getProperties().getVarPointer (name);
}

bool hasOnlyProperties (const juce::DynamicObject& object,
                        std::initializer_list<const char*> allowed,
                        const juce::String& context,
                        juce::String& error)
{
    for (const auto& property : object.getProperties())
    {
        bool known = false;

        for (const auto* name : allowed)
        {
            if (property.name.toString() == name)
            {
                known = true;
                break;
            }
        }

        if (! known)
        {
            error = context + ": unknown property \"" + property.name.toString() + "\"";
            return false;
        }
    }

    return true;
}

bool getRequired (const juce::DynamicObject& object,
                  const char* name,
                  const juce::String& context,
                  const juce::var*& value,
                  juce::String& error)
{
    value = findProperty (object, name);

    if (value == nullptr)
    {
        error = context + ": missing \"" + name + "\"";
        return false;
    }

    return true;
}

bool readString (const juce::var& value,
                 const juce::String& context,
                 bool allowEmpty,
                 juce::String& destination,
                 juce::String& error)
{
    if (! value.isString())
    {
        error = context + ": expected a string";
        return false;
    }

    const auto text = value.toString();

    if (! allowEmpty && text.isEmpty())
    {
        error = context + ": must not be empty";
        return false;
    }

    destination = text;
    return true;
}

bool readNumber (const juce::var& value,
                 const juce::String& context,
                 double& destination,
                 juce::String& error)
{
    if (! (value.isInt() || value.isInt64() || value.isDouble()))
    {
        error = context + ": expected a number";
        return false;
    }

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number))
    {
        error = context + ": must be finite";
        return false;
    }

    destination = number;
    return true;
}

bool readInteger (const juce::var& value,
                  const juce::String& context,
                  int& destination,
                  juce::String& error)
{
    double number = 0.0;

    if (! readNumber (value, context, number, error))
        return false;

    if (number < (double) std::numeric_limits<int>::min()
        || number > (double) std::numeric_limits<int>::max()
        || std::abs (std::floor (number) - number) > 1.0e-9)
    {
        error = context + ": expected an integer";
        return false;
    }

    destination = (int) number;
    return true;
}

bool readOptionalNumber (const juce::DynamicObject& object,
                         const char* name,
                         const juce::String& context,
                         double& destination,
                         juce::String& error)
{
    const auto* value = findProperty (object, name);

    if (value == nullptr)
        return true;

    return readNumber (*value, context + "." + name, destination, error);
}

bool readOptionalString (const juce::DynamicObject& object,
                         const char* name,
                         const juce::String& context,
                         bool allowEmpty,
                         std::string& destination,
                         juce::String& error)
{
    const auto* value = findProperty (object, name);

    if (value == nullptr)
        return true;

    juce::String text;

    if (! readString (*value, context + "." + name, allowEmpty, text, error))
        return false;

    destination = text.toStdString();
    return true;
}

} // namespace ihub::sampler::kit_detail
