#include "Lifecycle.h"

#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include "plugins/PluginWindow.h"

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace ApiTest::PluginEditorApi
{

bool runLifecycle (ihub::Session& session, httplib::Client& client)
{
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
        return false;

    response = postWithDispatch (client, "/patches/0/source",
                                 patchSourceJson (testPluginPath, "TestPlugin"));
    check (response && response->status == 200, "the patch accepts a VST3 source");
    check (waitForWarm (session, 0, 15000), "the hosted TestPlugin instance is resident");

    response = postWithDispatch (client, "/patches/0/editor", "{}");
    check (response && response->status == 200, "POST /patches/0/editor returns 200");
    check (response && boolProperty (parseJson (response->body), "open"),
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
    check (response && boolProperty (parseJson (response->body), "reused"),
           "the second POST reports the existing window was reused");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 1,
           "the second POST reuses the same window");

    response = deleteWithDispatch (client, "/patches/0/editor");
    check (response && response->status == 200, "DELETE /patches/0/editor returns 200");
    check (response && ! boolProperty (parseJson (response->body), "open"),
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

    return true;
}

} // namespace ApiTest::PluginEditorApi
