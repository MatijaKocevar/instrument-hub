#pragma once

#include "Server.h"

#include <httplib.h>

#include <string>

namespace Api
{

//==============================================================================
// JSON helpers shared by the HTTP routes
//==============================================================================

/** Serialises `value` compactly. */
std::string toJson (const juce::var& value);

/** Sets a JSON body plus the standard CORS/content headers. */
void setJson (httplib::Response& response, int status, const juce::var& value);

/** Overwrites `response` with a `{"error": ...}` body. */
void setError (httplib::Response& response, int status, const std::string& message);

/** Adds the headers the embedded UI and the tablet remote need. */
void addCorsHeaders (httplib::Response& response);

/** Parses a request body as a JSON object. Returns false with a message when
    the body is missing, malformed or not an object.
*/
bool parseJsonObject (const httplib::Request& request,
                      juce::var& result,
                      std::string& error);

//==============================================================================
// Validation helpers (shared with StateStore)
//==============================================================================

namespace detail
{
bool isNumber (const juce::var& value) noexcept;

bool readNumber (const juce::var& value,
                 const char* name,
                 double minimum,
                 double maximum,
                 double& destination,
                 std::string& error);

bool readBool (const juce::var& value,
               const char* name,
               bool& destination,
               std::string& error);

bool readInteger (const juce::var& value,
                  const char* name,
                  std::int64_t minimum,
                  std::int64_t maximum,
                  std::int64_t& destination,
                  std::string& error);
} // namespace detail

//==============================================================================
// State serialisation
//==============================================================================

juce::var stateToVar (const StateSnapshot& state);
juce::var settingsToVar (const SettingsState& settings);

juce::var transportToVar (const TransportState& transport);
juce::var meterFrameToVar (const MeterFrame& frame);

//==============================================================================
// Route registration
//==============================================================================

/** The pieces of the server a route can use while answering a request. */
struct RouteContext
{
    StateStore& state;

    /** Extra response headers for every JSON reply. */
    std::function<void (httplib::Response&)> decorate;
};

/** Installs every HTTP route and the WebSocket meter upgrade.

    Authentication and CORS pre-flight are handled by `Server` before routing,
    so the handlers here only deal with their payloads.
*/
void registerRoutes (httplib::Server& server, RouteContext context);

} // namespace Api
