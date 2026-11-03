#include "api/StateStore.h"

#include <utility>

namespace Api
{

void StateStore::setPatchSourceHandler (PatchSourceHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchSourceHandler = std::move (handler);
}

bool StateStore::applyPatchSource (int slot,
                                   const PatchSourceRequest& source,
                                   std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    if (source.kind != "empty"
        && source.kind != "init-tone"
        && source.kind != "sampler"
        && source.kind != "plugin"
        && source.kind != "audio-input")
    {
        error = "kind must be empty, init-tone, sampler, plugin or audio-input";
        return false;
    }

    PatchSourceHandler applySource;
    PatchInstrumentState previous;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        applySource = patchSourceHandler;
        previous = slotInstruments[static_cast<std::size_t> (slot)];
    }

    if (applySource == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    if (! applySource (slot, source, error))
    {
        return false;
    }

    PatchInstrumentState next;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        next = slotInstruments[static_cast<std::size_t> (slot)];
        next.kind = source.kind;
        next.id = previous.id;

        if (! source.name.empty())
            next.name = source.name;
        else if (! source.pluginName.empty())
            next.name = source.pluginName;
        else if (next.name.empty())
            next.name = slotPatches[static_cast<std::size_t> (slot)];

        next.pluginFormat.clear();
        next.pluginId.clear();
        next.pluginName.clear();
        next.pluginManufacturer.clear();
        next.pluginVersion.clear();
        next.samplePath.clear();

        if (source.kind == "plugin")
        {
            next.pluginFormat = source.pluginFormat.empty() ? "VST3" : source.pluginFormat;
            next.pluginId = source.pluginId;
            next.pluginName = source.pluginName.empty() ? source.name : source.pluginName;
            next.pluginManufacturer = source.pluginManufacturer;
            next.pluginVersion = source.pluginVersion;
        }
        else if (source.kind == "sampler")
        {
            next.samplePath = source.samplePath.empty() ? source.path : source.samplePath;
        }

        slotInstruments[static_cast<std::size_t> (slot)] = next;
        slotEditorOpen[static_cast<std::size_t> (slot)] = false;
    }

    notifyEdited ("patch", slot);
    notifyChanged ("patch", slot);

    return true;
}

void StateStore::setPatchEditorHandlers (PatchEditorHandler openEditor,
                                         PatchEditorHandler closeEditor)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchEditorHandler = std::move (openEditor);
    patchEditorCloseHandler = std::move (closeEditor);
}

bool StateStore::openPatchEditor (int slot, std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    PatchEditorHandler open;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        open = patchEditorHandler;
    }

    if (open == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    std::string handlerError;

    if (! open (slot, handlerError))
    {
        error = handlerError.empty() ? "could not open the plugin editor" : handlerError;
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (mutex);
        slotEditorOpen[static_cast<std::size_t> (slot)] = true;
    }

    notifyChanged ("patch", slot);

    error.clear();
    return true;
}

bool StateStore::closePatchEditor (int slot, std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    PatchEditorHandler close;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        close = patchEditorCloseHandler;
    }

    if (close == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    std::string handlerError;

    if (! close (slot, handlerError))
    {
        error = handlerError.empty() ? "could not close the plugin editor" : handlerError;
        return false;
    }

    {
        const std::lock_guard<std::mutex> lock (mutex);
        slotEditorOpen[static_cast<std::size_t> (slot)] = false;
    }

    notifyChanged ("patch", slot);

    error.clear();
    return true;
}

void StateStore::setPatchStateCaptureHandler (PatchStateCaptureHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    patchStateCaptureHandler = std::move (handler);
}

bool StateStore::capturePatchState (int slot,
                                    PatchStateCapture& capture,
                                    std::string& error)
{
    if (slot < 0 || slot >= numSlots)
    {
        error = "patch slot is out of range";
        return false;
    }

    PatchStateCaptureHandler handler;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        handler = patchStateCaptureHandler;
    }

    if (handler == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    PatchStateCapture result;
    result.slot = slot;
    std::string handlerError;

    if (! handler (slot, result, handlerError))
    {
        error = handlerError.empty() ? "could not save the patch sound"
                                     : handlerError;
        return false;
    }

    result.slot = slot;
    result.captured = true;
    capture = result;
    error.clear();

    notifyEdited ("patch", slot);
    notifyChanged ("patch", slot);

    return true;
}

}
