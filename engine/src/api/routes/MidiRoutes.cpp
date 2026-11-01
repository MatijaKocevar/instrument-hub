#include "api/routes/RouteHelpers.h"

namespace Api::Routes
{

void registerMidiRoutes (httplib::Server& server, const RouteContext& context)
{

    const auto readMidiState = [context]
    {
        if (context.midiProvider == nullptr)
            return MidiStateSnapshot {};

        try
        {
            return context.midiProvider();
        }
        catch (...)
        {
            MidiStateSnapshot failed;
            failed.error = "the MIDI device list failed";
            return failed;
        }
    };

    server.Get ("/midi", [context, readMidiState] (const httplib::Request&,
                                                   httplib::Response& response)
    {
        decorate (context, response);
        setJson (response, 200, midiStateToVar (readMidiState()));
    });

    server.Post ("/midi/inputs", [context, readMidiState] (const httplib::Request& request,
                                                          httplib::Response& response)
    {
        if (context.midiEnableInput == nullptr)
        {
            setError (response, 503, "MIDI inputs are not available");
            return;
        }

        juce::var body;
        std::string error;

        if (! parseJsonObject (request, body, error))
        {
            setError (response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();

        if (! object->hasProperty ("id"))
        {
            setError (response, 400, "body must contain \"id\" (a string)");
            return;
        }

        if (! object->hasProperty ("enabled"))
        {
            setError (response, 400, "body must contain \"enabled\" (boolean)");
            return;
        }

        std::string id;

        if (! readString (object->getProperty ("id"), "id", id, error))
        {
            setError (response, 400, error);
            return;
        }

        bool enabled = false;

        if (! detail::readBool (object->getProperty ("enabled"), "enabled", enabled, error))
        {
            setError (response, 400, error);
            return;
        }

        if (! context.midiEnableInput (id, enabled, error))
        {
            const int status = error.find ("unknown MIDI input") != std::string::npos
                                   ? 404
                                   : 400;
            setError (response, status, error);
            return;
        }

        decorate (context, response);
        setJson (response, 200, midiStateToVar (readMidiState()));
    });

    server.Post ("/midi/pad-test", [context, readMidiState] (const httplib::Request&,
                                                             httplib::Response& response)
    {
        if (context.midiPadTest == nullptr)
        {
            setError (response, 503, "the Launchpad pad test is not available");
            return;
        }

        std::string error;

        if (! context.midiPadTest (error))
        {
            setError (response, 409, error);
            return;
        }

        decorate (context, response);
        setJson (response, 200, midiStateToVar (readMidiState()));
    });
}

}
