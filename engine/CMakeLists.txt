get_filename_component(IHUB_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ui" ABSOLUTE)
set(IHUB_UI_DIST "${IHUB_UI_DIR}/dist")
set(IHUB_UI_INDEX "${IHUB_UI_DIST}/index.html")
set(IHUB_UI_JS "${IHUB_UI_DIST}/assets/app.js")
set(IHUB_UI_CSS "${IHUB_UI_DIST}/assets/index.css")

find_program(IHUB_PNPM_EXECUTABLE pnpm DOC "pnpm executable used to build the React UI")

if(IHUB_PNPM_EXECUTABLE)
    set(IHUB_PNPM_COMMAND "${IHUB_PNPM_EXECUTABLE}")
else()
    find_program(IHUB_COREPACK_EXECUTABLE corepack DOC "corepack, used to run pnpm")

    if(NOT IHUB_COREPACK_EXECUTABLE)
        message(FATAL_ERROR "pnpm (or corepack) was not found. Install Node.js to build ui/.")
    endif()

    set(IHUB_PNPM_COMMAND "${IHUB_COREPACK_EXECUTABLE}" pnpm)
endif()

file(GLOB_RECURSE IHUB_UI_SOURCES CONFIGURE_DEPENDS "${IHUB_UI_DIR}/src/*")

set(IHUB_UI_BUILD_INPUTS
    "${IHUB_UI_DIR}/index.html"
    "${IHUB_UI_DIR}/package.json"
    "${IHUB_UI_DIR}/pnpm-lock.yaml"
    "${IHUB_UI_DIR}/tsconfig.json"
    "${IHUB_UI_DIR}/vite.config.ts"
    ${IHUB_UI_SOURCES}
)

set(IHUB_UI_NODE_MODULES "${IHUB_UI_DIR}/node_modules")
set(IHUB_UI_INSTALL_STAMP "${IHUB_UI_NODE_MODULES}/.modules.yaml")

add_custom_command(
    OUTPUT "${IHUB_UI_INSTALL_STAMP}"
    COMMAND ${IHUB_PNPM_COMMAND} install --frozen-lockfile
    WORKING_DIRECTORY "${IHUB_UI_DIR}"
    DEPENDS "${IHUB_UI_DIR}/package.json" "${IHUB_UI_DIR}/pnpm-lock.yaml"
    COMMENT "Installing React UI dependencies"
    VERBATIM
)

add_custom_command(
    OUTPUT "${IHUB_UI_INDEX}" "${IHUB_UI_JS}" "${IHUB_UI_CSS}"
    COMMAND ${IHUB_PNPM_COMMAND} run build
    WORKING_DIRECTORY "${IHUB_UI_DIR}"
    DEPENDS "${IHUB_UI_INSTALL_STAMP}" ${IHUB_UI_BUILD_INPUTS}
    COMMENT "Building React UI into ui/dist"
    VERBATIM
)

add_custom_target(instrument-hub-ui
    DEPENDS "${IHUB_UI_INDEX}" "${IHUB_UI_JS}" "${IHUB_UI_CSS}"
)

juce_add_binary_data(InstrumentHubAssets
    SOURCES
        "${IHUB_UI_INDEX}"
        "${IHUB_UI_JS}"
        "${IHUB_UI_CSS}"
    NAMESPACE BinaryData
    HEADER_NAME BinaryData.h
)

add_dependencies(InstrumentHubAssets instrument-hub-ui)

juce_add_gui_app(instrument-hub
    PRODUCT_NAME "Instrument Hub"
    COMPANY_NAME "Instrument Hub"
    BUNDLE_ID "com.instrumenthub.app"
    VERSION 0.1.0
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
)

target_sources(instrument-hub
    PRIVATE
        src/Main.cpp
        src/api/Discovery.cpp
        src/api/discovery/DnsPacket.cpp
        src/api/discovery/MdnsSocket.cpp
        src/api/MidiBridge.cpp
        src/api/midi/MidiPadTest.cpp
        src/api/Pairing.cpp
        src/api/PluginBridge.cpp
        src/api/AuthGuard.cpp
        src/api/MeterSampler.cpp
        src/api/Routes.cpp
        src/api/routes/AudioRoutes.cpp
        src/api/routes/AudioSerialization.cpp
        src/api/routes/FxRoutes.cpp
        src/api/routes/Json.cpp
        src/api/routes/MeterSerialization.cpp
        src/api/routes/MidiRoutes.cpp
        src/api/routes/MidiSerialization.cpp
        src/api/routes/PairingRoutes.cpp
        src/api/routes/PairingSerialization.cpp
        src/api/routes/PatchRoutes.cpp
        src/api/routes/PluginRoutes.cpp
        src/api/routes/RouteHelpers.cpp
        src/api/routes/StateRoutes.cpp
        src/api/routes/StateSerialization.cpp
        src/api/routes/TransportRoutes.cpp
        src/api/Server.cpp
        src/api/state/StateStore.cpp
        src/api/state/StateStoreFx.cpp
        src/api/state/StateStorePatchSource.cpp
        src/api/state/StateStorePatches.cpp
        src/api/state/StateStoreSettings.cpp
        src/api/state/StateStoreTransport.cpp
        src/api/state/StateStoreSnapshot.cpp
        src/api/SessionBridge.cpp
        src/api/StateBroadcast.cpp
        src/api/WebSocketRoutes.cpp
        src/app/CommandLine.cpp
        src/app/ControlApi.cpp
        src/app/ControllerSurface.cpp
        src/app/Modes.cpp
        src/app/SessionSetup.cpp
        src/app/SnapshotCapture.cpp
        src/app/SourcePersistence.cpp
        src/app/UiModes.cpp
        src/app/diagnostics/DeviceListMode.cpp
        src/app/diagnostics/DiagnosticsMode.cpp
        src/app/diagnostics/LatencyMode.cpp
        src/app/diagnostics/PluginScanMode.cpp
        src/app/diagnostics/RtProbeSource.cpp
        src/app/diagnostics/RtStrictMode.cpp
        src/app/diagnostics/SessionSmokeMode.cpp
        src/Bridge/WebView.cpp
        src/audio/AudioEngine.cpp
        src/audio/AudioSettings.cpp
        src/audio/Bus.cpp
        src/audio/MasterLimiter.cpp
        src/audio/ClockIn.cpp
        src/audio/ClockOut.cpp
        src/audio/DeviceManager.cpp
        src/audio/FxChain.cpp
        src/audio/FxRack.cpp
        src/audio/device/DeviceChannels.cpp
        src/audio/device/DeviceDescription.cpp
        src/audio/device/DeviceEnumeration.cpp
        src/audio/device/DeviceLatency.cpp
        src/audio/device/DeviceLookup.cpp
        src/audio/device/DeviceOpen.cpp
        src/audio/device/DeviceState.cpp
        src/audio/fx/FxInsertFactory.cpp
        src/audio/fx/FxInsertParameters.cpp
        src/audio/fx/FxInsertType.cpp
        src/audio/fx/FxRackEditing.cpp
        src/audio/fx/FxRackLatency.cpp
        src/audio/fx/FxRackOrder.cpp
        src/audio/fx/FxRackSnapshot.cpp
        src/audio/fx/InsertEffect.cpp
        src/audio/latency/AlsaLoopback.cpp
        src/audio/latency/LatencyCapture.cpp
        src/audio/latency/LatencyCorrelation.cpp
        src/audio/LatencyProbe.cpp
        src/audio/RtGuard.cpp
        src/audio/rtguard/AllocatorInterposition.cpp
        src/audio/rtguard/LockInterposition.cpp
        src/audio/settings/AudioSettingsApply.cpp
        src/audio/settings/AudioSettingsChannels.cpp
        src/audio/settings/AudioSettingsMeasure.cpp
        src/audio/settings/AudioSettingsOpen.cpp
        src/audio/settings/AudioSettingsSnapshot.cpp
        src/audio/settings/LatencyBudget.cpp
        src/audio/Slot.cpp
        src/audio/SlotSwitcher.cpp
        src/audio/TimingMonitor.cpp
        src/audio/TapTempo.cpp
        src/audio/Transport.cpp
        src/audio/WarmPool.cpp
        src/audio/warm/WarmPoolEviction.cpp
        src/audio/warm/WarmPoolSlots.cpp
        src/audio/warm/WarmPoolWarming.cpp
        src/audio/warm/WarmPoolWorker.cpp
        src/controller/launchpad/LaunchpadDriverConnection.cpp
        src/controller/launchpad/LaunchpadDriverDevice.cpp
        src/controller/launchpad/LaunchpadDriverLeds.cpp
        src/controller/launchpad/LaunchpadDriverMidi.cpp
        src/controller/launchpad/LaunchpadDriverOutput.cpp
        src/controller/LaunchpadLed.cpp
        src/controller/LaunchpadProtocol.cpp
        src/controller/Mapping.cpp
        src/controller/mapping/MappingApply.cpp
        src/controller/mapping/MappingButtonEvents.cpp
        src/controller/mapping/MappingDecoder.cpp
        src/controller/mapping/MappingDefaults.cpp
        src/controller/mapping/MappingDiagnostics.cpp
        src/controller/mapping/MappingFamilies.cpp
        src/controller/mapping/MappingFile.cpp
        src/controller/mapping/MappingFx.cpp
        src/controller/mapping/MappingMacros.cpp
        src/controller/mapping/MappingPadBindings.cpp
        src/controller/mapping/MappingPadEvents.cpp
        src/controller/mapping/MappingReload.cpp
        src/controller/mapping/MappingScenes.cpp
        src/controller/mapping/MappingWatcher.cpp
        src/controller/mapping/YamlParser.cpp
        src/controller/mapping/YamlScalar.cpp
        src/controller/mapping/YamlScanner.cpp
        src/controller/MappingModel.cpp
        src/controller/MappingYaml.cpp
        src/dsp/Compressor.cpp
        src/dsp/Delay.cpp
        src/dsp/Eq.cpp
        src/dsp/Reverb.cpp
        src/diag/CrashDump.cpp
        src/diag/Diagnostics.cpp
        src/diag/LogRing.cpp
        src/diag/LogSink.cpp
        src/instruments/Instrument.cpp
        src/instruments/AudioInputInstrument.cpp
        src/instruments/PluginFx.cpp
        src/plugins/PluginInstrument.cpp
        src/plugins/host/PluginInstrumentBus.cpp
        src/plugins/host/PluginInstrumentMidi.cpp
        src/plugins/host/PluginInstrumentNotifications.cpp
        src/plugins/host/PluginInstrumentParameters.cpp
        src/plugins/host/PluginInstrumentProcess.cpp
        src/plugins/host/PluginInstrumentState.cpp
        src/plugins/PluginCache.cpp
        src/plugins/scanner/PluginCacheFingerprint.cpp
        src/plugins/scanner/PluginCachePersistence.cpp
        src/plugins/PluginInventory.cpp
        src/plugins/scanner/PluginInfo.cpp
        src/plugins/scanner/PluginInventoryRecords.cpp
        src/plugins/scanner/PluginInventoryScan.cpp
        src/plugins/PluginWindow.cpp
        src/plugins/window/PluginEditorWindow.cpp
        src/plugins/window/PluginWindowEditor.cpp
        src/plugins/SandboxHost.cpp
        src/plugins/sandbox/SandboxHostMidi.cpp
        src/plugins/sandbox/SandboxHostProcess.cpp
        src/plugins/sandbox/SandboxHostState.cpp
        src/plugins/sandbox/SandboxHostStatus.cpp
        src/plugins/sandbox/SandboxHostStop.cpp
        src/plugins/sandbox/SandboxHostTransport.cpp
        src/plugins/sandbox/SandboxWatchdog.cpp
        src/plugins/Scanner.cpp
        src/plugins/scanner/ScannerDiscovery.cpp
        src/plugins/scanner/ScannerRun.cpp
        src/plugins/ScannerProcess.cpp
        src/project/Migrations.cpp
        src/project/SetFile.cpp
        src/project/Serializer.cpp
        src/project/serializer/Container.cpp
        src/project/serializer/JsonFields.cpp
        src/project/serializer/PluginStateBlob.cpp
        src/project/serializer/SetManifest.cpp
        src/project/serializer/SetPatch.cpp
        src/Session.cpp
        src/session/DeviceController.cpp
        src/session/EditorHost.cpp
        src/session/FxChainController.cpp
        src/session/MeterBridge.cpp
        src/session/MidiBridge.cpp
        src/session/PluginStateFile.cpp
        src/session/PluginStateStore.cpp
        src/session/PluginStateStoreData.cpp
        src/session/SessionBuild.cpp
        src/session/SessionControl.cpp
        src/session/SessionDevice.cpp
        src/session/SessionFx.cpp
        src/session/SessionMeasure.cpp
        src/session/SessionPluginMirror.cpp
        src/session/SessionPluginSandbox.cpp
        src/session/SessionPluginState.cpp
        src/session/SessionRender.cpp
        src/session/SessionWarm.cpp
        src/session/SlotSources.cpp
        src/session/TransportControl.cpp
        src/session/WarmPoolController.cpp
        src/session/warm/WarmPoolArm.cpp
        src/session/warm/WarmPoolMeasurement.cpp
        src/session/warm/WarmPoolVoice.cpp
        src/sampler/Kit.cpp
        src/sampler/KitLoader.cpp
        src/sampler/LoadedKit.cpp
        src/sampler/PanGains.cpp
        src/sampler/SampleHandoff.cpp
        src/sampler/SampleLoader.cpp
        src/sampler/SamplerInstrument.cpp
        src/sampler/instrument/SamplerInstrumentEvents.cpp
        src/sampler/instrument/SamplerInstrumentRender.cpp
        src/sampler/instrument/SamplerInstrumentVoices.cpp
        src/sampler/kit/KitBuilder.cpp
        src/sampler/kit/KitJson.cpp
        src/sampler/kit/KitParser.cpp
        src/sampler/kit/KitValidation.cpp
        src/sampler/sample/SampleDecoder.cpp
        src/sampler/sample/SampleResampler.cpp
)

target_include_directories(instrument-hub
    PRIVATE
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib"
)

add_library(ihub-httplib INTERFACE)
target_include_directories(ihub-httplib SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib")
target_link_libraries(instrument-hub PRIVATE ihub-httplib)

target_compile_definitions(instrument-hub
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_VERSION>"
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(instrument-hub
    PRIVATE
        InstrumentHubAssets
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_link_libraries(instrument-hub PRIVATE X11::X11)
endif()

if(IHUB_ENABLE_RT_GUARD)
    target_compile_definitions(instrument-hub PRIVATE IHUB_RT_GUARD=1)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_definitions(instrument-hub PRIVATE
            IHUB_RT_GUARD_WRAP_MALLOC=1
            IHUB_RT_GUARD_WRAP_LOCKS=1
        )

        target_link_options(instrument-hub PRIVATE
            "-Wl,--wrap=malloc"
            "-Wl,--wrap=calloc"
            "-Wl,--wrap=realloc"
            "-Wl,--wrap=pthread_mutex_lock"
            "-Wl,--wrap=pthread_mutex_trylock"
            "-Wl,--wrap=pthread_mutex_timedlock"
            "-Wl,--wrap=pthread_spin_lock"
            "-Wl,--wrap=pthread_spin_trylock"
            "-Wl,--wrap=pthread_rwlock_rdlock"
            "-Wl,--wrap=pthread_rwlock_wrlock"
            "-Wl,--wrap=pthread_rwlock_tryrdlock"
            "-Wl,--wrap=pthread_rwlock_trywrlock"
        )

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(instrument-hub PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
        endif()
    endif()
endif()

set_target_properties(instrument-hub PROPERTIES
    OUTPUT_NAME "instrument-hub"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-scanner
    PRODUCT_NAME "ihub-scanner"
)

target_sources(ihub-scanner
    PRIVATE
        src/plugins/scanner_main.cpp
)

target_include_directories(ihub-scanner
    PRIVATE
        src
)

target_compile_definitions(ihub-scanner
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ihub-scanner
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-scanner PROPERTIES
    OUTPUT_NAME "ihub-scanner"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_dependencies(instrument-hub ihub-scanner)

juce_add_console_app(ihub-sandbox
    PRODUCT_NAME "ihub-sandbox"
)

target_sources(ihub-sandbox
    PRIVATE
        src/plugins/sandbox_main.cpp
        src/plugins/SandboxClient.cpp
        src/plugins/sandbox/FakeToneInstrument.cpp
        src/plugins/sandbox/SandboxClientArguments.cpp
        src/plugins/sandbox/SandboxClientControl.cpp
        src/plugins/PluginInstrument.cpp
        src/plugins/host/PluginInstrumentBus.cpp
        src/plugins/host/PluginInstrumentMidi.cpp
        src/plugins/host/PluginInstrumentNotifications.cpp
        src/plugins/host/PluginInstrumentParameters.cpp
        src/plugins/host/PluginInstrumentProcess.cpp
        src/plugins/host/PluginInstrumentState.cpp
        src/instruments/Instrument.cpp
)

target_include_directories(ihub-sandbox
    PRIVATE
        src
)

target_compile_definitions(ihub-sandbox
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ihub-sandbox
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-sandbox PROPERTIES
    OUTPUT_NAME "ihub-sandbox"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_dependencies(instrument-hub ihub-sandbox)
