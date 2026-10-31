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
