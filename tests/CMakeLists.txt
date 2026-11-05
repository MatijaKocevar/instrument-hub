
include("${CMAKE_CURRENT_LIST_DIR}/TestEngine.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Runner.cmake")

add_library(ihub-app-modes-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/app/AppModesTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_app_modes_test")


target_sources(ihub-app-modes-test
    PRIVATE
        app/AppModesTest.cpp
        app/TestSupport.cpp
        app/CommandLine.cpp
        app/Modes.cpp
)

target_include_directories(ihub-app-modes-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-app-modes-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-app-modes
    COMMAND ihub-runner-app ihub-app-modes
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-settings-store-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/settings/SettingsStoreTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_settings_store_test")


target_sources(ihub-settings-store-test
    PRIVATE
        settings/SettingsStoreTest.cpp
        settings/TestSupport.cpp
        settings/Defaults.cpp
        settings/RoundTrip.cpp
        settings/CorruptRecovery.cpp
        settings/UnknownKeys.cpp
        settings/AtomicReplace.cpp
        settings/NewerVersion.cpp
        settings/MidiInputs.cpp
)

target_include_directories(ihub-settings-store-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-settings-store-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-settings-store
    COMMAND ihub-runner-app ihub-settings-store
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(
        NAME ihub-app-list-devices
        COMMAND $<TARGET_FILE:instrument-hub> --list-devices --show-test-devices
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-list-devices PROPERTIES
        PASS_REGULAR_EXPRESSION "Loopback"
    )

    add_test(
        NAME ihub-app-list-devices-raw
        COMMAND $<TARGET_FILE:instrument-hub> --list-devices --raw-devices
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-list-devices-raw PROPERTIES
        PASS_REGULAR_EXPRESSION "Loopback"
    )

    add_test(
        NAME ihub-app-list-devices-default
        COMMAND $<TARGET_FILE:instrument-hub> --list-devices
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-app-list-devices-default PROPERTIES
        FAIL_REGULAR_EXPRESSION "Loopback"
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

add_library(ihub-device-manager-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/device_manager/DeviceManagerTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_device_manager_test")


target_sources(ihub-device-manager-test
    PRIVATE
        device_manager/DeviceManagerTest.cpp
        device_manager/DeviceTypes.cpp
)

target_include_directories(ihub-device-manager-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-device-manager-test
    PRIVATE
        JUCE_JACK=1
)

target_link_libraries(ihub-device-manager-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-device-manager
    COMMAND ihub-runner-app ihub-device-manager
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
)

target_include_directories(ihub-audio-engine-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

ihub_link_test_engine(ihub-audio-engine-test)


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
    )

    target_include_directories(ihub-instrument-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_link_libraries(ihub-instrument-test
        PRIVATE
            ALSA::ALSA
    )
ihub_link_test_engine(ihub-instrument-test)


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

add_library(ihub-timing-monitor-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/timing_monitor/TimingMonitorTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_timing_monitor_test")


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
)

target_include_directories(ihub-timing-monitor-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-timing-monitor-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-timing-monitor
    COMMAND ihub-runner-audio ihub-timing-monitor
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-diagnostics-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/diag/DiagnosticsTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_diagnostics_test")


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
)

target_include_directories(ihub-diagnostics-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-diagnostics-test
    PRIVATE
        Threads::Threads
)
target_link_libraries(ihub-diagnostics-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-diagnostics
    COMMAND ihub-runner-app ihub-diagnostics
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-latency-probe-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/latency_probe/LatencyProbeTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_latency_probe_test")


target_sources(ihub-latency-probe-test
    PRIVATE
        latency_probe/LatencyProbeTest.cpp
        latency_probe/TestSupport.cpp
        latency_probe/CrossCorrelation.cpp
        latency_probe/Loopback.cpp
)

target_include_directories(ihub-latency-probe-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-latency-probe-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-latency-probe
    COMMAND ihub-runner-app ihub-latency-probe
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-latency-probe PROPERTIES DISABLED TRUE)
endif()

if(IHUB_ENABLE_RT_GUARD)
add_library(ihub-rt-guard-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/rt_guard/RtGuardTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_rt_guard_test")


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
    )

    target_include_directories(ihub-rt-guard-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-rt-guard-test PRIVATE IHUB_RT_GUARD=1)

target_link_libraries(ihub-rt-guard-test PRIVATE ihub-test-engine-rt-usage)



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



    add_test(
        NAME ihub-rt-guard
        COMMAND ihub-runner-rt ihub-rt-guard
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

ihub_link_test_engine(ihub-fx-chain-test)


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

add_library(ihub-plugin-fx-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/instruments/PluginFxTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_plugin_fx_test")


target_sources(ihub-plugin-fx-test
    PRIVATE
        instruments/PluginFxTest.cpp
        instruments/plugin_fx/Fixtures.cpp
        instruments/plugin_fx/GainLatency.cpp
        instruments/plugin_fx/State.cpp
        instruments/plugin_fx/Editor.cpp
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

target_link_libraries(ihub-plugin-fx-test PRIVATE ihub-test-engine-gui-usage)



add_dependencies(ihub-plugin-fx-test TestPlugin_VST3)



add_test(
    NAME ihub-plugin-fx
    COMMAND ihub-runner-gui ihub-plugin-fx
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

ihub_link_test_engine(ihub-slot-switcher-test)


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

ihub_link_test_engine(ihub-warm-pool-test)


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

add_library(ihub-switch-continuity-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/audio/SwitchContinuity.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_switch_continuity_test")


target_sources(ihub-switch-continuity-test
    PRIVATE
        audio/SwitchContinuity.cpp
        audio/switch_continuity/Cases.cpp
        audio/switch_continuity/DeltaDetector.cpp
        audio/switch_continuity/Render.cpp
        audio/switch_continuity/SineSource.cpp
        audio/switch_continuity/SpectralDetector.cpp
)

target_include_directories(ihub-switch-continuity-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-switch-continuity-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-switch-continuity
    COMMAND ihub-runner-audio ihub-switch-continuity
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-session-smoke-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/audio/SessionSmoke.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_session_smoke_test")


target_sources(ihub-session-smoke-test
    PRIVATE
        audio/SessionSmoke.cpp
        audio/TestSupport.cpp
        audio/session_smoke/Composition.cpp
        audio/session_smoke/Fixtures.cpp
        audio/session_smoke/HardSwitch.cpp
        audio/session_smoke/Switch.cpp
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

target_link_libraries(ihub-session-smoke-test PRIVATE ihub-test-engine-rt-usage)



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



add_test(
    NAME ihub-session-smoke
    COMMAND ihub-runner-rt ihub-session-smoke
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

add_library(ihub-session-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/SessionApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_session_api_test")


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
        api/session_api/TrackFx.cpp
        api/session_api/Tracks.cpp
        api/session_api/TrackVolume.cpp
        api/session_api/Transport.cpp
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
)
target_link_libraries(ihub-session-api-test PRIVATE ihub-test-engine-rt-usage)





add_test(
    NAME ihub-session-api
    COMMAND ihub-runner-rt ihub-session-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-patch-fx-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/PatchFxApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_patch_fx_api_test")


target_sources(ihub-patch-fx-api-test
    PRIVATE
        api/PatchFxApiTest.cpp
        api/patch_fx_api/Bypass.cpp
        api/patch_fx_api/Chain.cpp
        api/patch_fx_api/HostedInsert.cpp
        api/patch_fx_api/Replace.cpp
        api/patch_fx_api/Validation.cpp
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
)
target_link_libraries(ihub-patch-fx-api-test PRIVATE ihub-test-engine-usage)



add_dependencies(ihub-patch-fx-api-test TestPlugin_VST3)



add_test(
    NAME ihub-patch-fx-api
    COMMAND ihub-runner-api ihub-patch-fx-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-instrument-page-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/InstrumentPageApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_instrument_page_api_test")


target_sources(ihub-instrument-page-api-test
    PRIVATE
        api/InstrumentPageApiTest.cpp
        api/instrument_page_api/Inserts.cpp
        api/instrument_page_api/Mixer.cpp
        api/instrument_page_api/PluginSource.cpp
        api/instrument_page_api/Source.cpp
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
)
target_link_libraries(ihub-instrument-page-api-test PRIVATE ihub-test-engine-usage)



add_dependencies(ihub-instrument-page-api-test TestPlugin_VST3)



add_test(
    NAME ihub-instrument-page-api
    COMMAND ihub-runner-api ihub-instrument-page-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-patch-state-capture-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/PatchStateCaptureApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_patch_state_capture_api_test")


target_sources(ihub-patch-state-capture-api-test
    PRIVATE
        api/PatchStateCaptureApiTest.cpp
        api/patch/Autosave.cpp
        api/patch_state_capture_api/Capture.cpp
        api/patch_state_capture_api/Fixtures.cpp
        api/patch_state_capture_api/Restart.cpp
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
)
target_link_libraries(ihub-patch-state-capture-api-test PRIVATE ihub-test-engine-usage)



add_dependencies(ihub-patch-state-capture-api-test TestPlugin_VST3)



add_test(
    NAME ihub-patch-state-capture-api
    COMMAND ihub-runner-api ihub-patch-state-capture-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
add_library(ihub-plugin-editor-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/PluginEditorApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_plugin_editor_api_test")


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
            api/plugin_editor_api/Reload.cpp
            api/plugin_editor_api/SourceChange.cpp
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
    )
target_link_libraries(ihub-plugin-editor-api-test PRIVATE ihub-test-engine-gui-usage)



    add_dependencies(ihub-plugin-editor-api-test TestPlugin_VST3)



    add_test(
        NAME ihub-plugin-editor-api
        COMMAND ihub-runner-gui ihub-plugin-editor-api
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-plugin-editor-api PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
else()
    message(STATUS "ihub-plugin-editor-api-test: Linux-only; skipping")
endif()

add_library(ihub-session-warm-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/session_warm/SessionWarmPoolTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_session_warm_test")


target_sources(ihub-session-warm-test
    PRIVATE
        session_warm/SessionWarmPoolTest.cpp
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
)
target_link_libraries(ihub-session-warm-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-session-warm
    COMMAND ihub-runner-session ihub-session-warm
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-session-warm-protected-test
    PRODUCT_NAME "ihub-session-warm-protected-test"
)


target_sources(ihub-session-warm-protected-test
    PRIVATE
        session_warm/ProtectedWarmSetTest.cpp
        session_warm/ProtectedWarmFixtures.cpp
        session_warm/WarmPoolProtectedTest.cpp
        session_warm/SessionProtectedTest.cpp
        session_warm/EmptyTrackArmTest.cpp
)

target_include_directories(ihub-session-warm-protected-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-session-warm-protected-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(ihub-session-warm-protected-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
ihub_link_test_engine(ihub-session-warm-protected-test)
    set_target_properties(ihub-session-warm-protected-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )






add_test(
    NAME ihub-session-warm-protected
    COMMAND ihub-session-warm-protected-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-shutdown-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/app/shutdown/ShutdownTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_shutdown_test")


target_sources(ihub-shutdown-test
    PRIVATE
        app/shutdown/Scenarios.cpp
        app/shutdown/ShutdownTest.cpp
        app/shutdown/TestSupport.cpp
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
)
target_link_libraries(ihub-shutdown-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-shutdown
    COMMAND ihub-runner-session ihub-shutdown
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
)

target_compile_features(ihub-launchpad-protocol-test PRIVATE cxx_std_20)

target_include_directories(ihub-launchpad-protocol-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-launchpad-protocol-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

ihub_link_test_engine(ihub-launchpad-protocol-test)

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
)
ihub_link_test_engine(ihub-launchpad-mapping-test)


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

add_library(ihub-launchpad-driver-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/controller/LaunchpadDriverTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_launchpad_driver_test")


target_sources(ihub-launchpad-driver-test
    PRIVATE
        controller/LaunchpadDriverTest.cpp
        controller/driver/ArmingCapture.cpp
        controller/driver/ArmingCaptureGrid.cpp
        controller/driver/Grid.cpp
        controller/driver/Handshake.cpp
        controller/driver/LedStream.cpp
        controller/driver/PatchLeds.cpp
        controller/driver/Rig.cpp
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
)
target_link_libraries(ihub-launchpad-driver-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-launchpad-driver
    COMMAND ihub-runner-session ihub-launchpad-driver
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

juce_add_console_app(ihub-launchpad-track-test
    PRODUCT_NAME "ihub-launchpad-track-test"
)


target_sources(ihub-launchpad-track-test
    PRIVATE
        controller/LaunchpadTrackTest.cpp
        controller/TestSupport.cpp
        controller/track_pads/Capture.cpp
        controller/track_pads/Fixture.cpp
        controller/track_pads/Leds.cpp
        controller/track_pads/Routing.cpp
)

target_include_directories(ihub-launchpad-track-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-launchpad-track-test
    PRIVATE
        "IHUB_TRACKS_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/tracks-set.yaml\""
)

target_link_libraries(ihub-launchpad-track-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
ihub_link_test_engine(ihub-launchpad-track-test)
    set_target_properties(ihub-launchpad-track-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )






add_test(
    NAME ihub-launchpad-track
    COMMAND ihub-launchpad-track-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

find_package(Threads REQUIRED)

add_library(ihub-mapping-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/controller/MappingTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_mapping_test")


target_sources(ihub-mapping-test
    PRIVATE
        controller/MappingTest.cpp
        controller/mapping/Document.cpp
        controller/mapping/Fixtures.cpp
        controller/mapping/HotReload.cpp
        controller/mapping/Render.cpp
        controller/mapping/TrackDocument.cpp
        controller/mapping/TrackFixtures.cpp
        controller/mapping/TrackValidation.cpp
        controller/mapping/Tracks.cpp
        controller/mapping/Validation.cpp
        controller/mapping/ValidationStructure.cpp
        controller/mapping/ValidationSupport.cpp
        controller/mapping/ValidationValues.cpp
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
)
target_link_libraries(ihub-mapping-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-mapping
    COMMAND ihub-runner-session ihub-mapping
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/ApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_api_test")


target_sources(ihub-api-test
    PRIVATE
        api/ApiTest.cpp
        api/core_api/Arm.cpp
        api/core_api/Auth.cpp
        api/core_api/Fixtures.cpp
        api/core_api/Meters.cpp
        api/core_api/RouteRegistration.cpp
        api/core_api/Settings.cpp
        api/core_api/State.cpp
        api/core_api/Transport.cpp
)

target_include_directories(ihub-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
target_link_libraries(ihub-api-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-api
    COMMAND ihub-runner-api ihub-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-app-settings-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/AppSettingsApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_app_settings_api_test")


target_sources(ihub-app-settings-api-test
    PRIVATE
        api/AppSettingsApiTest.cpp
        api/app_settings_api/Account.cpp
        api/app_settings_api/Fixtures.cpp
        api/app_settings_api/Mixer.cpp
)

target_include_directories(ihub-app-settings-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-app-settings-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
target_link_libraries(ihub-app-settings-api-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-app-settings-api
    COMMAND ihub-runner-api ihub-app-settings-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-pairing-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/PairingTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_pairing_test")


target_sources(ihub-pairing-test
    PRIVATE
        api/PairingTest.cpp
        api/pairing/Fixtures.cpp
        api/pairing/LanOff.cpp
        api/pairing/Pairing.cpp
        api/pairing/StartupFlag.cpp
)

target_include_directories(ihub-pairing-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-pairing-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
target_link_libraries(ihub-pairing-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-pairing
    COMMAND ihub-runner-api ihub-pairing
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
add_library(ihub-state-broadcast-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/StateBroadcastTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_state_broadcast_test")


    target_sources(ihub-state-broadcast-test
        PRIVATE
            api/StateBroadcastTest.cpp
            api/EventStream.cpp
            api/state_broadcast/Accounting.cpp
            api/state_broadcast/Fixtures.cpp
            api/state_broadcast/InitialState.cpp
            api/state_broadcast/PadPath.cpp
            api/state_broadcast/RevisionPath.cpp
            api/state_broadcast/UiPath.cpp
    )

    target_include_directories(ihub-state-broadcast-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_link_libraries(ihub-state-broadcast-test
        PRIVATE
            Threads::Threads
            ihub-httplib
    )
target_link_libraries(ihub-state-broadcast-test PRIVATE ihub-test-engine-usage)





    add_test(
        NAME ihub-state-broadcast
        COMMAND ihub-runner-api ihub-state-broadcast
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-state-broadcast-test: the live server test is Linux-only; skipping")
endif()

add_library(ihub-audio-settings-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/AudioSettingsTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_audio_settings_test")


target_sources(ihub-audio-settings-test
    PRIVATE
        api/AudioSettingsTest.cpp
        api/audio_settings/Channels.cpp
        api/audio_settings/DeviceBridge.cpp
        api/audio_settings/DeviceSwitch.cpp
        api/audio_settings/Fixtures.cpp
        api/audio_settings/Persistence.cpp
        api/audio_settings/Profiles.cpp
        api/audio_settings/Restore.cpp
        api/audio_settings/RestoreCli.cpp
        api/audio_settings/RestoreStored.cpp
        api/audio_settings/RestoreSupport.cpp
        api/audio_settings/Routes.cpp
        api/audio_settings/Wizard.cpp
)

target_include_directories(ihub-audio-settings-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-audio-settings-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
target_link_libraries(ihub-audio-settings-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-audio-settings
    COMMAND ihub-runner-api ihub-audio-settings
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-audio-settings PROPERTIES DISABLED TRUE)
endif()

add_library(ihub-midi-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/MidiApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_midi_api_test")


target_sources(ihub-midi-api-test
    PRIVATE
        api/MidiApiTest.cpp
        api/midi_api/DeviceEnumeration.cpp
        api/midi_api/Fixtures.cpp
        api/midi_api/GenericGridRouting.cpp
        api/midi_api/GridLearnPersistence.cpp
        api/midi_api/PadTest.cpp
        api/midi_api/Persistence.cpp
        api/midi_api/Routes.cpp
)

target_include_directories(ihub-midi-api-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-midi-api-test
    PRIVATE
        Threads::Threads
        ihub-httplib
)
target_link_libraries(ihub-midi-api-test PRIVATE ihub-test-engine-usage)



if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(ihub-midi-api-test PRIVATE ALSA::ALSA)
target_link_libraries(ihub-midi-api-test PRIVATE ihub-test-engine-usage)


endif()



add_test(
    NAME ihub-midi-api
    COMMAND ihub-runner-api ihub-midi-api
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-midi-api PROPERTIES DISABLED TRUE)
endif()

add_library(ihub-discovery-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/api/DiscoveryTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_discovery_test")


target_sources(ihub-discovery-test
    PRIVATE
        api/DiscoveryTest.cpp
        api/discovery/Advertisement.cpp
        api/discovery/Dns.cpp
        api/discovery/Helpers.cpp
        api/discovery/Query.cpp
        api/discovery/Socket.cpp
)

target_include_directories(ihub-discovery-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_link_libraries(ihub-discovery-test
    PRIVATE
        Threads::Threads
)
target_link_libraries(ihub-discovery-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-discovery
    COMMAND ihub-runner-api ihub-discovery
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-compressor-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/dsp/CompressorTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_compressor_test")


target_sources(ihub-compressor-test
    PRIVATE
        dsp/CompressorTest.cpp
        dsp/compressor/Bypass.cpp
        dsp/compressor/Dynamics.cpp
        dsp/compressor/Fixtures.cpp
        dsp/compressor/Latency.cpp
        dsp/compressor/MakeupGain.cpp
        dsp/compressor/Realtime.cpp
        dsp/compressor/State.cpp
        dsp/compressor/StaticCurve.cpp
        dsp/compressor/StereoLink.cpp
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

target_link_libraries(ihub-compressor-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-compressor
    COMMAND ihub-runner-dsp ihub-compressor
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-delay-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/dsp/DelayTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_delay_test")


target_sources(ihub-delay-test
    PRIVATE
        dsp/DelayTest.cpp
        dsp/delay/Acceptance.cpp
        dsp/delay/Bypass.cpp
        dsp/delay/Feedback.cpp
        dsp/delay/Fixtures.cpp
        dsp/delay/FreeTime.cpp
        dsp/delay/Realtime.cpp
        dsp/delay/State.cpp
        dsp/delay/SyncTable.cpp
        dsp/delay/Transport.cpp
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

target_link_libraries(ihub-delay-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-delay
    COMMAND ihub-runner-dsp ihub-delay
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-reverb-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/dsp/ReverbTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_reverb_test")


target_sources(ihub-reverb-test
    PRIVATE
        dsp/ReverbTest.cpp
        dsp/reverb/Damping.cpp
        dsp/reverb/Decay.cpp
        dsp/reverb/DryPath.cpp
        dsp/reverb/Fixtures.cpp
        dsp/reverb/PreDelay.cpp
        dsp/reverb/Realtime.cpp
        dsp/reverb/Stability.cpp
        dsp/reverb/State.cpp
        dsp/reverb/Width.cpp
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

target_link_libraries(ihub-reverb-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-reverb
    COMMAND ihub-runner-dsp ihub-reverb
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-eq-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/dsp/EqTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_eq_test")


target_sources(ihub-eq-test
    PRIVATE
        dsp/EqTest.cpp
        dsp/eq/Flat.cpp
        dsp/eq/Fixtures.cpp
        dsp/eq/Realtime.cpp
        dsp/eq/Response.cpp
        dsp/eq/State.cpp
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

target_link_libraries(ihub-eq-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-eq
    COMMAND ihub-runner-dsp ihub-eq
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-transport-test OBJECT
    audio/TransportTest.cpp
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
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/audio/TransportTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_transport_test")


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



add_test(
    NAME ihub-transport
    COMMAND ihub-runner-audio ihub-transport
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-clock-out-test OBJECT
    audio/ClockOutTest.cpp
    audio/clock_out/Acceptance.cpp
    audio/clock_out/Fixtures.cpp
    audio/clock_out/Midi.cpp
    audio/clock_out/Overflow.cpp
    audio/clock_out/Realtime.cpp
    audio/clock_out/Rewind.cpp
    audio/clock_out/Tempo.cpp
    audio/clock_out/Transport.cpp
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/audio/ClockOutTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_clock_out_test")


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



add_test(
    NAME ihub-clock-out
    COMMAND ihub-runner-audio ihub-clock-out
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-clock-in-test OBJECT
    audio/ClockInTest.cpp
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
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/audio/ClockInTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_clock_in_test")


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



add_test(
    NAME ihub-clock-in
    COMMAND ihub-runner-audio ihub-clock-in
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-tap-tempo-test OBJECT
    audio/TapTempoTest.cpp
    audio/tap_tempo/Acceptance.cpp
    audio/tap_tempo/Fixtures.cpp
    audio/tap_tempo/Median.cpp
    audio/tap_tempo/Range.cpp
    audio/tap_tempo/Realtime.cpp
    audio/tap_tempo/Reset.cpp
    audio/tap_tempo/SequenceReset.cpp
    audio/tap_tempo/TransportIntegration.cpp
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/audio/TapTempoTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_tap_tempo_test")


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



add_test(
    NAME ihub-tap-tempo
    COMMAND ihub-runner-audio ihub-tap-tempo
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-set-file-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/project/SetFileTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_set_file_test")


target_sources(ihub-set-file-test
    PRIVATE
        project/SetFileTest.cpp
)

target_include_directories(ihub-set-file-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-set-file-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
)

target_link_libraries(ihub-set-file-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-set-file
    COMMAND ihub-runner-project ihub-set-file
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-migrations-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/project/MigrationsTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_migrations_test")


target_sources(ihub-migrations-test
    PRIVATE
        project/MigrationsTest.cpp
)

target_include_directories(ihub-migrations-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-migrations-test
    PRIVATE
        "IHUB_V1_SET_FIXTURE=\"${CMAKE_CURRENT_SOURCE_DIR}/project/fixtures/set-v1.ihub\""
)

target_link_libraries(ihub-migrations-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-migrations
    COMMAND ihub-runner-project ihub-migrations
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-patch-persistence-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/project/PatchPersistenceTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_patch_persistence_test")


target_sources(ihub-patch-persistence-test
    PRIVATE
        project/PatchPersistenceTest.cpp
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
)
target_link_libraries(ihub-patch-persistence-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-patch-persistence
    COMMAND ihub-runner-project ihub-patch-persistence
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-project-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_project_test")


target_sources(ihub-project-test
    PRIVATE
        project/ProjectTest.cpp
        project/project/Autosave.cpp
        project/project/Import.cpp
        project/project/Reload.cpp
        project/project/SessionDescription.cpp
        project/project/Support.cpp
        project/project/TrackLayout.cpp
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
)
target_link_libraries(ihub-project-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-project
    COMMAND ihub-runner-project ihub-project
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-sample-loader-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/sampler/SampleLoaderTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_sample_loader_test")


target_sources(ihub-sample-loader-test
    PRIVATE
        sampler/SampleLoaderTest.cpp
        sampler/sample/Assertions.cpp
        sampler/sample/Decode.cpp
        sampler/sample/Failure.cpp
        sampler/sample/Handoff.cpp
        sampler/sample/Resample.cpp
        sampler/sample/Stress.cpp
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

target_link_libraries(ihub-sample-loader-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-sample-loader
    COMMAND ihub-runner-sampler ihub-sample-loader
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-kit-loader-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/sampler/KitLoaderTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_kit_loader_test")


target_sources(ihub-kit-loader-test
    PRIVATE
        sampler/KitLoaderTest.cpp
        sampler/kit/AudioFixtures.cpp
        sampler/kit/Fixtures.cpp
        sampler/kit/Layers.cpp
        sampler/kit/Load.cpp
        sampler/kit/Parse.cpp
        sampler/kit/RenderHit.cpp
        sampler/kit/Resolve.cpp
        sampler/kit/Validation.cpp
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

target_link_libraries(ihub-kit-loader-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-kit-loader
    COMMAND ihub-runner-sampler ihub-kit-loader
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(ihub-sampler-instrument-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/sampler/SamplerInstrumentTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_sampler_instrument_test")


target_sources(ihub-sampler-instrument-test
    PRIVATE
        sampler/SamplerInstrumentTest.cpp
        sampler/voices/Choke.cpp
        sampler/voices/Fixtures.cpp
        sampler/voices/Gate.cpp
        sampler/voices/Pool.cpp
        sampler/voices/Realtime.cpp
        sampler/voices/Setup.cpp
        sampler/voices/Stealing.cpp
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

target_link_libraries(ihub-sampler-instrument-test PRIVATE ihub-test-engine-usage)





add_test(
    NAME ihub-sampler-instrument
    COMMAND ihub-runner-sampler ihub-sampler-instrument
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
add_library(ihub-session-midi-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/session_midi/SessionMidiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_session_midi_test")


    target_sources(ihub-session-midi-test
        PRIVATE
            session_midi/SessionMidiTest.cpp
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
    )
target_link_libraries(ihub-session-midi-test PRIVATE ihub-test-engine-usage)



    add_dependencies(ihub-session-midi-test NoteTonePlugin_VST3)



    add_test(
        NAME ihub-session-midi
        COMMAND ihub-runner-session ihub-session-midi
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-session-midi-test: ALSA MIDI forwarding is Linux-only; skipping")
endif()

add_library(ihub-session-plugin-state-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/plugin_state/SessionPluginStateTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_session_plugin_state_test")


target_sources(ihub-session-plugin-state-test
    PRIVATE
        plugin_state/SessionPluginStateTest.cpp
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
)
target_link_libraries(ihub-session-plugin-state-test PRIVATE ihub-test-engine-usage)



add_dependencies(ihub-session-plugin-state-test TestPlugin_VST3)



add_test(
    NAME ihub-session-plugin-state
    COMMAND ihub-runner-session ihub-session-plugin-state
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-session-concurrent-instances-test
        PRODUCT_NAME "ihub-session-concurrent-instances-test"
    )

    target_sources(ihub-session-concurrent-instances-test
        PRIVATE
            plugin_state/SessionConcurrentInstancesTest.cpp
            plugin_state/concurrent/ConcurrentFixture.cpp
            plugin_state/concurrent/EditorCapture.cpp
            plugin_state/concurrent/Reload.cpp
            plugin_state/shared_page/SharedPageScenario.cpp
    )

    target_include_directories(ihub-session-concurrent-instances-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-session-concurrent-instances-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
            "IHUB_SANDBOX_HELPER=\"$<TARGET_FILE:ihub-sandbox>\""
    )

    target_link_libraries(ihub-session-concurrent-instances-test
        PRIVATE
            Threads::Threads
    )
ihub_link_test_engine(ihub-session-concurrent-instances-test)


    add_dependencies(ihub-session-concurrent-instances-test
        ihub-sandbox
        TestPlugin_VST3
    )

    set_target_properties(ihub-session-concurrent-instances-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-session-concurrent-instances
        COMMAND ihub-session-concurrent-instances-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-session-concurrent-instances PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-session-sandbox-death-test
        PRODUCT_NAME "ihub-session-sandbox-death-test"
    )

    target_sources(ihub-session-sandbox-death-test
        PRIVATE
            plugin_state/SessionSandboxDeathTest.cpp
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
    )
ihub_link_test_engine(ihub-session-sandbox-death-test)


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
add_library(ihub-session-editor-follow-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/plugin_state/SessionEditorFollowTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_session_editor_follow_test")


    target_sources(ihub-session-editor-follow-test
        PRIVATE
            plugin_state/SessionEditorFollowTest.cpp
            plugin_state/editor_follow/EditorAudioScenario.cpp
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
    )
target_link_libraries(ihub-session-editor-follow-test PRIVATE ihub-test-engine-gui-usage)



    add_dependencies(ihub-session-editor-follow-test TestPlugin_VST3)



    add_test(
        NAME ihub-session-editor-follow
        COMMAND ihub-runner-gui ihub-session-editor-follow
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
            e2e/track_grid/Actions.cpp
            e2e/track_grid/LedChecks.cpp
            e2e/track_grid/Leds.cpp
            e2e/track_grid/Memory.cpp
            e2e/track_grid/Record.cpp
            e2e/track_grid/Rig.cpp
            e2e/track_grid/RigConfig.cpp
            e2e/track_grid/RigInput.cpp
            e2e/track_grid/Scenario.cpp
            e2e/track_grid/Switches.cpp
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
            "IHUB_TRACKS_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/tracks-set.yaml\""
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    add_dependencies(ihub-e2e-test
        TestPlugin_VST3
        ihub-sandbox
    )

    target_link_libraries(ihub-e2e-test
        PRIVATE
            Threads::Threads
            ALSA::ALSA
            ihub-httplib
    )
ihub_link_test_engine(ihub-e2e-test)


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
add_library(ihub-per-patch-state-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/e2e/PerPatchStateTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_per_patch_state_test")


    target_sources(ihub-per-patch-state-test
        PRIVATE
            e2e/PerPatchStateTest.cpp
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
    )
target_link_libraries(ihub-per-patch-state-test PRIVATE ihub-test-engine-usage)



    add_dependencies(ihub-per-patch-state-test TestPlugin_VST3)



    add_test(
        NAME ihub-per-patch-state
        COMMAND ihub-runner-session ihub-per-patch-state
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
    )
ihub_link_test_engine(ihub-analog-lab-state-test)

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
    )
ihub_link_test_engine(ihub-multipatch-batch-test)

    set_target_properties(ihub-multipatch-batch-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_dependencies(ihub-multipatch-batch-test
        TestPlugin_VST3
        ihub-sandbox
    )

    add_test(
        NAME ihub-multipatch-batch
        COMMAND ihub-multipatch-batch-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-multipatch-batch-test: Linux VST3 hosting; skipping")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
juce_add_console_app(ihub-project-lifecycle-api-test
    PRODUCT_NAME "ihub-project-lifecycle-api-test"
)


    target_sources(ihub-project-lifecycle-api-test
        PRIVATE
            api/ProjectLifecycleTest.cpp
            api/JsonSupport.cpp
            api/SessionRig.cpp
            api/TestSupport.cpp
            api/project/ProjectFixtures.cpp
            api/project/History.cpp
            api/project/ProjectLifecycle.cpp
            api/project/TrackSetup.cpp
    )

    target_include_directories(ihub-project-lifecycle-api-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-project-lifecycle-api-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
    )

    target_link_libraries(ihub-project-lifecycle-api-test
        PRIVATE
            Threads::Threads
            ihub-httplib
    )
ihub_link_test_engine(ihub-project-lifecycle-api-test)
    set_target_properties(ihub-project-lifecycle-api-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )






    add_test(
        NAME ihub-project-lifecycle-api
        COMMAND ihub-project-lifecycle-api-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-project-lifecycle-api-test: needs the Linux VST3 host; skipping")
endif()

ihub_add_test_runner(ihub-runner-audio
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    TARGETS
        ihub-transport-test
        ihub-clock-in-test
        ihub-clock-out-test
        ihub-tap-tempo-test
        ihub-switch-continuity-test
        ihub-timing-monitor-test
    SCENARIOS
        ihub-transport=ihub_scenario_ihub_transport_test
        ihub-clock-in=ihub_scenario_ihub_clock_in_test
        ihub-clock-out=ihub_scenario_ihub_clock_out_test
        ihub-tap-tempo=ihub_scenario_ihub_tap_tempo_test
        ihub-switch-continuity=ihub_scenario_ihub_switch_continuity_test
        ihub-timing-monitor=ihub_scenario_ihub_timing_monitor_test
    SHARED
        audio/AllocationCount.cpp
        audio/TestSupport.cpp
)

ihub_add_test_runner(ihub-runner-dsp
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    WRAP_ALLOC
    TARGETS
        ihub-compressor-test
        ihub-delay-test
        ihub-reverb-test
        ihub-eq-test
    SCENARIOS
        ihub-compressor=ihub_scenario_ihub_compressor_test
        ihub-delay=ihub_scenario_ihub_delay_test
        ihub-reverb=ihub_scenario_ihub_reverb_test
        ihub-eq=ihub_scenario_ihub_eq_test
    SHARED
        dsp/AllocationCount.cpp
        dsp/TestSupport.cpp
)

ihub_add_test_runner(ihub-runner-sampler
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    WRAP_ALLOC
    TARGETS
        ihub-sample-loader-test
        ihub-kit-loader-test
        ihub-sampler-instrument-test
    SCENARIOS
        ihub-sample-loader=ihub_scenario_ihub_sample_loader_test
        ihub-kit-loader=ihub_scenario_ihub_kit_loader_test
        ihub-sampler-instrument=ihub_scenario_ihub_sampler_instrument_test
    SHARED
        sampler/Fixtures.cpp
        sampler/Measurements.cpp
        sampler/TestSupport.cpp
)

ihub_add_test_runner(ihub-runner-app
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    TARGETS
        ihub-app-modes-test
        ihub-settings-store-test
        ihub-device-manager-test
        ihub-diagnostics-test
        ihub-latency-probe-test
    SCENARIOS
        ihub-app-modes=ihub_scenario_ihub_app_modes_test
        ihub-settings-store=ihub_scenario_ihub_settings_store_test
        ihub-device-manager=ihub_scenario_ihub_device_manager_test
        ihub-diagnostics=ihub_scenario_ihub_diagnostics_test
        ihub-latency-probe=ihub_scenario_ihub_latency_probe_test
)

ihub_add_test_runner(ihub-runner-project
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    TARGETS
        ihub-set-file-test
        ihub-migrations-test
        ihub-patch-persistence-test
        ihub-project-test
    SCENARIOS
        ihub-set-file=ihub_scenario_ihub_set_file_test
        ihub-migrations=ihub_scenario_ihub_migrations_test
        ihub-patch-persistence=ihub_scenario_ihub_patch_persistence_test
        ihub-project=ihub_scenario_ihub_project_test
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
ihub_add_test_runner(ihub-runner-api
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    TARGETS
        ihub-api-test
        ihub-app-settings-api-test
        ihub-pairing-test
        ihub-state-broadcast-test
        ihub-audio-settings-test
        ihub-midi-api-test
        ihub-discovery-test
        ihub-patch-fx-api-test
        ihub-instrument-page-api-test
        ihub-patch-state-capture-api-test
    SCENARIOS
        ihub-api=ihub_scenario_ihub_api_test
        ihub-app-settings-api=ihub_scenario_ihub_app_settings_api_test
        ihub-pairing=ihub_scenario_ihub_pairing_test
        ihub-state-broadcast=ihub_scenario_ihub_state_broadcast_test
        ihub-audio-settings=ihub_scenario_ihub_audio_settings_test
        ihub-midi-api=ihub_scenario_ihub_midi_api_test
        ihub-discovery=ihub_scenario_ihub_discovery_test
        ihub-patch-fx-api=ihub_scenario_ihub_patch_fx_api_test
        ihub-instrument-page-api=ihub_scenario_ihub_instrument_page_api_test
        ihub-patch-state-capture-api=ihub_scenario_ihub_patch_state_capture_api_test
    SHARED
        api/JsonSupport.cpp
        api/TestSupport.cpp
        api/AudioMeasurements.cpp
        api/FxChainJson.cpp
        api/RequestDispatch.cpp
        api/SessionRig.cpp
)
target_link_libraries(ihub-runner-api PRIVATE ihub-httplib Threads::Threads)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
ihub_add_test_runner(ihub-runner-session
    ENGINE ihub-test-engine
    USAGE ihub-test-engine-usage
    TARGETS
        ihub-session-warm-test
        ihub-shutdown-test
        ihub-launchpad-driver-test
        ihub-mapping-test
        ihub-session-midi-test
        ihub-session-plugin-state-test
        ihub-per-patch-state-test
    SCENARIOS
        ihub-session-warm=ihub_scenario_ihub_session_warm_test
        ihub-shutdown=ihub_scenario_ihub_shutdown_test
        ihub-launchpad-driver=ihub_scenario_ihub_launchpad_driver_test
        ihub-mapping=ihub_scenario_ihub_mapping_test
        ihub-session-midi=ihub_scenario_ihub_session_midi_test
        ihub-session-plugin-state=ihub_scenario_ihub_session_plugin_state_test
        ihub-per-patch-state=ihub_scenario_ihub_per_patch_state_test
    SHARED
        controller/TestSupport.cpp
        e2e/Check.cpp
)
add_dependencies(ihub-runner-session
    TestPlugin_VST3
    NoteTonePlugin_VST3
    ihub-sandbox
)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
ihub_add_test_runner(ihub-runner-gui
    ENGINE ihub-test-engine-gui
    USAGE ihub-test-engine-gui-usage
    TARGETS
        ihub-plugin-fx-test
        ihub-plugin-window-test
        ihub-plugin-editor-api-test
        ihub-session-editor-follow-test
        ihub-plugin-scanner-test
        ihub-plugin-cache-test
        ihub-plugin-inventory-api-test
    SCENARIOS
        ihub-plugin-fx=ihub_scenario_ihub_plugin_fx_test=args
        ihub-plugin-window=ihub_scenario_ihub_plugin_window_test=args
        ihub-plugin-editor-api=ihub_scenario_ihub_plugin_editor_api_test
        ihub-session-editor-follow=ihub_scenario_ihub_session_editor_follow_test
        ihub-plugin-scanner=ihub_scenario_ihub_plugin_scanner_test=args
        ihub-plugin-cache=ihub_scenario_ihub_plugin_cache_test=args
        ihub-plugin-inventory-api=ihub_scenario_ihub_plugin_inventory_api_test
)
add_dependencies(ihub-runner-gui
    TestPlugin_VST3
    ihub-crashing-vst3
    ihub-scanner
)
endif()

if(IHUB_ENABLE_RT_GUARD)
ihub_add_test_runner(ihub-runner-rt
    ENGINE ihub-test-engine-rt
    USAGE ihub-test-engine-rt-usage
    TARGETS
        ihub-rt-guard-test
        ihub-session-smoke-test
        ihub-session-api-test
    SCENARIOS
        ihub-rt-guard=ihub_scenario_ihub_rt_guard_test
        ihub-session-smoke=ihub_scenario_ihub_session_smoke_test
        ihub-session-api=ihub_scenario_ihub_session_api_test
    RT
)
endif()

get_property(IHUB_ALL_TESTS DIRECTORY PROPERTY TESTS)
if(IHUB_ALL_TESTS)
    set_tests_properties(${IHUB_ALL_TESTS} PROPERTIES TIMEOUT 120)
endif()
