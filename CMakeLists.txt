cmake_minimum_required(VERSION 3.22)

project(InstrumentHub VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED IHUB_ENABLE_RT_GUARD)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(IHUB_ENABLE_RT_GUARD_DEFAULT OFF)
    else()
        set(IHUB_ENABLE_RT_GUARD_DEFAULT ON)
    endif()
else()
    set(IHUB_ENABLE_RT_GUARD_DEFAULT ${IHUB_ENABLE_RT_GUARD})
endif()

option(IHUB_ENABLE_RT_GUARD
       "Compile the realtime safety guard (allocation/lock/log assertions)"
       ${IHUB_ENABLE_RT_GUARD_DEFAULT})

add_subdirectory(third_party/JUCE)
add_subdirectory(engine)
