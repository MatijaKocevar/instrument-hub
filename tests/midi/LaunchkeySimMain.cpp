// ihub-launchkey-sim: standalone ALSA-sequencer Launchkey Mini MK3 simulator.
//
//   ihub-launchkey-sim [--replay FILE] [--record FILE] [--duration MS]
//                      [--list-ports] [--quiet]
//
// With --replay the fixture is written to the `Launchkey Mini MK3 SIM` port,
// then the simulator keeps listening (1 s by default) so the application can
// answer with LED/handshake messages. Everything the application sends back is
// printed as JSONL on stdout, or written to --record FILE. Without --replay the
// simulator stays up until SIGINT/SIGTERM.

#include "LaunchkeySim.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace
{
std::atomic<bool> stopRequested { false };

void onSignal (int)
{
    stopRequested.store (true, std::memory_order_relaxed);
}

void printUsage (const char* argv0)
{
    std::cout << "usage: " << argv0 << " [options]\n"
              << "  --replay FILE     replay a JSON fixture through the SIM port\n"
              << "  --record FILE     write received MIDI as JSONL to FILE\n"
              << "  --duration MS     stay up for MS milliseconds after replay\n"
              << "  --list-ports      list ALSA sequencer ports and exit\n"
              << "  --quiet           suppress progress lines on stderr\n";
}

struct Options
{
    std::string replay;
    std::string record;
    int durationMs = -1;
    bool listPorts = false;
    bool quiet = false;
};
} // namespace

int main (int argc, char** argv)
{
    Options options;

    try
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                printUsage (argv[0]);
                return 0;
            }

            if (arg == "--replay" && i + 1 < argc)
                options.replay = argv[++i];
            else if (arg == "--record" && i + 1 < argc)
                options.record = argv[++i];
            else if (arg == "--duration" && i + 1 < argc)
                options.durationMs = std::atoi (argv[++i]);
            else if (arg == "--list-ports")
                options.listPorts = true;
            else if (arg == "--quiet")
                options.quiet = true;
            else
                throw std::runtime_error ("unknown or incomplete argument: " + arg);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "ihub-launchkey-sim: " << e.what() << std::endl;
        printUsage (argv[0]);
        return 2;
    }

    try
    {
        if (options.listPorts)
        {
            for (const auto& port : LaunchkeySim::enumeratePorts())
                std::cout << "port " << port.describe() << std::endl;

            return 0;
        }

        LaunchkeySim::Simulator simulator;
        simulator.open();

        if (! options.quiet)
            std::cerr << "sim: client=" << simulator.clientId()
                      << " port=" << simulator.portId()
                      << " name='" << LaunchkeySim::kPortName << "'" << std::endl;

        if (! options.replay.empty())
        {
            const auto fixture = LaunchkeySim::loadFixture (options.replay);

            if (! options.quiet)
                std::cerr << "sim: replaying " << fixture.events.size()
                          << " events from " << options.replay << std::endl;

            simulator.replay (fixture);
        }

        std::signal (SIGINT, onSignal);
        std::signal (SIGTERM, onSignal);

        const int duration = options.durationMs >= 0
                                 ? options.durationMs
                                 : (options.replay.empty() ? -1 : 1000);

        const auto start = std::chrono::steady_clock::now();

        while (! stopRequested.load (std::memory_order_relaxed))
        {
            simulator.pump();
            std::this_thread::sleep_for (std::chrono::milliseconds (2));

            if (duration >= 0)
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
                    std::chrono::steady_clock::now() - start);

                if (elapsed.count() >= duration)
                    break;
            }
        }

        simulator.pump();

        const auto& recorded = simulator.recorded();

        if (! options.record.empty())
        {
            LaunchkeySim::writeRecording (options.record, recorded);

            if (! options.quiet)
                std::cerr << "sim: recorded " << recorded.size()
                          << " messages to " << options.record << std::endl;
        }
        else
        {
            for (const auto& message : recorded)
                std::cout << message.toJson() << '\n';

            std::cout.flush();

            if (! options.quiet)
                std::cerr << "sim: recorded " << recorded.size()
                          << " messages on stdout" << std::endl;
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
