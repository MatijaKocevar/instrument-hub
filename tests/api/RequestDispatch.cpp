#include "RequestDispatch.h"

#include <juce_events/juce_events.h>

#include <chrono>
#include <future>

namespace ApiTest
{

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

httplib::Result patchWithDispatch (httplib::Client& client,
                                   const std::string& path,
                                   const std::string& body)
{
    return requestWithDispatch ([&client, path, body]
    {
        return client.Patch (path, body, "application/json");
    });
}

httplib::Result putWithDispatch (httplib::Client& client,
                                 const std::string& path,
                                 const std::string& body)
{
    return requestWithDispatch ([&client, path, body]
    {
        return client.Put (path, body, "application/json");
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

} // namespace ApiTest
