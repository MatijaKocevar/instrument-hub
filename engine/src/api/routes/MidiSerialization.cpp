#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api {

juce::var midiStateToVar(const MidiStateSnapshot& midi) {
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    juce::Array<juce::var> inputs;

    for (const auto& input : midi.inputs) {
        auto item = Routes::makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty("id", juce::String(input.id));
        itemObject->setProperty("name", juce::String(input.name));
        itemObject->setProperty("role", juce::String(input.role));
        itemObject->setProperty("port", juce::String(input.port));
        itemObject->setProperty("enabled", input.enabled);
        itemObject->setProperty("open", input.open);
        itemObject->setProperty("handshake", input.handshake);
        itemObject->setProperty("leds", input.leds);
        itemObject->setProperty("noteBase", input.noteBase);
        itemObject->setProperty("learnedNotes", input.learnedNotes);
        itemObject->setProperty("learnActive", input.learnActive);
        itemObject->setProperty("events", static_cast<juce::int64>(input.events));
        itemObject->setProperty("lastEvent", juce::String(input.lastEvent));
        itemObject->setProperty("error", juce::String(input.error));
        inputs.add(item);
    }

    object->setProperty("inputs", juce::var(inputs));
    object->setProperty("inputCount", static_cast<int>(midi.inputs.size()));

    object->setProperty("error", juce::String(midi.error));

    return root;
}

}  // namespace Api
