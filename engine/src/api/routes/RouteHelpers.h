#pragma once

#include "api/Routes.h"

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace Api::Routes
{

//==============================================================================
// Shared route helpers
//==============================================================================

/** Creates an empty JSON object. */
juce::var makeObject();

/** Reads a required string field. */
bool readString (const juce::var& value,
                 const char* name,
                 std::string& destination,
                 std::string& error);

/** Reads an array of non-negative integers no larger than 63. */
bool readIntegerList (const juce::var& value,
                      const char* name,
                      std::vector<int>& destination,
                      std::string& error);

/** Parses `request.matches[1]` as a patch slot.

    Returns -1 when the match is missing, malformed or outside the store.
*/
int readPatchSlot (const RouteContext& context, const httplib::Request& request);

/** Parses `request.matches[2]` as an FX insert index. */
int readFxIndex (const httplib::Request& request);

/** Runs the context's response decorator when one is installed. */
void decorate (const RouteContext& context, httplib::Response& response);

//==============================================================================
// Route module registration
//==============================================================================

void registerStateRoutes (httplib::Server& server, const RouteContext& context);
void registerPatchRoutes (httplib::Server& server, const RouteContext& context);
void registerFxRoutes (httplib::Server& server, const RouteContext& context);
void registerAudioRoutes (httplib::Server& server, const RouteContext& context);
void registerMidiRoutes (httplib::Server& server, const RouteContext& context);
void registerPluginRoutes (httplib::Server& server, const RouteContext& context);
void registerTransportRoutes (httplib::Server& server, const RouteContext& context);
void registerPairingRoutes (httplib::Server& server, const RouteContext& context);

} // namespace Api::Routes
