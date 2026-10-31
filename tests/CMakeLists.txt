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

# --- Instrument interface and audio input (T030) ----------------------------
#
# Checks the AudioInputInstrument in isolation (unity/gain/channel selection/
# latency) and then plays a sine into the ALSA Loopback, captures it, and feeds
# every block through the instrument -> slot -> master bus path. The bus output
# must reproduce the captured input at bit-exact unity gain.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-instrument-test
        PRODUCT_NAME "ihub-instrument-test"
    )

    target_sources(ihub-instrument-test
        PRIVATE
            instruments/AudioInputInstrumentTest.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/Instrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/instruments/AudioInputInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Bus.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/FxChain.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/audio/Slot.cpp"
    )

    target_include_directories(ihub-instrument-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src"
    )

    target_link_libraries(ihub-instrument-test
        PRIVATE
            ALSA::ALSA
            juce::juce_audio_basics
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
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Routes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine/src/api/Server.cpp"
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
