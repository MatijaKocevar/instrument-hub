# Instrument Hub engine (GUI application).
#
# The target directory is build/engine, so the executable lands at
# build/engine/instrument-hub as documented in plan/TESTING.md.

juce_add_gui_app(instrument-hub
    PRODUCT_NAME "Instrument Hub"
    COMPANY_NAME "Instrument Hub"
    BUNDLE_ID "com.instrumenthub.app"
    VERSION 0.1.0
)

target_sources(instrument-hub
    PRIVATE
        src/Main.cpp
)

target_compile_definitions(instrument-hub
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_VERSION>"
)

target_link_libraries(instrument-hub
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# JUCE puts artefacts in engine/instrument-hub_artefacts/<config>/. The plan
# (plan/TESTING.md) expects build/engine/instrument-hub, so emit the binary
# directly into this directory for every configuration.
set_target_properties(instrument-hub PROPERTIES
    OUTPUT_NAME "instrument-hub"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)
