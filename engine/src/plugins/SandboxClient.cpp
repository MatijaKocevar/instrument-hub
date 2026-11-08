#include "SandboxClient.h"

#include "PluginInstrument.h"
#include "PluginWindow.h"
#include "sandbox/FakeToneInstrument.h"
#include "sandbox/SandboxClientArguments.h"
#include "sandbox/SandboxClientControl.h"
#include "sandbox/SandboxClientEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <thread>

#if JUCE_LINUX || JUCE_MAC
 #include <unistd.h>
#endif

#if JUCE_LINUX
 #include <csignal>
 #include <sys/prctl.h>
#endif

namespace ihub::plugins
{
namespace
{
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

bool hostIsAlive (const sandbox::SharedMemory& shared) noexcept
{
   #if JUCE_LINUX || JUCE_MAC
    if (shared.parentProcessId <= 0)
        return true;

    const auto parent = getppid();

    return parent != 1 && static_cast<std::int32_t> (parent) == shared.parentProcessId;
   #else
    juce::ignoreUnused (shared);
    return true;
   #endif
}

bool hasDisplay()
{
    return juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isNotEmpty()
        || juce::SystemStats::getEnvironmentVariable ("WAYLAND_DISPLAY", {}).isNotEmpty();
}

using sandbox_client::consumeEditorCapture;
using sandbox_client::publishEditorState;
}

int SandboxClient::run (const juce::StringArray& commandLine)
{
    const auto arguments = sandbox_client::parseArguments (commandLine);

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

   #if JUCE_LINUX
    prctl (PR_SET_PDEATHSIG, SIGKILL);

    if (shared.parentProcessId > 0
        && static_cast<std::int32_t> (getppid()) != shared.parentProcessId)
        return 0;
   #endif

   #if JUCE_LINUX || JUCE_MAC
    sandbox::storeRelaxed (shared.childProcessId,
                           static_cast<std::int32_t> (getpid()));
   #endif

    setState (shared, sandbox::ProcessState::loading);

    const auto sampleRate = shared.sampleRate > 0.0 ? shared.sampleRate : 48000.0;
    const auto maximumBlockSize = juce::jlimit (1, sandbox::maximumBlockSize,
                                                static_cast<int> (shared.maximumBlockSize));
    const auto numChannels = juce::jlimit (1, sandbox::maximumChannels,
                                           static_cast<int> (shared.numChannels));

    const auto displayAvailable = hasDisplay();
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::unique_ptr<PluginInstrument> plugin;
    std::unique_ptr<sandbox_client::FakeToneInstrument> fake;

    int reportedLatency = 0;

    try
    {
        if (arguments.pluginBundle != juce::File())
        {
            juce::AudioPluginFormatManager formatManager;
            juce::addDefaultFormatsToManager (formatManager);

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
            fake = std::make_unique<sandbox_client::FakeToneInstrument> (sampleRate,
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

    std::unique_ptr<PluginWindow> editorWindow;

    const auto editorControl = [&] (bool open, juce::String& error) -> bool
    {
        if (! displayAvailable)
        {
            error = "no display is available for the plugin editor";
            return false;
        }

        if (plugin == nullptr)
        {
            error = "this sandbox has no plugin editor";
            return false;
        }

        const auto result = juce::MessageManager::callSync ([&] () -> bool
        {
            if (open)
            {
                if (editorWindow != nullptr && editorWindow->isOpen())
                {
                    if (auto* component = editorWindow->getWindowComponent())
                        component->toFront (true);

                    return true;
                }

                if (editorWindow == nullptr)
                {
                    editorWindow = std::make_unique<PluginWindow>();
                    editorWindow->setHideOnClose (true);
                }

                editorWindow->onClose = [&shared]
                {
                    sandbox::storeRelaxed (shared.editorOpen, 0);
                    sandbox::storeRelease (shared.editorCaptureRequested, 1);
                };

                juce::String openError;

                try
                {
                    if (! editorWindow->open (*plugin, {}, nullptr, &openError))
                    {
                        error = openError.isNotEmpty()
                                    ? openError
                                    : juce::String ("the plugin did not create an editor");
                        editorWindow.reset();
                        return false;
                    }
                }
                catch (const std::exception& exception)
                {
                    error = juce::String ("the plugin editor failed: ") + exception.what();
                    editorWindow.reset();
                    return false;
                }

                sandbox::storeRelaxed (shared.editorOpen, 1);
                return true;
            }

            if (editorWindow != nullptr)
                editorWindow->close();

            sandbox::storeRelaxed (shared.editorOpen, 0);
            return true;
        });

        consumeEditorCapture (shared, plugin.get());

        if (! result.has_value())
        {
            error = "the sandbox message loop is not running";
            return false;
        }

        return *result;
    };

    sandbox::storeRelaxed (shared.latencySamples, static_cast<std::int32_t> (reportedLatency));
    setState (shared, sandbox::ProcessState::ready);

    const auto runAudioLoop = [&] () -> int
    {
        const auto stopDispatch = juce::ScopeGuard ([]
        {
            juce::MessageManager::getInstance()->stopDispatchLoop();
        });

        juce::ignoreUnused (stopDispatch);

        juce::AudioBuffer<float> renderBuffer (numChannels, maximumBlockSize);
        juce::AudioBuffer<float> view;
        juce::MidiBuffer midi;

        std::uint64_t nextRequest = 0;
        std::uint64_t heartbeat = 0;
        std::uint32_t lastControlSequence = sandbox::loadAcquire (shared.control.sequence);
        double lastParentCheckMs = juce::Time::getMillisecondCounterHiRes();

        for (;;)
        {
            if (sandbox::loadAcquire (shared.state)
                == static_cast<std::int32_t> (sandbox::ProcessState::stopping))
                break;

            if (sandbox::loadAcquire (shared.editorCaptureRequested) != 0)
            {
                sandbox::storeRelaxed (shared.editorCaptureRequested, 0);

                (void) juce::MessageManager::callAsync ([&shared, plugin = plugin.get()]
                {
                    publishEditorState (shared, plugin);
                });
            }

            const auto controlSequence =
                sandbox::loadAcquire (shared.control.sequence);

            if (controlSequence != lastControlSequence)
            {
                const auto command = static_cast<sandbox::ControlCommand> (
                    sandbox::loadRelaxed (shared.control.command));

                if (command == sandbox::ControlCommand::openEditor
                    || command == sandbox::ControlCommand::closeEditor)
                {
                    lastControlSequence = controlSequence;
                    const bool open = command
                                      == sandbox::ControlCommand::openEditor;

                    (void) juce::MessageManager::callAsync (
                        [&shared, controlSequence, open, editorControl]
                        {
                            sandbox_client::handleEditorControl (
                                shared, controlSequence, open, editorControl);
                        });
                }
                else
                {
                    const auto handled = juce::MessageManager::callSync ([&]
                    {
                        sandbox_client::handleControl (shared,
                                                       plugin.get(),
                                                       lastControlSequence,
                                                       editorControl);
                        return true;
                    });

                    if (! handled.has_value())
                    {
                        juce::Thread::sleep (1);
                        continue;
                    }
                }
            }

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
                juce::Thread::sleep (1);
                continue;
            }

            auto& request = shared.requests[nextRequest % sandbox::requestRingSize];

            if (request.sequence != static_cast<std::uint32_t> (nextRequest))
                return fail (shared, "request ring out of sequence", 5);

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
                return fail (shared,
                             juce::String ("exception in the plugin: ") + exception.what(),
                             5);
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

            sandbox::storeRelease (shared.replyPublished, nextRequest + 1);
            sandbox::storeRelease (shared.requestConsumed, nextRequest + 1);

            ++nextRequest;
            ++heartbeat;
            sandbox::storeRelaxed (shared.heartbeat,
                                   static_cast<std::uint32_t> (heartbeat));
        }

        setState (shared, sandbox::ProcessState::stopping);
        return 0;
    };

    std::thread audioThread (runAudioLoop);
    juce::MessageManager::getInstance()->runDispatchLoop();
    audioThread.join();

    if (editorWindow != nullptr)
    {
        editorWindow->close();
        editorWindow.reset();
    }

    sandbox::storeRelaxed (shared.editorOpen, 0);
    return 0;
}

}
