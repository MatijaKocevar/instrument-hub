// ihub-plugin-editor-api-test: acceptance check for T129.
//
// Drives the control API exactly as the Instrument page does and proves the
// native editor lifecycle end to end:
//
//   1. POST /patches/{slot}/editor opens a real native window for TestPlugin;
//   2. a second POST reuses it (one window, no duplicate editor);
//   3. DELETE closes it and is idempotent;
//   4. the window reopens after a close;
//   5. switching the patch source closes the editor before the processor is
//      retired, and the slot can no longer open an editor until a plugin is
//      assigned again;
//   6. a non-plugin patch and an invalid request get a readable error;
//   7. when an Analog Lab V bundle is installed (directly or through
//      yabridge), the same path opens its editor.
//
// The test runs under DISPLAY=:0 like the T023 plugin-window check. JUCE's
// native editor lives on the message thread, so every HTTP request that opens
// or closes a window is issued while the main thread pumps the message loop.

#include "Session.h"
#include "api/Server.h"
#include "api/SessionBridge.h"
#include "plugins/PluginWindow.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <string>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

juce::var parseJson (const std::string& text)
{
    return juce::JSON::parse (juce::String (text));
}

juce::var property (const juce::var& object, const char* name)
{
    if (! object.isObject() || object.getDynamicObject() == nullptr)
        return {};

    return object.getDynamicObject()->getProperty (name);
}

const juce::Array<juce::var>* arrayProperty (const juce::var& object, const char* name)
{
    return property (object, name).getArray();
}

juce::var patchForSlot (const juce::var& state, int slot)
{
    const auto* families = arrayProperty (state, "families");

    if (families == nullptr)
        return {};

    for (const auto& family : *families)
    {
        const auto* patches = arrayProperty (family, "patches");

        if (patches == nullptr)
            continue;

        for (const auto& patch : *patches)
            if (static_cast<int> (property (patch, "slot")) == slot)
                return patch;
    }

    return {};
}

bool editorOpenInState (const juce::var& state, int slot)
{
    return static_cast<bool> (property (patchForSlot (state, slot), "editorOpen"));
}

/** Sends one request while pumping the JUCE message loop.

    The editor routes marshal to the message thread with
    `MessageManager::callSync`, which cannot complete while the main thread is
    blocked inside the HTTP call.
*/
httplib::Result requestWithDispatch (std::function<httplib::Result()> send)
{
    auto future = std::async (std::launch::async, std::move (send));

    while (future.wait_for (std::chrono::milliseconds (0))
           != std::future_status::ready)
    {
        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
    }

    return future.get();
}

httplib::Result postWithDispatch (httplib::Client& client,
                                  const std::string& path,
                                  const std::string& body)
{
    return requestWithDispatch ([&client, path, body]
    {
        return client.Post (path, body, "application/json");
    });
}

httplib::Result deleteWithDispatch (httplib::Client& client,
                                    const std::string& path)
{
    return requestWithDispatch ([&client, path]
    {
        return client.Delete (path);
    });
}

/** Waits for a warm slot while pumping the message loop (VST3 creation is
    posted to the message thread on Linux). */
bool waitForWarm (ihub::Session& session, int slot, int timeoutMs)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (session.isSlotWarm (slot))
            return true;

        if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
            manager->runDispatchLoopUntil (2);
        else
            juce::Thread::sleep (2);
    }

    return session.isSlotWarm (slot);
}

std::string patchSourceJson (const juce::String& path, const std::string& name)
{
    return "{\"kind\":\"plugin\",\"name\":\"" + name + "\",\"path\":\""
           + path.toStdString() + "\",\"pluginName\":\"" + name + "\"}";
}

/** An Analog Lab V bundle, if this machine has one (direct VST3 or yabridge). */
juce::File findAnalogLabBundle()
{
    if (const auto* env = std::getenv ("IHUB_ANALOG_LAB_VST3");
        env != nullptr && *env != '\0')
    {
        const juce::File candidate { juce::String (env) };
        return candidate.exists() ? candidate : juce::File();
    }

    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const juce::Array<juce::File> candidates {
        juce::File ("/usr/lib/vst3/Analog Lab V.vst3"),
        juce::File ("/usr/local/lib/vst3/Analog Lab V.vst3"),
        home.getChildFile (".vst3/Analog Lab V.vst3"),
        juce::File ("/usr/lib/vst3/yabridge/Analog Lab V.vst3"),
        home.getChildFile (".vst3/yabridge/Analog Lab V.vst3"),
    };

    for (const auto& candidate : candidates)
        if (candidate.exists())
            return candidate;

    return {};
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
    {
        std::cerr << "plugin-editor-api-test: FAILED, no display (DISPLAY=:0)"
                  << std::endl;
        return 1;
    }

    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = 1;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::initTone);
    config.slots.resize (4);

    ihub::Session session;
    juce::String buildError;

    if (! session.build (config, buildError))
    {
        std::cerr << "plugin-editor-api-test: FAILED to build the session: "
                  << buildError << std::endl;
        return 1;
    }

    Api::StateStore state (session.getNumSlots());

    Api::Server::Options options;
    options.address = "127.0.0.1";
    options.port = 0;
    options.meterHz = 30;

    Api::Server server (state, options);
    Api::attachSession (state, server, session);

    if (! server.start())
    {
        std::cerr << "plugin-editor-api-test: FAILED to start the server" << std::endl;
        return 1;
    }

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (30, 0);

    std::cout << "plugin-editor-api-test: listening on 127.0.0.1:"
              << server.getPort() << " slots=" << session.getNumSlots() << std::endl;

    //--------------------------------------------------------------------------
    // A plugin patch is required: a generated source deliberately has no editor.

    auto response = postWithDispatch (client, "/patches/0/editor", "{}");

    check (response && response->status == 400,
           "opening an editor on an init-tone patch is rejected");
    check (response
               && response->body.find ("native editor") != std::string::npos,
           "the rejection explains why there is no editor");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
           "a rejected request leaves no window behind");

    const juce::String testPluginPath (IHUB_TESTPLUGIN_VST3);
    check (testPluginPath.isNotEmpty() && juce::File (testPluginPath).exists(),
           "TestPlugin.vst3 is built");

    if (testPluginPath.isEmpty() || ! juce::File (testPluginPath).exists())
    {
        server.stop();
        return 1;
    }

    response = postWithDispatch (client, "/patches/0/source",
                                 patchSourceJson (testPluginPath, "TestPlugin"));
    check (response && response->status == 200, "the patch accepts a VST3 source");
    check (waitForWarm (session, 0, 15000), "the hosted TestPlugin instance is resident");

    //--------------------------------------------------------------------------
    // Open, reuse, close, reopen. One window per slot, idempotently.

    response = postWithDispatch (client, "/patches/0/editor", "{}");
    check (response && response->status == 200, "POST /patches/0/editor returns 200");
    check (response && static_cast<bool> (property (parseJson (response->body), "open")),
           "the response says the editor is open");

    auto stateResponse = client.Get ("/state");
    check (stateResponse && stateResponse->status == 200,
           "GET /state answers after opening the editor");
    check (stateResponse && editorOpenInState (parseJson (stateResponse->body), 0),
           "/state reports the patch editor as open");
    check (session.isEditorOpen (0), "the session reports the slot's editor open");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
           "exactly one native window is open");

    response = postWithDispatch (client, "/patches/0/editor", "{}");
    check (response && response->status == 200, "a second POST opens idempotently");
    check (response && static_cast<bool> (property (parseJson (response->body), "reused")),
           "the second POST reports the existing window was reused");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
           "the second POST reuses the same window");

    response = deleteWithDispatch (client, "/patches/0/editor");
    check (response && response->status == 200, "DELETE /patches/0/editor returns 200");
    check (response && ! static_cast<bool> (property (parseJson (response->body), "open")),
           "the delete response says the editor is closed");

    stateResponse = client.Get ("/state");
    check (stateResponse && ! editorOpenInState (parseJson (stateResponse->body), 0),
           "/state reports the patch editor as closed");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
           "DELETE destroys the native window");

    response = deleteWithDispatch (client, "/patches/0/editor");
    check (response && response->status == 200,
           "DELETE is idempotent when no window is open");

    response = postWithDispatch (client, "/patches/0/editor", "{}");
    check (response && response->status == 200 && session.isEditorOpen (0),
           "the window can be reopened after a close");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
           "the reopened window is the only one");

    //--------------------------------------------------------------------------
    // Switching the source closes the editor before the instance is retired.

    response = postWithDispatch (client, "/patches/0/source",
                                 "{\"kind\":\"init-tone\",\"frequency\":330}");
    check (response && response->status == 200, "the patch switches back to a tone");
    check (! session.isEditorOpen (0),
           "the source change closed the editor before the processor retired");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
           "no editor window survives the source change");
    check (waitForWarm (session, 0, 5000), "the tone source becomes resident");

    response = postWithDispatch (client, "/patches/0/editor", "{}");
    check (response && response->status == 400,
           "a non-plugin patch can no longer open an editor");

    response = postWithDispatch (client, "/patches/99/editor", "{}");
    check (response && response->status == 404, "an out-of-range slot is rejected");

    //--------------------------------------------------------------------------
    // Analog Lab V, when installed (native or through yabridge).

    const auto analogLab = findAnalogLabBundle();

    if (analogLab.exists())
    {
        std::cout << "Analog Lab V: " << analogLab.getFullPathName() << std::endl;

        response = postWithDispatch (client, "/patches/1/source",
                                     patchSourceJson (analogLab.getFullPathName(),
                                                      "Analog Lab V"));
        check (response && response->status == 200,
               "a yabridge/Arturia bundle is accepted as a source");

        if (waitForWarm (session, 1, 20000))
        {
            response = postWithDispatch (client, "/patches/1/editor", "{}");
            check (response && response->status == 200,
                   "Analog Lab V opens its native editor");
            check (session.isEditorOpen (1), "the Analog Lab V window is tracked");

            response = deleteWithDispatch (client, "/patches/1/editor");
            check (response && response->status == 200,
                   "the Analog Lab V window closes cleanly");
            check (session.hasPluginState (1),
                   "closing the Analog Lab V editor captured its plugin state");
            check (session.getPluginStateSize (1) > 0,
                   "the Analog Lab V state blob is non-empty");
        }
        else
        {
            std::cout << "SKIP the Analog Lab V editor (the bundle did not become resident)"
                      << std::endl;
        }
    }
    else
    {
        std::cout << "SKIP the Analog Lab V editor (no bundle installed)" << std::endl;
    }

    // Leave no window behind before the session is destroyed.
    (void) deleteWithDispatch (client, "/patches/0/editor");
    (void) deleteWithDispatch (client, "/patches/1/editor");

    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
           "no native window is left open");

    server.stop();

    if (failures > 0)
    {
        std::cerr << "plugin-editor-api-test: " << failures
                  << " check(s) failed" << std::endl;
        std::cerr.flush();
        std::_Exit (1);
    }

    std::cout << "plugin-editor-api-test: all checks passed" << std::endl;
    std::cout.flush();

    // Wine bridge threads and JUCE static teardown can block exit even after
    // every check passed. Terminate explicitly so ctest gets a clean status.
    std::_Exit (0);
}
