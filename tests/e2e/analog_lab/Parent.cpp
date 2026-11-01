#include "Check.h"
#include "analog_lab/Bundle.h"
#include "analog_lab/ChildProcess.h"
#include "analog_lab/Constants.h"
#include "analog_lab/Parent.h"
#include "analog_lab/SessionRig.h"
#include "analog_lab/StateFile.h"

#include <fstream>
#include <iostream>
#include <string>

namespace AnalogLab
{
using E2E::check;


int runParent()
{
    const auto bundle = findAnalogLabBundle();

    if (! bundle.exists())
    {
        std::cout << "SKIP ihub-analog-lab-state: Analog Lab V is not installed "
                     "(set IHUB_ANALOG_LAB_VST3 to its .vst3 bundle)"
                  << std::endl;
        return kSkipReturnCode;
    }

    std::cout << "Analog Lab V: " << bundle.getFullPathName() << std::endl;

    const auto tempRoot = makeTempRoot();
    const auto stateDirectory = tempRoot.getChildFile ("set.plugin-state");
    const auto hashFile = tempRoot.getChildFile ("state-hashes.txt");

    (void) tempRoot.deleteRecursively();

    if (! tempRoot.createDirectory().wasOk())
    {
        std::cerr << "FAIL ihub-analog-lab-state: could not create "
                  << tempRoot.getFullPathName() << std::endl;
        return 1;
    }

    const auto executable = juce::File::getSpecialLocation (
        juce::File::currentExecutableFile);
    const auto firstLog = tempRoot.getChildFile ("first.log");
    const auto restartLog = tempRoot.getChildFile ("restart.log");

    const int firstExit = runChildWithRetry (executable,
                                             { "--first",
                                               stateDirectory.getFullPathName(),
                                               hashFile.getFullPathName() },
                                             firstLog,
                                             "the first phase");

    std::string hashA;
    std::string hashB;
    const bool haveHashes = readHashes (hashFile, hashA, hashB);

    int restartExit = -1;

    if (firstExit == 0 && haveHashes)
    {
        restartExit = runChildWithRetry (executable,
                                         { "--restart",
                                           stateDirectory.getFullPathName(),
                                           juce::String (hashA),
                                           juce::String (hashB) },
                                         restartLog,
                                         "the restart phase");
    }

    check (firstExit == 0,
           "the first Analog Lab process exited cleanly (rc "
               + std::to_string (firstExit) + ")");
    check (haveHashes, "the first process left two state hashes for the restart");
    check (restartExit == 0,
           "the restarted Analog Lab process exited cleanly (rc "
               + std::to_string (restartExit) + ")");

    {
        std::ofstream record ("ihub-analog-lab-state-record.txt");
        record << "test=ihub-analog-lab-state\n"
               << "plugin=Analog Lab V\n"
               << "bundle=" << bundle.getFullPathName() << "\n"
               << "slots=" << kSlots << "\n"
               << "states=2\n"
               << "state_a_hash=" << hashA << "\n"
               << "state_b_hash=" << hashB << "\n"
               << "first_process_exit=" << firstExit << "\n"
               << "restart_process_exit=" << restartExit << "\n"
               << "status=" << (E2E::failures == 0 ? "PASS" : "FAIL") << "\n";

        check (record.good(),
               "the measurement record was written "
               "(ihub-analog-lab-state-record.txt)");
    }

    (void) tempRoot.deleteRecursively();

    std::cout << (E2E::failures == 0 ? "\nPASS" : "\nFAIL") << ": "
              << E2E::failures << " failed check(s)" << std::endl;

    return E2E::failures == 0 ? 0 : 1;
}

} // namespace AnalogLab
