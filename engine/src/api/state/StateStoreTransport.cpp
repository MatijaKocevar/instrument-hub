#include "api/StateStore.h"

#include "api/Routes.h"

#include <limits>
#include <utility>

namespace Api
{

TransportState StateStore::getTransport() const
{
    TransportProvider provider;
    TransportState current;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        current = transport;
        provider = transportProvider;
    }

    if (provider != nullptr)
    {
        try
        {
            return provider();
        }
        catch (...)
        {
        }
    }

    return current;
}

bool StateStore::updateTransport (const juce::var& patch, std::string& error)
{
    if (! patch.isObject())
    {
        error = "request body must be a JSON object";
        return false;
    }

    TransportHandler handler;
    TransportState next;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        next = transport;
        const auto& properties = patch.getDynamicObject()->getProperties();

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);

            if (name.toString() == "playing")
            {
                if (! detail::readBool (value, "playing", next.playing, error))
                    return false;
            }
            else if (name.toString() == "bpm")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "bpm", 20.0, 300.0, parsed, error))
                    return false;

                next.bpm = parsed;
            }
            else if (name.toString() == "ppq")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "ppq", 0.0, 1.0e9, parsed, error))
                    return false;

                next.ppq = parsed;
            }
            else if (name.toString() == "positionSamples")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "positionSamples",
                                   0, std::numeric_limits<std::int64_t>::max(),
                                   parsed, error))
                {
                    return false;
                }

                next.positionSamples = parsed;
            }
            else
            {
                error = "unknown transport field: " + name.toString().toStdString();
                return false;
            }
        }

        transport = next;
        handler = transportHandler;
    }

    if (handler != nullptr)
        handler (next, patch);

    notifyChanged ("transport", -1);

    return true;
}

void StateStore::setTransportHandler (TransportHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    transportHandler = std::move (handler);
}

void StateStore::setTransportProvider (TransportProvider provider)
{
    const std::lock_guard<std::mutex> lock (mutex);
    transportProvider = std::move (provider);
}

}
