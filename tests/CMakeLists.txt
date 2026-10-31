# Headless verification tools.
#
# ihub-selftest is plain ALSA; the device manager test links JUCE so it can
# exercise the same AudioDeviceManager wrapper the application uses.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA REQUIRED)

    set(IHUB_MIDI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/midi")

    add_executable(ihub-selftest
        selftest/AudioSmoke.cpp
        midi/LaunchkeySim.cpp
        midi/MidiProbe.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyProtocol.cpp"
    )

    target_compile_features(ihub-selftest PRIVATE cxx_std_20)

    target_include_directories(ihub-selftest
        PRIVATE
            "${IHUB_MIDI_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-selftest
        PRIVATE
            "IHUB_MIDI_FIXTURE_DIR=\"${IHUB_MIDI_DIR}/fixtures\""
    )

    target_link_libraries(ihub-selftest
        PRIVATE
            ALSA::ALSA
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ihub-selftest PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # Keep the documented path: build/tests/ihub-selftest.
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

    # --- Virtual Launchkey (T041) -------------------------------------------
    #
    # ihub-selftest --midi-probe creates the `Launchkey Mini MK3 SIM` ALSA
    # sequencer port, lists it, replays tests/midi/fixtures/pads-16.json and
    # checks the 16 pad presses arrive in order, then writes LED feedback back
    # and checks the recording. The standalone binary is the same simulator
    # without the probe.

    add_executable(ihub-launchkey-sim
        midi/LaunchkeySim.cpp
        midi/LaunchkeySimMain.cpp
    )

    target_compile_features(ihub-launchkey-sim PRIVATE cxx_std_20)

    target_include_directories(ihub-launchkey-sim
        PRIVATE
            "${IHUB_MIDI_DIR}"
    )

    target_link_libraries(ihub-launchkey-sim
        PRIVATE
            ALSA::ALSA
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ihub-launchkey-sim PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set_target_properties(ihub-launchkey-sim PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-midi-sim
        COMMAND ihub-selftest --midi-probe
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-selftest: ALSA smoke test is Linux-only; skipping")
endif()

# --- Test plugins -----------------------------------------------------------
#
# TestPlugin.vst3 is built from source so plugin tests (scanner, hosting,
# state, editor) never depend on a third-party binary being installed.

add_subdirectory(plugins)

# --- Audio device manager ---------------------------------------------------
#
# Opens the ALSA Loopback device through AudioEngine::DeviceManager, then
# switches buffer size 128 -> 256 -> 128 and hot-swaps between two Loopback
# subdevices while the stream runs.

juce_add_console_app(ihub-device-manager-test
    PRODUCT_NAME "ihub-device-manager-test"
)

target_sources(ihub-device-manager-test
    PRIVATE
        device_manager/DeviceManagerTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
)

target_include_directories(ihub-device-manager-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
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

# The test is bound to the ALSA Loopback card, so only register it on Linux.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ihub-device-manager PROPERTIES DISABLED TRUE)
endif()

# --- Fixed slot graph -------------------------------------------------------
# Renders 2 s through AudioEngine with three slots and compares the bus output
# against a manually mixed reference within -90 dBFS. It also counts heap
# allocations across processBlock() by replacing the global operator new and
# (on Linux) wrapping malloc/calloc/realloc.

juce_add_console_app(ihub-audio-engine-test
    PRODUCT_NAME "ihub-audio-engine-test"
)

target_sources(ihub-audio-engine-test
    PRIVATE
        audio_engine/AudioEngineTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# The replacement operator new below is backed by malloc/free (which is a valid
# implementation), so GCC's pairing heuristic fires at inlined call sites.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-audio-engine-test PRIVATE
        -Wno-mismatched-new-delete
    )
endif()

# The counting allocator's __wrap_* hooks are GNU-ld specific. On other
# platforms the test still runs and counts operator new.
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

# --- Instrument interface and audio input (T030, T070) ----------------------
#
# Checks the AudioInputInstrument in isolation (unity/gain/channel selection/
# latency), the audio-input slot path through an FX chain, direct monitoring
# (the chain must not run and its latency must drop out), the input's own
# latency budget, and AudioEngine feeding device input to the slot. Then it
# plays a sine into the ALSA Loopback, captures it, and feeds every block
# through the instrument -> slot -> master bus path. The bus output must
# reproduce the captured input at bit-exact unity gain; a second loopback pass
# runs half the capture through a gain effect and half in direct-monitor mode.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-instrument-test
        PRODUCT_NAME "ihub-instrument-test"
    )

    target_sources(ihub-instrument-test
        PRIVATE
            instruments/AudioInputInstrumentTest.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# --- Xrun and timing monitor ------------------------------------------------
# Injects exact callback durations (including a 2x-deadline stall) through the
# deterministic recording seam and checks the overrun, dropped-block and rolling
# maximum accounting.

juce_add_console_app(ihub-timing-monitor-test
    PRODUCT_NAME "ihub-timing-monitor-test"
)

target_sources(ihub-timing-monitor-test
    PRIVATE
        timing_monitor/TimingMonitorTest.cpp
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

# --- Diagnostics (T111) -----------------------------------------------------
#
# Structured log ring under four concurrent producers, crash-safe dump format,
# a forked child that really dies by SIGABRT with the handler installed, and
# the formatted report (device, buffer, latency, xruns, plugins, log).

juce_add_console_app(ihub-diagnostics-test
    PRODUCT_NAME "ihub-diagnostics-test"
)

find_package(Threads REQUIRED)

target_sources(ihub-diagnostics-test
    PRIVATE
        diag/DiagnosticsTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/CrashDump.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/Diagnostics.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/diag/LogRing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
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

# --- Round-trip latency -----------------------------------------------------
#
# Opens the ALSA Loopback full-duplex, fires an impulse, records the capture,
# and cross-correlates it. Includes a pure synthetic correlation test and five
# real measurements at 128 frames.

juce_add_console_app(ihub-latency-probe-test
    PRODUCT_NAME "ihub-latency-probe-test"
)

target_sources(ihub-latency-probe-test
    PRIVATE
        latency_probe/LatencyProbeTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# --- Realtime safety harness (T014) -----------------------------------------
#
# Renders the fixed graph under RtGuard, requires zero violations on the normal
# path, and forks a child per injected allocation/lock/log to prove that strict
# mode aborts with SIGABRT and names the violation.

if(IHUB_ENABLE_RT_GUARD)
    juce_add_console_app(ihub-rt-guard-test
        PRODUCT_NAME "ihub-rt-guard-test"
    )

    target_sources(ihub-rt-guard-test
        PRIVATE
            rt_guard/RtGuardTest.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
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

# --- Slot FX chain (T031) ---------------------------------------------------
#
# 1. An empty chain is a bit-exact passthrough, both directly through FxChain
#    and through the full Slot path (instrument -> chain -> output).
# 2. Two TestPlugin instances loaded as effects apply in list order:
#    gain-then-sine produces a different result from sine-then-gain.
# 3. Slot::latencySamples() sums the source latency and the chain latency.

juce_add_console_app(ihub-fx-chain-test
    PRODUCT_NAME "ihub-fx-chain-test"
)

target_sources(ihub-fx-chain-test
    PRIVATE
        fx_chain/FxChainTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
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

# --- Hosted VST3 FX slot (T063) ---------------------------------------------
#
# Loads TestPlugin through PluginFx in FX mode (input + output bus), inserts
# it into an FxChain, proves its gain is applied, checks that its 5 ms latency
# is included in the chain total, round-trips its state into a second
# instance, and opens/closes its native editor in an app-owned window.

juce_add_console_app(ihub-plugin-fx-test
    PRODUCT_NAME "ihub-plugin-fx-test"
)

target_sources(ihub-plugin-fx-test
    PRIVATE
        instruments/PluginFxTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/PluginFx.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginWindow.cpp"
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

# --- Gain-ramp switching (T032) ---------------------------------------------
#
# Switches between two sine-generating slots and compares the output against a
# continuous, equal-power reference. The worst deviation must be below
# -60 dBFS; a hard switch or a broken ramp shows up as a large residual.
# Also checks the four published states and that process() allocates nothing.

juce_add_console_app(ihub-slot-switcher-test
    PRODUCT_NAME "ihub-slot-switcher-test"
)

target_sources(ihub-slot-switcher-test
    PRIVATE
        slot_switcher/SlotSwitcherTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
)

target_include_directories(ihub-slot-switcher-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# The replacement operator new is backed by malloc/free (which is a valid
# implementation), so GCC's pairing heuristic fires at inlined call sites.
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

# --- Warm pool and preload (T033) -------------------------------------------
#
# Eight slots, K = 2, real TestPlugin instances created on the pool's
# background thread by a counting wrapper:
#
# 1. The live-instance counter (updated in the wrapper's constructor and
#    destructor) peaks at 3 and never reports a violation, including across a
#    sweep of the active slot over all eight slots and a preload of the set.
# 2. Arming a warm slot — acquire + Slot::setSource + SlotSwitcher::armSlot —
#    completes in under 5 ms. A cold PluginInstrument load is printed for
#    comparison.
# 3. A pinned instance survives its slot leaving the neighbourhood; the
#    neighbour that could not fit is warmed after the pin is released.
# 4. Changing a request id retires and re-warms; a failing factory is reported
#    without spinning the background thread.

juce_add_console_app(ihub-warm-pool-test
    PRODUCT_NAME "ihub-warm-pool-test"
)

target_sources(ihub-warm-pool-test
    PRIVATE
        warm_pool/WarmPoolTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
)

target_include_directories(ihub-warm-pool-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-warm-pool-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        # A console host has no running dispatch loop: the test pumps it while
        # the pool's background thread waits for JUCE to instantiate a plugin
        # on the message thread.
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

# --- Click/discontinuity test (T034) ----------------------------------------
#
# Renders the master bus while 50 seeded, randomly timed switches move the
# armed slot across four sine sources. The output is checked for
# sample-to-sample steps above the maximum continuous slew and for spectral
# splatter above 1 kHz. The same schedule then runs with the ramp disabled and
# must be caught, proving the detectors can fail.

juce_add_console_app(ihub-switch-continuity-test
    PRODUCT_NAME "ihub-switch-continuity-test"
)

target_sources(ihub-switch-continuity-test
    PRIVATE
        audio/SwitchContinuity.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
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

# --- Composed audio session (T114) ------------------------------------------
#
# Builds the default session (AudioEngine + SlotSwitcher + eight instrument
# slots) offline, checks that the armed slot has a non-zero RMS, switches slots
# and requires no discontinuity above -60 dBFS. A second pass with the ramp
# disabled proves the detector can fail. The application's --session-smoke mode
# runs the same measurement and then streams the graph through the ALSA
# Loopback, so the callback path is covered too.

juce_add_console_app(ihub-session-smoke-test
    PRODUCT_NAME "ihub-session-smoke-test"
)

target_sources(ihub-session-smoke-test
    PRIVATE
        audio/SessionSmoke.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
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

# The real acceptance command: the app composes the session, measures it and
# streams the same graph through the ALSA Loopback.
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

# --- Session behind the control API (T115) ---------------------------------
#
# Builds the headless default session, attaches the HTTP + WebSocket server
# through Api::attachSession() and proves the API drives the real graph:
# POST /arm switches the audible slot (the rendered tone moves from 110 Hz to
# 123.47 Hz), /meters reports the playing slot's real peak, GET /state reports
# the live slot states, and POST /transport starts/stops the session transport.

juce_add_console_app(ihub-session-api-test
    PRODUCT_NAME "ihub-session-api-test"
)

target_sources(ihub-session-api-test
    PRIVATE
        api/SessionApiTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
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

# --- Per-patch FX chain API (T121) ------------------------------------------
#
# Builds a small headless session and edits patch 0's insert chain only through
# the HTTP API: add a look-ahead compressor and an EQ, reorder them and prove
# the rendered audio changes, bypass and wet/dry change both the audio and the
# reported latency, a hosted TestPlugin VST3 joins the chain, PUT replaces the
# whole chain and DELETE removes inserts. The render is driven by the test, so
# nothing depends on a sound card.

juce_add_console_app(ihub-patch-fx-api-test
    PRODUCT_NAME "ihub-patch-fx-api-test"
)

target_sources(ihub-patch-fx-api-test
    PRIVATE
        api/PatchFxApiTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
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

# --- Instrument page controls (T124) ----------------------------------------
#
# Drives the same requests the instrument page sends and measures the engine:
# the volume fader changes the rendered level (PATCH volumeDb), mute silences
# the slot, the insert chain can be added/bypassed/reordered/replaced/removed,
# and the instrument source can be swapped (a tone frequency is measured by
# zero crossings, a hosted TestPlugin loads and renders). The render is driven
# by the test, so nothing depends on a sound card or wall-clock timing.

juce_add_console_app(ihub-instrument-page-api-test
    PRODUCT_NAME "ihub-instrument-page-api-test"
)

target_sources(ihub-instrument-page-api-test
    PRIVATE
        api/InstrumentPageApiTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
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

# --- 64 logical slots and the warm pool (T118) ------------------------------
#
# Configures all 64 Launchpad patches, proves that only the armed slot, its K
# neighbours and the previous patch are instantiated (peak <= K+1), arms a cold
# patch and observes warming -> active with non-silent audio, returns to the
# previous patch instantly, sweeps every pad while the bound holds, and checks
# that GET /state carries the 8 x 8 family/patch tree and per-slot warm state.
# A 40 ms artificial load delay makes the warming state observable; it is zero
# in production.

juce_add_console_app(ihub-session-warm-test
    PRODUCT_NAME "ihub-session-warm-test"
)

target_sources(ihub-session-warm-test
    PRIVATE
        session_warm/SessionWarmPoolTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/RtGuard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
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

# --- Launchkey protocol (T040) ----------------------------------------------
#
# Pure, hardware-free checks on the controller contract: the 36..51 pad note
# map, palette velocity uniqueness, encoder CCs, key range, and the session-mode
# SysEx frame documented in docs/launchkey-mini-mk3.md. No MIDI port is opened.

add_executable(ihub-launchkey-protocol-test
    controller/LaunchkeyProtocolTest.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyProtocol.cpp"
)

target_compile_features(ihub-launchkey-protocol-test PRIVATE cxx_std_20)

target_include_directories(ihub-launchkey-protocol-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ihub-launchkey-protocol-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(ihub-launchkey-protocol-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-launchkey-protocol
    COMMAND ihub-launchkey-protocol-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# --- Launchpad protocol -----------------------------------------------------
#
# Hardware-free checks for the Launchpad Mini MK3: the 11..88 grid note map,
# the documented SysEx frames and the LED Note On encoding. No port is opened.

add_executable(ihub-launchpad-protocol-test
    controller/LaunchpadProtocolTest.cpp
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

# --- Launchpad 64-pad mapping (T116) ----------------------------------------
#
# Pure C++: loads the shipped shared/set.yaml and resolves all 64 grid pads to
# eight families of eight patches, then exercises the top-row macro pages and
# right-column scenes. No MIDI port and no audio device.

find_package(Threads REQUIRED)

add_executable(ihub-launchpad-mapping-test
    controller/LaunchpadMappingTest.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
)

target_compile_features(ihub-launchpad-mapping-test PRIVATE cxx_std_20)

target_include_directories(ihub-launchpad-mapping-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-launchpad-mapping-test
    PRIVATE
        "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
)

target_link_libraries(ihub-launchpad-mapping-test
    PRIVATE
        Threads::Threads
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

# --- Scripted Launchpad driver (T116) ---------------------------------------
#
# Injects note/CC events straight into LaunchpadDriver and captures every LED
# message with a sink, so the armed slot and the idle/warming/active/error
# stream are checked without a MIDI port or hardware.

juce_add_console_app(ihub-launchpad-driver-test
    PRODUCT_NAME "ihub-launchpad-driver-test"
)

target_sources(ihub-launchpad-driver-test
    PRIVATE
        controller/LaunchpadDriverTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadDriver.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadProtocol.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
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

# --- Launchkey driver against the virtual controller (T042) -----------------
#
# This is the T042 acceptance check. ihub-launchkey-sim's port is opened
# through the same juce::MidiInput/MidiOutput calls the application uses:
#
#   connect -> handshake bytes match tests/midi/fixtures/session-handshake.json
#   pad 0 press -> mapping arms slot 0 -> pad 0 LED with the active colour.
#
# It also checks key-note and encoder dispatch. Linux-only because the
# simulator is an ALSA-sequencer client.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-launchkey-driver-test
        PRODUCT_NAME "ihub-launchkey-driver-test"
    )

    target_sources(ihub-launchkey-driver-test
        PRIVATE
            controller/LaunchkeyDriverTest.cpp
            midi/LaunchkeySim.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyDriver.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyProtocol.cpp"
    )

    target_include_directories(ihub-launchkey-driver-test
        PRIVATE
            "${IHUB_MIDI_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-launchkey-driver-test
        PRIVATE
            "IHUB_MIDI_FIXTURE_DIR=\"${IHUB_MIDI_DIR}/fixtures\""
    )

    target_link_libraries(ihub-launchkey-driver-test
        PRIVATE
            ALSA::ALSA
            juce::juce_audio_devices
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-launchkey-driver-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-launchkey-driver
        COMMAND ihub-launchkey-driver-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-launchkey-driver-test: ALSA simulator is Linux-only; skipping")
endif()

# --- set.yaml mapping and hot reload (T043) ---------------------------------
#
# Loads the shipped shared/set.yaml against its schema, rejects malformed
# documents, then edits a per-test set.yaml while a SlotSwitcher renders:
#
#   pad 0 -> slot 0, edit instrument -> slot 1, watcher publishes the new
#   document and arms slot 1 within 500 ms, every block keeps audio flowing and
#   no sample-to-sample step appears. A malformed edit keeps the old routing.

find_package(Threads REQUIRED)

juce_add_console_app(ihub-mapping-test
    PRODUCT_NAME "ihub-mapping-test"
)

target_sources(ihub-mapping-test
    PRIVATE
        controller/MappingTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
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

# --- Driver integration (T044) ----------------------------------------------
#
# The full controller path against the ALSA simulator:
#
#   virtual Launchkey -> LaunchkeyDriver -> Mapping -> SlotSwitcher
#                     -> LED feedback -> simulator recording
#
# pads-16.json is replayed one press at a time. Pads 0..7 are mapped by the
# shipped shared/set.yaml and must arm slots 0,1,2,3,4,5,6,0; pads 8..15 are
# unmapped and must change nothing. The recorded LED byte stream is compared
# exactly with tests/midi/fixtures/driver-integration-leds.golden.
#
# The connect handshake is checked byte for byte first, so removing it from the
# driver fails the test.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-driver-integration-test
        PRODUCT_NAME "ihub-driver-integration-test"
    )

    target_sources(ihub-driver-integration-test
        PRIVATE
            midi/DriverIntegration.cpp
            midi/LaunchkeySim.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyDriver.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyProtocol.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
    )

    target_include_directories(ihub-driver-integration-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/midi"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-driver-integration-test
        PRIVATE
            "IHUB_MIDI_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/midi/fixtures\""
            "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
    )

    target_link_libraries(ihub-driver-integration-test
        PRIVATE
            Threads::Threads
            ALSA::ALSA
            juce::juce_audio_devices
            juce::juce_audio_basics
            juce::juce_audio_processors_headless
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(ihub-driver-integration-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-driver-integration
        COMMAND ihub-driver-integration-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-driver-integration-test: ALSA simulator is Linux-only; skipping")
endif()

# --- Control API (T050) -----------------------------------------------------
#
# Starts the HTTP + WebSocket server on an ephemeral port, exercises every
# route (including token auth) and measures the `/meters` frame rate.

juce_add_console_app(ihub-api-test
    PRODUCT_NAME "ihub-api-test"
)

target_sources(ihub-api-test
    PRIVATE
        api/ApiTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
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

# --- QR pairing (T101) ------------------------------------------------------
#
# Acceptance: unpaired request -> 401, POST /pair with the 6-digit code -> a
# token, token -> GET /state 200. Also checks the runtime LAN rebind, single-use
# codes, revocation, loopback-only management endpoints and --lan startup.

juce_add_console_app(ihub-pairing-test
    PRODUCT_NAME "ihub-pairing-test"
)

target_sources(ihub-pairing-test
    PRIVATE
        api/PairingTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
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

# --- Live state mirror (T103) -----------------------------------------------
#
# Acceptance: an event injected by the MIDI simulator appears in a WS
# client's state within 100 ms. Two /events clients are connected; a pad
# press goes simulator -> LaunchkeyDriver -> StateBroadcast -> both sockets,
# and a POST /arm (the UI tap path) must reach both sockets too. Every
# measured latency is compared against the 100 ms budget.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-state-broadcast-test
        PRODUCT_NAME "ihub-state-broadcast-test"
    )

    target_sources(ihub-state-broadcast-test
        PRIVATE
            api/StateBroadcastTest.cpp
            midi/LaunchkeySim.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyDriver.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchkeyProtocol.cpp"
    )

    target_include_directories(ihub-state-broadcast-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/midi"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_link_libraries(ihub-state-broadcast-test
        PRIVATE
            Threads::Threads
            ALSA::ALSA
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
    message(STATUS "ihub-state-broadcast-test: ALSA simulator is Linux-only; skipping")
endif()

# --- Audio settings panel backend (T053) ------------------------------------
#
# Part 1 starts the control server against an in-memory audio backend and
# exercises GET/POST /audio plus POST /audio/measure. Part 2 drives the real
# AudioSettingsBridge on the ALSA Loopback: buffer 128 -> 256 -> 128, channel
# remapping, and a round-trip measurement.

juce_add_console_app(ihub-audio-settings-test
    PRODUCT_NAME "ihub-audio-settings-test"
)

target_sources(ihub-audio-settings-test
    PRIVATE
        api/AudioSettingsTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioSettings.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# --- MIDI inputs and the pad test (T126) ------------------------------------
#
# Part 1 starts the control server against an in-memory MIDI backend and
# exercises GET /midi, POST /midi/inputs and POST /midi/pad-test. Part 2
# enumerates the real ALSA inputs through JUCE, then drives the pad test
# through a scripted LaunchpadDriver message sink and verifies the 64-pad LED
# stream, its clear and the grid restore. No controller or sound card needed.

juce_add_console_app(ihub-midi-api-test
    PRODUCT_NAME "ihub-midi-api-test"
)

target_sources(ihub-midi-api-test
    PRIVATE
        api/MidiApiTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/MidiBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadDriver.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadProtocol.cpp"
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

# --- mDNS discovery (T102) --------------------------------------------------
#
# Acceptance: `avahi-browse -t _instrumenthub._tcp` lists the service. The test
# performs the same multicast round trip directly: it starts Discovery, sends a
# PTR query and checks the PTR/SRV/TXT records and the advertised port. Port
# 5353 is shared with avahi-daemon through SO_REUSEPORT.

juce_add_console_app(ihub-discovery-test
    PRODUCT_NAME "ihub-discovery-test"
)

target_sources(ihub-discovery-test
    PRIVATE
        api/DiscoveryTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
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

# --- Built-in compressor (T061) ---------------------------------------------
#
# Steady tones 3/6/12 dB over threshold at ratios 2:1, 4:1 and 8:1 must settle
# to the textbook static curve within 1 dB (the acceptance check), and an
# impulse must arrive exactly at the reported lookahead latency. Also checks
# makeup gain, bit-exact bypass with unchanged latency, stereo linking,
# attack/release direction, allocation-free processing, and state round-trip.

juce_add_console_app(ihub-compressor-test
    PRODUCT_NAME "ihub-compressor-test"
)

target_sources(ihub-compressor-test
    PRIVATE
        dsp/CompressorTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
)

target_include_directories(ihub-compressor-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# Same non-counting pairing note as the other allocation-counting tests.
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

# --- Built-in delay and reverb (T062) ---------------------------------------
#
# ihub-delay-test is the acceptance check: a 1/4-note delay at 120 BPM must
# place an impulse echo at exactly 12000 samples at 48 kHz. It also checks the
# whole sync table at three sample rates, free (ms) time, feedback repeats, a
# live BPM change, the reported PPQ repeat position, bit-exact bypass and dry
# mix, allocation-free processing and state round-trip.
#
# ihub-reverb-test checks the Freeverb-style network: bit-exact dry path,
# monotonic tail decay, room size -> tail length, damping -> darker tail,
# pre-delay silence, stereo width, stability at maximum room size, allocation
# freedom and state round-trip.

juce_add_console_app(ihub-delay-test
    PRODUCT_NAME "ihub-delay-test"
)

target_sources(ihub-delay-test
    PRIVATE
        dsp/DelayTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
)

target_include_directories(ihub-delay-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# Same non-counting pairing note as the other allocation-counting tests.
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

# --- Built-in 3-band EQ (T060) ----------------------------------------------
#
# An impulse is pushed through the juce::dsp::IIR cascade and its DFT is
# compared with an independent RBJ-cookbook target at 50 Hz, 1 kHz and 10 kHz.
# The tolerance is 0.5 dB. Also checks the flat/bypassed path is bit-exact, that
# a message-thread coefficient update is applied on the next block without
# allocating on the audio thread, and that state round-trips.

juce_add_console_app(ihub-eq-test
    PRODUCT_NAME "ihub-eq-test"
)

target_sources(ihub-eq-test
    PRIVATE
        dsp/EqTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
)

target_include_directories(ihub-eq-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# Same non-counting pairing note as the other allocation-counting tests.
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

# --- Transport core (T071) --------------------------------------------------
#
# Acceptance: feeding the exact frames of a 10 s render at 48 kHz and 120 BPM
# must leave PPQ at 20.0 +/- one internal tick. Also covers start/stop/continue,
# rewind and reposition, tempo changes, other sample rates, the tap-tempo
# median, the external-sync hook, and allocation-free processing.

add_executable(ihub-transport-test
    audio/TransportTest.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-transport-test PRIVATE cxx_std_20)

target_include_directories(ihub-transport-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# The replacement operator new is backed by malloc/free (which is a valid
# implementation), so GCC's pairing heuristic fires at inlined call sites.
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

# --- MIDI clock out (T072) --------------------------------------------------
#
# Acceptance: ten simulated seconds at 48 kHz and 120 BPM must emit exactly
# 480 clock messages, each within 1 ms (48 samples) of the ideal 1000-frame
# grid. Also covers start/stop/continue, rewind/reposition, tempo and format
# changes, the lock-free audio -> message queue, the MIDI bytes, and
# allocation-free processing.

add_executable(ihub-clock-out-test
    audio/ClockOutTest.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/ClockOut.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-clock-out-test PRIVATE cxx_std_20)

target_include_directories(ihub-clock-out-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# The replacement operator new is backed by malloc/free (which is a valid
# implementation), so GCC's pairing heuristic fires at inlined call sites.
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

# --- MIDI clock in (T073) ---------------------------------------------------
#
# Acceptance: a frame-timed 120 BPM clock master locks the transport within 2 s.
# Also covers other tempos, timestamp jitter, start/stop/continue, position hold
# while stopped, clock loss and relock, non-real-time bytes, queue overflow,
# MIDI byte values, and allocation-free processing.

add_executable(ihub-clock-in-test
    audio/ClockInTest.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/ClockIn.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-clock-in-test PRIVATE cxx_std_20)

target_include_directories(ihub-clock-in-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# The replacement operator new is backed by malloc/free (which is a valid
# implementation), so GCC's pairing heuristic fires at inlined call sites.
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

# --- Tap tempo (T074) -------------------------------------------------------
#
# Acceptance: taps spaced 500 ms +/- 20 ms converge to 120 +/- 1 BPM after four
# taps. The taps are exact timestamps (no wall clock), with several jitter
# patterns. Also covers the median window, sequence resets, the 20/300 BPM
# range boundaries, non-finite input, allocation-free tapping, and the
# Transport integration used by the controller button and the control API.

add_executable(ihub-tap-tempo-test
    audio/TapTempoTest.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
)

target_compile_features(ihub-tap-tempo-test PRIVATE cxx_std_20)

target_include_directories(ihub-tap-tempo-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# The replacement operator new is backed by malloc/free (which is a valid
# implementation), so GCC's pairing heuristic fires at inlined call sites.
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

# --- Set file container (T080) ----------------------------------------------
#
# Builds a four-slot set with two plugin state chunks, writes a real `.ihub`
# ZIP (set.json + mapping.yaml + plugin-state/<uuid>.bin), inspects the
# container, reloads it into a fresh model and a fresh AudioEngine, and asserts
# deep equality of every field including the opaque blobs and mapping bytes.
# Also rejects unsafe ids, duplicate ids, dangling references, out-of-range
# values, corrupt archives and unsupported schema versions.

juce_add_console_app(ihub-set-file-test
    PRODUCT_NAME "ihub-set-file-test"
)

target_sources(ihub-set-file-test
    PRIVATE
        project/SetFileTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Migrations.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/SetFile.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Serializer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# --- Set migrations (T082) --------------------------------------------------
#
# Loads the committed v1 fixture (fixtures/set-v1.ihub) through
# Serializer::load() and asserts it arrives as a complete v2 document: every v1
# value preserved, `notes` and every slot `colour` filled in with their
# defaults, and the migrated result writable and reloadable as v2. Also checks
# the chain directly (step count, idempotence, explicit values preserved) and
# that a newer schema version is rejected rather than guessed at.

juce_add_console_app(ihub-migrations-test
    PRODUCT_NAME "ihub-migrations-test"
)

target_sources(ihub-migrations-test
    PRIVATE
        project/MigrationsTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Migrations.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/SetFile.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Serializer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# --- Per-patch persistence (T119) -------------------------------------------
#
# Parses the shipped 64-patch set.yaml for instrument descriptors, mixer state,
# family/override colour and FX chains, then builds a 64-slot `.ihub` set with
# exactly one loaded patch. The container round-trip must preserve every patch,
# store only the loaded patch's plugin state, and keep the other 63 as
# descriptors.

juce_add_console_app(ihub-patch-persistence-test
    PRODUCT_NAME "ihub-patch-persistence-test"
)

target_sources(ihub-patch-persistence-test
    PRIVATE
        project/PatchPersistenceTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Migrations.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/SetFile.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Serializer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
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

# --- Plugin state store (T081) ----------------------------------------------
#
# Acceptance: a set whose second slot names a plugin that is not installed
# loads with a silent placeholder slot and a warning, while the installed slot
# keeps working. Also captures a live instance into a chunk with a fresh UUID,
# plugin identity and version stamp, then restores that blob into a fresh set,
# engine and store.

juce_add_console_app(ihub-plugin-state-store-test
    PRODUCT_NAME "ihub-plugin-state-store-test"
)

target_sources(ihub-plugin-state-store-test
    PRIVATE
        project/PluginStateStoreTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Migrations.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/PluginStateStore.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/SetFile.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/project/Serializer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
)

target_include_directories(ihub-plugin-state-store-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

target_compile_definitions(ihub-plugin-state-store-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-plugin-state-store-test
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-plugin-state-store-test TestPlugin_VST3)

set_target_properties(ihub-plugin-state-store-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-plugin-state-store
    COMMAND ihub-plugin-state-store-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# --- Sample loader (T090) ---------------------------------------------------
#
# Acceptance: a 48 kHz file loads unchanged, and a 44.1 kHz file becomes the
# right number of frames at 48 kHz with the same pitch. Fixtures are generated
# as float WAV, AIFF and FLAC inside the test, so there are no committed
# binaries. Also checks the background request path and that the audio-thread
# hand-off allocates nothing.

juce_add_console_app(ihub-sample-loader-test
    PRODUCT_NAME "ihub-sample-loader-test"
)

target_sources(ihub-sample-loader-test
    PRIVATE
        sampler/SampleLoaderTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
)

target_include_directories(ihub-sample-loader-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
)

# Same non-counting pairing note as the other allocation-counting tests.
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

# --- Kit format (T091) ------------------------------------------------------
#
# Acceptance: a 2-layer kit plays layer 1 below the split velocity and layer 2
# above it. The test generates two soft variants and one hard sample, loads a
# JSON kit, resolves at several velocities, renders the chosen one-shots and
# measures their pitch. It also checks round-robin, gain/pan/tune composition,
# path resolution, validation errors and that resolve() allocates nothing.

juce_add_console_app(ihub-kit-loader-test
    PRODUCT_NAME "ihub-kit-loader-test"
)

target_sources(ihub-kit-loader-test
    PRIVATE
        sampler/KitLoaderTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
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

# --- Sampler instrument (T092) ----------------------------------------------
#
# Acceptance: 16 simultaneous hits do not clip the 64-voice pool, and a closed
# hat chokes the open hat within 5 ms. The test generates a kit whose closed
# hat is digital silence, so the choke measurement is unambiguous. It also
# checks one-shot/gate behaviour, sample-accurate MIDI, voice stealing and that
# process() allocates nothing.

juce_add_console_app(ihub-sampler-instrument-test
    PRODUCT_NAME "ihub-sampler-instrument-test"
)

target_sources(ihub-sampler-instrument-test
    PRIVATE
        sampler/SamplerInstrumentTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
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

# --- UI snapshot (T054) -----------------------------------------------------
#
# Launches the real application with `--snapshot <path>` and validates the
# produced PNG: 1200x800, decodable and non-blank. In non-Debug builds it also
# checks that the web view served the embedded assets instead of the Vite dev
# server. Debug builds need `pnpm dev` running, so their ctest entry is
# disabled; snapshot acceptance is a Release check.

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

# --- Page snapshots (T127) ---------------------------------------------------
#
# Runs the server-rendered UI harness (`ui/src/testing/snapshot.tsx`) through
# pnpm, then validates the Grid, Instrument, Plugins and Audio snapshots it
# writes to `tests/artifacts/`. No browser or display is needed; the harness
# renders the real React tree against a fixture control API and asserts the
# page content, the 64 coloured cells and the red armed cell.

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

# --- End-to-end pad-to-audio (T117) -----------------------------------------
#
# One headless acceptance test for the whole shipped path:
#
#   LaunchpadDriver pad event -> Mapping (shared/set.yaml) -> Session::armSlot
#     -> SlotSwitcher 3 ms crossfade -> AudioEngine -> ALSA Loopback
#
# It checks the intended slot arms, that the armed slot renders non-silent
# audio, that the switch is click-free (residual against the ideal equal-power
# crossfade, plus a delta detector with a hard-switch negative control), and
# that the same pad events produce non-silent audio captured from the Loopback.
# Linux-only because it opens the ALSA Loopback card.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-e2e-test
        PRODUCT_NAME "ihub-e2e-test"
    )

    target_sources(ihub-e2e-test
        PRIVATE
            e2e/PadToAudio.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/Session.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Discovery.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Pairing.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/SessionBridge.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/StateBroadcast.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/AudioEngine.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/DeviceManager.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxRack.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Compressor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Delay.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Eq.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/dsp/Reverb.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/LatencyProbe.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/SlotSwitcher.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TapTempo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/TimingMonitor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Transport.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/WarmPool.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadDriver.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/LaunchpadProtocol.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/controller/Mapping.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/plugins/PluginInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/Kit.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/KitLoader.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SampleLoader.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/sampler/SamplerInstrument.cpp"
    )

    target_include_directories(ihub-e2e-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_compile_definitions(ihub-e2e-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_SET_YAML=\"${CMAKE_CURRENT_SOURCE_DIR}/../shared/set.yaml\""
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
