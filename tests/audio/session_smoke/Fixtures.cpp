#include "Fixtures.h"

namespace AudioTest::SessionSmoke
{

ihub::SessionConfig makeConfig (float fadeMilliseconds)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = fadeMilliseconds;
    config.slots = ihub::Session::makeDefaultSet();

    // The shipping default is silent. This smoke measurement needs a source,
    // so it uses the engine's deterministic initialisation tone fixture.
    for (auto& slot : config.slots)
    {
        slot.kind = ihub::SessionSlotKind::initTone;
        slot.instrumentKind = "init-tone";
    }

    return config;
}

void checkReport (const ihub::SessionSwitchReport& report, const std::string& label)
{
    std::cout << "      " << label
              << ": rms=" << report.rms
              << " (" << report.rmsDbfs << " dBFS)"
              << " discontinuity=" << report.discontinuityDbfs << " dBFS"
              << " max_delta=" << report.maxSampleDeltaDbfs << " dBFS"
              << " samples=" << report.samplesMeasured << std::endl;

    check (report.rendered, label + ": the render produced samples");
    check (report.samplesMeasured > 0, label + ": samples were measured");
    check (report.rms > 0.0 && report.rmsDbfs > kMinimumRmsDbfs,
           label + ": the armed slot has a non-zero RMS");
    check (report.discontinuityDbfs <= kDiscontinuityLimitDbfs,
           label + ": no discontinuity above -60 dBFS across the switch");
}

double renderCallbackRms (ihub::Session& session)
{
    const int blockSize = session.getEngine().getMaximumBlockSize();
    std::vector<float> left (static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float> right (static_cast<std::size_t> (blockSize), 0.0f);
    float* outputs[2] { left.data(), right.data() };

    const juce::AudioIODeviceCallbackContext context;
    session.audioDeviceIOCallbackWithContext (nullptr, 0, outputs, 2, blockSize, context);

    double sumSquares = 0.0;

    for (int frame = 0; frame < blockSize; ++frame)
        sumSquares += static_cast<double> (left[static_cast<std::size_t> (frame)])
                      * static_cast<double> (left[static_cast<std::size_t> (frame)]);

    return blockSize > 0 ? std::sqrt (sumSquares / static_cast<double> (blockSize)) : 0.0;
}

} // namespace AudioTest::SessionSmoke
