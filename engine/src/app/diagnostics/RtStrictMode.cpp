#include "app/DiagnosticModes.h"

#include "app/diagnostics/RtProbeSource.h"

#include "audio/AudioEngine.h"
#include "audio/RtGuard.h"

#include <iostream>

namespace ihub::app
{

int runRtStrictCheck (const juce::String& commandLine)
{
    if (! AudioEngine::RtGuard::isCompiledIn())
    {
        std::cerr << "rt-strict: FAILED, the realtime guard is compiled out of this "
                     "build (configure with -DIHUB_ENABLE_RT_GUARD=ON)" << std::endl;
        return 2;
    }

    auto injection = detail::RtInjection::none;

    if (commandLine.contains ("--rt-inject-allocation"))
        injection = detail::RtInjection::allocation;
    else if (commandLine.contains ("--rt-inject-lock"))
        injection = detail::RtInjection::lock;
    else if (commandLine.contains ("--rt-inject-log"))
        injection = detail::RtInjection::log;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int measuredBlocks = 200;

    AudioEngine::RtGuard::resetCounters();
    AudioEngine::RtGuard::setEnabled (true);
    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::installLogProbe();

    AudioEngine::AudioEngine engine (2, 2);
    detail::RtProbeSource source (injection);

    engine.getSlot (0).setSource (&source);
    engine.getSlot (0).activate();

    engine.prepare (sampleRate, blockSize, 2);

    juce::AudioBuffer<float> output (2, blockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < 4; ++block)
        engine.processBlock (output, midi, blockSize);

    const auto warmupCounters = AudioEngine::RtGuard::getCounters();

    AudioEngine::RtGuard::resetCounters();
    source.setArmed (true);
    AudioEngine::RtGuard::setStrict (true);

    if (injection != detail::RtInjection::none)
        std::cout << "rt-strict: injecting " << detail::toString (injection)
                  << ", expecting an abort" << std::endl;

    for (int block = 0; block < measuredBlocks; ++block)
        engine.processBlock (output, midi, blockSize);

    const auto measuredCounters = AudioEngine::RtGuard::getCounters();
    const bool balanced = AudioEngine::RtGuard::assertNoOpenScopes();
    const int openScopes = AudioEngine::RtGuard::getOpenScopeCount();

    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::uninstallLogProbe();

    const auto total = warmupCounters.total() + measuredCounters.total();
    const bool passed = total == 0 && balanced && openScopes == 0;

    std::cout << "rt-strict: blocks=" << measuredBlocks
              << " warmup_violations=" << warmupCounters.total()
              << " violations=" << measuredCounters.total()
              << " (alloc=" << measuredCounters.allocations
              << " lock=" << measuredCounters.locks
              << " log=" << measuredCounters.logs
              << " scope=" << measuredCounters.scopeErrors << ")"
              << " open_scopes=" << openScopes
              << " " << (passed ? "PASS" : "FAIL") << std::endl;

    return passed ? 0 : 1;
}

}
