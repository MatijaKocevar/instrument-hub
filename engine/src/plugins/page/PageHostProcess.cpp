#include "../PageHost.h"
#include "../SandboxHost.h"

#include "PageHostProcessTree.h"
#include "PageHostWine.h"

#include <cstdio>
#include <cstring>

#if JUCE_LINUX || JUCE_MAC
#include <unistd.h>
#endif

namespace ihub::plugins
{
    namespace
    {
        constexpr int processOutputDrainMs = 50;
        constexpr int helperGracefulStopMs = 8000;
        constexpr int shutdownHelperGracefulStopMs = 2000;
        constexpr int maximumOutputTailLength = 12000;
        constexpr int maximumRestartAssertionLines = 10;
        constexpr const char* restartAssertionMarker = "juce_VST3PluginFormatImpl.h:3498";

        std::int32_t currentProcessId () noexcept
        {
#if JUCE_LINUX || JUCE_MAC
            return static_cast<std::int32_t> (getpid ());
#else
            return 0;
#endif
        }

        bool createSizedFile (const juce::File& file, juce::int64 size)
        {
            file.deleteFile ();

            juce::FileOutputStream stream (file);

            if (!stream.openedOk () || size <= 0)
                return false;

            if (!stream.setPosition (size - 1))
                return false;

            if (!stream.writeByte (0))
                return false;

            stream.flush ();
            return true;
        }

        juce::String readProcessOutput (juce::ChildProcess& process)
        {
            return process.readAllProcessOutput ();
        }
    }

    juce::File PageHost::createPageFile () const
    {
        const auto directory = options.tempDirectory != juce::File ()
                                   ? options.tempDirectory
                                   : juce::File::getSpecialLocation (juce::File::tempDirectory);

        return directory.getChildFile ("ihub-page-" + juce::Uuid ().toString () + ".ihubshm");
    }

    bool PageHost::mapNewPage (juce::String& error)
    {
        sharedMemoryFile = createPageFile ();

        if (!createSizedFile (sharedMemoryFile,
                              static_cast<juce::int64> (sizeof (sandbox::SharedPage))))
        {
            error = "could not create the plugin page shared memory: " +
                    sharedMemoryFile.getFullPathName ();
            return false;
        }

        auto newMapping = std::make_unique<juce::MemoryMappedFile> (
            sharedMemoryFile, juce::MemoryMappedFile::readWrite, false);

        if (newMapping->getData () == nullptr ||
            newMapping->getSize () < sizeof (sandbox::SharedPage))
        {
            error = "could not map the plugin page shared memory";
            return false;
        }

        auto* page = static_cast<sandbox::SharedPage*> (newMapping->getData ());
        std::memset (page, 0, sizeof (sandbox::SharedPage));

        page->magic = sandbox::sharedPageMagic;
        page->version = sandbox::sharedPageVersion;
        page->numSlots = static_cast<std::int32_t> (slots.size ());
        page->state = static_cast<std::int32_t> (sandbox::ProcessState::empty);
        page->parentProcessId = currentProcessId ();

        std::snprintf (page->pageId, sizeof (page->pageId), "%s", options.pageId.toRawUTF8 ());

        for (int index = 0; index < page->numSlots; ++index)
        {
            auto& shared = page->slots[index];
            shared.magic = sandbox::sharedMemoryMagic;
            shared.version = sandbox::sharedMemoryVersion;
            shared.sampleRate = options.sampleRate;
            shared.maximumBlockSize = options.maxBlockSize;
            shared.numChannels = options.numChannels;
            shared.state = static_cast<std::int32_t> (sandbox::ProcessState::empty);
            shared.parentProcessId = currentProcessId ();
            shared.latencySamples = -1;

            editorStateFiles.resize (static_cast<std::size_t> (page->numSlots));
            auto& editorStateFile = editorStateFiles[static_cast<std::size_t> (index)];
            editorStateFile =
                juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("ihub-page-editor-" + juce::Uuid ().toString () + ".bin");
            editorStateFile.deleteFile ();

            std::snprintf (shared.editorStatePath,
                           sizeof (shared.editorStatePath),
                           "%s",
                           editorStateFile.getFullPathName ().toRawUTF8 ());
        }

        if (mapping != nullptr)
            retiredMappings.push_back (std::move (mapping));

        mapping = std::move (newMapping);
        sharedPage.store (page, std::memory_order_release);
        return true;
    }

    bool PageHost::startProcess (juce::String& error)
    {
        error.clear ();

        {
            const std::lock_guard<std::mutex> lock (outputMutex);
            outputTail.clear ();
        }

        const auto appendOutput = [this] (juce::String& message)
        {
            const std::lock_guard<std::mutex> lock (outputMutex);
            const auto trimmed = outputTail.trim ();

            if (trimmed.isEmpty ())
                return;

            const auto tail =
                trimmed.length () > 2000 ? trimmed.substring (trimmed.length () - 2000) : trimmed;

            message << ": " << tail;
        };

        if (slots.empty ())
        {
            error = "the plugin page has no slots";
            return false;
        }

        if (!bundleViewRoot.exists () && !prepareBundleView (error))
            return false;

        if (winePrefix.isEmpty ())
        {
            for (const auto& slot : slots)
            {
                if (slot->descriptor.pluginBundle == juce::File ())
                    continue;

                winePrefix = page_detail::winePrefixForBundle (slot->descriptor.pluginBundle);

                if (winePrefix.isNotEmpty ())
                    break;
            }
        }

        if (winePrefix.isNotEmpty ())
        {
            page_detail::stopWinePrefixes (vstbridgeGroup, winePrefix);
            page_detail::startWinePrefix (winePrefix);
            (void)page_detail::warmWinePrefix (winePrefix, 20000);
        }

        if (!mapNewPage (error))
            return false;

        const auto helper = options.helperExecutable != juce::File ()
                                ? options.helperExecutable
                                : SandboxHost::findHelperExecutable ();

        if (!helper.existsAsFile ())
        {
            error = "sandbox helper not found: " + helper.getFullPathName ();
            return false;
        }

        juce::StringArray arguments;
        arguments.add (helper.getFullPathName ());
        arguments.add (sandbox::cli::pageMapFlag);
        arguments.add (sharedMemoryFile.getFullPathName ());
        arguments.add (sandbox::cli::pageIdFlag);
        arguments.add (options.pageId);

        for (const auto& slot : slots)
        {
            const auto* descriptor = &slot->descriptor;
            auto bundle = descriptor->pluginBundle;
            const auto view = bundleViews.find (bundle.getFullPathName ());

            if (view != bundleViews.end ())
                bundle = view->second;

            if (bundle != juce::File ())
            {
                arguments.add (sandbox::cli::pageSlotFlag);
                arguments.add (juce::String (descriptor->slotIndex));
                arguments.add (bundle.getFullPathName ());
            }
            else if (descriptor->fakeToneFrequency > 0.0)
            {
                arguments.add (sandbox::cli::pageFakeToneFlag);
                arguments.add (juce::String (descriptor->slotIndex));
                arguments.add (juce::String (descriptor->fakeToneFrequency));
            }
            else
            {
                error = "page slot " + juce::String (descriptor->slotIndex) +
                        " has no plugin bundle and no fake tone";
                return false;
            }

            if (descriptor->abortAfterBlocks >= 0)
            {
                arguments.add (sandbox::cli::pageAbortAfterFlag);
                arguments.add (juce::String (descriptor->slotIndex));
                arguments.add (juce::String (descriptor->abortAfterBlocks));
            }

            arguments.addArray (descriptor->extraArguments);
        }

        childProcess = std::make_unique<juce::ChildProcess> ();

        if (!childProcess->start (arguments,
                                  juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            error = "could not launch the shared plugin host: " + helper.getFullPathName ();
            return false;
        }

        outputThreadStop.store (false, std::memory_order_relaxed);
        outputThread = std::thread (
            [this]
            {
                int restartAssertionLines = 0;
                bool restartAssertionsCapped = false;
                juce::String pendingLine;

                const auto appendLine = [&] (const juce::String& line)
                {
                    if (line.contains (restartAssertionMarker))
                    {
                        if (restartAssertionLines >= maximumRestartAssertionLines)
                        {
                            if (!restartAssertionsCapped)
                            {
                                outputTail += "[ihub: further repeated restartComponent assertion "
                                              "lines suppressed]\n";
                                restartAssertionsCapped = true;
                            }

                            return;
                        }

                        ++restartAssertionLines;
                    }

                    outputTail += line;
                    outputTail += '\n';

                    if (outputTail.length () > maximumOutputTailLength)
                        outputTail =
                            outputTail.substring (outputTail.length () - maximumOutputTailLength);
                };

                while (!outputThreadStop.load (std::memory_order_relaxed))
                {
                    if (childProcess != nullptr)
                    {
                        const auto output = readProcessOutput (*childProcess);

                        if (output.isNotEmpty ())
                        {
                            const std::lock_guard<std::mutex> lock (outputMutex);
                            pendingLine += output;

                            for (;;)
                            {
                                const auto newline = pendingLine.indexOfChar ('\n');

                                if (newline < 0)
                                    break;

                                appendLine (pendingLine.substring (0, newline).trimEnd ());
                                pendingLine = pendingLine.substring (newline + 1);
                            }
                        }
                    }

                    juce::Thread::sleep (processOutputDrainMs);
                }

                if (pendingLine.isNotEmpty ())
                {
                    const std::lock_guard<std::mutex> lock (outputMutex);
                    appendLine (pendingLine.trimEnd ());
                }
            });

        const auto deadline = juce::Time::getMillisecondCounterHiRes () +
                              static_cast<double> (options.startupTimeoutMs);

        for (;;)
        {
            if (abortRequested () || (options.shouldAbortStart && options.shouldAbortStart ()))
            {
                stopProcess (true);
                error = "shared plugin host startup cancelled";
                return false;
            }

            auto* page = sharedPage.load (std::memory_order_acquire);

            if (page == nullptr)
            {
                stopProcess (true);
                error = "the plugin page shared memory is not mapped";
                return false;
            }

            const auto state =
                static_cast<sandbox::ProcessState> (sandbox::loadAcquire (page->state));

            if (state == sandbox::ProcessState::ready)
            {
                childProcessId.store (page->childProcessId, std::memory_order_relaxed);

                for (auto& slot : slots)
                {
                    if (slot->internalIndex >= 0 && slot->internalIndex < page->numSlots)
                    {
                        const auto latency =
                            sandbox::loadRelaxed (page->slots[slot->internalIndex].latencySamples);
                        slot->latencySamples.store (juce::jmax (0, latency),
                                                    std::memory_order_relaxed);
                    }
                }

                return true;
            }

            if (state == sandbox::ProcessState::failed)
            {
                const auto detail = juce::String::fromUTF8 (page->error).trim ();

                stopProcess (true);
                error = detail.isNotEmpty ()
                            ? detail
                            : juce::String ("the shared plugin host failed to start");
                appendOutput (error);
                return false;
            }

            if (childProcess == nullptr || !childProcess->isRunning ())
            {
                stopProcess (true);
                error = "the shared plugin host exited during startup";
                appendOutput (error);
                return false;
            }

            if (juce::Time::getMillisecondCounterHiRes () > deadline)
            {
                stopProcess (true);
                error = "the shared plugin host did not start within " +
                        juce::String (options.startupTimeoutMs) + " ms";
                appendOutput (error);
                return false;
            }

            juce::Thread::sleep (2);
        }
    }

    void PageHost::killChildProcessTree () noexcept
    {
#if JUCE_LINUX || JUCE_MAC
        const auto helperPid = childProcessId.load (std::memory_order_relaxed);

        if (helperPid > 0)
            page_detail::killProcessTree (static_cast<pid_t> (helperPid));
#endif

        if (childProcess != nullptr && childProcess->isRunning ())
            childProcess->kill ();
    }

    void PageHost::stopProcess (bool resetWinePrefix, int helperStopTimeoutMs) noexcept
    {
        auto* page = sharedPage.load (std::memory_order_acquire);

        if (page != nullptr)
            sandbox::storeRelease (page->state,
                                   static_cast<std::int32_t> (sandbox::ProcessState::stopping));

        const auto helperTimeout =
            helperStopTimeoutMs > 0 ? helperStopTimeoutMs : helperGracefulStopMs;

        bool helperExited = false;

        if (childProcess != nullptr)
            helperExited = childProcess->waitForProcessToFinish (helperTimeout);

        if (childProcess != nullptr && !helperExited)
        {
            killChildProcessTree ();
            childProcess->waitForProcessToFinish (1000);
        }

        if (resetWinePrefix && (winePrefix.isNotEmpty () || vstbridgeGroup.isNotEmpty ()))
        {
            page_detail::stopWinePrefixes (vstbridgeGroup, winePrefix);

            if (winePrefix.isNotEmpty ())
                page_detail::killProcessesWithWinePrefix (winePrefix.toRawUTF8 ());
        }

        if (sharedMemoryFile != juce::File ())
            page_detail::killProcessesMatching (sharedMemoryFile.getFullPathName ().toRawUTF8 ());

        if (vstbridgeGroup.isNotEmpty ())
            page_detail::killProcessesMatching (vstbridgeGroup.toRawUTF8 ());

        removeBundleView ();

        if (outputThread.joinable ())
        {
            outputThreadStop.store (true, std::memory_order_relaxed);
            outputThread.join ();
        }

        if (childProcess != nullptr)
        {
            (void)childProcess->readAllProcessOutput ();
            childProcess.reset ();
        }

        sharedPage.store (nullptr, std::memory_order_release);
        childProcessId.store (0, std::memory_order_relaxed);

        if (mapping != nullptr)
            retiredMappings.push_back (std::move (mapping));

        stateTransferFiles.clear ();
        editorStateFiles.clear ();
    }

    bool PageHost::start (juce::String& error)
    {
        error.clear ();

        std::lock_guard<std::mutex> lock (lifecycleMutex);

        if (status.load (std::memory_order_acquire) == Status::ready)
            return true;

        markStatus (Status::starting, {});
        startThreads ();

        if (!startProcess (error))
        {
            markStatus (Status::failed, error);
            return false;
        }

        markStatus (Status::ready, {});
        return true;
    }

    void PageHost::shutdown () noexcept
    {
        {
            const std::lock_guard<std::mutex> lock (recoveryMutex);
            shuttingDown = true;
            recoveryRequested = false;
        }

        recoveryWake.notify_all ();

        if (recoveryThread.joinable ())
            recoveryThread.join ();

        if (watchdogThread.joinable ())
            watchdogThread.join ();

        std::lock_guard<std::mutex> lock (lifecycleMutex);

        stopProcess (true, shutdownHelperGracefulStopMs);
        removeBundleView ();
        closeRetiredMappings ();
        markStatus (Status::stopped, {});
    }

    void PageHost::startThreads ()
    {
        if (!watchdogThread.joinable ())
            watchdogThread = std::thread ([this] { watchdogLoop (); });

        if (!recoveryThread.joinable ())
            recoveryThread = std::thread ([this] { recoveryLoop (); });
    }

} // namespace ihub::plugins
