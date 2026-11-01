#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

juce::var midiStateToVar (const MidiStateSnapshot& midi)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    juce::Array<juce::var> inputs;

    for (const auto& input : midi.inputs)
    {
        auto item = Routes::makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty ("id", juce::String (input.id));
        itemObject->setProperty ("name", juce::String (input.name));
        itemObject->setProperty ("role", juce::String (input.role));
        itemObject->setProperty ("port", juce::String (input.port));
        itemObject->setProperty ("enabled", input.enabled);
        itemObject->setProperty ("open", input.open);
        itemObject->setProperty ("events", static_cast<juce::int64> (input.events));
        itemObject->setProperty ("lastEvent", juce::String (input.lastEvent));
        inputs.add (item);
    }

    object->setProperty ("inputs", juce::var (inputs));
    object->setProperty ("inputCount", static_cast<int> (midi.inputs.size()));

    const auto portToVar = [] (const MidiPortState& port)
    {
        auto item = Routes::makeObject();
        auto* itemObject = item.getDynamicObject();

        itemObject->setProperty ("name", juce::String (port.name));
        itemObject->setProperty ("port", juce::String (port.port));
        itemObject->setProperty ("connected", port.connected);
        itemObject->setProperty ("handshake", port.handshake);
        itemObject->setProperty ("inputName", juce::String (port.inputName));
        itemObject->setProperty ("outputName", juce::String (port.outputName));
        itemObject->setProperty ("error", juce::String (port.error));
        return item;
    };

    object->setProperty ("launchpad", portToVar (midi.launchpad));
    object->setProperty ("keystation", portToVar (midi.keystation));

    auto padTest = Routes::makeObject();
    padTest.getDynamicObject()->setProperty ("active", midi.padTest.active);
    padTest.getDynamicObject()->setProperty ("lit", midi.padTest.lit);
    padTest.getDynamicObject()->setProperty ("durationMs", midi.padTest.durationMs);
    object->setProperty ("padTest", padTest);

    object->setProperty ("error", juce::String (midi.error));

    return root;
}

} // namespace Api
