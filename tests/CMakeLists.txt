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
