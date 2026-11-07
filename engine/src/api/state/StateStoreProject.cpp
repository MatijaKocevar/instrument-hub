#include "api/StateStore.h"

#include <utility>

namespace Api
{

ProjectSaveState StateStore::getProjectState() const
{
    ProjectStateProvider provider;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        provider = projectStateProvider;
    }

    if (provider == nullptr)
        return {};

    try
    {
        return provider();
    }
    catch (...)
    {
        return {};
    }
}

void StateStore::setProjectStateProvider (ProjectStateProvider provider)
{
    const std::lock_guard<std::mutex> lock (mutex);
    projectStateProvider = std::move (provider);
}

void StateStore::notifyProjectStateChanged()
{
    notifyChanged ("project", -1);
}

void StateStore::setEditHandler (EditHandler handler)
{
    const std::lock_guard<std::mutex> lock (mutex);
    editHandler = std::move (handler);
}

void StateStore::notePluginStateEdit (int slot)
{
    notifyEdited ("plugin", slot);
}

void StateStore::notifyEdited (const char* kind, int slot)
{
    EditHandler handler;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        handler = editHandler;
    }

    if (handler != nullptr)
        handler (kind, slot);
}

}
