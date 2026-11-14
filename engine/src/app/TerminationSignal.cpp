#include "TerminationSignal.h"

#if defined(__linux__)
#include <juce_events/juce_events.h>

#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <unistd.h>

namespace
{
    int terminationPipe[2] = {-1, -1};

    void handleTerminationSignal (int) noexcept
    {
        if (terminationPipe[1] < 0)
            return;

        const char byte = 1;
        const auto written = ::write (terminationPipe[1], &byte, 1);
        (void)written;
    }

    void monitorTerminationSignals ()
    {
        pollfd descriptor{};
        descriptor.fd = terminationPipe[0];
        descriptor.events = POLLIN;

        if (::poll (&descriptor, 1, -1) <= 0)
            return;

        char buffer[16];

        while (::read (terminationPipe[0], buffer, sizeof (buffer)) > 0)
        {
        }

        juce::MessageManager::callAsync (
            []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance ())
                    app->systemRequestedQuit ();
            });
    }
}

namespace ihub::app
{

    bool installTerminationSignalHandler () noexcept
    {
        if (terminationPipe[0] >= 0)
            return true;

        if (::pipe2 (terminationPipe, O_CLOEXEC | O_NONBLOCK) != 0)
            return false;

        struct sigaction action{};
        action.sa_handler = handleTerminationSignal;
        sigemptyset (&action.sa_mask);
        action.sa_flags = SA_RESTART;

        if (::sigaction (SIGTERM, &action, nullptr) != 0 ||
            ::sigaction (SIGINT, &action, nullptr) != 0)
        {
            return false;
        }

        std::thread (monitorTerminationSignals).detach ();
        return true;
    }

} // namespace ihub::app

#else

namespace ihub::app
{

    bool installTerminationSignalHandler () noexcept
    {
        return false;
    }

} // namespace ihub::app

#endif
