#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

juce::var pairingToVar (const PairingSnapshot& pairing)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("lanEnabled", pairing.lanEnabled);
    object->setProperty ("address", juce::String (pairing.address));
    object->setProperty ("port", pairing.port);
    object->setProperty ("code", juce::String (pairing.code));
    object->setProperty ("codeExpiresInMs", pairing.codeExpiresInMs);
    object->setProperty ("payload", juce::String (pairing.payload));
    object->setProperty ("pairedClients", pairing.pairedClients);

    return root;
}

} // namespace Api
