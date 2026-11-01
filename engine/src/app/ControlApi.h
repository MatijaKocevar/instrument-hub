#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

namespace Api
{
class MidiBridge;
class Server;
class StateStore;
}

namespace AudioEngine
{
class AudioSettingsBridge;
}

namespace ihub
{
class Session;
}

namespace ihub::plugins
{
class PluginInventory;
}

namespace ihub::app
{

class ControlApi
{
public:
    ControlApi();
    ~ControlApi();

    bool start (const juce::String& commandLine,
                ihub::Session* session,
                bool required,
                std::function<void()> midiRestore = {});

    void stopServer();
    void detachMidi();
    void resetServer();
    void resetAudio();

    Api::StateStore* getState() noexcept { return apiState.get(); }
    Api::MidiBridge* getMidiBridge() noexcept { return midiBridge.get(); }

private:
    std::unique_ptr<ihub::plugins::PluginInventory> pluginInventory;
    std::unique_ptr<Api::StateStore> apiState;
    std::unique_ptr<Api::Server> apiServer;
    std::unique_ptr<Api::MidiBridge> midiBridge;
    std::unique_ptr<AudioEngine::AudioSettingsBridge> audioSettings;
};

}
