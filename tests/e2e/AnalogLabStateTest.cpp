// ihub-analog-lab-state-test: real-plugin acceptance for T131.
//
// Two patches host the installed Analog Lab V through its VST3 bridge. The
// test uses the normal asynchronous warm pool: it never enables a synchronous
// loading mode, never creates a plugin on the caller's thread, and never opens
// an editor. A process never holds more than two Analog Lab instances.
//
// The test runs in two child processes so that "restart" really is a process
// restart, not two sessions sharing one Wine client:
//
//   --first    builds two patches, gives them two different parameter states,
//              captures both blobs into the sidecar, switches A -> B -> A,
//              renders a held note on each patch and writes the two hashes.
//   --restart  is a fresh process. It loads the sidecar, verifies both state
//              hashes and repeats the non-silent/different audio check.
//
// The parent only orchestrates those two phases and writes a measurement
// record. The full 64-pad pass is the manual checklist in
// docs/manual-analog-lab-64.md.
//
// Skips with exit code 77 only when no Analog Lab V bundle is installed.

#include "Session.h"
#include "plugins/PluginInstrument.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
 #include <sys/wait.h>
#endif

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 2;
constexpr int kSkipReturnCode = 77;
constexpr int kNeighbourCount = 1; // warm limit 2, exactly the two patches
constexpr int kArmTimeoutMs = 5000;
constexpr int kWarmTimeoutMs = 20000; // Wine/vstbridge startup can exceed 5 s
constexpr int kChildTimeoutSeconds = 35;
constexpr int kSettleBlocks = 24;
constexpr int kRenderBlocks = 400; // ~1.07 s at 48 kHz / 128
constexpr int kTailBlocks = 16;
constexpr double kSilenceFloor = 1.0e-4;
constexpr double kMinimumRelativeDifference = 0.05;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

/** An Analog Lab V bundle, if this machine has one (native, vstbridge or
    yabridge). IHUB_ANALOG_LAB_VST3 overrides the search. */
juce::File findAnalogLabBundle()
{
    if (const auto* env = std::getenv ("IHUB_ANALOG_LAB_VST3");
        env != nullptr && *env != '\0')
    {
        const juce::File candidate { juce::String (env) };
        return candidate.exists() ? candidate : juce::File();
    }

    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const juce::Array<juce::File> candidates {
        juce::File ("/usr/lib/vst3/Analog Lab V.vst3"),
        juce::File ("/usr/local/lib/vst3/Analog Lab V.vst3"),
        home.getChildFile (".vst3/Analog Lab V.vst3"),
        juce::File ("/usr/lib/vst3/vstbridge/Analog Lab V.vst3"),
        home.getChildFile (".vst3/vstbridge/Analog Lab V.vst3"),
        juce::File ("/usr/lib/vst3/yabridge/Analog Lab V.vst3"),
        home.getChildFile (".vst3/yabridge/Analog Lab V.vst3"),
    };

    for (const auto& candidate : candidates)
        if (candidate.exists())
            return candidate;

    return {};
}

/** Stable identity hash for an opaque plugin blob (FNV-1a 64). */
std::string hashOf (const juce::MemoryBlock& data)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto* bytes = static_cast<const std::uint8_t*> (data.getData());

    for (std::size_t index = 0; index < data.getSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }

    char text[32] {};
    std::snprintf (text, sizeof (text), "%016llx",
                   static_cast<unsigned long long> (hash));
    return text;
}

/** Pumps the JUCE message loop once. JUCE posts plugin creation there, so the
    loop must keep running while a warm load is in flight. */
void pumpMessages()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

void pumpFor (int milliseconds)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (milliseconds);

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
        pumpMessages();
}

/** Calls `armSlot(slot)` and pumps until that slot is active or the 5 s
    deadline expires. On timeout the failure names the observed warm state. */
bool armAndWait (ihub::Session& session, int slot)
{
    session.armSlot (slot);

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (kArmTimeoutMs);

    if (session.getWarmState (slot) == ihub::SessionWarmState::active)
        return true;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        pumpMessages();

        if (session.getWarmState (slot) == ihub::SessionWarmState::active)
            return true;
    }

    check (false,
           "slot " + std::to_string (slot) + " did not become active within "
               + std::to_string (kArmTimeoutMs) + " ms (state: "
               + session.getWarmStateName (slot) + ")");
    return false;
}

/** Waits until every slot's instance is resident, with the same bounded pump.
    This keeps a background load from running while the audio probe renders. */
bool waitForAllWarm (ihub::Session& session, int slotCount)
{
    const double deadline = juce::Time::getMillisecondCounterHiRes()
                          + static_cast<double> (kWarmTimeoutMs);

    for (;;)
    {
        bool allWarm = true;

        for (int slot = 0; slot < slotCount; ++slot)
            allWarm = session.isSlotWarm (slot) && allWarm;

        if (allWarm)
            return true;

        if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            break;

        pumpMessages();
    }

    std::string states;

    for (int slot = 0; slot < slotCount; ++slot)
    {
        if (! states.empty())
            states += ", ";

        states += "slot " + std::to_string (slot) + "="
                + session.getWarmStateName (slot);
    }

    check (false, "not every Analog Lab patch warmed within "
                      + std::to_string (kWarmTimeoutMs) + " ms (" + states + ")");
    return false;
}

double toDbfs (double linear)
{
    return linear > 0.0 ? 20.0 * std::log10 (linear) : -1000.0;
}

/** Waits until two consecutive reads agree after reloading the candidate into
    the live instance. Only one Analog Lab instance is alive at this point. */
bool stabiliseState (ihub::plugins::PluginInstrument& instrument,
                     juce::MemoryBlock& result)
{
    juce::MemoryBlock candidate = instrument.getStateInformation();

    for (int attempt = 0; attempt < 6; ++attempt)
    {
        if (candidate.getSize() == 0)
            return false;

        (void) instrument.setStateInformation (candidate);
        pumpFor (250);

        auto next = instrument.getStateInformation();

        if (next.getSize() == 0)
            return false;

        if (next == candidate)
        {
            result = candidate;
            return true;
        }

        candidate = std::move (next);
    }

    result = candidate;
    return result.getSize() > 0;
}

bool verifySlotState (ihub::Session& session,
                      int slot,
                      const std::string& expectedHash,
                      const std::string& phase)
{
    auto* instrument = session.getPluginInstrument (slot);

    if (instrument == nullptr)
    {
        check (false, phase + ": slot " + std::to_string (slot)
                          + " has no live Analog Lab V instance");
        return false;
    }

    const auto liveHash = hashOf (instrument->getStateInformation());
    const bool matches = liveHash == expectedHash;

    check (matches,
           phase + ": slot " + std::to_string (slot) + " live state hash "
               + liveHash + " matches " + expectedHash);

    return matches;
}

ihub::SessionConfig makeConfig (const juce::String& bundlePath)
{
    ihub::SessionConfig config;
    config.device.sampleRate = kSampleRate;
    config.device.bufferSize = kBlockSize;
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;
    config.fadeMilliseconds = 3.0f;
    config.neighbourCount = kNeighbourCount;
    config.initialSlot = 0;

    config.slots = ihub::Session::makeDefaultSet (ihub::SessionSlotKind::empty);
    config.slots.resize (kSlots);

    for (int slot = 0; slot < kSlots; ++slot)
    {
        auto& slotConfig = config.slots[static_cast<std::size_t> (slot)];
        slotConfig.kind = ihub::SessionSlotKind::plugin;
        slotConfig.path = bundlePath;
        slotConfig.pluginFormat = "VST3";
        slotConfig.pluginName = "Analog Lab V";
        slotConfig.pluginManufacturer = "Arturia";
        slotConfig.instrumentKind = "plugin";
        slotConfig.name = slot == 0 ? "Analog Lab A" : "Analog Lab B";
        slotConfig.patch = slot == 0 ? "analog-lab-a" : "analog-lab-b";
        slotConfig.patchName = slotConfig.name;
        slotConfig.family = slot == 0 ? "keys" : "pads";
        slotConfig.familyName = slot == 0 ? "Keys" : "Pads";
    }

    return config;
}

juce::File makeTempRoot()
{
    const juce::File approvedBase ("/tmp/opencode");
    const auto base = approvedBase.isDirectory()
                          ? approvedBase
                          : juce::File::getSpecialLocation (
                                juce::File::tempDirectory);

    return base.getChildFile ("ihub-t131-analog-"
                              + juce::String::toHexString (
                                    juce::Random::getSystemRandom().nextInt64()));
}

bool buildSession (ihub::Session& session,
                   const ihub::SessionConfig& config,
                   const juce::File& stateDirectory)
{
    session.setPluginStateDirectory (stateDirectory);

    juce::String error;

    if (! session.build (config, error))
    {
        std::cerr << "FAIL analog-lab-state: session build failed: " << error
                  << std::endl;
        return false;
    }

    return true;
}

struct AudioProbe
{
    double rms = 0.0;
    double peak = 0.0;
    std::vector<float> samples;
};

/** Renders a held note on the armed patch and returns the mono (left) signal. */
AudioProbe renderNote (ihub::Session& session, int slot)
{
    AudioProbe probe;

    if (session.getPluginInstrument (slot) == nullptr)
        return probe;

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer emptyMidi;

    for (int block = 0; block < kSettleBlocks; ++block)
    {
        buffer.clear();
        emptyMidi.clear();
        session.render (buffer, emptyMidi, kBlockSize);
    }

    check (session.pushMidi (juce::MidiMessage::noteOn (1, 60, 0.9f)),
           "a note-on is queued for slot " + std::to_string (slot));

    double sumSquares = 0.0;
    int frames = 0;

    for (int block = 0; block < kRenderBlocks; ++block)
    {
        buffer.clear();
        emptyMidi.clear();
        session.render (buffer, emptyMidi, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const float sample = buffer.getSample (0, frame);
            const auto value = static_cast<double> (sample);
            sumSquares += value * value;
            probe.peak = juce::jmax (probe.peak, std::abs (value));
            probe.samples.push_back (sample);
            ++frames;
        }
    }

    probe.rms = frames > 0 ? std::sqrt (sumSquares / static_cast<double> (frames))
                           : 0.0;

    check (session.pushMidi (juce::MidiMessage::noteOff (1, 60)),
           "a note-off is queued for slot " + std::to_string (slot));

    for (int block = 0; block < kTailBlocks; ++block)
    {
        buffer.clear();
        emptyMidi.clear();
        session.render (buffer, emptyMidi, kBlockSize);
    }

    return probe;
}

double relativeDifference (const AudioProbe& first, const AudioProbe& second)
{
    const auto frames = juce::jmin (first.samples.size(), second.samples.size());

    if (frames == 0)
        return 0.0;

    double difference = 0.0;

    for (std::size_t index = 0; index < frames; ++index)
    {
        const auto delta = static_cast<double> (first.samples[index])
                         - static_cast<double> (second.samples[index]);
        difference += delta * delta;
    }

    const double differenceRms = std::sqrt (difference / static_cast<double> (frames));
    const double reference = juce::jmax (first.rms, second.rms);
    return reference > 0.0 ? differenceRms / reference : 0.0;
}

struct Measurement
{
    AudioProbe audioA;
    AudioProbe audioB;
    double difference = 0.0;
    int peakLiveInstances = 0;
};

bool measurePass (ihub::Session& session,
                  const std::string& hashA,
                  const std::string& hashB,
                  const std::string& phase,
                  Measurement& measurement)
{
    if (! armAndWait (session, 0))
        return false;

    check (verifySlotState (session, 0, hashA, phase + " A"),
           phase + ": patch A keeps its state hash");

    measurement.audioA = renderNote (session, 0);

    if (! armAndWait (session, 1))
        return false;

    check (verifySlotState (session, 1, hashB, phase + " B"),
           phase + ": patch B keeps its state hash");

    measurement.audioB = renderNote (session, 1);

    // The explicit return leg: A -> B -> A.
    if (! armAndWait (session, 0))
        return false;

    check (verifySlotState (session, 0, hashA, phase + " A return"),
           phase + ": patch A keeps its state hash after A -> B -> A");

    measurement.difference = relativeDifference (measurement.audioA,
                                                 measurement.audioB);
    measurement.peakLiveInstances = session.getPeakInstrumentCount();

    std::cout << phase << " AUDIO A: rms=" << measurement.audioA.rms << " ("
              << toDbfs (measurement.audioA.rms) << " dBFS), peak="
              << measurement.audioA.peak << std::endl;
    std::cout << phase << " AUDIO B: rms=" << measurement.audioB.rms << " ("
              << toDbfs (measurement.audioB.rms) << " dBFS), peak="
              << measurement.audioB.peak << std::endl;
    std::cout << phase << " AUDIO relative difference: "
              << measurement.difference << std::endl;

    bool ok = true;

    auto expect = [&ok, &phase] (bool condition, const std::string& what)
    {
        check (condition, phase + ": " + what);
        ok = ok && condition;
    };

    expect (measurement.audioA.rms > kSilenceFloor,
            "patch A renders a non-silent note");
    expect (measurement.audioB.rms > kSilenceFloor,
            "patch B renders a non-silent note");
    expect (measurement.audioA.peak > 1.0e-3 && measurement.audioB.peak > 1.0e-3,
            "both patches have a usable peak level");
    expect (measurement.difference > kMinimumRelativeDifference,
            "the two patch renders differ (relative difference > "
                + std::to_string (kMinimumRelativeDifference) + ")");
    expect (measurement.peakLiveInstances <= 1 + kNeighbourCount,
            "at most two Analog Lab instances were alive (peak "
                + std::to_string (measurement.peakLiveInstances) + ")");

    return ok;
}

bool writeHashes (const juce::File& file,
                  const std::string& hashA,
                  const std::string& hashB)
{
    return file.replaceWithText (juce::String (hashA) + "\n" + juce::String (hashB)
                                 + "\n");
}

bool readHashes (const juce::File& file, std::string& hashA, std::string& hashB)
{
    if (! file.existsAsFile())
        return false;

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    lines.removeEmptyStrings();

    if (lines.size() < 2)
        return false;

    hashA = lines[0].toStdString();
    hashB = lines[1].toStdString();
    return hashA.size() == 16 && hashB.size() == 16;
}

/** First child process: capture two states, switch A -> B -> A, render audio. */
int runFirstPhase (const juce::File& stateDirectory, const juce::File& hashFile)
{
    const auto bundle = findAnalogLabBundle();

    if (! bundle.exists())
    {
        std::cout << "SKIP ihub-analog-lab-state: Analog Lab V is not installed"
                  << std::endl;
        return kSkipReturnCode;
    }

    const auto config = makeConfig (bundle.getFullPathName());
    juce::MemoryBlock stateA;
    juce::MemoryBlock stateB;
    std::string hashA;
    std::string hashB;
    Measurement first;

    {
        ihub::Session session;

        if (! buildSession (session, config, stateDirectory))
            return 1;

        if (! waitForAllWarm (session, kSlots))
            return 1;

        //----------------------------------------------------------------------
        // Two distinct, captured Analog Lab states.

        if (! armAndWait (session, 0))
            return 1;

        auto* instrumentA = session.getPluginInstrument (0);
        check (instrumentA != nullptr, "patch A has a live Analog Lab V instance");

        if (instrumentA == nullptr)
            return 1;

        check (instrumentA->setParameterValue ("Master", 0.85f),
               "patch A has an Analog Lab Master parameter");
        check (instrumentA->setParameterValue ("P1 VCF Cutoff", 0.25f),
               "patch A has an Analog Lab filter cutoff parameter");
        check (instrumentA->setParameterValue ("P1 Brightness", 0.5f),
               "patch A has an Analog Lab brightness parameter");
        pumpFor (300);
        check (stabiliseState (*instrumentA, stateA),
               "patch A state blob stabilised");
        hashA = hashOf (stateA);
        check (stateA.getSize() > 0,
               "patch A captured a non-empty state ("
                   + std::to_string (stateA.getSize()) + " bytes, hash "
                   + hashA + ")");
        check (session.capturePluginState (0),
               "patch A state was written to the sidecar");

        if (! armAndWait (session, 1))
            return 1;

        auto* instrumentB = session.getPluginInstrument (1);
        check (instrumentB != nullptr, "patch B has a live Analog Lab V instance");

        if (instrumentB == nullptr)
            return 1;

        check (instrumentB->setParameterValue ("Master", 0.35f),
               "patch B has an Analog Lab Master parameter");
        check (instrumentB->setParameterValue ("P1 VCF Cutoff", 0.85f),
               "patch B has an Analog Lab filter cutoff parameter");
        check (instrumentB->setParameterValue ("P1 Brightness", 0.8f),
               "patch B has an Analog Lab brightness parameter");
        pumpFor (300);
        check (stabiliseState (*instrumentB, stateB),
               "patch B state blob stabilised");
        hashB = hashOf (stateB);
        check (stateB.getSize() > 0,
               "patch B captured a non-empty state ("
                   + std::to_string (stateB.getSize()) + " bytes, hash "
                   + hashB + ")");
        check (stateA != stateB && hashA != hashB,
               "the two Analog Lab states are distinct (" + hashA + " vs "
                   + hashB + ")");
        check (session.capturePluginState (1),
               "patch B state was written to the sidecar");
        check (session.savePluginStates(), "the sidecar saved explicitly");

        check (measurePass (session, hashA, hashB, "first pass", first),
               "the first session renders and distinguishes both patches");

        check (first.peakLiveInstances <= 1 + kNeighbourCount,
               "the first process held at most two Analog Lab instances");
    }

    check (failures == 0, "the first phase completed without failures");

    if (failures == 0)
        check (writeHashes (hashFile, hashA, hashB),
               "the expected state hashes were written for the restart phase");

    return failures == 0 ? 0 : 1;
}

/** Second child process: fresh session from the sidecar, hashes and audio. */
int runRestartPhase (const juce::File& stateDirectory,
                     const std::string& hashA,
                     const std::string& hashB)
{
    const auto bundle = findAnalogLabBundle();

    if (! bundle.exists())
    {
        std::cout << "SKIP ihub-analog-lab-state: Analog Lab V is not installed"
                  << std::endl;
        return kSkipReturnCode;
    }

    const auto config = makeConfig (bundle.getFullPathName());
    Measurement restarted;

    {
        ihub::Session session;

        if (! buildSession (session, config, stateDirectory))
            return 1;

        if (! waitForAllWarm (session, kSlots))
            return 1;

        check (session.hasPluginState (0) && session.hasPluginState (1),
               "the Analog Lab sidecar is loaded at startup");

        check (measurePass (session, hashA, hashB, "restart", restarted),
               "the restarted session renders and distinguishes both patches");

        check (restarted.peakLiveInstances <= 1 + kNeighbourCount,
               "the restarted process held at most two Analog Lab instances");
    }

    check (failures == 0, "the restart phase completed without failures");
    return failures == 0 ? 0 : 1;
}

void printChildSummary (const juce::String& output)
{
    juce::StringArray lines;
    lines.addLines (output);

    for (const auto& line : lines)
    {
        if (line.contains ("ok  :") || line.contains ("FAIL:")
            || line.contains ("PASS:") || line.contains ("AUDIO")
            || line.contains ("SKIP") || line.contains ("stack overflow"))
        {
            std::cout << line.toStdString() << std::endl;
        }
    }
}

int runChild (const juce::File& executable,
              const juce::StringArray& arguments,
              const juce::File& logFile,
              bool printSummary)
{
    auto quote = [] (const juce::String& value)
    {
        return "\"" + value.replace ("\"", "\\\"") + "\"";
    };

    juce::String command = "/usr/bin/timeout --kill-after=5 "
                         + juce::String (kChildTimeoutSeconds) + " "
                         + quote (executable.getFullPathName());

    for (const auto& argument : arguments)
        command += " " + quote (argument);

    command += " > " + quote (logFile.getFullPathName()) + " 2>&1";

    const int status = std::system (command.toRawUTF8());
    int exitCode = -1;

   #if defined(__linux__)
    if (WIFEXITED (status))
        exitCode = WEXITSTATUS (status);
    else if (WIFSIGNALED (status))
        exitCode = 128 + WTERMSIG (status);
   #else
    exitCode = status;
   #endif

    const auto output = logFile.existsAsFile() ? logFile.loadFileAsString()
                                               : juce::String();

    if (printSummary)
        printChildSummary (output);
    else if (exitCode != 0)
        std::cout << output.toStdString() << std::endl;

    return exitCode;
}

int runChildWithRetry (const juce::File& executable,
                       const juce::StringArray& arguments,
                       const juce::File& logFile,
                       const juce::String& label)
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const int exitCode = runChild (executable, arguments, logFile, true);

        if (exitCode != 124)
            return exitCode;

        std::cout << "note: " << label << " timed out after "
                  << kChildTimeoutSeconds << " s; retrying once" << std::endl;
    }

    return 124;
}

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
               << "status=" << (failures == 0 ? "PASS" : "FAIL") << "\n";

        check (record.good(),
               "the measurement record was written "
               "(ihub-analog-lab-state-record.txt)");
    }

    (void) tempRoot.deleteRecursively();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc == 4 && juce::String (argv[1]) == "--first")
        return runFirstPhase (juce::File (juce::String (argv[2])),
                              juce::File (juce::String (argv[3])));

    if (argc == 5 && juce::String (argv[1]) == "--restart")
        return runRestartPhase (juce::File (juce::String (argv[2])),
                                std::string (argv[3]),
                                std::string (argv[4]));

    return runParent();
}
