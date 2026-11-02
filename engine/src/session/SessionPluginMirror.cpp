#include "Session.h"

#include "plugins/PluginInstrument.h"
#include "plugins/SandboxHost.h"
#include "session/EditorHost.h"

#include <thread>
#include <utility>

namespace ihub
{
bool Session::ensurePluginMirror (int slot, std::string& error)
{
    error.clear();

    if (pluginState.getInstrument (slot) != nullptr)
        return true;

    auto* manager = juce::MessageManager::getInstanceWithoutCreating();

    if (manager != nullptr && ! manager->isThisTheMessageThread())
    {
        bool opened = false;
        std::string threadError;

        std::thread worker ([this, slot, &opened, &threadError]
        {
            opened = ensurePluginMirrorNow (slot, threadError);
        });
        worker.join();

        error = std::move (threadError);
        return opened;
    }

    return ensurePluginMirrorNow (slot, error);
}

bool Session::ensurePluginMirrorNow (int slot, std::string& error)
{
    error.clear();

    if (pluginState.getInstrument (slot) != nullptr)
        return true;

    auto* sandbox = sandboxForSlot (slot);

    if (sandbox == nullptr)
    {
        error = "the patch is not hosted by the plugin sandbox";
        return false;
    }

    if (sandbox->isFailed())
    {
        error = "the plugin host failed: " + sandbox->getFailureReason().toStdString();
        return false;
    }

    session::PluginStateIdentity identity;

    if (! pluginIdentityForSlot (slot, identity))
    {
        error = "patch slot is out of range or is not a plugin";
        return false;
    }

    if (pluginFormats == nullptr)
    {
        error = "the plugin formats are not available";
        return false;
    }

    auto mirror = std::make_unique<plugins::PluginInstrument>();
    juce::String loadError;

    if (! mirror->loadFromFile (*pluginFormats,
                                juce::File (identity.pluginPath),
                                sampleRate,
                                maximumBlockSize,
                                loadError))
    {
        error = loadError.toStdString();
        return false;
    }

    auto state = sandbox->getStateInformation();

    if (state.getSize() == 0)
        state = pluginState.readState (slot, identity);

    if (state.getSize() > 0)
        (void) mirror->setStateInformation (state);

    mirror->setStateChangedCallback ([this, slot]
    {
        syncPluginMirrorToSandbox (slot);
    });

    plugins::PluginInstrument* stored = nullptr;

    {
        const std::lock_guard<std::mutex> lock (mirrorMutex);

        if (slot < 0 || slot >= static_cast<int> (pluginMirrors.size()))
        {
            error = "patch slot is out of range";
            return false;
        }

        auto& existing = pluginMirrors[static_cast<std::size_t> (slot)];

        if (existing == nullptr)
            existing = std::move (mirror);

        stored = existing.get();
    }

    pluginState.registerInstrument (slot, *stored);

    if (editorHost != nullptr)
        editorHost->registerInstrument (slot, stored);

    return true;
}

void Session::releasePluginMirror (int slot)
{
    std::unique_ptr<plugins::PluginInstrument> mirror;

    {
        const std::lock_guard<std::mutex> lock (mirrorMutex);

        if (slot < 0 || slot >= static_cast<int> (pluginMirrors.size()))
            return;

        mirror = std::move (pluginMirrors[static_cast<std::size_t> (slot)]);
    }

    if (mirror == nullptr)
        return;

    mirror->setStateChangedCallback (nullptr);

    if (editorHost != nullptr)
    {
        std::string error;
        (void) editorHost->close (slot, error);
    }

    pluginState.unregisterInstrument (*mirror);
}

void Session::releaseAllPluginMirrors()
{
    for (int slot = 0; slot < defaultLogicalSlots; ++slot)
        releasePluginMirror (slot);
}

void Session::syncPluginMirrorToSandbox (int slot)
{
    plugins::PluginInstrument* mirror = nullptr;

    {
        const std::lock_guard<std::mutex> lock (mirrorMutex);

        if (slot < 0 || slot >= static_cast<int> (pluginMirrors.size()))
            return;

        mirror = pluginMirrors[static_cast<std::size_t> (slot)].get();
    }

    auto* sandbox = sandboxForSlot (slot);

    if (mirror == nullptr || sandbox == nullptr || ! sandbox->isRunning())
        return;

    const auto state = mirror->getStateInformation();

    if (state.getSize() > 0)
        (void) sandbox->setStateInformation (state);
}

}
