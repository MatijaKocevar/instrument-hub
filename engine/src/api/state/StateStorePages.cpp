#include "api/StateStore.h"

#include <utility>

namespace Api {

void StateStore::setPageRestartHandler(PageRestartHandler handler) {
    const std::lock_guard<std::mutex> lock(mutex);
    pageRestartHandler = std::move(handler);
}

bool StateStore::restartPage(const std::string& page, std::string& error) {
    error.clear();

    PageRestartHandler handler;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        handler = pageRestartHandler;
    }

    if (handler == nullptr) {
        error = "no audio session is attached";
        return false;
    }

    return handler(page, error);
}

void StateStore::setPluginPageProvider(PluginPageProvider provider) {
    const std::lock_guard<std::mutex> lock(mutex);
    pluginPageProvider = std::move(provider);
}

std::vector<PluginPageStatus> StateStore::getPluginPageStatuses() const {
    PluginPageProvider provider;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        provider = pluginPageProvider;
    }

    if (provider == nullptr) return {};

    try {
        return provider();
    } catch (...) {
        return {};
    }
}

}  // namespace Api
