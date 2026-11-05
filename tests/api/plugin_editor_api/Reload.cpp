#include "Reload.h"

#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"
#include "Fixtures.h"

#include "plugins/SandboxHost.h"

#include <iostream>
#include <vector>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace ApiTest::PluginEditorApi
{

void runReload (ihub::Session& session, httplib::Client& client)
{
    const juce::String testPluginPath (IHUB_TESTPLUGIN_VST3);

    if (testPluginPath.isEmpty() || ! juce::File (testPluginPath).exists())
    {
        std::cout << "SKIP the sandbox reload check (no TestPlugin)" << std::endl;
        return;
    }

    (void) deleteWithDispatch (client, "/patches/0/editor");

    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("ihub-t207-api-"
                                             + juce::String::toHexString (
                                                 juce::Random::getSystemRandom().nextInt64()));
    (void) tempRoot.deleteRecursively();
    (void) tempRoot.createDirectory();

    auto directory = tempRoot.getChildFile ("vstbridge");
    (void) directory.createDirectory();

    const auto link = directory.getChildFile (juce::File (testPluginPath).getFileName());
    const bool linked = juce::File (testPluginPath).createSymbolicLink (link, true);

    check (linked, "a Wine-style TestPlugin bundle link is created");

    if (! linked)
    {
        (void) tempRoot.deleteRecursively();
        return;
    }

    auto response = postWithDispatch (client, "/patches/1/source",
                                      patchSourceJson (link.getFullPathName(),
                                                       "TestPlugin"));
    check (response && response->status == 200,
           "the Wine-style TestPlugin becomes a source");

    if (! waitForWarm (session, 1, 20000))
    {
        check (false, "the Wine-style TestPlugin becomes resident");
        (void) tempRoot.deleteRecursively();
        return;
    }

    const int processIdBefore = session.getSandboxProcessId (1);
    const int otherProcessId = session.getSandboxProcessId (0);

    response = postWithDispatch (client, "/patches/1/editor", "{}");
    check (response && response->status == 200,
           "the sandboxed TestPlugin opens its native editor");
    check (session.isEditorOpen (1), "the sandbox editor is tracked");

    auto stateResponse = client.Get ("/state");
    check (stateResponse
               && editorOpenInState (parseJson (stateResponse->body), 1),
           "/state reports the sandbox editor open");

    if (auto* host = session.getSandboxSlotControl (1); host != nullptr)
    {
        (void) host->setParameters ({ { "State Marker", 0.42f },
                                      { "Sine Level", 1.0f } });
    }

    response = deleteWithDispatch (client, "/patches/1/editor");
    check (response && response->status == 200,
           "closing the sandbox editor succeeds");
    check (! session.isEditorOpen (1),
           "the sandbox editor is closed immediately");

    stateResponse = client.Get ("/state");
    check (stateResponse
               && ! editorOpenInState (parseJson (stateResponse->body), 1),
           "/state reports the sandbox editor closed without a close call");
    check (session.hasPluginState (1),
           "closing the sandbox editor captures its plugin state");

    response = postWithDispatch (client, "/patches/1/reload", "{}");
    check (response && response->status == 200,
           "POST /patches/1/reload restarts only that slot");

    const int processIdAfter = session.getSandboxProcessId (1);
    check (processIdAfter > 0 && processIdAfter != processIdBefore,
           "the reloaded slot has a new sandbox process");
    check (session.getSandboxProcessId (0) == otherProcessId,
           "the reload leaves the other slot's process alone");
    check (session.hasPluginState (1),
           "the reload keeps the slot's committed state");
    check (session.getSlotError (1).isEmpty(),
           "the reloaded slot reports no error");

    stateResponse = client.Get ("/state");
    check (stateResponse
               && ! editorOpenInState (parseJson (stateResponse->body), 1),
           "/state still reports the reloaded editor closed");

    response = postWithDispatch (client, "/patches/2/reload", "{}");
    check (response && response->status == 400,
           "reloading a non-plugin slot is rejected");
    check (session.getSandboxProcessId (0) == otherProcessId
               && session.getSandboxProcessId (1) == processIdAfter,
           "a rejected reload leaves both plugin slots running");

    (void) deleteWithDispatch (client, "/patches/1/editor");
    (void) tempRoot.deleteRecursively();
}

} // namespace ApiTest::PluginEditorApi
