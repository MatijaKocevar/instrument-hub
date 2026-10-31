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
        src/audio/AudioEngine.cpp
        src/audio/Bus.cpp
        src/audio/DeviceManager.cpp
        src/audio/LatencyProbe.cpp
        src/audio/RtGuard.cpp
        src/audio/Slot.cpp
        src/audio/TimingMonitor.cpp
        src/plugins/PluginInstrument.cpp
        src/plugins/Scanner.cpp
        src/plugins/ScannerProcess.cpp
)

target_include_directories(instrument-hub
    PRIVATE
        src
)

target_compile_definitions(instrument-hub
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_VERSION>"
        JUCE_PLUGINHOST_VST3=1
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

# --- Realtime safety harness (T014) -----------------------------------------
#
# See the top-level CMakeLists.txt for the option. The linker wrap is what lets
# RtGuard see raw malloc and pthread locks; the operator new replacement lives
# in RtGuard.cpp and needs no linker help.

if(IHUB_ENABLE_RT_GUARD)
    target_compile_definitions(instrument-hub PRIVATE IHUB_RT_GUARD=1)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_definitions(instrument-hub PRIVATE
            IHUB_RT_GUARD_WRAP_MALLOC=1
            IHUB_RT_GUARD_WRAP_LOCKS=1
        )

        target_link_options(instrument-hub PRIVATE
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

        # The replacement operator new is backed by malloc/free, so GCC flags
        # the pairing at call sites it inlines.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(instrument-hub PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
        endif()
    endif()
endif()

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

# --- Out-of-process plugin scanner (T021) -----------------------------------
#
# A tiny console helper, launched once per plugin bundle:
#
#     ihub-scanner --scan <bundle.vst3> --out <result.xml> [--format VST3]
#
# It is the only binary that ever loads plugin code, so a VST3 that segfaults
# during scan cannot take the application down with it. ScannerProcess looks
# for it next to instrument-hub.

juce_add_console_app(ihub-scanner
    PRODUCT_NAME "ihub-scanner"
)

target_sources(ihub-scanner
    PRIVATE
        src/plugins/scanner_main.cpp
)

target_include_directories(ihub-scanner
    PRIVATE
        src
)

target_compile_definitions(ihub-scanner
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ihub-scanner
    PRIVATE
        juce::juce_audio_processors_headless
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-scanner PROPERTIES
    OUTPUT_NAME "ihub-scanner"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

# The app shells out to the helper; building the app should always produce it.
add_dependencies(instrument-hub ihub-scanner)
