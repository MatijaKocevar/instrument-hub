#pragma once

#include "Pairing.h"
#include "StateStore.h"

#include <httplib.h>

#include <cstdint>
#include <functional>
#include <string>

namespace Api
{

std::string toJson (const juce::var& value);

void setJson (httplib::Response& response, int status, const juce::var& value);

void setError (httplib::Response& response, int status, const std::string& message);

void addCorsHeaders (httplib::Response& response);

bool parseJsonObject (const httplib::Request& request,
                      juce::var& result,
                      std::string& error);

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
}

juce::var stateToVar (const StateSnapshot& state);
juce::var settingsToVar (const SettingsState& settings);

juce::var transportToVar (const TransportState& transport);
juce::var meterFrameToVar (const MeterFrame& frame);

juce::var audioStateToVar (const AudioStateSnapshot& audio);
juce::var audioMeasurementToVar (const AudioMeasurementState& measurement);
juce::var audioProfilesToVar (const std::vector<AudioProfileState>& profiles);
juce::var audioWizardToVar (const AudioWizardState& wizard);

juce::var pairingToVar (const PairingSnapshot& pairing);

juce::var projectOverviewToVar (const ProjectOverview& overview);

juce::var midiStateToVar (const MidiStateSnapshot& midi);

struct RouteContext
{
    StateStore& state;

    std::function<void (httplib::Response&)> decorate;

    AudioStateProvider audioProvider;
    AudioApplyHandler audioApply;
    AudioMeasureHandler audioMeasure;
    AudioMeasureHandler audioMeasureDevice;
    AudioWizardHandler audioWizard;
    AudioWizardApplyHandler audioWizardApply;

    PairingManager* pairing = nullptr;

    std::function<PairingSnapshot (bool)> setLanMode;

    PluginInventoryProvider pluginProvider;
    PluginRescanHandler pluginRescan;

    MidiStateProvider midiProvider;
    MidiInputHandler midiEnableInput;
    MidiPadTestHandler midiPadTest;

    ProjectOverviewProvider projectProvider;
    ProjectCommandHandler projectCommand;
};

void registerRoutes (httplib::Server& server, RouteContext context);

}
