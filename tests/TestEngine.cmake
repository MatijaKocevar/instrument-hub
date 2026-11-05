include("${CMAKE_CURRENT_LIST_DIR}/../engine/ihub-engine.cmake")

set(IHUB_TEST_JUCE_HEADLESS_MODULES
    juce::juce_core
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors_headless
    juce::juce_dsp
    juce::juce_data_structures
)

set(IHUB_TEST_JUCE_GUI_MODULES
    juce::juce_core
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

set(IHUB_TEST_JUCE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/JUCE/modules")

function(ihub_test_juce_definitions target)
    target_compile_definitions(${target} INTERFACE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_JACK=1
        LINUX=1
        $<$<CONFIG:Debug>:DEBUG=1>
        $<$<CONFIG:Debug>:_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:_NDEBUG=1>
    )
endfunction()

add_library(ihub-test-engine-usage INTERFACE)
target_include_directories(ihub-test-engine-usage INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src"
    "${IHUB_TEST_JUCE_INCLUDE_DIR}"
)
ihub_test_juce_definitions(ihub-test-engine-usage)
target_compile_definitions(ihub-test-engine-usage INTERFACE
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
)
target_link_libraries(ihub-test-engine-usage INTERFACE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_library(ihub-test-engine-gui-usage INTERFACE)
target_include_directories(ihub-test-engine-gui-usage INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src"
    "${IHUB_TEST_JUCE_INCLUDE_DIR}"
)
ihub_test_juce_definitions(ihub-test-engine-gui-usage)
target_compile_definitions(ihub-test-engine-gui-usage INTERFACE
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    JUCE_MODULE_AVAILABLE_juce_graphics=1
    JUCE_MODULE_AVAILABLE_juce_gui_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1
)
target_link_libraries(ihub-test-engine-gui-usage INTERFACE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_library(ihub-test-engine-rt-usage INTERFACE)
target_include_directories(ihub-test-engine-rt-usage INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/../engine/src"
    "${IHUB_TEST_JUCE_INCLUDE_DIR}"
)
ihub_test_juce_definitions(ihub-test-engine-rt-usage)
target_compile_definitions(ihub-test-engine-rt-usage INTERFACE
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_MODULE_AVAILABLE_juce_events=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1
    JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_data_structures=1
    IHUB_RT_GUARD=1
    IHUB_RT_GUARD_WRAP_MALLOC=1
    IHUB_RT_GUARD_WRAP_LOCKS=1
)
target_link_libraries(ihub-test-engine-rt-usage INTERFACE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

function(ihub_test_engine_link_juce target modules)
    target_link_libraries(${target} PRIVATE ${modules})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(ALSA REQUIRED)
        target_link_libraries(${target} PRIVATE ALSA::ALSA Threads::Threads)
    endif()
endfunction()

add_library(ihub-test-engine STATIC ${IHUB_ENGINE_TEST_HEADLESS_SOURCES})
target_link_libraries(ihub-test-engine PUBLIC ihub-test-engine-usage)
target_link_libraries(ihub-test-engine PRIVATE ihub-httplib)
ihub_test_engine_link_juce(ihub-test-engine "${IHUB_TEST_JUCE_HEADLESS_MODULES}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-test-engine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
endif()

add_library(ihub-test-engine-rt STATIC ${IHUB_ENGINE_TEST_HEADLESS_SOURCES})
target_link_libraries(ihub-test-engine-rt PUBLIC ihub-test-engine-rt-usage)
target_link_libraries(ihub-test-engine-rt PRIVATE ihub-httplib)
ihub_test_engine_link_juce(ihub-test-engine-rt "${IHUB_TEST_JUCE_HEADLESS_MODULES}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-test-engine-rt PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
endif()

add_library(ihub-test-engine-gui STATIC ${IHUB_ENGINE_TEST_SOURCES})
target_link_libraries(ihub-test-engine-gui PUBLIC ihub-test-engine-gui-usage)
target_link_libraries(ihub-test-engine-gui PRIVATE ihub-httplib)
ihub_test_engine_link_juce(ihub-test-engine-gui "${IHUB_TEST_JUCE_GUI_MODULES}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_link_libraries(ihub-test-engine-gui PRIVATE X11::X11)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ihub-test-engine-gui PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-new-delete>)
endif()

function(ihub_link_test_engine target)
    cmake_parse_arguments(ARG "GUI;RT" "" "" ${ARGN})
    if(ARG_RT)
        target_link_libraries(${target} PRIVATE ihub-test-engine-rt)
    elseif(ARG_GUI)
        target_link_libraries(${target} PRIVATE ihub-test-engine-gui)
    else()
        target_link_libraries(${target} PRIVATE ihub-test-engine)
    endif()
endfunction()
