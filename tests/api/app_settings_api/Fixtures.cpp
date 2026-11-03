#include "Fixtures.h"

#include "settings/SettingsStore.h"

#include <chrono>
#include <iostream>
#include <thread>

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

namespace ApiTest::AppSettings
{

Fixture::Fixture()
{
    directoryFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("ihub-app-settings-test-"
                                       + juce::Uuid().toString());
    directoryFile.createDirectory();
    settingsFile = directoryFile.getChildFile ("settings.json");
}

Fixture::~Fixture()
{
    stop();
    directoryFile.deleteRecursively();
}

bool Fixture::start()
{
    stop();

    store = std::make_unique<ihub::settings::SettingsStore> (settingsFile);
    const auto status = store->load();

    if (status != ihub::settings::LoadStatus::loaded
        && status != ihub::settings::LoadStatus::missing)
    {
        std::cerr << "app-settings-test: could not load the settings: "
                  << store->getLastError() << std::endl;
        return false;
    }

    apiState = std::make_unique<Api::StateStore> (4, store.get());

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 5;

    server = std::make_unique<Api::Server> (*apiState, options);

    if (! server->start())
    {
        std::cerr << "app-settings-test: FAILED to start the server" << std::endl;
        stop();
        return false;
    }

    client = std::make_unique<httplib::Client> ("127.0.0.1", server->getPort());
    client->set_connection_timeout (2, 0);
    client->set_read_timeout (2, 0);
    return true;
}

void Fixture::stop()
{
    client.reset();

    if (server != nullptr)
        server->stop();

    server.reset();
    apiState.reset();
    store.reset();
}

bool Fixture::restart()
{
    return start();
}

juce::var Fixture::document() const
{
    juce::var parsed;
    juce::JSON::parse (settingsFile.loadFileAsString(), parsed);
    return parsed;
}

bool Fixture::waitForStateStatus (int status, int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto response = client->Get ("/state");

        if (response && response->status == status)
            return true;

        std::this_thread::sleep_for (std::chrono::milliseconds (25));
    }

    const auto response = client->Get ("/state");
    return response && response->status == status;
}

bool isOwnerOnly (const juce::File& file)
{
   #if JUCE_WINDOWS
    juce::ignoreUnused (file);
    return true;
   #else
    struct stat info = {};

    if (::stat (file.getFullPathName().toRawUTF8(), &info) != 0)
        return false;

    return (info.st_mode & 0777) == 0600;
   #endif
}

}
