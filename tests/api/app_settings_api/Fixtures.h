#pragma once

#include "api/Server.h"
#include "api/StateStore.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <memory>

namespace ihub::settings
{
class SettingsStore;
}

namespace ApiTest::AppSettings
{

struct Fixture
{
    Fixture();
    ~Fixture();

    bool start();
    void stop();
    bool restart();

    httplib::Client& http() noexcept { return *client; }
    Api::Server& api() noexcept { return *server; }
    Api::StateStore& state() noexcept { return *apiState; }
    ihub::settings::SettingsStore& settings() noexcept { return *store; }

    juce::File file() const { return settingsFile; }
    juce::var document() const;

    bool waitForStateStatus (int status, int timeoutMs = 5000);

private:
    juce::File directoryFile;
    juce::File settingsFile;

    std::unique_ptr<ihub::settings::SettingsStore> store;
    std::unique_ptr<Api::StateStore> apiState;
    std::unique_ptr<Api::Server> server;
    std::unique_ptr<httplib::Client> client;
};

bool isOwnerOnly (const juce::File& file);

}
