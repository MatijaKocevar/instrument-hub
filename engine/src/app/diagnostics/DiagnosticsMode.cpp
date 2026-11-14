#include "app/DiagnosticModes.h"

#include "app/CommandLine.h"

#include "audio/AudioEngine.h"
#include "audio/DeviceManager.h"
#include "diag/CrashDump.h"
#include "diag/Diagnostics.h"
#include "diag/LogRing.h"
#include "plugins/Scanner.h"

#include <iostream>
#include <string>
#include <vector>

namespace ihub::app
{

    int runDiagnostics (const juce::String& commandLine)
    {
        using namespace ihub::diag;

        const auto durationValue = getCommandLineValue (commandLine, "--diagnostics-ms");

        int durationMs = durationValue.isNotEmpty () ? durationValue.getIntValue () : 1000;
        durationMs = juce::jlimit (100, 20000, durationMs);

        juce::File pluginList = ihub::plugins::Scanner::defaultReportFile ();

        if (const auto pluginDataDir = getCommandLineValue (commandLine, "--plugin-data-dir");
            pluginDataDir.isNotEmpty ())
        {
            pluginList =
                juce::File (pluginDataDir).getChildFile (ihub::plugins::Scanner::reportFileName);
        }

        auto& ring = defaultLog ();

        DiagnosticsOptions options;
        options.knownPluginListFile = pluginList;
        options.crashDumpPath = getCrashDumpPath ();
        options.maxLogRecords = 128;

        ring.log (LogLevel::info,
                  LogSource::diagnostics,
                  100,
                  "diagnostics started, crash dump " + getCrashDumpPath ());

        AudioEngine::DeviceManager devices;
        AudioEngine::DeviceSetup setup;

        bool found = false;

        for (const auto& info : devices.getDevices ())
        {
            if (!info.hasOutput || !info.name.containsIgnoreCase ("Loopback"))
                continue;

            setup.deviceTypeName = info.typeName;
            setup.outputDeviceName = info.name;
            setup.inputDeviceName = info.hasInput ? info.name : juce::String ();
            setup.sampleRate = 48000.0;
            setup.bufferSize = 128;
            setup.numOutputChannels = juce::jmin (2, info.outputChannelNames.size ());
            setup.numInputChannels =
                info.hasInput ? juce::jmin (2, info.inputChannelNames.size ()) : 0;
            setup.outputChannels = AudioEngine::DeviceSetup::channelMask (setup.numOutputChannels);
            setup.inputChannels = AudioEngine::DeviceSetup::channelMask (setup.numInputChannels);
            found = true;
            break;
        }

        AudioEngine::AudioEngine engine;

        if (!found)
        {
            ring.log (LogLevel::error,
                      LogSource::diagnostics,
                      110,
                      "no Loopback output device was enumerated");
        }
        else
        {
            juce::String error;

            if (!devices.open (setup, &error))
            {
                ring.log (LogLevel::error,
                          LogSource::diagnostics,
                          111,
                          "could not open Loopback: " + error.toStdString ());
            }
            else
            {
                ring.log (LogLevel::info,
                          LogSource::diagnostics,
                          101,
                          "opened " + setup.outputDeviceName.toStdString () + " at " +
                              juce::String (juce::roundToInt (devices.getCurrentSampleRate ()))
                                  .toStdString () +
                              " Hz / " +
                              juce::String (devices.getCurrentBufferSize ()).toStdString () +
                              " frames");

                devices.setAudioCallback (&engine);

                juce::Thread::sleep (durationMs);

                devices.setAudioCallback (nullptr);

                const auto snapshotTiming = engine.getTimingMonitor ().snapshot ();

                ring.log (LogLevel::info,
                          LogSource::diagnostics,
                          102,
                          "rendered " + std::to_string (snapshotTiming.callbacks) +
                              " callbacks in " + std::to_string (durationMs) + " ms, " +
                              std::to_string (snapshotTiming.overruns) + " callback xruns");
            }
        }

        const auto timing = engine.getTimingMonitor ().snapshot ();
        const std::vector<PluginDiagnostics> loadedPlugins;
        const auto snapshot = collectDiagnostics (devices, timing, loadedPlugins, ring, options);

        const auto report = formatDiagnostics (snapshot);
        std::cout << report;
        std::cout.flush ();

        if (const auto outPath = getCommandLineValue (commandLine, "--diagnostics-out");
            outPath.isNotEmpty ())
        {
            if (!writeDiagnostics (snapshot, juce::File (outPath)))
                std::cerr << "diagnostics: could not write " << outPath << std::endl;
        }

        return snapshot.device.open ? 0 : 1;
    }

}
