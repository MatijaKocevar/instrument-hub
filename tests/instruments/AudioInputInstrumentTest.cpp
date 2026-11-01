#include "audio_input/TestSupport.h"
#include "audio_input/Channels.h"
#include "audio_input/DirectMonitor.h"
#include "audio_input/EngineInput.h"
#include "audio_input/GainLatency.h"
#include "audio_input/LatencyBudget.h"
#include "audio_input/LoopbackMaster.h"
#include "audio_input/LoopbackMonitor.h"
#include "audio_input/Unity.h"

#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        AudioInputTest::runUnityGainIntoBus();
        AudioInputTest::runGainAndLatency();
        AudioInputTest::runChannelSelectionAndMissingInput();
        AudioInputTest::runDirectMonitorBypassesFxChain();
        AudioInputTest::runLatencyBudget();
        AudioInputTest::runEngineFeedsDeviceInput();
        AudioInputTest::runLoopbackThroughMasterBus();
        AudioInputTest::runLoopbackThroughFxChainAndDirectMonitor();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }

    std::cout << (AudioInputTest::failures == 0 ? "PASS" : "FAIL")
              << ": ihub-instrument audio input" << std::endl;

    return AudioInputTest::failures == 0 ? 0 : 1;
}
