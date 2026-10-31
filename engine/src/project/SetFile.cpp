#include "SetFile.h"

#include "../audio/AudioEngine.h"

#include <cmath>
#include <set>

namespace ihub::project
{

std::string pluginStateEntryPath (std::string_view uuid)
{
    return std::string (pluginStateDirectory) + "/" + std::string (uuid) + ".bin";
}

bool isValidPluginStateId (std::string_view id) noexcept
{
    if (id.empty() || id.size() > 128)
        return false;

    for (const auto character : id)
    {
        const auto isAlpha = (character >= 'a' && character <= 'z')
                             || (character >= 'A' && character <= 'Z');
        const auto isDigit = character >= '0' && character <= '9';

        if (! isAlpha && ! isDigit && character != '-' && character != '_')
            return false;
    }

    return true;
}

std::string makePluginStateId()
{
    return juce::Uuid().toString().toStdString();
}

bool isValidSlotColour (std::string_view colour) noexcept
{
    if (colour.size() != 7 || colour.front() != '#')
        return false;

    for (std::size_t i = 1; i < colour.size(); ++i)
    {
        const auto character = colour[i];
        const auto isDigit = character >= '0' && character <= '9';
        const auto isLower = character >= 'a' && character <= 'f';
        const auto isUpper = character >= 'A' && character <= 'F';

        if (! isDigit && ! isLower && ! isUpper)
            return false;
    }

    return true;
}

const char* toString (SourceKind kind) noexcept
{
    switch (kind)
    {
        case SourceKind::empty:      return "empty";
        case SourceKind::plugin:     return "plugin";
        case SourceKind::audioInput: return "audio-input";
        case SourceKind::sampler:    return "sampler";
    }

    return "empty";
}

bool sourceKindFromString (std::string_view text, SourceKind& kind) noexcept
{
    if (text == "empty")       { kind = SourceKind::empty;      return true; }
    if (text == "plugin")      { kind = SourceKind::plugin;     return true; }
    if (text == "audio-input") { kind = SourceKind::audioInput; return true; }
    if (text == "sampler")     { kind = SourceKind::sampler;    return true; }

    return false;
}

const char* toString (FxKind kind) noexcept
{
    switch (kind)
    {
        case FxKind::builtin: return "builtin";
        case FxKind::plugin:  return "plugin";
    }

    return "builtin";
}

bool fxKindFromString (std::string_view text, FxKind& kind) noexcept
{
    if (text == "builtin") { kind = FxKind::builtin; return true; }
    if (text == "plugin")  { kind = FxKind::plugin;  return true; }

    return false;
}

const PluginStateChunk* SetFile::findPluginState (std::string_view uuid) const noexcept
{
    for (const auto& chunk : pluginStates)
        if (chunk.uuid == uuid)
            return &chunk;

    return nullptr;
}

PluginStateChunk* SetFile::findPluginState (std::string_view uuid) noexcept
{
    for (auto& chunk : pluginStates)
        if (chunk.uuid == uuid)
            return &chunk;

    return nullptr;
}

bool SetFile::validate (juce::String& error) const
{
    const auto fail = [&error] (const juce::String& message)
    {
        error = message;
        return false;
    };

    if (schemaVersion != currentVersion)
        return fail ("unsupported set schema version " + juce::String (schemaVersion)
                     + " (expected " + juce::String (currentVersion) + ")");

    if (name.empty())
        return fail ("set name must not be empty");

    if (notes.size() > maximumSetNotesBytes)
        return fail ("set notes are longer than "
                     + juce::String (static_cast<juce::int64> (maximumSetNotesBytes))
                     + " bytes");

    if (slots.size() > 64)
        return fail ("a set can have at most 64 slots");

    if (! std::isfinite (transport.bpm) || transport.bpm < 20.0 || transport.bpm > 300.0)
        return fail ("transport.bpm must be between 20 and 300");

    if (! std::isfinite (transport.positionPpq) || transport.positionPpq < 0.0)
        return fail ("transport.positionPpq must be finite and non-negative");

    std::set<int> usedIndices;
    std::set<std::string> usedStateIds;

    for (const auto& chunk : pluginStates)
    {
        if (! isValidPluginStateId (chunk.uuid))
            return fail ("plugin state id is not a safe entry name: \""
                         + juce::String (chunk.uuid) + "\"");

        if (! usedStateIds.insert (chunk.uuid).second)
            return fail ("duplicate plugin state id: " + juce::String (chunk.uuid));

        if (chunk.format.empty())
            return fail ("plugin state " + juce::String (chunk.uuid) + " has no format");
    }

    const auto checkStateReference = [this, &fail] (const std::string& uuid,
                                                    const juce::String& context)
    {
        if (uuid.empty())
            return fail (context + " has no plugin state id");

        if (findPluginState (uuid) == nullptr)
            return fail (context + " references missing plugin state "
                         + juce::String (uuid));

        return true;
    };

    for (const auto& slot : slots)
    {
        if (slot.index < 0 || slot.index >= 64)
            return fail ("slot index out of range: " + juce::String (slot.index));

        if (! usedIndices.insert (slot.index).second)
            return fail ("duplicate slot index: " + juce::String (slot.index));

        const auto slotName = slot.name.empty()
            ? juce::String ("slot ") + juce::String (slot.index)
            : juce::String (slot.name);

        if (! std::isfinite (slot.gain) || slot.gain < 0.0f || slot.gain > 4.0f)
            return fail (slotName + ": gain must be between 0 and 4");

        if (! isValidSlotColour (slot.colour))
            return fail (slotName + ": colour must be a #rrggbb hex string");

        if (! std::isfinite (slot.instrument.gain)
            || slot.instrument.gain < 0.0f || slot.instrument.gain > 4.0f)
            return fail (slotName + ": instrument gain must be between 0 and 4");

        if (slot.instrument.kind == SourceKind::plugin)
        {
            if (! checkStateReference (slot.instrument.pluginState,
                                       slotName + ": instrument"))
                return false;
        }

        if (slot.instrument.kind == SourceKind::audioInput && slot.instrument.inputChannel < 0)
            return fail (slotName + ": input channel must be non-negative");

        for (std::size_t i = 0; i < slot.fxChain.size(); ++i)
        {
            const auto& fx = slot.fxChain[i];
            const auto context = slotName + ": fx[" + juce::String (static_cast<int> (i)) + "]";

            if (fx.kind == FxKind::plugin
                && ! checkStateReference (fx.pluginState, context))
                return false;

            if (fx.id.empty())
                return fail (context + " has no id");

            for (const auto& [key, value] : fx.parameters)
            {
                if (key.empty())
                    return fail (context + " has an empty parameter name");

                if (! std::isfinite (value))
                    return fail (context + ": parameter \"" + juce::String (key)
                                 + "\" is not finite");
            }
        }
    }

    error.clear();
    return true;
}

bool SetFile::applyTo (AudioEngine::AudioEngine& engine, juce::String& error) const
{
    if (! validate (error))
        return false;

    if (engine.getNumSlots() != static_cast<int> (slots.size()))
    {
        error = "the engine has " + juce::String (engine.getNumSlots())
                + " slots but the set has " + juce::String (static_cast<int> (slots.size()));
        return false;
    }

    for (const auto& slot : slots)
    {
        if (slot.index < 0 || slot.index >= engine.getNumSlots())
        {
            error = "slot index " + juce::String (slot.index)
                    + " does not exist in the engine";
            return false;
        }

        auto& live = engine.getSlot (slot.index);
        live.setGain (slot.muted ? 0.0f : slot.gain);

        if (slot.active)
            live.activate();
        else
            live.deactivate();
    }

    error.clear();
    return true;
}

} // namespace ihub::project
