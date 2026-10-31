# Instrument Hub engine (GUI application).
#
# The target directory is build/engine, so the executable lands at
# build/engine/instrument-hub as documented in plan/TESTING.md.

# --- React UI ---------------------------------------------------------------
#
# `ui/dist` is a build artefact (gitignored). These custom commands make the
# normal CMake build produce it, so a clean checkout only needs npm available:
# package-lock.json drives `npm ci`, then Vite writes the three stable files the
# embedding below expects.

get_filename_component(IHUB_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ui" ABSOLUTE)
set(IHUB_UI_DIST "${IHUB_UI_DIR}/dist")
set(IHUB_UI_INDEX "${IHUB_UI_DIST}/index.html")
set(IHUB_UI_JS "${IHUB_UI_DIST}/assets/app.js")
set(IHUB_UI_CSS "${IHUB_UI_DIST}/assets/index.css")

find_program(IHUB_NPM_EXECUTABLE npm DOC "npm executable used to build the React UI")

if(NOT IHUB_NPM_EXECUTABLE)
    message(FATAL_ERROR "npm was not found. Install Node.js/npm to build ui/.")
endif()

file(GLOB_RECURSE IHUB_UI_SOURCES CONFIGURE_DEPENDS "${IHUB_UI_DIR}/src/*")

set(IHUB_UI_BUILD_INPUTS
    "${IHUB_UI_DIR}/index.html"
    "${IHUB_UI_DIR}/package.json"
    "${IHUB_UI_DIR}/package-lock.json"
    "${IHUB_UI_DIR}/tsconfig.json"
    "${IHUB_UI_DIR}/vite.config.ts"
    ${IHUB_UI_SOURCES}
)

set(IHUB_UI_NODE_MODULES "${IHUB_UI_DIR}/node_modules")

add_custom_command(
    OUTPUT "${IHUB_UI_NODE_MODULES}/.package-lock.json"
    COMMAND "${IHUB_NPM_EXECUTABLE}" ci --no-audit --no-fund
    WORKING_DIRECTORY "${IHUB_UI_DIR}"
    DEPENDS "${IHUB_UI_DIR}/package.json" "${IHUB_UI_DIR}/package-lock.json"
    COMMENT "Installing React UI dependencies"
    VERBATIM
)

add_custom_command(
    OUTPUT "${IHUB_UI_INDEX}" "${IHUB_UI_JS}" "${IHUB_UI_CSS}"
    COMMAND "${IHUB_NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${IHUB_UI_DIR}"
    DEPENDS "${IHUB_UI_NODE_MODULES}/.package-lock.json" ${IHUB_UI_BUILD_INPUTS}
    COMMENT "Building React UI into ui/dist"
    VERBATIM
)

add_custom_target(instrument-hub-ui
    DEPENDS "${IHUB_UI_INDEX}" "${IHUB_UI_JS}" "${IHUB_UI_CSS}"
)

# Embed the production build. WebView's release resource provider serves these
# three files; debug builds load the Vite dev server instead.
juce_add_binary_data(InstrumentHubAssets
    SOURCES
        "${IHUB_UI_INDEX}"
        "${IHUB_UI_JS}"
        "${IHUB_UI_CSS}"
    NAMESPACE BinaryData
    HEADER_NAME BinaryData.h
)

add_dependencies(InstrumentHubAssets instrument-hub-ui)

# --- Application ------------------------------------------------------------

juce_add_gui_app(instrument-hub
    PRODUCT_NAME "Instrument Hub"
    COMPANY_NAME "Instrument Hub"
    BUNDLE_ID "com.instrumenthub.app"
    VERSION 0.1.0
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
)

target_sources(instrument-hub
    PRIVATE
        src/Main.cpp
        src/Bridge/WebView.cpp
)

target_include_directories(instrument-hub
    PRIVATE
        src
)

target_compile_definitions(instrument-hub
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_VERSION>"
)

target_link_libraries(instrument-hub
    PRIVATE
        InstrumentHubAssets
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
