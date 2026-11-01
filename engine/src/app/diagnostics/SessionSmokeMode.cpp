#include "app/DiagnosticModes.h"

#include "app/CommandLine.h"

#include "Session.h"

#include <iostream>

namespace ihub::app
{

int runSessionSmoke (const juce::String& commandLine)
{
    constexpr double switchLimitDbfs = -60.0;
    constexpr double minimumRmsDbfs = -60.0;

    ihub::SessionConfig config;
    config.device.sampleRate = 48000.0;
    config.device.bufferSize = 128;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.device.preferLoopback = true;

    config.slots = ihub::Session::makeDefaultSet();

    for (auto& slot : config.slots)
    {
        slot.kind = ihub::SessionSlotKind::initTone;
        slot.instrumentKind = "init-tone";
    }

    if (const auto value = getCommandLineValue (commandLine, "--audio-device"); value.isNotEmpty())
        config.device.outputDeviceName = value;

    if (const auto value = getCommandLineValue (commandLine, "--audio-sample-rate"); value.isNotEmpty())
        config.device.sampleRate = value.getDoubleValue();

    if (const auto value = getCommandLineValue (commandLine, "--audio-buffer-size"); value.isNotEmpty())
        config.device.bufferSize = value.getIntValue();

    ihub::Session session;
    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "session-smoke: FAILED to build the default set: "
                  << error << std::endl;
        return 1;
    }

    std::cout << "session-smoke: built " << session.getNumSlots()
              << " slots" << std::endl;

    const auto report = session.measureSwitch (0, 1);

    if (! report.rendered)
    {
        std::cerr << "session-smoke: FAILED, the switch render produced no samples"
                  << std::endl;
        return 1;
    }

    std::cout << "session-smoke: armed_slot=" << report.fromSlot
              << " rms=" << report.rms
              << " rms_dbfs=" << report.rmsDbfs
              << " switch_to=" << report.toSlot
              << " discontinuity_dbfs=" << report.discontinuityDbfs
              << " max_slew_dbfs=" << report.maxSampleDeltaDbfs
              << " samples=" << report.samplesMeasured
              << std::endl;

    const bool rmsOk = report.rms > 0.0 && report.rmsDbfs > minimumRmsDbfs;
    const bool switchOk = report.discontinuityDbfs <= switchLimitDbfs;

    if (! rmsOk)
        std::cerr << "session-smoke: FAILED, armed slot RMS is zero/silent ("
                  << report.rmsDbfs << " dBFS)" << std::endl;

    if (! switchOk)
        std::cerr << "session-smoke: FAILED, switch discontinuity "
                  << report.discontinuityDbfs << " dBFS is above "
                  << switchLimitDbfs << " dBFS" << std::endl;

    if (! session.openDevice (&error))
    {
        std::cerr << "session-smoke: FAILED to open the ALSA Loopback: "
                  << error << std::endl;
        return 1;
    }

    const auto deviceName = session.getDevices().getCurrentDeviceName();
    const auto deviceRate = session.getDevices().getCurrentSampleRate();
    const auto deviceBlock = session.getDevices().getCurrentBufferSize();

    session.armSlot (0);
    juce::Thread::sleep (300);

    const auto timing = session.getEngine().getTimingMonitor().snapshot();
    session.closeDevice();

    const bool deviceOk = timing.callbacks > 0;

    std::cout << "session-smoke: device=\"" << deviceName << "\""
              << " rate=" << deviceRate
              << " block=" << deviceBlock
              << " callbacks=" << timing.callbacks
              << " " << (deviceOk ? "callback-running" : "callback-silent")
              << std::endl;

    const bool passed = rmsOk && switchOk && deviceOk;

    std::cout << "session-smoke: "
              << (passed ? "PASS" : "FAIL")
              << " (rms>" << minimumRmsDbfs << "dBFS, discontinuity<="
              << switchLimitDbfs << "dBFS, device callback running)"
              << std::endl;

    return passed ? 0 : 1;
}

}
