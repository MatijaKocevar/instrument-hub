#include "api/routes/RouteHelpers.h"

#include <set>

namespace Api::Routes
{

namespace
{
bool readChannelList (const juce::var& value,
                      const char* name,
                      std::vector<int>& destination,
                      std::string& error)
{
    const auto* array = value.getArray();

    if (array == nullptr)
    {
        error = std::string (name) + " must be an array of channel indices";
        return false;
    }

    std::vector<int> channels;
    std::set<int> seen;

    for (const auto& item : *array)
    {
        std::int64_t parsed = 0;

        if (! detail::readInteger (item, name, 0, 63, parsed, error))
            return false;

        if (! seen.insert (static_cast<int> (parsed)).second)
        {
            error = std::string (name) + " contains duplicate channel "
                    + juce::String (parsed).toStdString();
            return false;
        }

        channels.push_back (static_cast<int> (parsed));
    }

    destination = std::move (channels);
    return true;
}
}

void registerAudioRoutes (httplib::Server& server, const RouteContext& context)
{

    server.Get ("/audio", [context] (const httplib::Request&, httplib::Response& response)
    {
        decorate (context, response);

        const auto audio = context.audioProvider != nullptr
                               ? context.audioProvider()
                               : AudioStateSnapshot {};

        setJson (response, 200, audioStateToVar (audio));
    });

    server.Post ("/audio", [context] (const httplib::Request& request,
                                     httplib::Response& response)
    {
        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        AudioSettingsPatch patch;
        const auto& properties = body.getDynamicObject()->getProperties();

        if (properties.isEmpty())
        {
            setError (response, 400, "body must contain at least one audio setting");
            return;
        }

        for (int index = 0; index < properties.size(); ++index)
        {
            const auto& name = properties.getName (index);
            const auto& value = properties.getValueAt (index);
            const auto key = name.toString();

            if (key == "deviceType")
            {
                std::string parsed;

                if (! readString (value, "deviceType", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.deviceType = std::move (parsed);
            }
            else if (key == "deviceName")
            {
                std::string parsed;

                if (! readString (value, "deviceName", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.deviceName = std::move (parsed);
            }
            else if (key == "sampleRate")
            {
                double parsed = 0.0;

                if (! detail::readNumber (value, "sampleRate", 8000.0, 384000.0, parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.sampleRate = parsed;
            }
            else if (key == "bufferSize")
            {
                std::int64_t parsed = 0;

                if (! detail::readInteger (value, "bufferSize", 16, 8192, parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.bufferSize = static_cast<int> (parsed);
            }
            else if (key == "inputChannels")
            {
                std::vector<int> parsed;

                if (! readChannelList (value, "inputChannels", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.inputChannels = std::move (parsed);
            }
            else if (key == "outputChannels")
            {
                std::vector<int> parsed;

                if (! readChannelList (value, "outputChannels", parsed, error))
                {
                    setError (response, 400, error);
                    return;
                }

                patch.outputChannels = std::move (parsed);
            }
            else
            {
                setError (response, 400, "unknown audio setting: " + key.toStdString());
                return;
            }
        }

        if (context.audioApply == nullptr)
        {
            setError (response, 503, "no audio backend is attached");
            return;
        }

        if (! context.audioApply (patch, error))
        {
            setError (response, 400, error);
            return;
        }

        context.state.notifyChanged ("device", -1);

        decorate (context, response);

        const auto audio = context.audioProvider != nullptr
                               ? context.audioProvider()
                               : AudioStateSnapshot {};

        setJson (response, 200, audioStateToVar (audio));
    });

    server.Post ("/audio/measure", [context] (const httplib::Request&,
                                             httplib::Response& response)
    {
        if (context.audioMeasure == nullptr)
        {
            setError (response, 503, "no audio backend is attached");
            return;
        }

        decorate (context, response);
        setJson (response, 200, audioMeasurementToVar (context.audioMeasure()));
    });
}

}
