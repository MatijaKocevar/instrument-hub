#pragma once

#include "api/Routes.h"

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace Api::Routes
{

juce::var makeObject();

bool readString (const juce::var& value,
                 const char* name,
                 std::string& destination,
                 std::string& error);

bool readIntegerList (const juce::var& value,
                      const char* name,
                      std::vector<int>& destination,
                      std::string& error);

int readPatchSlot (const RouteContext& context, const httplib::Request& request);

int readFxIndex (const httplib::Request& request);

void decorate (const RouteContext& context, httplib::Response& response);

void registerStateRoutes (httplib::Server& server, const RouteContext& context);
void registerTrackRoutes (httplib::Server& server, const RouteContext& context);
void registerProjectRoutes (httplib::Server& server, const RouteContext& context);
void registerProjectDialogRoutes (httplib::Server& server,
                                  const RouteContext& context);
void registerPatchRoutes (httplib::Server& server, const RouteContext& context);
void registerFxRoutes (httplib::Server& server, const RouteContext& context);
void registerAudioRoutes (httplib::Server& server, const RouteContext& context);
void registerMidiRoutes (httplib::Server& server, const RouteContext& context);
void registerPluginRoutes (httplib::Server& server, const RouteContext& context);
void registerTransportRoutes (httplib::Server& server, const RouteContext& context);
void registerPairingRoutes (httplib::Server& server, const RouteContext& context);

}
