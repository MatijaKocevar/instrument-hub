include("${CMAKE_CURRENT_SOURCE_DIR}/ihub-engine.cmake")

get_filename_component(IHUB_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ui" ABSOLUTE)
set(IHUB_UI_DIST "${IHUB_UI_DIR}/dist")
set(IHUB_UI_INDEX "${IHUB_UI_DIST}/index.html")
set(IHUB_UI_JS "${IHUB_UI_DIST}/assets/app.js")
set(IHUB_UI_CSS "${IHUB_UI_DIST}/assets/index.css")

find_program(IHUB_PNPM_EXECUTABLE pnpm DOC "pnpm executable used to build the React UI")

if(IHUB_PNPM_EXECUTABLE)
    set(IHUB_PNPM_COMMAND "${IHUB_PNPM_EXECUTABLE}")
else()
    find_program(IHUB_COREPACK_EXECUTABLE corepack DOC "corepack, used to run pnpm")

    if(NOT IHUB_COREPACK_EXECUTABLE)
        message(FATAL_ERROR "pnpm (or corepack) was not found. Install Node.js to build ui/.")
    endif()

    set(IHUB_PNPM_COMMAND "${IHUB_COREPACK_EXECUTABLE}" pnpm)
endif()

file(GLOB_RECURSE IHUB_UI_SOURCES CONFIGURE_DEPENDS "${IHUB_UI_DIR}/src/*")

set(IHUB_UI_BUILD_INPUTS
    "${IHUB_UI_DIR}/index.html"
    "${IHUB_UI_DIR}/package.json"
    "${IHUB_UI_DIR}/pnpm-lock.yaml"
    "${IHUB_UI_DIR}/tsconfig.json"
    "${IHUB_UI_DIR}/vite.config.ts"
    ${IHUB_UI_SOURCES}
)

set(IHUB_UI_NODE_MODULES "${IHUB_UI_DIR}/node_modules")
set(IHUB_UI_INSTALL_STAMP "${IHUB_UI_NODE_MODULES}/.modules.yaml")

add_custom_command(
    OUTPUT "${IHUB_UI_INSTALL_STAMP}"
    COMMAND ${IHUB_PNPM_COMMAND} install --frozen-lockfile
    WORKING_DIRECTORY "${IHUB_UI_DIR}"
    DEPENDS "${IHUB_UI_DIR}/package.json" "${IHUB_UI_DIR}/pnpm-lock.yaml"
    COMMENT "Installing React UI dependencies"
    VERBATIM
)

add_custom_command(
    OUTPUT "${IHUB_UI_INDEX}" "${IHUB_UI_JS}" "${IHUB_UI_CSS}"
    COMMAND ${IHUB_PNPM_COMMAND} run build
    WORKING_DIRECTORY "${IHUB_UI_DIR}"
    DEPENDS "${IHUB_UI_INSTALL_STAMP}" ${IHUB_UI_BUILD_INPUTS}
    COMMENT "Building React UI into ui/dist"
    VERBATIM
)

add_custom_target(instrument-hub-ui
    DEPENDS "${IHUB_UI_INDEX}" "${IHUB_UI_JS}" "${IHUB_UI_CSS}"
)

juce_add_binary_data(InstrumentHubAssets
    SOURCES
        "${IHUB_UI_INDEX}"
        "${IHUB_UI_JS}"
        "${IHUB_UI_CSS}"
    NAMESPACE BinaryData
    HEADER_NAME BinaryData.h
)

add_dependencies(InstrumentHubAssets instrument-hub-ui)

get_filename_component(IHUB_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared" ABSOLUTE)
set(IHUB_TRACKS_TEMPLATE "${IHUB_SHARED_DIR}/tracks-set.yaml")
set(IHUB_BLANK_TEMPLATE "${IHUB_SHARED_DIR}/blank-set.yaml")

juce_add_binary_data(InstrumentHubTemplates
    SOURCES
        "${IHUB_TRACKS_TEMPLATE}"
        "${IHUB_BLANK_TEMPLATE}"
    NAMESPACE BundledData
    HEADER_NAME BundledTemplates.h
)

juce_add_gui_app(instrument-hub
    PRODUCT_NAME "Instrument Hub"
    COMPANY_NAME "Instrument Hub"
    BUNDLE_ID "com.instrumenthub.app"
    VERSION ${PROJECT_VERSION}
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
)
target_sources(instrument-hub
    PRIVATE
        ${IHUB_ENGINE_SOURCES}
)

target_include_directories(instrument-hub
    PRIVATE
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib"
)

add_library(ihub-httplib INTERFACE)
target_include_directories(ihub-httplib SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib")
target_link_libraries(instrument-hub PRIVATE ihub-httplib)

target_compile_definitions(instrument-hub
    PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:instrument-hub,JUCE_VERSION>"
        JUCE_JACK=1
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(instrument-hub
    PRIVATE
        InstrumentHubAssets
        InstrumentHubTemplates
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    find_package(ALSA REQUIRED)
    target_link_libraries(instrument-hub PRIVATE X11::X11 ALSA::ALSA)
endif()

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

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(instrument-hub PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
        endif()
    endif()
endif()

set_target_properties(instrument-hub PROPERTIES
    OUTPUT_NAME "instrument-hub"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

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

add_dependencies(instrument-hub ihub-scanner)

juce_add_console_app(ihub-sandbox
    PRODUCT_NAME "ihub-sandbox"
)

target_sources(ihub-sandbox
    PRIVATE
        src/plugins/sandbox_main.cpp
        src/plugins/SandboxClient.cpp
        src/plugins/PluginWindow.cpp
        src/plugins/window/PluginEditorWindow.cpp
        src/plugins/window/PluginWindowEditor.cpp
        src/plugins/sandbox/FakeToneInstrument.cpp
        src/plugins/sandbox/SandboxClientArguments.cpp
        src/plugins/sandbox/SandboxClientControl.cpp
        src/plugins/sandbox/SandboxClientEditor.cpp
        src/plugins/sandbox/SandboxPageClient.cpp
        src/plugins/PluginInstrument.cpp
        src/plugins/host/PluginInstrumentBus.cpp
        src/plugins/host/PluginInstrumentMidi.cpp
        src/plugins/host/PluginInstrumentNotifications.cpp
        src/plugins/host/PluginInstrumentParameters.cpp
        src/plugins/host/PluginInstrumentProcess.cpp
        src/plugins/host/PluginInstrumentState.cpp
        src/instruments/Instrument.cpp
)

target_include_directories(ihub-sandbox
    PRIVATE
        src
)

target_compile_definitions(ihub-sandbox
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ihub-sandbox
    PRIVATE
        juce::juce_audio_processors
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

set_target_properties(ihub-sandbox PROPERTIES
    OUTPUT_NAME "ihub-sandbox"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
)

add_dependencies(instrument-hub ihub-sandbox)
