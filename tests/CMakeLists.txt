# Headless verification tools.
#
# ihub-selftest is plain ALSA; the device manager test links JUCE so it can
# exercise the same AudioDeviceManager wrapper the application uses.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA REQUIRED)

    add_executable(ihub-selftest
        selftest/AudioSmoke.cpp
    )

    target_compile_features(ihub-selftest PRIVATE cxx_std_20)

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
else()
    message(STATUS "ihub-selftest: ALSA smoke test is Linux-only; skipping")
endif()

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
