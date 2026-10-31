#include "SandboxClient.h"

#include "PluginInstrument.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#if JUCE_LINUX || JUCE_MAC
 #include <unistd.h>
#endif

namespace ihub::plugins
{
namespace
{
struct ParameterAssignment
{
    juce::String name;
    float value = 0.0f;
};

struct ChildArguments
{
    juce::File mapFile;
    juce::File pluginBundle;
    double fakeToneFrequency = 0.0;
    int abortAfterBlocks = -1;
    std::vector<ParameterAssignment> parameters;

    bool valid = false;
    juce::String error;
};

ChildArguments parseArguments (const juce::StringArray& commandLine)
{
    ChildArguments arguments;

    auto takeString = [&commandLine] (int& index, juce::String& target) -> bool
    {
        if (index + 1 >= commandLine.size())
            return false;

        target = commandLine[++index];
        return true;
    };

    for (int i = 1; i < commandLine.size(); ++i)
    {
        const auto& argument = commandLine[i];

        if (argument == sandbox::cli::mapFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.mapFile = juce::File (value).getFullPathName();
        }
        else if (argument == sandbox::cli::pluginFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.pluginBundle = juce::File (value).getFullPathName();
        }
        else if (argument == sandbox::cli::fakeToneFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.fakeToneFrequency = value.getDoubleValue();

            if (arguments.fakeToneFrequency <= 0.0)
            {
                arguments.error = "invalid fake tone frequency: " + value;
                return arguments;
            }
        }
        else if (argument == sandbox::cli::abortAfterFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            arguments.abortAfterBlocks = value.getIntValue();

            if (arguments.abortAfterBlocks < 0)
            {
                arguments.error = "invalid abort-after block count: " + value;
                return arguments;
            }
        }
        else if (argument == sandbox::cli::paramFlag)
        {
            juce::String value;

            if (! takeString (i, value))
            {
                arguments.error = "missing value after " + argument;
                return arguments;
            }

            const auto separator = value.indexOfChar ('=');

            if (separator <= 0 || separator + 1 >= value.length())
            {
                arguments.error = "invalid parameter assignment (expected name=value): " + value;
                return arguments;
            }

            ParameterAssignment assignment;
            assignment.name = value.substring (0, separator).trim();
            assignment.value = value.substring (separator + 1).trim().getFloatValue();
            arguments.parameters.push_back (assignment);
        }
        else
        {
            arguments.error = "unknown argument: " + argument;
            return arguments;
        }
    }

    if (arguments.mapFile == juce::File())
        arguments.error = "no --map path given";
    else if (arguments.pluginBundle == juce::File() && arguments.fakeToneFrequency <= 0.0)
        arguments.error = "no --plugin bundle and no --fake-tone frequency given";

    arguments.valid = arguments.error.isEmpty();
    return arguments;
}

void setState (sandbox::SharedMemory& shared, sandbox::ProcessState state) noexcept
{
    sandbox::storeRelease (shared.state, static_cast<std::int32_t> (state));
}

void setError (sandbox::SharedMemory& shared, const juce::String& message) noexcept
{
    std::snprintf (shared.error, sizeof (shared.error), "%s", message.toRawUTF8());
}

int fail (sandbox::SharedMemory& shared, const juce::String& message, int exitCode)
{
    setError (shared, message);
    setState (shared, sandbox::ProcessState::failed);
    std::cerr << "ihub-sandbox: " << message << std::endl;
    return exitCode;
}

/** The test seam: a fake instrument that needs no VST3.

    It renders a steady tone so the host's shared-memory transport can be
    verified end to end. `--abort-after N` makes the fake plugin call
    `std::abort()` on block N+1, which is exactly the hostile-plugin case the
    sandbox exists for: only this process dies.
*/
class FakeToneInstrument
{
public:
    FakeToneInstrument (double sampleRateToUse, double frequency, int abortAfter)
        : increment (juce::MathConstants<double>::twoPi * frequency / sampleRateToUse),
          abortAfterBlocks (abortAfter)
    {
    }

    int latencySamples() const noexcept { return 0; }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        juce::ignoreUnused (midi);

        if (abortAfterBlocks >= 0 && blocksRendered >= abortAfterBlocks)
            std::abort(); // deliberate: the sandbox must die, not the host

        const auto numSamples = buffer.getNumSamples();
        const auto amplitude = 0.25f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto value = amplitude * static_cast<float> (std::sin (phase));

            phase += increment;

            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, sample, value);
        }

        ++blocksRendered;
    }

private:
    double increment;
    int abortAfterBlocks;
    double phase = 0.0;
    int blocksRendered = 0;
};

bool hostIsAlive (const sandbox::SharedMemory& shared) noexcept
{
   #if JUCE_LINUX || JUCE_MAC
    if (shared.parentProcessId <= 0)
        return true;

    const auto parent = getppid();

    // Reparented to init/subreaper: the engine that owns this mapping is gone.
    return parent != 1 && static_cast<std::int32_t> (parent) == shared.parentProcessId;
   #else
    juce::ignoreUnused (shared);
    return true;
   #endif
}
} // namespace

//==============================================================================
int SandboxClient::run (const juce::StringArray& commandLine)
{
    const auto arguments = parseArguments (commandLine);

    if (! arguments.valid)
    {
        std::cerr << "ihub-sandbox: " << arguments.error << std::endl;
        return 2;
    }

    juce::MemoryMappedFile mapping (arguments.mapFile, juce::MemoryMappedFile::readWrite, false);

    if (mapping.getData() == nullptr || mapping.getSize() < sizeof (sandbox::SharedMemory))
    {
        std::cerr << "ihub-sandbox: could not map " << arguments.mapFile.getFullPathName()
                  << " (expected at least " << sizeof (sandbox::SharedMemory) << " bytes)"
                  << std::endl;
        return 3;
    }

    auto& shared = *static_cast<sandbox::SharedMemory*> (mapping.getData());

    if (shared.magic != sandbox::sharedMemoryMagic || shared.version != sandbox::sharedMemoryVersion)
    {
        std::cerr << "ihub-sandbox: shared-memory magic/version mismatch" << std::endl;
        return 3;
    }

    if (sandbox::loadAcquire (shared.state) == static_cast<std::int32_t> (sandbox::ProcessState::stopping))
        return 0;

    setState (shared, sandbox::ProcessState::loading);

    const auto sampleRate = shared.sampleRate > 0.0 ? shared.sampleRate : 48000.0;
    const auto maximumBlockSize = juce::jlimit (1, sandbox::maximumBlockSize,
                                                static_cast<int> (shared.maximumBlockSize));
    const auto numChannels = juce::jlimit (1, sandbox::maximumChannels,
                                           static_cast<int> (shared.numChannels));

    std::unique_ptr<PluginInstrument> plugin;
    std::unique_ptr<FakeToneInstrument> fake;

    int reportedLatency = 0;

    try
    {
        if (arguments.pluginBundle != juce::File())
        {
            juce::AudioPluginFormatManager formatManager;
            juce::addHeadlessDefaultFormatsToManager (formatManager);

            plugin = std::make_unique<PluginInstrument>();

            juce::String loadError;

            if (! plugin->loadFromFile (formatManager, arguments.pluginBundle,
                                        sampleRate, maximumBlockSize, loadError))
            {
                return fail (shared, "could not load plugin: " + loadError, 4);
            }

            for (const auto& assignment : arguments.parameters)
            {
                if (! plugin->setParameterValue (assignment.name, assignment.value))
                {
                    return fail (shared,
                                 "plugin has no parameter named '" + assignment.name + "'",
                                 4);
                }
            }

            plugin->configureBuses (numChannels);
            plugin->prepare (sampleRate, maximumBlockSize);
            reportedLatency = plugin->latencySamples();
        }
        else
        {
            fake = std::make_unique<FakeToneInstrument> (sampleRate,
                                                         arguments.fakeToneFrequency,
                                                         arguments.abortAfterBlocks);
        }
    }
    catch (const std::exception& exception)
    {
        return fail (shared, juce::String ("exception while loading the plugin: ")
                                + exception.what(),
                     4);
    }
    catch (...)
    {
        return fail (shared, "unknown exception while loading the plugin", 4);
    }

    sandbox::storeRelaxed (shared.latencySamples, static_cast<std::int32_t> (reportedLatency));
    setState (shared, sandbox::ProcessState::ready);

    // Every audio buffer the render loop uses is allocated here, before the
    // first request is answered.
    juce::AudioBuffer<float> renderBuffer (numChannels, maximumBlockSize);
    juce::AudioBuffer<float> view;
    juce::MidiBuffer midi;

    std::uint64_t nextRequest = 0;
    std::uint64_t heartbeat = 0;
    double lastParentCheckMs = juce::Time::getMillisecondCounterHiRes();

    for (;;)
    {
        if (sandbox::loadAcquire (shared.state) == static_cast<std::int32_t> (sandbox::ProcessState::stopping))
            break;

        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (now - lastParentCheckMs >= 250.0)
        {
            lastParentCheckMs = now;

            if (! hostIsAlive (shared))
                break;
        }

        const auto published = sandbox::loadAcquire (shared.requestPublished);

        if (published <= nextRequest)
        {
            // Nothing to render yet. Sleeping keeps the helper off a core while
            // the host's slot is idle; one millisecond is far below a block.
            juce::Thread::sleep (1);
            continue;
        }

        auto& request = shared.requests[nextRequest % sandbox::requestRingSize];

        if (request.sequence != static_cast<std::uint32_t> (nextRequest))
        {
            return fail (shared, "request ring out of sequence", 5);
        }

        const auto numSamples = juce::jlimit (0, maximumBlockSize,
                                              static_cast<int> (request.numSamples));

        view.setDataToReferTo (renderBuffer.getArrayOfWritePointers(),
                               numChannels, numSamples);
        view.clear();
        midi.clear();

        const auto eventCount = juce::jmin (static_cast<int> (request.numMidiEvents),
                                            sandbox::maximumMidiEvents);

        for (int event = 0; event < eventCount; ++event)
        {
            const auto& entry = request.midi[event];

            if (entry.numBytes == 0 || entry.numBytes > 3)
                continue;

            midi.addEvent (entry.bytes, entry.numBytes,
                           juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                         static_cast<int> (entry.sampleOffset)));
        }

        try
        {
            if (plugin != nullptr)
                plugin->process (view, midi);
            else if (fake != nullptr)
                fake->process (view, midi);
        }
        catch (const std::exception& exception)
        {
            return fail (shared, juce::String ("exception in the plugin: ") + exception.what(), 5);
        }
        catch (...)
        {
            return fail (shared, "unknown exception in the plugin", 5);
        }

        auto& reply = shared.replies[nextRequest % sandbox::replyRingSize];
        reply.sequence = static_cast<std::uint32_t> (nextRequest);
        reply.numSamples = numSamples;
        reply.latencySamples = reportedLatency;
        reply.flags = 0;

        for (int channel = 0; channel < numChannels; ++channel)
            juce::FloatVectorOperations::copy (reply.audio[channel],
                                               view.getReadPointer (channel),
                                               numSamples);

        // Publish the reply, then free the request slot for reuse.
        sandbox::storeRelease (shared.replyPublished, nextRequest + 1);
        sandbox::storeRelease (shared.requestConsumed, nextRequest + 1);

        ++nextRequest;
        ++heartbeat;
        sandbox::storeRelaxed (shared.heartbeat, static_cast<std::uint32_t> (heartbeat));
    }

    setState (shared, sandbox::ProcessState::stopping);
    return 0;
}

} // namespace ihub::plugins
