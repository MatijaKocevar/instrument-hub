set(IHUB_API_ROUTES_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/AudioRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/AudioSerialization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/FxRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/Json.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/MeterSerialization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/MidiRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/MidiSerialization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/PairingRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/PairingSerialization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/PatchRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/PluginRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/RouteHelpers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/StateRoutes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/StateSerialization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/routes/TransportRoutes.cpp"
)

set(IHUB_DISCOVERY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/discovery/DnsPacket.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/discovery/MdnsSocket.cpp"
)

set(IHUB_MIDI_BRIDGE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/MidiBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/midi/MidiPadTest.cpp"
)

set(IHUB_BUS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/MasterLimiter.cpp"
)

set(IHUB_RT_GUARD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/rtguard/AllocatorInterposition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/rtguard/LockInterposition.cpp"
)

set(IHUB_API_SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/AuthGuard.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/MeterSampler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStoreFx.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStorePatchSource.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStorePatches.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStoreSettings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStoreTransport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStoreSnapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/state/StateStoreProject.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/WebSocketRoutes.cpp"
)

set(IHUB_PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Migrations.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/SetFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Serializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/serializer/Container.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/serializer/JsonFields.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/serializer/PluginStateBlob.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/serializer/SetManifest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/serializer/SetPatch.cpp"
)

set(IHUB_MAPPING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingApply.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingButtonEvents.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingDefaults.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingDiagnostics.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingFamilies.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingFx.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingMacros.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingPadBindings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingPadEvents.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingReload.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingScenes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/MappingWatcher.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/YamlParser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/YamlScalar.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/mapping/YamlScanner.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/MappingModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/MappingYaml.cpp"
)

set(IHUB_LAUNCHPAD_PROTOCOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadProtocol.cpp"
)

set(IHUB_LAUNCHPAD_DRIVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadLed.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/launchpad/LaunchpadDriverConnection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/launchpad/LaunchpadDriverDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/launchpad/LaunchpadDriverLeds.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/launchpad/LaunchpadDriverMidi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/launchpad/LaunchpadDriverOutput.cpp"
    ${IHUB_LAUNCHPAD_PROTOCOL_SOURCES}
)

set(IHUB_FX_RACK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxInsertFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxInsertParameters.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxInsertType.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxRackEditing.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxRackLatency.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxRackOrder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxRackSnapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/InsertEffect.cpp"
)

set(IHUB_WARM_POOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/warm/WarmPoolEviction.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/warm/WarmPoolShutdown.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/warm/WarmPoolSlots.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/warm/WarmPoolWarming.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/warm/WarmPoolWorker.cpp"
)

set(IHUB_DEVICE_MANAGER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceChannels.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceDescription.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceEnumeration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceLatency.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceLookup.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceOpen.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/device/DeviceState.cpp"
)

set(IHUB_LATENCY_PROBE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/latency/AlsaLoopback.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/latency/LatencyCapture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/latency/LatencyCorrelation.cpp"
)

set(IHUB_AUDIO_SETTINGS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioSettings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/settings/AudioSettingsApply.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/settings/AudioSettingsChannels.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/settings/AudioSettingsMeasure.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/settings/AudioSettingsOpen.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/settings/AudioSettingsSnapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/settings/LatencyBudget.cpp"
)

set(IHUB_SAMPLER_KIT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/LoadedKit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/PanGains.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/kit/KitBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/kit/KitJson.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/kit/KitParser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/kit/KitValidation.cpp"
)

set(IHUB_SAMPLE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleHandoff.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/sample/SampleDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/sample/SampleResampler.cpp"
)

set(IHUB_SAMPLER_INSTRUMENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/instrument/SamplerInstrumentEvents.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/instrument/SamplerInstrumentRender.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/instrument/SamplerInstrumentVoices.cpp"
)

set(IHUB_SAMPLER_SOURCES
    ${IHUB_SAMPLER_KIT_SOURCES}
    ${IHUB_SAMPLE_SOURCES}
    ${IHUB_SAMPLER_INSTRUMENT_SOURCES}
)

set(IHUB_PLUGIN_INSTRUMENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/host/PluginInstrumentBus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/host/PluginInstrumentMidi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/host/PluginInstrumentNotifications.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/host/PluginInstrumentParameters.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/host/PluginInstrumentProcess.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/host/PluginInstrumentState.cpp"
)

set(IHUB_PLUGIN_WINDOW_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginWindow.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/window/PluginEditorWindow.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/window/PluginWindowEditor.cpp"
)

set(IHUB_PLUGIN_SCAN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginCache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/PluginCacheFingerprint.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/PluginCachePersistence.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/Scanner.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/ScannerDiscovery.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/ScannerRun.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/ScannerProcess.cpp"
)

set(IHUB_PLUGIN_INVENTORY_SOURCES
    ${IHUB_PLUGIN_SCAN_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInventory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/PluginInfo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/PluginInventoryRecords.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/scanner/PluginInventoryScan.cpp"
)

set(IHUB_SANDBOX_HOST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/SandboxHost.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxHostMidi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxHostProcess.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxHostState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxHostStatus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxHostStop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxHostTransport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/sandbox/SandboxWatchdog.cpp"
)

set(IHUB_SESSION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/DeviceController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/EditorHost.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/FxChainController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/MeterBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/MidiBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/PluginStateFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/PluginStateStore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/PluginStateStoreData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionBuild.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionControl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionFx.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionMeasure.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionPluginMirror.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionPluginSandbox.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionPluginState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionRender.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SessionWarm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/SlotSources.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/TransportControl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/WarmPoolController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/warm/WarmPoolArm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/warm/WarmPoolMeasurement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/warm/WarmPoolVoice.cpp"
    ${IHUB_SANDBOX_HOST_SOURCES}
)

juce_add_console_app(ihub-app-modes-test
    PRODUCT_NAME "ihub-app-modes-test"
)

target_sources(ihub-app-modes-test
    PRIVATE
        app/AppModesTest.cpp
        app/TestSupport.cpp
        app/CommandLine.cpp
        app/Modes.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/CommandLine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/Modes.cpp"
)

target_include_directories(ihub-app-modes-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-app-modes-test
    PRIVATE
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-app-modes-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-app-modes
    COMMAND ihub-app-modes-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(
        NAME ihub-app-list-devices
        COMMAND $<TARGET_FILE:instrument-hub> --list-devices
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-list-devices PROPERTIES
        PASS_REGULAR_EXPRESSION "Loopback"
    )

    add_test(
        NAME ihub-app-diagnostics
        COMMAND $<TARGET_FILE:instrument-hub> --diagnostics --diagnostics-ms 200
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-diagnostics PROPERTIES
        PASS_REGULAR_EXPRESSION "Instrument Hub diagnostics"
    )

    add_test(
        NAME ihub-app-selftest
        COMMAND $<TARGET_FILE:instrument-hub> --selftest
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-selftest PROPERTIES
        PASS_REGULAR_EXPRESSION "selftest: window on desktop at"
        ENVIRONMENT "DISPLAY=:0"
    )

    add_test(
        NAME ihub-app-serve
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/app/serve-smoke.sh"
                $<TARGET_FILE:instrument-hub>
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-serve PROPERTIES TIMEOUT 30)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA REQUIRED)

    add_executable(ihub-selftest
        selftest/AudioSmoke.cpp
        selftest/Options.cpp
        selftest/Alsa.cpp
        selftest/Wav.cpp
        selftest/Analysis.cpp
    )

    target_compile_features(ihub-selftest PRIVATE cxx_std_20)

    target_link_libraries(ihub-selftest
        PRIVATE
            ALSA::ALSA
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ihub-selftest PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set_target_properties(ihub-selftest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-selftest-audio
        COMMAND ihub-selftest --out "${CMAKE_CURRENT_BINARY_DIR}/out.wav"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

else()
    message(STATUS "ihub-selftest: ALSA smoke test is Linux-only; skipping")
endif()

add_subdirectory(plugins)

juce_add_console_app(ihub-device-manager-test
    PRODUCT_NAME "ihub-device-manager-test"
)

target_sources(ihub-device-manager-test
    PRIVATE
        device_manager/DeviceManagerTest.cpp
        device_manager/DeviceTypes.cpp
        ${IHUB_DEVICE_MANAGER_SOURCES}
)

target_include_directories(ihub-device-manager-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-device-manager-test
    PRIVATE
        JUCE_JACK=1
)

target_link_libraries(ihub-device-manager-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-device-manager-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-device-manager
    COMMAND ihub-device-manager-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-device-manager PROPERTIES DISABLED TRUE)
endif()

juce_add_console_app(ihub-audio-engine-test
    PRODUCT_NAME "ihub-audio-engine-test"
)

target_sources(ihub-audio-engine-test
    PRIVATE
        audio_engine/AudioEngineTest.cpp
        audio_engine/AllocationCount.cpp
        audio_engine/Fixtures.cpp
        audio_engine/Mix.cpp
        audio_engine/Scheduling.cpp
        audio_engine/Realtime.cpp
        audio_engine/Limiter.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-audio-engine-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-audio-engine-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-audio-engine-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-audio-engine-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

set_target_properties(ihub-audio-engine-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-audio-engine
    COMMAND ihub-audio-engine-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-instrument-test
        PRODUCT_NAME "ihub-instrument-test"
    )

    target_sources(ihub-instrument-test
        PRIVATE
            instruments/AudioInputInstrumentTest.cpp
            instruments/audio_input/TestSupport.cpp
            instruments/audio_input/Alsa.cpp
            instruments/audio_input/Unity.cpp
            instruments/audio_input/GainLatency.cpp
            instruments/audio_input/Channels.cpp
            instruments/audio_input/DirectMonitor.cpp
            instruments/audio_input/LatencyBudget.cpp
            instruments/audio_input/EngineInput.cpp
            instruments/audio_input/LoopbackMaster.cpp
            instruments/audio_input/LoopbackMonitor.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
    )

    target_include_directories(ihub-instrument-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_link_libraries(ihub-instrument-test
        PRIVATE
            ALSA::ALSA
            juce::juce_audio_basics
            juce::juce_audio_devices
            juce::juce_audio_processors_headless
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-instrument-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-instrument
        COMMAND ihub-instrument-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-instrument-test: ALSA loopback test is Linux-only; skipping")
endif()

juce_add_console_app(ihub-timing-monitor-test
    PRODUCT_NAME "ihub-timing-monitor-test"
)

target_sources(ihub-timing-monitor-test
    PRIVATE
        timing_monitor/TimingMonitorTest.cpp
        timing_monitor/TestSupport.cpp
        timing_monitor/Deadline.cpp
        timing_monitor/CallbackBoundary.cpp
        timing_monitor/Stall.cpp
        timing_monitor/DroppedBlocks.cpp
        timing_monitor/Window.cpp
        timing_monitor/Measured.cpp
        timing_monitor/Reset.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-timing-monitor-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-timing-monitor-test
    PRIVATE
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-timing-monitor-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-timing-monitor
    COMMAND ihub-timing-monitor-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-diagnostics-test
    PRODUCT_NAME "ihub-diagnostics-test"
)

find_package(Threads REQUIRED)

target_sources(ihub-diagnostics-test
    PRIVATE
        diag/DiagnosticsTest.cpp
        diag/TestSupport.cpp
        diag/Records.cpp
        diag/Names.cpp
        diag/Concurrency.cpp
        diag/CrashFormat.cpp
        diag/SignalHandler.cpp
        diag/Plugins.cpp
        diag/Report.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/CrashDump.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/Diagnostics.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/LogRing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/LogSink.cpp"
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-diagnostics-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-diagnostics-test
    PRIVATE
        Threads::Threads
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-diagnostics-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-diagnostics
    COMMAND ihub-diagnostics-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-latency-probe-test
    PRODUCT_NAME "ihub-latency-probe-test"
)

target_sources(ihub-latency-probe-test
    PRIVATE
        latency_probe/LatencyProbeTest.cpp
        latency_probe/TestSupport.cpp
        latency_probe/CrossCorrelation.cpp
        latency_probe/Loopback.cpp
        ${IHUB_LATENCY_PROBE_SOURCES}
)

target_include_directories(ihub-latency-probe-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-latency-probe-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-latency-probe-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-latency-probe
    COMMAND ihub-latency-probe-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-latency-probe PROPERTIES DISABLED TRUE)
endif()

if(IHUB_ENABLE_RT_GUARD)
    juce_add_console_app(ihub-rt-guard-test
        PRODUCT_NAME "ihub-rt-guard-test"
    )

    target_sources(ihub-rt-guard-test
        PRIVATE
            rt_guard/RtGuardTest.cpp
            rt_guard/Fixtures.cpp
            rt_guard/CompiledIn.cpp
            rt_guard/Scope.cpp
            rt_guard/NormalPath.cpp
            rt_guard/NonStrict.cpp
            rt_guard/Strict.cpp
            rt_guard/Disabled.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
    )

    target_include_directories(ihub-rt-guard-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-rt-guard-test PRIVATE IHUB_RT_GUARD=1)

    target_link_libraries(ihub-rt-guard-test
        PRIVATE
            juce::juce_audio_devices
            juce::juce_audio_processors_headless
            juce::juce_core
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_definitions(ihub-rt-guard-test PRIVATE
            IHUB_RT_GUARD_WRAP_MALLOC=1
            IHUB_RT_GUARD_WRAP_LOCKS=1
        )

        target_link_options(ihub-rt-guard-test PRIVATE
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
            target_compile_options(ihub-rt-guard-test PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
        endif()
    endif()

    set_target_properties(ihub-rt-guard-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-rt-guard
        COMMAND ihub-rt-guard-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-rt-guard-test: disabled (IHUB_ENABLE_RT_GUARD=OFF)")
endif()

juce_add_console_app(ihub-fx-chain-test
    PRODUCT_NAME "ihub-fx-chain-test"
)

target_sources(ihub-fx-chain-test
    PRIVATE
        fx_chain/FxChainTest.cpp
        fx_chain/Fixtures.cpp
        fx_chain/EmptyChain.cpp
        fx_chain/ChainOrder.cpp
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
)

target_include_directories(ihub-fx-chain-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-fx-chain-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-fx-chain-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-fx-chain-test TestPlugin_VST3)

set_target_properties(ihub-fx-chain-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-fx-chain
    COMMAND ihub-fx-chain-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-plugin-fx-test
    PRODUCT_NAME "ihub-plugin-fx-test"
)

target_sources(ihub-plugin-fx-test
    PRIVATE
        instruments/PluginFxTest.cpp
        instruments/plugin_fx/Fixtures.cpp
        instruments/plugin_fx/GainLatency.cpp
        instruments/plugin_fx/State.cpp
        instruments/plugin_fx/Editor.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/PluginFx.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_PLUGIN_WINDOW_SOURCES}
)

target_include_directories(ihub-plugin-fx-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-plugin-fx-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-plugin-fx-test
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-plugin-fx-test TestPlugin_VST3)

set_target_properties(ihub-plugin-fx-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-plugin-fx
    COMMAND ihub-plugin-fx-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

set_tests_properties(ihub-plugin-fx PROPERTIES
    ENVIRONMENT "DISPLAY=:0"
)

juce_add_console_app(ihub-slot-switcher-test
    PRODUCT_NAME "ihub-slot-switcher-test"
)

target_sources(ihub-slot-switcher-test
    PRIVATE
        slot_switcher/SlotSwitcherTest.cpp
        slot_switcher/AllocationCount.cpp
        slot_switcher/Fixtures.cpp
        slot_switcher/Continuity.cpp
        slot_switcher/Realtime.cpp
        slot_switcher/Disarm.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
)

target_include_directories(ihub-slot-switcher-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-slot-switcher-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

target_link_libraries(ihub-slot-switcher-test
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-slot-switcher-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-slot-switcher
    COMMAND ihub-slot-switcher-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-warm-pool-test
    PRODUCT_NAME "ihub-warm-pool-test"
)

target_sources(ihub-warm-pool-test
    PRIVATE
        warm_pool/WarmPoolTest.cpp
        warm_pool/Fixtures.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
)

target_include_directories(ihub-warm-pool-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-warm-pool-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-warm-pool-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-warm-pool-test TestPlugin_VST3)

set_target_properties(ihub-warm-pool-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-warm-pool
    COMMAND ihub-warm-pool-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-switch-continuity-test
    PRODUCT_NAME "ihub-switch-continuity-test"
)

target_sources(ihub-switch-continuity-test
    PRIVATE
        audio/SwitchContinuity.cpp
        audio/TestSupport.cpp
        audio/switch_continuity/Cases.cpp
        audio/switch_continuity/DeltaDetector.cpp
        audio/switch_continuity/Render.cpp
        audio/switch_continuity/SineSource.cpp
        audio/switch_continuity/SpectralDetector.cpp
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
)

target_include_directories(ihub-switch-continuity-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-switch-continuity-test
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-switch-continuity-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-switch-continuity
    COMMAND ihub-switch-continuity-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-session-smoke-test
    PRODUCT_NAME "ihub-session-smoke-test"
)

target_sources(ihub-session-smoke-test
    PRIVATE
        audio/SessionSmoke.cpp
        audio/TestSupport.cpp
        audio/session_smoke/Composition.cpp
        audio/session_smoke/Fixtures.cpp
        audio/session_smoke/HardSwitch.cpp
        audio/session_smoke/Switch.cpp
        ${IHUB_SESSION_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-session-smoke-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-session-smoke-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        IHUB_RT_GUARD=1
)

target_link_libraries(ihub-session-smoke-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(ihub-session-smoke-test PRIVATE
        IHUB_RT_GUARD_WRAP_MALLOC=1
        IHUB_RT_GUARD_WRAP_LOCKS=1
    )

    target_link_options(ihub-session-smoke-test PRIVATE
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
        target_compile_options(ihub-session-smoke-test PRIVATE
            -Wno-mismatched-new-delete
        )
    endif()
endif()

set_target_properties(ihub-session-smoke-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-session-smoke
    COMMAND ihub-session-smoke-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(
        NAME ihub-session-smoke-app
        COMMAND $<TARGET_FILE:instrument-hub> --session-smoke
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-session-smoke-app PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
endif()

juce_add_console_app(ihub-session-api-test
    PRODUCT_NAME "ihub-session-api-test"
)

target_sources(ihub-session-api-test
    PRIVATE
        api/SessionApiTest.cpp
        api/AudioMeasurements.cpp
        api/EventStream.cpp
        api/JsonSupport.cpp
        api/SessionRig.cpp
        api/TestSupport.cpp
        api/session_api/Arm.cpp
        api/session_api/ArmRejection.cpp
        api/session_api/Meters.cpp
        api/session_api/PatchMix.cpp
        api/session_api/Transport.cpp
        ${IHUB_SESSION_SOURCES}
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-session-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-session-api-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        IHUB_RT_GUARD=1
)

target_link_libraries(ihub-session-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-session-api-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-session-api
    COMMAND ihub-session-api-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-patch-fx-api-test
    PRODUCT_NAME "ihub-patch-fx-api-test"
)

target_sources(ihub-patch-fx-api-test
    PRIVATE
        api/PatchFxApiTest.cpp
        api/AudioMeasurements.cpp
        api/FxChainJson.cpp
        api/JsonSupport.cpp
        api/RequestDispatch.cpp
        api/SessionRig.cpp
        api/TestSupport.cpp
        api/patch_fx_api/Bypass.cpp
        api/patch_fx_api/Chain.cpp
        api/patch_fx_api/HostedInsert.cpp
        api/patch_fx_api/Replace.cpp
        api/patch_fx_api/Validation.cpp
        ${IHUB_SESSION_SOURCES}
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-patch-fx-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-patch-fx-api-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-patch-fx-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-patch-fx-api-test TestPlugin_VST3)

set_target_properties(ihub-patch-fx-api-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-patch-fx-api
    COMMAND ihub-patch-fx-api-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-instrument-page-api-test
    PRODUCT_NAME "ihub-instrument-page-api-test"
)

target_sources(ihub-instrument-page-api-test
    PRIVATE
        api/InstrumentPageApiTest.cpp
        api/AudioMeasurements.cpp
        api/FxChainJson.cpp
        api/JsonSupport.cpp
        api/RequestDispatch.cpp
        api/SessionRig.cpp
        api/TestSupport.cpp
        api/instrument_page_api/Inserts.cpp
        api/instrument_page_api/Mixer.cpp
        api/instrument_page_api/PluginSource.cpp
        api/instrument_page_api/Source.cpp
        ${IHUB_SESSION_SOURCES}
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-instrument-page-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-instrument-page-api-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-instrument-page-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-instrument-page-api-test TestPlugin_VST3)

set_target_properties(ihub-instrument-page-api-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-instrument-page-api
    COMMAND ihub-instrument-page-api-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-patch-state-capture-api-test
    PRODUCT_NAME "ihub-patch-state-capture-api-test"
)

target_sources(ihub-patch-state-capture-api-test
    PRIVATE
        api/PatchStateCaptureApiTest.cpp
        api/JsonSupport.cpp
        api/RequestDispatch.cpp
        api/SessionRig.cpp
        api/TestSupport.cpp
        api/patch/Autosave.cpp
        api/patch_state_capture_api/Capture.cpp
        api/patch_state_capture_api/Fixtures.cpp
        api/patch_state_capture_api/Restart.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectAutosave.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectCapture.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectSession.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/SessionSlotMapping.cpp"
        ${IHUB_PROJECT_SOURCES}
        ${IHUB_MAPPING_SOURCES}
        ${IHUB_SESSION_SOURCES}
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-patch-state-capture-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-patch-state-capture-api-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-patch-state-capture-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-patch-state-capture-api-test TestPlugin_VST3)

set_target_properties(ihub-patch-state-capture-api-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-patch-state-capture-api
    COMMAND ihub-patch-state-capture-api-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-plugin-editor-api-test
        PRODUCT_NAME "ihub-plugin-editor-api-test"
    )

    target_sources(ihub-plugin-editor-api-test
        PRIVATE
            api/PluginEditorApiTest.cpp
            api/JsonSupport.cpp
            api/RequestDispatch.cpp
            api/SessionRig.cpp
            api/TestSupport.cpp
            api/plugin_editor_api/AnalogLab.cpp
            api/plugin_editor_api/Fixtures.cpp
            api/plugin_editor_api/Lifecycle.cpp
            api/plugin_editor_api/SourceChange.cpp
            ${IHUB_SESSION_SOURCES}
            ${IHUB_DISCOVERY_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
            ${IHUB_API_ROUTES_SOURCES}
            ${IHUB_API_SERVER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_PLUGIN_WINDOW_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-plugin-editor-api-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-plugin-editor-api-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    target_link_libraries(ihub-plugin-editor-api-test
        PRIVATE
            Threads::Threads
            ihub-httplib
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_core
            juce::juce_dsp
            juce::juce_events
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-plugin-editor-api-test TestPlugin_VST3)

    set_target_properties(ihub-plugin-editor-api-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-plugin-editor-api
        COMMAND ihub-plugin-editor-api-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-plugin-editor-api PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
else()
    message(STATUS "ihub-plugin-editor-api-test: Linux-only; skipping")
endif()

juce_add_console_app(ihub-session-warm-test
    PRODUCT_NAME "ihub-session-warm-test"
)

target_sources(ihub-session-warm-test
    PRIVATE
        session_warm/SessionWarmPoolTest.cpp
        ${IHUB_SESSION_SOURCES}
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-session-warm-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-session-warm-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(ihub-session-warm-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-session-warm-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-session-warm
    COMMAND ihub-session-warm-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-shutdown-test
    PRODUCT_NAME "ihub-shutdown-test"
)

target_sources(ihub-shutdown-test
    PRIVATE
        app/shutdown/Scenarios.cpp
        app/shutdown/ShutdownTest.cpp
        app/shutdown/TestSupport.cpp
        ${IHUB_SESSION_SOURCES}
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-shutdown-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-shutdown-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(ihub-shutdown-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-shutdown-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-shutdown
    COMMAND ihub-shutdown-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

set_tests_properties(ihub-shutdown PROPERTIES TIMEOUT 20)

add_executable(ihub-launchpad-protocol-test
    controller/LaunchpadProtocolTest.cpp
    controller/TestSupport.cpp
    controller/protocol/Buttons.cpp
    controller/protocol/Frames.cpp
    controller/protocol/Grid.cpp
    controller/protocol/Hex.cpp
    controller/protocol/Leds.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadProtocol.cpp"
)

target_compile_features(ihub-launchpad-protocol-test PRIVATE cxx_std_20)

target_include_directories(ihub-launchpad-protocol-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-launchpad-protocol-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-launchpad-protocol-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-launchpad-protocol
    COMMAND ihub-launchpad-protocol-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(Threads REQUIRED)

add_executable(ihub-launchpad-mapping-test
    controller/LaunchpadMappingTest.cpp
    controller/TestSupport.cpp
    controller/launchpad_mapping/Document.cpp
    controller/launchpad_mapping/Fixtures.cpp
    controller/launchpad_mapping/Mapping.cpp
    controller/launchpad_mapping/Validation.cpp
    ${IHUB_MAPPING_SOURCES}
)

target_compile_features(ihub-launchpad-mapping-test PRIVATE cxx_std_20)

target_include_directories(ihub-launchpad-mapping-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-launchpad-mapping-test
    PRIVATE
        JUCE_USE_CURL=0
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
)

target_link_libraries(ihub-launchpad-mapping-test
    PRIVATE
        Threads::Threads
        juce::juce_core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-launchpad-mapping-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-launchpad-mapping-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-launchpad-mapping
    COMMAND ihub-launchpad-mapping-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-launchpad-driver-test
    PRODUCT_NAME "ihub-launchpad-driver-test"
)

target_sources(ihub-launchpad-driver-test
    PRIVATE
        controller/LaunchpadDriverTest.cpp
        controller/TestSupport.cpp
        controller/driver/Grid.cpp
        controller/driver/Handshake.cpp
        controller/driver/LedStream.cpp
        controller/driver/PatchLeds.cpp
        controller/driver/Rig.cpp
        ${IHUB_LAUNCHPAD_DRIVER_SOURCES}
        ${IHUB_MAPPING_SOURCES}
)

target_include_directories(ihub-launchpad-driver-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-launchpad-driver-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
)

target_link_libraries(ihub-launchpad-driver-test
    PRIVATE
        Threads::Threads
        juce::juce_audio_devices
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-launchpad-driver-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-launchpad-driver
    COMMAND ihub-launchpad-driver-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(Threads REQUIRED)

juce_add_console_app(ihub-mapping-test
    PRODUCT_NAME "ihub-mapping-test"
)

target_sources(ihub-mapping-test
    PRIVATE
        controller/MappingTest.cpp
        controller/TestSupport.cpp
        controller/mapping/Document.cpp
        controller/mapping/Fixtures.cpp
        controller/mapping/HotReload.cpp
        controller/mapping/Render.cpp
        controller/mapping/Validation.cpp
        controller/mapping/ValidationStructure.cpp
        controller/mapping/ValidationSupport.cpp
        controller/mapping/ValidationValues.cpp
        ${IHUB_MAPPING_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
)

target_include_directories(ihub-mapping-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-mapping-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
        "IHUB_SET_SCHEMA=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/schema/set.schema.json\""
)

target_link_libraries(ihub-mapping-test
    PRIVATE
        Threads::Threads
        juce::juce_audio_basics
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-mapping-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-mapping
    COMMAND ihub-mapping-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-api-test
    PRODUCT_NAME "ihub-api-test"
)

target_sources(ihub-api-test
    PRIVATE
        api/ApiTest.cpp
        api/JsonSupport.cpp
        api/TestSupport.cpp
        api/core_api/Arm.cpp
        api/core_api/Auth.cpp
        api/core_api/Fixtures.cpp
        api/core_api/Meters.cpp
        api/core_api/RouteRegistration.cpp
        api/core_api/Settings.cpp
        api/core_api/State.cpp
        api/core_api/Transport.cpp
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
)

target_include_directories(ihub-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-api-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-api
    COMMAND ihub-api-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-pairing-test
    PRODUCT_NAME "ihub-pairing-test"
)

target_sources(ihub-pairing-test
    PRIVATE
        api/PairingTest.cpp
        api/JsonSupport.cpp
        api/TestSupport.cpp
        api/pairing/Fixtures.cpp
        api/pairing/LanOff.cpp
        api/pairing/Pairing.cpp
        api/pairing/StartupFlag.cpp
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
)

target_include_directories(ihub-pairing-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-pairing-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-pairing-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-pairing
    COMMAND ihub-pairing-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-state-broadcast-test
        PRODUCT_NAME "ihub-state-broadcast-test"
    )

    target_sources(ihub-state-broadcast-test
        PRIVATE
            api/StateBroadcastTest.cpp
            api/EventStream.cpp
            api/JsonSupport.cpp
            api/TestSupport.cpp
            api/state_broadcast/Accounting.cpp
            api/state_broadcast/Fixtures.cpp
            api/state_broadcast/InitialState.cpp
            api/state_broadcast/PadPath.cpp
            api/state_broadcast/RevisionPath.cpp
            api/state_broadcast/UiPath.cpp
            ${IHUB_DISCOVERY_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
            ${IHUB_API_ROUTES_SOURCES}
            ${IHUB_API_SERVER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
            ${IHUB_LAUNCHPAD_DRIVER_SOURCES}
    )

    target_include_directories(ihub-state-broadcast-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_link_libraries(ihub-state-broadcast-test
        PRIVATE
            Threads::Threads
            ihub-httplib
            juce::juce_audio_devices
            juce::juce_core
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-state-broadcast-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-state-broadcast
        COMMAND ihub-state-broadcast-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-state-broadcast-test: the live server test is Linux-only; skipping")
endif()

juce_add_console_app(ihub-audio-settings-test
    PRODUCT_NAME "ihub-audio-settings-test"
)

target_sources(ihub-audio-settings-test
    PRIVATE
        api/AudioSettingsTest.cpp
        api/JsonSupport.cpp
        api/TestSupport.cpp
        api/audio_settings/DeviceBridge.cpp
        api/audio_settings/Fixtures.cpp
        api/audio_settings/Routes.cpp
        ${IHUB_DISCOVERY_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        ${IHUB_AUDIO_SETTINGS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        ${IHUB_LATENCY_PROBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-audio-settings-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-audio-settings-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-audio-settings-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-audio-settings
    COMMAND ihub-audio-settings-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-audio-settings PROPERTIES DISABLED TRUE)
endif()

juce_add_console_app(ihub-midi-api-test
    PRODUCT_NAME "ihub-midi-api-test"
)

target_sources(ihub-midi-api-test
    PRIVATE
        api/MidiApiTest.cpp
        api/JsonSupport.cpp
        api/TestSupport.cpp
        api/midi_api/DeviceEnumeration.cpp
        api/midi_api/Fixtures.cpp
        api/midi_api/PadTest.cpp
        api/midi_api/Routes.cpp
        ${IHUB_DISCOVERY_SOURCES}
        ${IHUB_MIDI_BRIDGE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        ${IHUB_API_ROUTES_SOURCES}
        ${IHUB_API_SERVER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        ${IHUB_LAUNCHPAD_DRIVER_SOURCES}
)

target_include_directories(ihub-midi-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-midi-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
        juce::juce_audio_devices
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-midi-api-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-midi-api
    COMMAND ihub-midi-api-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-midi-api PROPERTIES DISABLED TRUE)
endif()

juce_add_console_app(ihub-discovery-test
    PRODUCT_NAME "ihub-discovery-test"
)

target_sources(ihub-discovery-test
    PRIVATE
        api/DiscoveryTest.cpp
        api/TestSupport.cpp
        api/discovery/Advertisement.cpp
        api/discovery/Dns.cpp
        api/discovery/Helpers.cpp
        api/discovery/Query.cpp
        api/discovery/Socket.cpp
        ${IHUB_DISCOVERY_SOURCES}
)

target_include_directories(ihub-discovery-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-discovery-test
    PRIVATE
        Threads::Threads
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-discovery-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-discovery
    COMMAND ihub-discovery-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-compressor-test
    PRODUCT_NAME "ihub-compressor-test"
)

target_sources(ihub-compressor-test
    PRIVATE
        dsp/CompressorTest.cpp
        dsp/AllocationCount.cpp
        dsp/TestSupport.cpp
        dsp/compressor/Bypass.cpp
        dsp/compressor/Dynamics.cpp
        dsp/compressor/Fixtures.cpp
        dsp/compressor/Latency.cpp
        dsp/compressor/MakeupGain.cpp
        dsp/compressor/Realtime.cpp
        dsp/compressor/State.cpp
        dsp/compressor/StaticCurve.cpp
        dsp/compressor/StereoLink.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
)

target_include_directories(ihub-compressor-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-compressor-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-compressor-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-compressor-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-compressor-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-compressor
    COMMAND ihub-compressor-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-delay-test
    PRODUCT_NAME "ihub-delay-test"
)

target_sources(ihub-delay-test
    PRIVATE
        dsp/DelayTest.cpp
        dsp/AllocationCount.cpp
        dsp/TestSupport.cpp
        dsp/delay/Acceptance.cpp
        dsp/delay/Bypass.cpp
        dsp/delay/Feedback.cpp
        dsp/delay/Fixtures.cpp
        dsp/delay/FreeTime.cpp
        dsp/delay/Realtime.cpp
        dsp/delay/State.cpp
        dsp/delay/SyncTable.cpp
        dsp/delay/Transport.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
)

target_include_directories(ihub-delay-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-delay-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-delay-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-delay-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-delay-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-delay
    COMMAND ihub-delay-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-reverb-test
    PRODUCT_NAME "ihub-reverb-test"
)

target_sources(ihub-reverb-test
    PRIVATE
        dsp/ReverbTest.cpp
        dsp/AllocationCount.cpp
        dsp/TestSupport.cpp
        dsp/reverb/Damping.cpp
        dsp/reverb/Decay.cpp
        dsp/reverb/DryPath.cpp
        dsp/reverb/Fixtures.cpp
        dsp/reverb/PreDelay.cpp
        dsp/reverb/Realtime.cpp
        dsp/reverb/Stability.cpp
        dsp/reverb/State.cpp
        dsp/reverb/Width.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
)

target_include_directories(ihub-reverb-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-reverb-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-reverb-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-reverb-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-reverb-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-reverb
    COMMAND ihub-reverb-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-eq-test
    PRODUCT_NAME "ihub-eq-test"
)

target_sources(ihub-eq-test
    PRIVATE
        dsp/EqTest.cpp
        dsp/AllocationCount.cpp
        dsp/TestSupport.cpp
        dsp/eq/Flat.cpp
        dsp/eq/Fixtures.cpp
        dsp/eq/Realtime.cpp
        dsp/eq/Response.cpp
        dsp/eq/State.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
)

target_include_directories(ihub-eq-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-eq-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-eq-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-eq-test
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-eq-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-eq
    COMMAND ihub-eq-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(ihub-transport-test
    audio/TransportTest.cpp
    audio/AllocationCount.cpp
    audio/TestSupport.cpp
    audio/transport/Acceptance.cpp
    audio/transport/ExternalSync.cpp
    audio/transport/Fixtures.cpp
    audio/transport/Position.cpp
    audio/transport/Projection.cpp
    audio/transport/Realtime.cpp
    audio/transport/Snapshot.cpp
    audio/transport/StartStop.cpp
    audio/transport/TapTempo.cpp
    audio/transport/Tempo.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-transport-test PRIVATE cxx_std_20)

target_include_directories(ihub-transport-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-transport-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-transport-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-transport-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-transport
    COMMAND ihub-transport-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(ihub-clock-out-test
    audio/ClockOutTest.cpp
    audio/AllocationCount.cpp
    audio/TestSupport.cpp
    audio/clock_out/Acceptance.cpp
    audio/clock_out/Fixtures.cpp
    audio/clock_out/Midi.cpp
    audio/clock_out/Overflow.cpp
    audio/clock_out/Realtime.cpp
    audio/clock_out/Rewind.cpp
    audio/clock_out/Tempo.cpp
    audio/clock_out/Transport.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/ClockOut.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-clock-out-test PRIVATE cxx_std_20)

target_include_directories(ihub-clock-out-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-clock-out-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-clock-out-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-clock-out-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-clock-out
    COMMAND ihub-clock-out-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(ihub-clock-in-test
    audio/ClockInTest.cpp
    audio/AllocationCount.cpp
    audio/TestSupport.cpp
    audio/clock_in/Acceptance.cpp
    audio/clock_in/ClockLoss.cpp
    audio/clock_in/InternalSync.cpp
    audio/clock_in/Jitter.cpp
    audio/clock_in/Midi.cpp
    audio/clock_in/Queue.cpp
    audio/clock_in/Realtime.cpp
    audio/clock_in/Reset.cpp
    audio/clock_in/TempoChange.cpp
    audio/clock_in/Tempos.cpp
    audio/clock_in/Transport.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/ClockIn.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-clock-in-test PRIVATE cxx_std_20)

target_include_directories(ihub-clock-in-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-clock-in-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-clock-in-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-clock-in-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-clock-in
    COMMAND ihub-clock-in-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(ihub-tap-tempo-test
    audio/TapTempoTest.cpp
    audio/AllocationCount.cpp
    audio/TestSupport.cpp
    audio/tap_tempo/Acceptance.cpp
    audio/tap_tempo/Fixtures.cpp
    audio/tap_tempo/Median.cpp
    audio/tap_tempo/Range.cpp
    audio/tap_tempo/Realtime.cpp
    audio/tap_tempo/Reset.cpp
    audio/tap_tempo/SequenceReset.cpp
    audio/tap_tempo/TransportIntegration.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-tap-tempo-test PRIVATE cxx_std_20)

target_include_directories(ihub-tap-tempo-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-tap-tempo-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-tap-tempo-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-tap-tempo-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-tap-tempo
    COMMAND ihub-tap-tempo-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-set-file-test
    PRODUCT_NAME "ihub-set-file-test"
)

target_sources(ihub-set-file-test
    PRIVATE
        project/SetFileTest.cpp
        ${IHUB_PROJECT_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-set-file-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-set-file-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
)

target_link_libraries(ihub-set-file-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-set-file-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-set-file
    COMMAND ihub-set-file-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-migrations-test
    PRODUCT_NAME "ihub-migrations-test"
)

target_sources(ihub-migrations-test
    PRIVATE
        project/MigrationsTest.cpp
        ${IHUB_PROJECT_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-migrations-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-migrations-test
    PRIVATE
        "IHUB_V1_SET_FIXTURE=\"${CMAKE_CURRENT_SOURCE_DIR}/project/fixtures/set-v1.ihub\""
)

target_link_libraries(ihub-migrations-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-migrations-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-migrations
    COMMAND ihub-migrations-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-patch-persistence-test
    PRODUCT_NAME "ihub-patch-persistence-test"
)

target_sources(ihub-patch-persistence-test
    PRIVATE
        project/PatchPersistenceTest.cpp
        ${IHUB_MAPPING_SOURCES}
        ${IHUB_PROJECT_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
)

target_include_directories(ihub-patch-persistence-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-patch-persistence-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
)

target_link_libraries(ihub-patch-persistence-test
    PRIVATE
        Threads::Threads
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-patch-persistence-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-patch-persistence
    COMMAND ihub-patch-persistence-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-project-test
    PRODUCT_NAME "ihub-project-test"
)

target_sources(ihub-project-test
    PRIVATE
        project/ProjectTest.cpp
        project/project/Autosave.cpp
        project/project/Import.cpp
        project/project/Reload.cpp
        project/project/SessionDescription.cpp
        project/project/Support.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/CommandLine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/MappingImport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectAutosave.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectImport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectPaths.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectSession.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/SessionSlotMapping.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/SourceImport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/fx/FxInsertType.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/session/PluginStateFile.cpp"
        ${IHUB_PROJECT_SOURCES}
        ${IHUB_MAPPING_SOURCES}
)

target_include_directories(ihub-project-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-project-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
        "IHUB_PLUGIN_STATE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.plugin-state\""
        "IHUB_SOURCES_JSON=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.sources.json\""
)

target_link_libraries(ihub-project-test
    PRIVATE
        Threads::Threads
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-project-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-project
    COMMAND ihub-project-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-sample-loader-test
    PRODUCT_NAME "ihub-sample-loader-test"
)

target_sources(ihub-sample-loader-test
    PRIVATE
        sampler/SampleLoaderTest.cpp
        sampler/Fixtures.cpp
        sampler/Measurements.cpp
        sampler/TestSupport.cpp
        sampler/sample/Assertions.cpp
        sampler/sample/Decode.cpp
        sampler/sample/Failure.cpp
        sampler/sample/Handoff.cpp
        sampler/sample/Resample.cpp
        sampler/sample/Stress.cpp
        ${IHUB_SAMPLE_SOURCES}
)

target_include_directories(ihub-sample-loader-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-sample-loader-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-sample-loader-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-sample-loader-test
    PRIVATE
        juce::juce_audio_formats
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-sample-loader-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-sample-loader
    COMMAND ihub-sample-loader-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-kit-loader-test
    PRODUCT_NAME "ihub-kit-loader-test"
)

target_sources(ihub-kit-loader-test
    PRIVATE
        sampler/KitLoaderTest.cpp
        sampler/Fixtures.cpp
        sampler/Measurements.cpp
        sampler/TestSupport.cpp
        sampler/kit/AudioFixtures.cpp
        sampler/kit/Fixtures.cpp
        sampler/kit/Layers.cpp
        sampler/kit/Load.cpp
        sampler/kit/Parse.cpp
        sampler/kit/RenderHit.cpp
        sampler/kit/Resolve.cpp
        sampler/kit/Validation.cpp
        ${IHUB_SAMPLER_KIT_SOURCES}
        ${IHUB_SAMPLE_SOURCES}
)

target_include_directories(ihub-kit-loader-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-kit-loader-test
    PRIVATE
        "IHUB_KIT_SCHEMA=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/schema/kit.schema.json\""
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-kit-loader-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-kit-loader-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-kit-loader-test
    PRIVATE
        juce::juce_audio_formats
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-kit-loader-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-kit-loader
    COMMAND ihub-kit-loader-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-sampler-instrument-test
    PRODUCT_NAME "ihub-sampler-instrument-test"
)

target_sources(ihub-sampler-instrument-test
    PRIVATE
        sampler/SamplerInstrumentTest.cpp
        sampler/Fixtures.cpp
        sampler/TestSupport.cpp
        sampler/voices/Choke.cpp
        sampler/voices/Fixtures.cpp
        sampler/voices/Gate.cpp
        sampler/voices/Pool.cpp
        sampler/voices/Realtime.cpp
        sampler/voices/Setup.cpp
        sampler/voices/Stealing.cpp
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-sampler-instrument-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-sampler-instrument-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(ihub-sampler-instrument-test PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_link_libraries(ihub-sampler-instrument-test
    PRIVATE
        juce::juce_audio_formats
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-sampler-instrument-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-sampler-instrument
    COMMAND ihub-sampler-instrument-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-ui-snapshot-test
        PRODUCT_NAME "ihub-ui-snapshot-test"
    )

    target_sources(ihub-ui-snapshot-test
        PRIVATE
            ui/screenshot.cpp
    )

    target_link_libraries(ihub-ui-snapshot-test
        PRIVATE
            juce::juce_graphics
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-ui-snapshot-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_dependencies(ihub-ui-snapshot-test instrument-hub)

    add_test(
        NAME ihub-ui-snapshot
        COMMAND ihub-ui-snapshot-test
                $<TARGET_FILE:instrument-hub>
                "${CMAKE_CURRENT_BINARY_DIR}/ui-snapshot.png"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_tests_properties(ihub-ui-snapshot PROPERTIES DISABLED TRUE)
    endif()
else()
    message(STATUS "ihub-ui-snapshot-test: X11/WebKit snapshot is Linux-only; skipping")
endif()

find_program(IHUB_UI_PAGES_PNPM NAMES pnpm DOC "pnpm used by the page snapshot test")

if(IHUB_UI_PAGES_PNPM)
    set(IHUB_UI_PAGES_USES_COREPACK 0)
else()
    find_program(IHUB_UI_PAGES_COREPACK NAMES corepack DOC "corepack used by the page snapshot test")

    if(IHUB_UI_PAGES_COREPACK)
        set(IHUB_UI_PAGES_PNPM "${IHUB_UI_PAGES_COREPACK}")
        set(IHUB_UI_PAGES_USES_COREPACK 1)
    endif()
endif()

if(IHUB_UI_PAGES_PNPM)
    juce_add_console_app(ihub-ui-pages-test
        PRODUCT_NAME "ihub-ui-pages-test"
    )

    target_sources(ihub-ui-pages-test
        PRIVATE
            ui/Pages.cpp
    )

    target_compile_definitions(ihub-ui-pages-test
        PRIVATE
            "IHUB_UI_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../ui\""
            "IHUB_TEST_ARTIFACTS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/artifacts\""
            "IHUB_UI_PNPM=\"${IHUB_UI_PAGES_PNPM}\""
            "IHUB_UI_PNPM_USES_COREPACK=${IHUB_UI_PAGES_USES_COREPACK}"
    )

    target_link_libraries(ihub-ui-pages-test
        PRIVATE
            juce::juce_core
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-ui-pages-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-ui-pages
        COMMAND ihub-ui-pages-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-ui-pages-test: pnpm/corepack is not installed; skipping the page snapshots")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-session-midi-test
        PRODUCT_NAME "ihub-session-midi-test"
    )

    target_sources(ihub-session-midi-test
        PRIVATE
            session_midi/SessionMidiTest.cpp
            ${IHUB_SESSION_SOURCES}
            ${IHUB_MIDI_BRIDGE_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            ${IHUB_LAUNCHPAD_DRIVER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-session-midi-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-session-midi-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_NOTETONE_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:NoteTonePlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    target_link_libraries(ihub-session-midi-test
        PRIVATE
            Threads::Threads
            ALSA::ALSA
            juce::juce_core
            juce::juce_audio_devices
            juce::juce_dsp
            juce::juce_audio_formats
            juce::juce_audio_processors_headless
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-session-midi-test NoteTonePlugin_VST3)

    set_target_properties(ihub-session-midi-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-session-midi
        COMMAND ihub-session-midi-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-session-midi-test: ALSA MIDI forwarding is Linux-only; skipping")
endif()

juce_add_console_app(ihub-session-plugin-state-test
    PRODUCT_NAME "ihub-session-plugin-state-test"
)

target_sources(ihub-session-plugin-state-test
    PRIVATE
        plugin_state/SessionPluginStateTest.cpp
        ${IHUB_SESSION_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        ${IHUB_BUS_SOURCES}
        ${IHUB_DEVICE_MANAGER_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        ${IHUB_FX_RACK_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        ${IHUB_LATENCY_PROBE_SOURCES}
        ${IHUB_RT_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        ${IHUB_WARM_POOL_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
        ${IHUB_SAMPLER_SOURCES}
)

target_include_directories(ihub-session-plugin-state-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-session-plugin-state-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-session-plugin-state-test
    PRIVATE
        Threads::Threads
        juce::juce_audio_devices
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors_headless
        juce::juce_core
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-session-plugin-state-test TestPlugin_VST3)

set_target_properties(ihub-session-plugin-state-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-session-plugin-state
    COMMAND ihub-session-plugin-state-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-session-sandbox-death-test
        PRODUCT_NAME "ihub-session-sandbox-death-test"
    )

    target_sources(ihub-session-sandbox-death-test
        PRIVATE
            plugin_state/SessionSandboxDeathTest.cpp
            ${IHUB_SESSION_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-session-sandbox-death-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-session-sandbox-death-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
            "IHUB_SANDBOX_HELPER=\"$<TARGET_FILE:ihub-sandbox>\""
    )

    target_link_libraries(ihub-session-sandbox-death-test
        PRIVATE
            Threads::Threads
            juce::juce_audio_devices
            juce::juce_dsp
            juce::juce_audio_formats
            juce::juce_audio_processors_headless
            juce::juce_core
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-session-sandbox-death-test
        ihub-sandbox
        TestPlugin_VST3
    )

    set_target_properties(ihub-session-sandbox-death-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-session-sandbox-death
        COMMAND ihub-session-sandbox-death-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-session-editor-follow-test
        PRODUCT_NAME "ihub-session-editor-follow-test"
    )

    target_sources(ihub-session-editor-follow-test
        PRIVATE
            plugin_state/SessionEditorFollowTest.cpp
            ${IHUB_SESSION_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_PLUGIN_WINDOW_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-session-editor-follow-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-session-editor-follow-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    target_link_libraries(ihub-session-editor-follow-test
        PRIVATE
            Threads::Threads
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_core
            juce::juce_dsp
            juce::juce_events
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-session-editor-follow-test TestPlugin_VST3)

    set_target_properties(ihub-session-editor-follow-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-session-editor-follow
        COMMAND ihub-session-editor-follow-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-session-editor-follow PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
else()
    message(STATUS "ihub-session-editor-follow-test: Linux-only; skipping")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-e2e-test
        PRODUCT_NAME "ihub-e2e-test"
    )

    target_sources(ihub-e2e-test
        PRIVATE
            e2e/PadToAudio.cpp
            e2e/Check.cpp
            e2e/project/ProjectScenario.cpp
            e2e/pad_audio/AlsaPcm.cpp
            e2e/pad_audio/GridLeds.cpp
            e2e/pad_audio/Loopback.cpp
            e2e/pad_audio/LoopbackScenario.cpp
            e2e/pad_audio/MappingScenario.cpp
            e2e/pad_audio/Metrics.cpp
            e2e/pad_audio/PadScenario.cpp
            e2e/pad_audio/PagesScenario.cpp
            e2e/pad_audio/Records.cpp
            e2e/pad_audio/Render.cpp
            e2e/pad_audio/Rig.cpp
            ${IHUB_SESSION_SOURCES}
            ${IHUB_PROJECT_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/CommandLine.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/MappingImport.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectImport.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectPaths.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/ProjectSession.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/SessionSlotMapping.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/app/project/SourceImport.cpp"
            ${IHUB_DISCOVERY_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
            ${IHUB_API_ROUTES_SOURCES}
            ${IHUB_API_SERVER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            ${IHUB_LAUNCHPAD_DRIVER_SOURCES}
            ${IHUB_MAPPING_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-e2e-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/e2e"
    )

    target_compile_definitions(ihub-e2e-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/e2e/set-init-tone.yaml\""
            "IHUB_SHARED_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
    )

    target_link_libraries(ihub-e2e-test
        PRIVATE
            Threads::Threads
            ALSA::ALSA
            ihub-httplib
            juce::juce_core
            juce::juce_audio_devices
            juce::juce_dsp
            juce::juce_audio_formats
            juce::juce_audio_processors_headless
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-e2e-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-e2e
        COMMAND ihub-e2e-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-e2e-test: the ALSA Loopback acceptance is Linux-only; skipping")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-per-patch-state-test
        PRODUCT_NAME "ihub-per-patch-state-test"
    )

    target_sources(ihub-per-patch-state-test
        PRIVATE
            e2e/PerPatchStateTest.cpp
            ${IHUB_SESSION_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-per-patch-state-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-per-patch-state-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    target_link_libraries(ihub-per-patch-state-test
        PRIVATE
            Threads::Threads
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors_headless
            juce::juce_core
            juce::juce_dsp
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-per-patch-state-test TestPlugin_VST3)

    set_target_properties(ihub-per-patch-state-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-per-patch-state
        COMMAND ihub-per-patch-state-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-per-patch-state-test: Linux VST3 hosting; skipping")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-analog-lab-state-test
        PRODUCT_NAME "ihub-analog-lab-state-test"
    )

    target_sources(ihub-analog-lab-state-test
        PRIVATE
            e2e/AnalogLabStateTest.cpp
            e2e/Check.cpp
            e2e/analog_lab/AudioProbe.cpp
            e2e/analog_lab/Bundle.cpp
            e2e/analog_lab/ChildProcess.cpp
            e2e/analog_lab/FirstPhase.cpp
            e2e/analog_lab/Measurement.cpp
            e2e/analog_lab/Parent.cpp
            e2e/analog_lab/RestartPhase.cpp
            e2e/analog_lab/SessionRig.cpp
            e2e/analog_lab/StateFile.cpp
            ${IHUB_SESSION_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-analog-lab-state-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/e2e"
    )

    target_compile_definitions(ihub-analog-lab-state-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
    )

    target_link_libraries(ihub-analog-lab-state-test
        PRIVATE
            Threads::Threads
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors_headless
            juce::juce_core
            juce::juce_dsp
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-analog-lab-state-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-analog-lab-state
        COMMAND ihub-analog-lab-state-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-analog-lab-state PROPERTIES
        SKIP_RETURN_CODE 77
    )
else()
    message(STATUS "ihub-analog-lab-state-test: Linux VST3 bridge; skipping")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-multipatch-batch-test
        PRODUCT_NAME "ihub-multipatch-batch-test"
    )

    target_sources(ihub-multipatch-batch-test
        PRIVATE
            e2e/MultiPatchBatchTest.cpp
            e2e/Check.cpp
            e2e/multipatch_batch/BatchPhase.cpp
            e2e/multipatch_batch/BatchRig.cpp
            e2e/multipatch_batch/CapturePhase.cpp
            e2e/multipatch_batch/ChildRunner.cpp
            e2e/multipatch_batch/Parent.cpp
            e2e/multipatch_batch/Patches.cpp
            e2e/multipatch_batch/PresetState.cpp
            e2e/multipatch_batch/Record.cpp
            e2e/multipatch_batch/RestorePhase.cpp
            e2e/multipatch_batch/Validation.cpp
            ${IHUB_SESSION_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            ${IHUB_BUS_SOURCES}
            ${IHUB_DEVICE_MANAGER_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            ${IHUB_FX_RACK_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            ${IHUB_LATENCY_PROBE_SOURCES}
            ${IHUB_RT_GUARD_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            ${IHUB_WARM_POOL_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            ${IHUB_PLUGIN_INSTRUMENT_SOURCES}
            ${IHUB_SAMPLER_SOURCES}
    )

    target_include_directories(ihub-multipatch-batch-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/e2e"
    )

    target_compile_definitions(ihub-multipatch-batch-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    target_link_libraries(ihub-multipatch-batch-test
        PRIVATE
            Threads::Threads
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors_headless
            juce::juce_core
            juce::juce_dsp
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-multipatch-batch-test
        TestPlugin_VST3
        ihub-sandbox
    )

    set_target_properties(ihub-multipatch-batch-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-multipatch-batch
        COMMAND ihub-multipatch-batch-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-multipatch-batch-test: Linux VST3 hosting; skipping")
endif()

get_property(IHUB_ALL_TESTS DIRECTORY PROPERTY TESTS)
if(IHUB_ALL_TESTS)
    set_tests_properties(${IHUB_ALL_TESTS} PROPERTIES TIMEOUT 120)
endif()
