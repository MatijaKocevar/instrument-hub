#include "PageHostProcessTree.h"

#if defined(__linux__)
 #include <chrono>
 #include <csignal>
 #include <dirent.h>
 #include <fstream>
 #include <set>
 #include <sstream>
 #include <string>
 #include <thread>
 #include <vector>
 #include <unistd.h>
#endif

namespace ihub::plugins::page_detail
{

#if defined(__linux__)
namespace
{
struct ProcessEntry
{
    pid_t pid = 0;
    pid_t parent = 0;
    std::vector<std::string> arguments;
};

std::vector<ProcessEntry> readProcesses()
{
    std::vector<ProcessEntry> result;

    if (auto* directory = opendir ("/proc"))
    {
        while (auto* entry = readdir (directory))
        {
            const std::string name (entry->d_name);

            if (name.empty()
                || name.find_first_not_of ("0123456789") != std::string::npos)
                continue;

            const auto pid = static_cast<pid_t> (std::stoi (name));
            ProcessEntry process;
            process.pid = pid;

            {
                std::ifstream stat ("/proc/" + name + "/stat");
                std::string line;

                if (! std::getline (stat, line))
                    continue;

                const auto close = line.rfind (')');

                if (close == std::string::npos)
                    continue;

                std::istringstream fields (line.substr (close + 1));
                char state = 0;
                int parent = 0;

                if (! (fields >> state >> parent))
                    continue;

                process.parent = static_cast<pid_t> (parent);
            }

            {
                std::ifstream cmdline ("/proc/" + name + "/cmdline",
                                       std::ios::in | std::ios::binary);
                std::string value;
                std::vector<char> bytes ((std::istreambuf_iterator<char> (cmdline)),
                                         std::istreambuf_iterator<char>());

                for (const auto byte : bytes)
                {
                    if (byte == '\0')
                    {
                        process.arguments.push_back (value);
                        value.clear();
                    }
                    else
                    {
                        value.push_back (byte);
                    }
                }

                if (! value.empty())
                    process.arguments.push_back (value);
            }

            result.push_back (std::move (process));
        }

        closedir (directory);
    }

    return result;
}

bool containsArgument (const ProcessEntry& process, const std::string& token)
{
    for (const auto& argument : process.arguments)
        if (argument == token)
            return true;

    return false;
}

bool processExists (pid_t pid)
{
    return kill (pid, 0) == 0;
}

void signalProcesses (const std::vector<pid_t>& pids, int signal)
{
    for (const auto pid : pids)
        if (pid > 1 && pid != getpid())
            kill (pid, signal);
}
}
#endif

void killProcessTree (pid_t root) noexcept
{
   #if defined(__linux__)
    if (root <= 1)
        return;

    const auto processes = readProcesses();
    std::set<pid_t> targets { root };
    bool changed = true;

    while (changed)
    {
        changed = false;

        for (const auto& process : processes)
        {
            if (targets.count (process.pid) != 0)
                continue;

            bool belongsToTree = targets.count (process.parent) != 0;

            if (! belongsToTree)
            {
                for (const auto pid : targets)
                {
                    if (containsArgument (process, std::to_string (pid)))
                    {
                        belongsToTree = true;
                        break;
                    }
                }
            }

            if (belongsToTree)
            {
                targets.insert (process.pid);
                changed = true;
            }
        }
    }

    std::vector<pid_t> ordered (targets.begin(), targets.end());
    signalProcesses (ordered, SIGTERM);

    for (int attempt = 0; attempt < 40; ++attempt)
    {
        bool anyAlive = false;

        for (const auto pid : ordered)
            anyAlive = anyAlive || processExists (pid);

        if (! anyAlive)
            break;

        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }

    signalProcesses (ordered, SIGKILL);
   #else
    (void) root;
   #endif
}

} // namespace ihub::plugins::page_detail
