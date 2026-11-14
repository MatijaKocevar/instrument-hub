#include "controller/Mapping.h"

#include "controller/launchpad/TrackPadRouting.h"

#include <algorithm>

namespace Controller
{

    bool Mapping::handlePad (int pad, bool pressed)
    {
        if (pad < 0 || pad >= SetMapping::maximumPads)
            return false;

        Listeners listenersCopy;
        std::optional<PadBinding> binding;
        int armedSlot = -1;
        bool arms = false;

        {
            const std::lock_guard<std::mutex> lock (mutex);
            listenersCopy = listeners;

            if (current != nullptr)
                if (const auto* currentBinding = current->padBinding (pad))
                    binding = *currentBinding;

            arms = binding.has_value () && padRoleArms (binding->role);

            if (pressed && arms)
            {
                activePad.store (pad, std::memory_order_release);
                armedSlot = binding->slot;
            }
        }

        if (pressed && arms && listenersCopy.armSlot)
            listenersCopy.armSlot (armedSlot);

        if (listenersCopy.padEvent)
            listenersCopy.padEvent (pad, binding.has_value () ? &*binding : nullptr, pressed);

        return pressed && binding.has_value ();
    }

    bool Mapping::handleEncoder (int encoder, int value)
    {
        if (encoder < 0 || encoder >= SetMapping::maximumEncoders)
            return false;

        Listeners listenersCopy;
        MacroChange change;
        bool mapped = false;

        {
            const std::lock_guard<std::mutex> lock (mutex);
            listenersCopy = listeners;

            if (current != nullptr)
            {
                if (const auto* binding = current->encoderBinding (encoder))
                {
                    if (const auto* macro = current->macroFor (*binding))
                    {
                        change.macro = *macro;
                        mapped = true;

                        const int pad = activePad.load (std::memory_order_relaxed);

                        if (pad >= 0)
                            if (const auto* padBinding = current->padBinding (pad))
                                change.slot = padBinding->slot;
                    }
                }
            }
        }

        if (!mapped)
            return false;

        change.rawValue = std::clamp (value, 0, 127);
        change.normalized = static_cast<float> (change.rawValue) / 127.0f;
        change.value = change.macro.minimum +
                       change.normalized * (change.macro.maximum - change.macro.minimum);

        if (listenersCopy.macroEvent)
            listenersCopy.macroEvent (change);

        return true;
    }

}
