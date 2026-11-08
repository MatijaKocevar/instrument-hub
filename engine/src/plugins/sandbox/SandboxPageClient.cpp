#include "SandboxPageClient.h"

#include "../PluginInstrument.h"
#include "../PluginWindow.h"
#include "FakeToneInstrument.h"
#include "SandboxClientArguments.h"
#include "SandboxClientControl.h"
#include "SandboxClientEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <vector>

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
void setSlotState (sandbox::SharedMemory& shared, sandbox::ProcessState state) noexcept
{
    sandbox::storeRelease (shared.state, static_cast<std::int32_t> (state));
}

void setSlotError (sandbox::SharedMemory& shared, const juce::String& message) noexcept
{
    std::snprintf (shared.error, sizeof (shared.error), "%s", message.toRawUTF8());
}

sandbox::ProcessState pageState (sandbox::SharedPage& page) noexcept
{
    return static_cast<sandbox::ProcessState> (sandbox::loadAcquire (page.state));
}

void setPageState (sandbox::SharedPage& page, sandbox::ProcessState state) noexcept
{
    sandbox::storeRelease (page.state, static_cast<std::int32_t> (state));
}

void setPageError (sandbox::SharedPage& page, const juce::String& message) noexcept
{
    std::snprintf (page.error, sizeof (page.error), "%s", message.toRawUTF8());
}

bool hostIsAlive (const sandbox::SharedPage& page) noexcept
{
   #if JUCE_LINUX || JUCE_MAC
    if (page.parentProcessId <= 0)
        return true;

    const auto parent = getppid();

    return parent != 1 && static_cast<std::int32_t> (parent) == page.parentProcessId;
   #else
    juce::ignoreUnused (page);
    return true;
   #endif
}

bool hasDisplay()
{
    return juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isNotEmpty()
        || juce::SystemStats::getEnvironmentVariable ("WAYLAND_DISPLAY", {}).isNotEmpty();
}

struct SlotRuntime
{
    std::unique_ptr<PluginInstrument> plugin;
    std::unique_ptr<sandbox_client::FakeToneInstrument> fake;
    std::unique_ptr<PluginWindow> editor;
    int latencySamples = 0;
    int abortAfterBlocks = -1;
    std::vector<sandbox_client::ParameterAssignment> parameters;
    double fakeToneFrequency = 0.0;
};
}

int SandboxPageClient::run (const juce::StringArray& commandLine)
{
    const auto arguments = sandbox_client::parseArguments (commandLine);

    if (! arguments.valid || ! arguments.pageMode)
    {
        std::cerr << "ihub-sandbox: " << (arguments.error.isNotEmpty()
                                             ? arguments.error
                                             : juce::String ("invalid page arguments"))
                  << std::endl;
        return 2;
    }

    juce::MemoryMappedFile mapping (arguments.pageMapFile,
                                    juce::MemoryMappedFile::readWrite,
                                    false);

    if (mapping.getData() == nullptr || mapping.getSize() < sizeof (sandbox::SharedPage))
    {
        std::cerr << "ihub-sandbox: could not map page " << arguments.pageMapFile.getFullPathName()
                  << " (expected at least " << sizeof (sandbox::SharedPage) << " bytes)"
                  << std::endl;
        return 3;
    }

    auto& page = *static_cast<sandbox::SharedPage*> (mapping.getData());

    if (page.magic != sandbox::sharedPageMagic || page.version != sandbox::sharedPageVersion
        || page.numSlots <= 0 || page.numSlots > sandbox::maximumPageSlots)
    {
        std::cerr << "ihub-sandbox: shared-page magic/version/slot mismatch" << std::endl;
        return 3;
    }

    if (pageState (page) == sandbox::ProcessState::stopping)
        return 0;

   #if JUCE_LINUX
    prctl (PR_SET_PDEATHSIG, SIGKILL);

    if (page.parentProcessId > 0
        && static_cast<std::int32_t> (getppid()) != page.parentProcessId)
        return 0;
   #endif

   #if JUCE_LINUX || JUCE_MAC
    page.childProcessId = static_cast<std::int32_t> (getpid());
   #endif

    setPageState (page, sandbox::ProcessState::loading);

    const auto sampleRate = page.slots[0].sampleRate > 0.0 ? page.slots[0].sampleRate : 48000.0;
    const auto maximumBlockSize = juce::jlimit (1, sandbox::maximumBlockSize,
                                                static_cast<int> (page.slots[0].maximumBlockSize));
    const auto numChannels = juce::jlimit (1, sandbox::maximumChannels,
                                           static_cast<int> (page.slots[0].numChannels));

    const auto displayAvailable = hasDisplay();
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::vector<SlotRuntime> runtimes (static_cast<std::size_t> (page.numSlots));
    juce::AudioPluginFormatManager formatManager;
    juce::addDefaultFormatsToManager (formatManager);

    const auto failPage = [&page] (const juce::String& message, int exitCode)
    {
        setPageError (page, message);
        setPageState (page, sandbox::ProcessState::failed);
        std::cerr << "ihub-sandbox: " << message << std::endl;
        return exitCode;
    };

    std::map<juce::String, juce::PluginDescription> descriptions;

    const auto findDescription = [&] (const juce::File& bundle,
                                      juce::PluginDescription& description,
                                      juce::String& loadError) -> bool
    {
        const auto path = bundle.getFullPathName();
        const auto cached = descriptions.find (path);

        if (cached != descriptions.end())
        {
            description = cached->second;
            return true;
        }

        juce::OwnedArray<juce::PluginDescription> types;

        for (auto* format : formatManager.getFormats())
            if (format->fileMightContainThisPluginType (path))
                format->findAllTypesForFile (types, path);

        if (types.isEmpty())
        {
            loadError = "no plugin types found in " + path;
            return false;
        }

        description = *types.getFirst();
        descriptions.emplace (path, description);
        return true;
    };

    for (const auto& slotArguments : arguments.pageSlots)
    {
        if (slotArguments.slotIndex < 0 || slotArguments.slotIndex >= page.numSlots)
            return failPage ("page slot " + juce::String (slotArguments.slotIndex)
                                 + " is outside the page",
                             4);

        auto& runtime = runtimes[static_cast<std::size_t> (slotArguments.slotIndex)];
        runtime.parameters = slotArguments.parameters;
        runtime.abortAfterBlocks = slotArguments.abortAfterBlocks;
        runtime.fakeToneFrequency = slotArguments.fakeToneFrequency;
        auto& shared = page.slots[slotArguments.slotIndex];
        setSlotState (shared, sandbox::ProcessState::loading);

        try
        {
            if (slotArguments.pluginBundle != juce::File())
            {
                juce::PluginDescription description;
                juce::String loadError;

                if (! findDescription (slotArguments.pluginBundle, description, loadError))
                {
                    return failPage ("slot " + juce::String (slotArguments.slotIndex)
                                         + " could not find plugin types: " + loadError,
                                     4);
                }

                runtime.plugin = std::make_unique<PluginInstrument>();

                if (! runtime.plugin->load (formatManager,
                                            description,
                                            sampleRate,
                                            maximumBlockSize,
                                            loadError))
                {
                    return failPage ("slot " + juce::String (slotArguments.slotIndex)
                                         + " could not load plugin: " + loadError,
                                     4);
                }

                for (const auto& assignment : slotArguments.parameters)
                {
                    if (! runtime.plugin->setParameterValue (assignment.name,
                                                             assignment.value))
                    {
                        return failPage ("slot " + juce::String (slotArguments.slotIndex)
                                             + " has no parameter named '"
                                             + assignment.name + "'",
                                         4);
                    }
                }

                runtime.plugin->configureBuses (numChannels);
                runtime.plugin->prepare (sampleRate, maximumBlockSize);
                runtime.latencySamples = runtime.plugin->latencySamples();
            }
            else if (slotArguments.fakeToneFrequency > 0.0)
            {
                runtime.fake = std::make_unique<sandbox_client::FakeToneInstrument> (
                    sampleRate,
                    slotArguments.fakeToneFrequency,
                    slotArguments.abortAfterBlocks);
            }
            else
            {
                return failPage ("page slot " + juce::String (slotArguments.slotIndex)
                                     + " has no plugin and no fake tone",
                                 4);
            }
        }
        catch (const std::exception& exception)
        {
            return failPage ("slot " + juce::String (slotArguments.slotIndex)
                                 + " threw while loading: " + exception.what(),
                             4);
        }
        catch (...)
        {
            return failPage ("slot " + juce::String (slotArguments.slotIndex)
                                 + " threw an unknown exception while loading",
                             4);
        }

        shared.latencySamples = static_cast<std::int32_t> (runtime.latencySamples);
        setSlotState (shared, sandbox::ProcessState::ready);
    }

    for (int slot = 0; slot < page.numSlots; ++slot)
    {
        if (runtimes[static_cast<std::size_t> (slot)].plugin == nullptr
            && runtimes[static_cast<std::size_t> (slot)].fake == nullptr)
        {
            setSlotState (page.slots[slot], sandbox::ProcessState::ready);
        }
    }

    std::atomic<bool> failed { false };
    std::atomic<int> runningThreads { page.numSlots };
    std::vector<std::thread> threads;
    threads.reserve (static_cast<std::size_t> (page.numSlots));

    const auto stopMessageLoop = []
    {
        juce::MessageManager::getInstance()->stopDispatchLoop();
    };

    const auto markPageFailed = [&page, &failed] (const juce::String& message)
    {
        if (failed.exchange (true))
            return;

        setPageError (page, message);

        for (int slot = 0; slot < page.numSlots; ++slot)
            if (page.slots[slot].state != static_cast<std::int32_t> (sandbox::ProcessState::empty))
                setSlotError (page.slots[slot], message);

        setPageState (page, sandbox::ProcessState::failed);
        std::cerr << "ihub-sandbox: " << message << std::endl;
    };

    const auto editorControlFor = [&] (int slot) -> sandbox_client::EditorControl
    {
        return [&, slot] (bool open, juce::String& error) -> bool
        {
            auto& runtime = runtimes[static_cast<std::size_t> (slot)];
            auto* plugin = runtime.plugin.get();

            if (! displayAvailable || plugin == nullptr)
            {
                error = ! displayAvailable ? "no display is available for the plugin editor"
                                           : "this sandbox slot has no plugin editor";
                return false;
            }

            const auto result = juce::MessageManager::callSync ([&] () -> bool
            {
                auto& shared = page.slots[slot];

                if (open)
                {
                    if (runtime.editor != nullptr && runtime.editor->isOpen())
                    {
                        if (auto* component = runtime.editor->getWindowComponent())
                            component->toFront (true);

                        return true;
                    }

                    if (runtime.editor == nullptr)
                    {
                        runtime.editor = std::make_unique<PluginWindow>();
                        runtime.editor->setHideOnClose (true);
                    }

                    runtime.editor->onClose = [&shared]
                    {
                        sandbox::storeRelaxed (shared.editorOpen, 0);
                        sandbox::storeRelease (shared.editorCaptureRequested, 1);
                    };

                    juce::String openError;

                    try
                    {
                        if (! runtime.editor->open (*plugin, {}, nullptr, &openError))
                        {
                            error = openError.isNotEmpty()
                                        ? openError
                                        : juce::String ("the plugin did not create an editor");
                            runtime.editor.reset();
                            return false;
                        }
                    }
                    catch (const std::exception& exception)
                    {
                        error = juce::String ("the plugin editor failed: ") + exception.what();
                        runtime.editor.reset();
                        return false;
                    }

                    sandbox::storeRelaxed (shared.editorOpen, 1);
                    return true;
                }

                if (runtime.editor != nullptr)
                    runtime.editor->close();

                sandbox::storeRelaxed (shared.editorOpen, 0);
                return true;
            });

            sandbox_client::consumeEditorCapture (page.slots[slot], plugin);

            if (! result.has_value())
            {
                error = "the sandbox message loop is not running";
                return false;
            }

            return *result;
        };
    };

    const auto runSlot = [&] (int slot)
    {
        auto& shared = page.slots[slot];
        auto& runtime = runtimes[static_cast<std::size_t> (slot)];
        auto* plugin = runtime.plugin.get();
        auto* fake = runtime.fake.get();
        const auto editorControl = editorControlFor (slot);

        juce::AudioBuffer<float> renderBuffer (numChannels, maximumBlockSize);
        juce::AudioBuffer<float> view;
        juce::MidiBuffer midi;

        std::uint64_t nextRequest = 0;
        std::uint32_t heartbeat = 0;
        std::uint32_t lastControlSequence = sandbox::loadAcquire (shared.control.sequence);
        double lastParentCheckMs = juce::Time::getMillisecondCounterHiRes();

        const auto exitSlot = [&]
        {
            setSlotState (shared, sandbox::ProcessState::stopping);

            if (runningThreads.fetch_sub (1) == 1)
                stopMessageLoop();
        };

        const auto consumeEditorCapture = [&]
        {
            if (sandbox::loadAcquire (shared.editorCaptureRequested) == 0)
                return;

            sandbox::storeRelaxed (shared.editorCaptureRequested, 0);

            (void) juce::MessageManager::callAsync ([&shared, plugin]
            {
                sandbox_client::publishEditorState (shared, plugin);
            });
        };

        for (;;)
        {
            if (pageState (page) == sandbox::ProcessState::stopping
                || pageState (page) == sandbox::ProcessState::failed)
            {
                exitSlot();
                return;
            }

            consumeEditorCapture();

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
                        sandbox_client::handleControl (shared, plugin,
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

                if (! hostIsAlive (page))
                {
                    setPageState (page, sandbox::ProcessState::stopping);
                    exitSlot();
                    return;
                }
            }

            const auto published = sandbox::loadAcquire (shared.requestPublished);

            if (published <= nextRequest)
            {
                juce::Thread::sleep (1);
                continue;
            }

            auto& request = shared.requests[nextRequest % sandbox::requestRingSize];

            if (request.sequence != static_cast<std::uint32_t> (nextRequest))
            {
                markPageFailed ("slot " + juce::String (slot) + " request ring out of sequence");
                exitSlot();
                return;
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
                markPageFailed ("slot " + juce::String (slot)
                                + " threw in the plugin: " + exception.what());
                exitSlot();
                return;
            }
            catch (...)
            {
                markPageFailed ("slot " + juce::String (slot)
                                + " threw an unknown exception in the plugin");
                exitSlot();
                return;
            }

            auto& reply = shared.replies[nextRequest % sandbox::replyRingSize];
            reply.sequence = static_cast<std::uint32_t> (nextRequest);
            reply.numSamples = numSamples;
            reply.latencySamples = runtime.latencySamples;
            reply.flags = 0;

            for (int channel = 0; channel < numChannels; ++channel)
                juce::FloatVectorOperations::copy (reply.audio[channel],
                                                   view.getReadPointer (channel),
                                                   numSamples);

            sandbox::storeRelease (shared.replyPublished, nextRequest + 1);
            sandbox::storeRelease (shared.requestConsumed, nextRequest + 1);

            ++nextRequest;
            ++heartbeat;
            sandbox::storeRelaxed (shared.heartbeat, heartbeat);
            sandbox::storeRelaxed (page.heartbeat,
                                   sandbox::loadRelaxed (page.heartbeat) + 1);
        }
    };

    for (int slot = 0; slot < page.numSlots; ++slot)
        threads.emplace_back (runSlot, slot);

    setPageState (page, sandbox::ProcessState::ready);

    juce::MessageManager::getInstance()->runDispatchLoop();

    for (auto& thread : threads)
        if (thread.joinable())
            thread.join();

    if (failed.load())
        std::_Exit (5);

    for (auto& runtime : runtimes)
        if (runtime.editor != nullptr)
        {
            runtime.editor->close();
            runtime.editor.reset();
        }

    setPageState (page, sandbox::ProcessState::stopping);
    return 0;
}

} // namespace ihub::plugins
