#include "api/routes/RouteHelpers.h"

namespace Api::Routes {

namespace {
constexpr int highestNoteBase = 127 - MidiInputState::gridPadCount + 1;
}

void registerMidiRoutes(httplib::Server& server, const RouteContext& context) {
    const auto readMidiState = [context] {
        if (context.midiProvider == nullptr) return MidiStateSnapshot{};

        try {
            return context.midiProvider();
        } catch (...) {
            MidiStateSnapshot failed;
            failed.error = "the MIDI device list failed";
            return failed;
        }
    };

    server.Get("/midi",
               [context, readMidiState](const httplib::Request&, httplib::Response& response) {
                   decorate(context, response);
                   setJson(response, 200, midiStateToVar(readMidiState()));
               });

    server.Post("/midi/inputs", [context, readMidiState](const httplib::Request& request,
                                                         httplib::Response& response) {
        if (context.midiUpdateInput == nullptr) {
            setError(response, 503, "MIDI inputs are not available");
            return;
        }

        juce::var body;
        std::string error;

        if (!parseJsonObject(request, body, error)) {
            setError(response, 400, error);
            return;
        }

        auto* object = body.getDynamicObject();

        if (!object->hasProperty("id")) {
            setError(response, 400, "body must contain \"id\" (a string)");
            return;
        }

        MidiInputPatch patch;

        if (!readString(object->getProperty("id"), "id", patch.id, error)) {
            setError(response, 400, error);
            return;
        }

        if (object->hasProperty("enabled")) {
            patch.hasEnabled = true;

            if (!detail::readBool(object->getProperty("enabled"), "enabled", patch.enabled,
                                  error)) {
                setError(response, 400, error);
                return;
            }
        }

        if (object->hasProperty("role")) {
            patch.hasRole = true;

            if (!readString(object->getProperty("role"), "role", patch.role, error)) {
                setError(response, 400, error);
                return;
            }
        }

        if (object->hasProperty("noteBase")) {
            patch.hasNoteBase = true;
            std::int64_t noteBase = -1;

            if (!detail::readInteger(object->getProperty("noteBase"), "noteBase", -1,
                                     highestNoteBase, noteBase, error)) {
                setError(response, 400, error);
                return;
            }

            patch.noteBase = static_cast<int>(noteBase);
        }

        if (object->hasProperty("learn")) {
            patch.hasLearn = true;

            if (!detail::readBool(object->getProperty("learn"), "learn", patch.learn, error)) {
                setError(response, 400, error);
                return;
            }
        }

        if (!patch.hasEnabled && !patch.hasRole && !patch.hasNoteBase && !patch.hasLearn) {
            setError(response, 400, "body must contain enabled, role, noteBase or learn");
            return;
        }

        if (!context.midiUpdateInput(patch, error)) {
            const int status = error.find("unknown MIDI input") != std::string::npos ? 404 : 400;
            setError(response, status, error);
            return;
        }

        context.state.notifyChanged("midi", -1);

        decorate(context, response);
        setJson(response, 200, midiStateToVar(readMidiState()));
    });
}

}  // namespace Api::Routes
