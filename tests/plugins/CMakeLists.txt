# TestPlugin: the in-repo VST3 used by every plugin test.
#
# No third-party plug-in binaries are required: CI builds this bundle and the
# scan test proves that juce::AudioPluginFormatManager can find and instantiate
# it. The bundle lands in
#
#     build/tests/plugins/TestPlugin_artefacts/<config>/TestPlugin.vst3
#
# (JUCE's normal artefact layout).

juce_add_plugin(TestPlugin
    PRODUCT_NAME "TestPlugin"
    COMPANY_NAME "Instrument Hub"
    BUNDLE_ID "com.instrumenthub.testplugin"
    PLUGIN_MANUFACTURER_CODE IHUB
    PLUGIN_CODE TSTP
    DESCRIPTION "Instrument Hub test plugin: gain, sine generator and artificial latency"
    FORMATS VST3
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST TRUE
)

target_sources(TestPlugin
    PRIVATE
        TestPlugin/TestPlugin.cpp
        TestPlugin/TestPluginProcessor.cpp
)

target_compile_definitions(TestPlugin
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TestPlugin
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- Acceptance check -------------------------------------------------------
#
# A headless host executable: it loads the VST3 bundle through the format
# manager, checks that a directory scan finds it, then instantiates it and
# verifies the reported latency and sine output.

juce_add_console_app(ihub-testplugin-scan-test
    PRODUCT_NAME "ihub-testplugin-scan-test"
)

target_sources(ihub-testplugin-scan-test
    PRIVATE
        PluginScanTest.cpp
)

target_compile_definitions(ihub-testplugin-scan-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-testplugin-scan-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# The scan test must never run before the bundle it inspects has been built.
add_dependencies(ihub-testplugin-scan-test TestPlugin_VST3)

set_target_properties(ihub-testplugin-scan-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-testplugin-scan
    COMMAND ihub-testplugin-scan-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# --- Plugin instance wrapper (T022) -----------------------------------------
#
# Hosts TestPlugin through PluginInstrument: sets the DSP-irrelevant State
# Marker, saves the state blob, restores it into a second instance and checks
# the parameter, then exercises prepare/latency/process/MIDI/unload.

juce_add_console_app(ihub-plugin-instance-test
    PRODUCT_NAME "ihub-plugin-instance-test"
)

target_sources(ihub-plugin-instance-test
    PRIVATE
        PluginInstanceTest.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src/plugins/PluginInstrument.cpp"
)

target_include_directories(ihub-plugin-instance-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src"
)

target_compile_definitions(ihub-plugin-instance-test
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
)

target_link_libraries(ihub-plugin-instance-test
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_dependencies(ihub-plugin-instance-test TestPlugin_VST3)

set_target_properties(ihub-plugin-instance-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_test(
    NAME ihub-plugin-instance
    COMMAND ihub-plugin-instance-test
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# --- Plugin editor window (T023) --------------------------------------------
#
# Loads TestPlugin, opens its native editor in an app-owned PluginWindow, keeps
# it on screen for two seconds under DISPLAY=:0, then closes it explicitly and
# again through PluginInstrument::unload(). The editor is snapshotted from
# inside JUCE to prove it rendered. A Debug build enables JUCE's leak detector,
# so a clean exit is the leak check.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-plugin-window-test
        PRODUCT_NAME "ihub-plugin-window-test"
    )

    target_sources(ihub-plugin-window-test
        PRIVATE
            PluginWindowTest.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src/plugins/PluginInstrument.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src/plugins/PluginWindow.cpp"
    )

    target_include_directories(ihub-plugin-window-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src"
    )

    target_compile_definitions(ihub-plugin-window-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

    target_link_libraries(ihub-plugin-window-test
        PRIVATE
            juce::juce_audio_processors
            juce::juce_gui_basics
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-plugin-window-test TestPlugin_VST3)

    set_target_properties(ihub-plugin-window-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-plugin-window
        COMMAND ihub-plugin-window-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-plugin-window PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
else()
    message(STATUS "ihub-plugin-window-test: Linux-only; skipping")
endif()

# --- Plugin scanner acceptance (T021) ---------------------------------------
#
# The scanner must survive a plugin that crashes while being loaded. The fake
# bundle below is a shared library whose ELF constructor raises SIGSEGV inside
# dlopen(); JUCE's VST3 fast path is disabled by simply not writing a
# moduleinfo.json, so the helper really loads it.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE IHUB_MACHINE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(IHUB_CRASHING_BUNDLE
        "${CMAKE_CURRENT_BINARY_DIR}/fake/CrashingPlugin.vst3")

    add_library(ihub-crashing-vst3 MODULE
        CrashingPlugin/CrashingPlugin.cpp
    )

    set_target_properties(ihub-crashing-vst3 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "CrashingPlugin"
        LIBRARY_OUTPUT_DIRECTORY
            "${IHUB_CRASHING_BUNDLE}/Contents/${IHUB_MACHINE}-linux"
    )

    juce_add_console_app(ihub-plugin-scanner-test
        PRODUCT_NAME "ihub-plugin-scanner-test"
    )

    target_sources(ihub-plugin-scanner-test
        PRIVATE
            ScannerTest.cpp
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src/plugins/Scanner.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src/plugins/ScannerProcess.cpp"
    )

    target_include_directories(ihub-plugin-scanner-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src"
    )

    target_compile_definitions(ihub-plugin-scanner-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
            "IHUB_CRASHING_VST3=\"${IHUB_CRASHING_BUNDLE}\""
            "IHUB_SCANNER_HELPER=\"$<TARGET_FILE:ihub-scanner>\""
    )

    target_link_libraries(ihub-plugin-scanner-test
        PRIVATE
            juce::juce_audio_processors
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_dependencies(ihub-plugin-scanner-test
        TestPlugin_VST3
        ihub-crashing-vst3
        ihub-scanner
    )

    set_target_properties(ihub-plugin-scanner-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-plugin-scanner
        COMMAND ihub-plugin-scanner-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-plugin-scanner-test: Linux-only; skipping")
endif()
