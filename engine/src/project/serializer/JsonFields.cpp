#include "project/serializer/JsonFields.h"

#include <cmath>
#include <limits>

namespace ihub::project::serializer {

using juce::DynamicObject;
using juce::var;

var makeObject() { return var(new DynamicObject()); }

juce::String fieldName(const juce::String& context, const char* key) {
    const auto name = juce::String(key);
    return context.isEmpty() ? name : context + "." + name;
}

bool readString(const var& parent, const char* key, const juce::String& context,
                std::string& destination, juce::String& error, bool required) {
    auto* object = parent.getDynamicObject();

    if (object == nullptr) {
        error = context + ": expected an object";
        return false;
    }

    if (!object->hasProperty(key)) {
        if (!required) return true;

        error = context + ": missing \"" + juce::String(key) + "\"";
        return false;
    }

    const auto value = object->getProperty(key);

    if (!value.isString()) {
        error = fieldName(context, key) + ": expected a string";
        return false;
    }

    destination = value.toString().toStdString();
    return true;
}

bool readNumber(const var& parent, const char* key, const juce::String& context,
                double& destination, juce::String& error, bool required) {
    auto* object = parent.getDynamicObject();

    if (object == nullptr) {
        error = context + ": expected an object";
        return false;
    }

    if (!object->hasProperty(key)) {
        if (!required) return true;

        error = context + ": missing \"" + juce::String(key) + "\"";
        return false;
    }

    const auto value = object->getProperty(key);

    if (!(value.isInt() || value.isInt64() || value.isDouble())) {
        error = fieldName(context, key) + ": expected a number";
        return false;
    }

    const auto number = static_cast<double>(value);

    if (!std::isfinite(number)) {
        error = fieldName(context, key) + ": must be finite";
        return false;
    }

    destination = number;
    return true;
}

bool readInteger(const var& parent, const char* key, const juce::String& context, int& destination,
                 int minimum, int maximum, juce::String& error, bool required) {
    double number = static_cast<double>(destination);

    if (!readNumber(parent, key, context, number, error, required)) return false;

    if (!parent.getDynamicObject()->hasProperty(key)) return true;

    if (number < static_cast<double>(minimum) || number > static_cast<double>(maximum) ||
        std::abs(number - std::floor(number)) > 1.0e-9) {
        error = fieldName(context, key) + ": expected an integer between " + juce::String(minimum) +
                " and " + juce::String(maximum);
        return false;
    }

    destination = static_cast<int>(number);
    return true;
}

bool readInteger64(const var& parent, const char* key, const juce::String& context,
                   juce::int64& destination, juce::String& error, bool required) {
    auto* object = parent.getDynamicObject();

    if (object == nullptr) {
        error = context + ": expected an object";
        return false;
    }

    if (!object->hasProperty(key)) {
        if (!required) return true;

        error = context + ": missing \"" + juce::String(key) + "\"";
        return false;
    }

    const auto value = object->getProperty(key);

    if (!(value.isInt() || value.isInt64() || value.isDouble())) {
        error = fieldName(context, key) + ": expected an integer";
        return false;
    }

    const auto number = static_cast<double>(value);

    if (!std::isfinite(number) || std::abs(number - std::floor(number)) > 1.0e-9 ||
        number < static_cast<double>(std::numeric_limits<juce::int64>::min()) ||
        number > static_cast<double>(std::numeric_limits<juce::int64>::max())) {
        error = fieldName(context, key) + ": expected an integer";
        return false;
    }

    destination = static_cast<juce::int64>(number);
    return true;
}

bool readBool(const var& parent, const char* key, const juce::String& context, bool& destination,
              juce::String& error, bool required) {
    auto* object = parent.getDynamicObject();

    if (object == nullptr) {
        error = context + ": expected an object";
        return false;
    }

    if (!object->hasProperty(key)) {
        if (!required) return true;

        error = context + ": missing \"" + juce::String(key) + "\"";
        return false;
    }

    const auto value = object->getProperty(key);

    if (!value.isBool()) {
        error = fieldName(context, key) + ": expected a boolean";
        return false;
    }

    destination = static_cast<bool>(value);
    return true;
}

bool readObject(const var& parent, const char* key, const juce::String& context, var& destination,
                juce::String& error, bool required) {
    auto* object = parent.getDynamicObject();

    if (object == nullptr) {
        error = context + ": expected an object";
        return false;
    }

    if (!object->hasProperty(key)) {
        if (!required) return true;

        error = context + ": missing \"" + juce::String(key) + "\"";
        return false;
    }

    destination = object->getProperty(key);

    if (!destination.isObject()) {
        error = fieldName(context, key) + ": expected an object";
        return false;
    }

    return true;
}

bool readArray(const var& parent, const char* key, const juce::String& context,
               const juce::Array<var>*& destination, juce::String& error, bool required) {
    auto* object = parent.getDynamicObject();

    if (object == nullptr) {
        error = context + ": expected an object";
        return false;
    }

    if (!object->hasProperty(key)) {
        if (!required) return true;

        error = context + ": missing \"" + juce::String(key) + "\"";
        return false;
    }

    const auto value = object->getProperty(key);

    if (!value.isArray()) {
        error = fieldName(context, key) + ": expected an array";
        return false;
    }

    destination = value.getArray();
    return destination != nullptr;
}

}  // namespace ihub::project::serializer
