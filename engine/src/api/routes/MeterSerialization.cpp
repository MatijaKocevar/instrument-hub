#include "api/routes/RouteHelpers.h"

#include <juce_core/juce_core.h>

namespace Api
{

juce::var meterFrameToVar (const MeterFrame& frame)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("type", "meters");
    object->setProperty ("sequence", static_cast<juce::int64> (frame.sequence));
    object->setProperty ("timestampMs", frame.timestampMs);

    auto master = Routes::makeObject();
    master.getDynamicObject()->setProperty ("peak", static_cast<double> (frame.masterPeak));
    object->setProperty ("master", master);

    juce::Array<juce::var> slots;

    for (const auto& slot : frame.slots)
    {
        auto slotVar = Routes::makeObject();
        slotVar.getDynamicObject()->setProperty ("slot", slot.slot);
        slotVar.getDynamicObject()->setProperty ("peak", static_cast<double> (slot.peak));
        slotVar.getDynamicObject()->setProperty ("gain", static_cast<double> (slot.gain));
        slotVar.getDynamicObject()->setProperty ("armed", slot.armed);
        slots.add (slotVar);
    }

    object->setProperty ("slots", juce::var (slots));

    return root;
}

} // namespace Api
