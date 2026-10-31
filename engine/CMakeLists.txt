# Instrument Hub engine (GUI application).
#
# The target directory is build/engine, so the executable lands at
# build/engine/instrument-hub as documented in plan/TESTING.md.

# --- React UI ---------------------------------------------------------------
#
# `ui/dist` is a build artefact (gitignored). These custom commands make the
# normal CMake build produce it, so a clean checkout only needs Node.js: pnpm
# (through the corepack shim when it is not installed globally) installs the
# locked dependencies, then Vite writes the three stable files the embedding
# below expects.

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
        src/api/Discovery.cpp
        src/api/Pairing.cpp
        src/api/Routes.cpp
        src/api/Server.cpp
        src/api/StateBroadcast.cpp
        src/Bridge/WebView.cpp
        src/audio/AudioEngine.cpp
        src/audio/AudioSettings.cpp
        src/audio/Bus.cpp
        src/audio/ClockIn.cpp
        src/audio/ClockOut.cpp
        src/audio/DeviceManager.cpp
        src/audio/FxChain.cpp
        src/audio/LatencyProbe.cpp
        src/audio/RtGuard.cpp
        src/audio/Slot.cpp
        src/audio/SlotSwitcher.cpp
        src/audio/TimingMonitor.cpp
        src/audio/TapTempo.cpp
        src/audio/Transport.cpp
        src/audio/WarmPool.cpp
        src/controller/LaunchkeyDriver.cpp
        src/controller/LaunchpadDriver.cpp
        src/controller/LaunchkeyProtocol.cpp
        src/controller/LaunchpadProtocol.cpp
        src/controller/Mapping.cpp
        src/dsp/Compressor.cpp
        src/dsp/Delay.cpp
        src/dsp/Eq.cpp
        src/dsp/Reverb.cpp
        src/diag/CrashDump.cpp
        src/diag/Diagnostics.cpp
        src/diag/LogRing.cpp
        src/instruments/Instrument.cpp
        src/instruments/AudioInputInstrument.cpp
        src/instruments/PluginFx.cpp
        src/plugins/PluginInstrument.cpp
        src/plugins/PluginCache.cpp
        src/plugins/PluginWindow.cpp
        src/plugins/SandboxHost.cpp
        src/plugins/Scanner.cpp
        src/plugins/ScannerProcess.cpp
        src/project/Migrations.cpp
        src/project/PluginStateStore.cpp
        src/project/SetFile.cpp
        src/project/Serializer.cpp
        src/Session.cpp
        src/sampler/Kit.cpp
        src/sampler/KitLoader.cpp
        src/sampler/SampleLoader.cpp
        src/sampler/SamplerInstrument.cpp
)

target_include_directories(instrument-hub
    PRIVATE
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib"
)

# cpp-httplib is vendored header-only (MIT). Its header is included by the API
# sources only; no find_package and no system dependency.
add_library(ihub-httplib INTERFACE)
target_include_directories(ihub-httplib SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cpp-httplib")
target_link_libraries(instrument-hub PRIVATE ihub-httplib)

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

# --- Native snapshot capture (T054) -----------------------------------------
#
# On Linux the web view is rendered by a separate WebKit process and composited
# by X, so `--snapshot` reads the real window back with XGetImage. That needs
# the Xlib symbols linked explicitly; JUCE does not export them.

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_link_libraries(instrument-hub PRIVATE X11::X11)
endif()

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

# --- Out-of-process plugin sandbox (T110) -----------------------------------
#
# Optional crash isolation for one instrument slot. SandboxHost creates a
# shared-memory file and launches this helper, which is the only process that
# loads the plugin:
#
#     ihub-sandbox --map <file> --plugin <bundle.vst3>
#     ihub-sandbox --map <file> --fake-tone <hz> [--abort-after <blocks>]
#
# The second form is the test seam: a fake plugin that can take the sandbox
# down on purpose. A plugin abort, segfault or hang can only end this helper;
# the host watchdog marks the slot failed and audio keeps running.

juce_add_console_app(ihub-sandbox
    PRODUCT_NAME "ihub-sandbox"
)

target_sources(ihub-sandbox
    PRIVATE
        src/plugins/sandbox_main.cpp
        src/plugins/SandboxClient.cpp
        src/plugins/PluginInstrument.cpp
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
        juce::juce_audio_processors_headless
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
