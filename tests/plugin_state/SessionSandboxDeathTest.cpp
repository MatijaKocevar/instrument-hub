#include "Session.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <unistd.h>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

#ifndef IHUB_SANDBOX_HELPER
#define IHUB_SANDBOX_HELPER ""
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kPluginSlot = 0;
constexpr int kToneSlot = 1;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.neighbourCount = 1;
    config.initialSlot = kPluginSlot;
    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (2);

    auto& plugin = config.slots[static_cast<std::size_t> (kPluginSlot)];
    plugin.kind = ihub::SessionSlotKind::plugin;
    plugin.path = bundlePath;
    plugin.pluginFormat = "VST3";
    plugin.pluginName = "TestPlugin";
    plugin.instrumentKind = "plugin";
    plugin.name = "Sandboxed Patch";
    plugin.patch = "sandboxed-patch";

    auto& tone = config.slots[static_cast<std::size_t> (kToneSlot)];
    tone.kind = ihub::SessionSlotKind::initTone;
    tone.instrumentKind = "init-tone";
    tone.frequency = 220.0;
    tone.name = "Tone";
    tone.patch = "tone";

    return config;
}

double rmsOf (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    int count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            const auto value = static_cast<double> (buffer.getSample (channel, frame));
            sum += value * value;
            ++count;
        }

    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (IHUB_SANDBOX_HELPER[0] != '\0')
        setenv ("IHUB_SANDBOX", IHUB_SANDBOX_HELPER, 1);

    const juce::String bundlePath (IHUB_TESTPLUGIN_VST3);

    if (bundlePath.isEmpty() || ! juce::File (bundlePath).exists())
    {
        std::cerr << "session-sandbox-death-test: FAILED, TestPlugin.vst3 is missing"
                  << std::endl;
        return 1;
    }

    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig (bundlePath), error))
    {
        std::cerr << "session-sandbox-death-test: FAILED to build: " << error
                  << std::endl;
        return 1;
    }

    check (session.waitForSlotWarm (kPluginSlot, 15000),
           "the plugin patch warms in a sandbox");
    check (session.waitForSlotWarm (kToneSlot, 15000),
           "the tone patch warms");

    const auto hostPid = session.getSandboxProcessId (kPluginSlot);
    check (hostPid > 0, "the plugin slot runs in a helper process");

    session.armSlot (kPluginSlot);
    check (session.waitForSlotActive (kPluginSlot, 5000),
           "the sandboxed patch arms");

    std::atomic<bool> stop { false };
    std::atomic<int> rendered { 0 };

    std::thread renderer ([&session, &stop, &rendered]
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;

        while (! stop.load (std::memory_order_relaxed))
        {
            buffer.clear();
            midi.clear();
            session.render (buffer, midi, kBlockSize);
            rendered.fetch_add (1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    const auto renderedDeadline = juce::Time::getMillisecondCounterHiRes() + 5000.0;

    while (rendered.load (std::memory_order_relaxed) < 16
           && juce::Time::getMillisecondCounterHiRes() < renderedDeadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (2));

    check (rendered.load (std::memory_order_relaxed) >= 16,
           "the engine renders blocks before the host dies");
    check (hostPid > 0 && ::kill (hostPid, SIGKILL) == 0,
           "the plugin host process was killed");

    const auto failureDeadline = juce::Time::getMillisecondCounterHiRes() + 5000.0;

    while (! session.isSlotPluginFailed (kPluginSlot)
           && juce::Time::getMillisecondCounterHiRes() < failureDeadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (2));

    stop.store (true, std::memory_order_relaxed);
    renderer.join();

    check (session.isSlotPluginFailed (kPluginSlot),
           "the slot reports the dead plugin host");
    check (! session.getSlotError (kPluginSlot).isEmpty (),
           "the slot exposes a failure reason: '"
               + session.getSlotError (kPluginSlot).toStdString() + "'");

    session.armSlot (kToneSlot);
    check (session.waitForSlotActive (kToneSlot, 5000),
           "the remaining patch still arms after the host died");

    juce::AudioBuffer<float> output (2, kBlockSize);
    juce::MidiBuffer midi;
    double peakRms = 0.0;

    for (int block = 0; block < 48; ++block)
    {
        output.clear();
        midi.clear();
        session.render (output, midi, kBlockSize);
        peakRms = juce::jmax (peakRms, rmsOf (output));
        juce::Thread::sleep (1);
    }

    check (peakRms > 0.03,
           "the engine keeps rendering other slots (RMS "
               + std::to_string (peakRms) + ")");
    check (! session.isSlotPluginFailed (kToneSlot),
           "the healthy slot did not inherit the failure");

    if (failures > 0)
    {
        std::cerr << "session-sandbox-death-test: " << failures
                  << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "session-sandbox-death-test: all checks passed" << std::endl;
    return 0;
}
