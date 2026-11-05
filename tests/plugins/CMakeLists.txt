find_package(Threads REQUIRED)

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
    NEEDS_MIDI_INPUT TRUE
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

juce_add_plugin(NoteTonePlugin
    PRODUCT_NAME "NoteTonePlugin"
    COMPANY_NAME "Instrument Hub"
    BUNDLE_ID "com.instrumenthub.notetoneplugin"
    PLUGIN_MANUFACTURER_CODE IHUB
    PLUGIN_CODE NTNP
    DESCRIPTION "Instrument Hub note-gated test instrument"
    FORMATS VST3
    VST3_CATEGORIES Instrument
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST TRUE
)

target_sources(NoteTonePlugin
    PRIVATE
        NoteTonePlugin/NoteTonePlugin.cpp
        NoteTonePlugin/NoteTonePluginProcessor.cpp
)

target_compile_definitions(NoteTonePlugin
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NoteTonePlugin
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

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

juce_add_console_app(ihub-plugin-instance-test
    PRODUCT_NAME "ihub-plugin-instance-test"
)

target_sources(ihub-plugin-instance-test
    PRIVATE
        PluginInstanceTest.cpp
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

ihub_link_test_engine(ihub-plugin-instance-test)


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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
add_library(ihub-plugin-window-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/PluginWindowTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_plugin_window_test")


    target_sources(ihub-plugin-window-test
        PRIVATE
            PluginWindowTest.cpp
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

target_link_libraries(ihub-plugin-window-test PRIVATE ihub-test-engine-gui-usage)



    add_dependencies(ihub-plugin-window-test TestPlugin_VST3)



    add_test(
        NAME ihub-plugin-window
        COMMAND ihub-runner-gui ihub-plugin-window
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ihub-plugin-window PROPERTIES
        ENVIRONMENT "DISPLAY=:0"
    )
else()
    message(STATUS "ihub-plugin-window-test: Linux-only; skipping")
endif()

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

add_library(ihub-plugin-scanner-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ScannerTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_plugin_scanner_test")


    target_sources(ihub-plugin-scanner-test
        PRIVATE
            ScannerTest.cpp
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

target_link_libraries(ihub-plugin-scanner-test PRIVATE ihub-test-engine-gui-usage)



    add_dependencies(ihub-plugin-scanner-test
        TestPlugin_VST3
        ihub-crashing-vst3
        ihub-scanner
    )



    add_test(
        NAME ihub-plugin-scanner
        COMMAND ihub-runner-gui ihub-plugin-scanner
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

add_library(ihub-plugin-cache-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/PluginCacheTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_plugin_cache_test")


    target_sources(ihub-plugin-cache-test
        PRIVATE
            PluginCacheTest.cpp
    )

    target_include_directories(ihub-plugin-cache-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src"
    )

    target_compile_definitions(ihub-plugin-cache-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
            "IHUB_CRASHING_VST3=\"${IHUB_CRASHING_BUNDLE}\""
            "IHUB_SCANNER_HELPER=\"$<TARGET_FILE:ihub-scanner>\""
    )

target_link_libraries(ihub-plugin-cache-test PRIVATE ihub-test-engine-gui-usage)



    add_dependencies(ihub-plugin-cache-test
        TestPlugin_VST3
        ihub-crashing-vst3
        ihub-scanner
    )



    add_test(
        NAME ihub-plugin-cache
        COMMAND ihub-runner-gui ihub-plugin-cache
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

add_library(ihub-plugin-inventory-api-test OBJECT)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/PluginInventoryApiTest.cpp" PROPERTIES COMPILE_DEFINITIONS "main=ihub_scenario_ihub_plugin_inventory_api_test")


    target_sources(ihub-plugin-inventory-api-test
        PRIVATE
            PluginInventoryApiTest.cpp
    )

    target_include_directories(ihub-plugin-inventory-api-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src"
    )

    target_compile_definitions(ihub-plugin-inventory-api-test
        PRIVATE
            JUCE_PLUGINHOST_VST3=1
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
            "IHUB_SCANNER_HELPER=\"$<TARGET_FILE:ihub-scanner>\""
    )

    target_link_libraries(ihub-plugin-inventory-api-test
        PRIVATE
            Threads::Threads
            ihub-httplib
    )
target_link_libraries(ihub-plugin-inventory-api-test PRIVATE ihub-test-engine-gui-usage)



    add_dependencies(ihub-plugin-inventory-api-test
        TestPlugin_VST3
        ihub-scanner
    )



    add_test(
        NAME ihub-plugin-inventory-api
        COMMAND ihub-runner-gui ihub-plugin-inventory-api
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-plugin-scanner-test: Linux-only; skipping")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    juce_add_console_app(ihub-sandbox-test
        PRODUCT_NAME "ihub-sandbox-test"
    )

    target_sources(ihub-sandbox-test
        PRIVATE
            SandboxTest.cpp
            sandbox/TestSupport.cpp
            sandbox/Transport.cpp
            sandbox/CrashIsolation.cpp
            sandbox/PageHostFailure.cpp
            sandbox/PageHostTransport.cpp
            sandbox/PageHostRecovery.cpp
            sandbox/RealPlugin.cpp
    )

    target_include_directories(ihub-sandbox-test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/src"
    )

    target_compile_definitions(ihub-sandbox-test
        PRIVATE
            "IHUB_SANDBOX_HELPER=\"$<TARGET_FILE:ihub-sandbox>\""
            "IHUB_TESTPLUGIN_VST3=\"$<GENEX_EVAL:$<TARGET_PROPERTY:TestPlugin_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>\""
    )

ihub_link_test_engine(ihub-sandbox-test)


    add_dependencies(ihub-sandbox-test
        ihub-sandbox
        TestPlugin_VST3
    )

    set_target_properties(ihub-sandbox-test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME ihub-sandbox
        COMMAND ihub-sandbox-test
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
else()
    message(STATUS "ihub-sandbox-test: POSIX process isolation is Linux-only; skipping")
endif()
