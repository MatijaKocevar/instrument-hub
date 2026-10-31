cmake_minimum_required(VERSION 3.22)

project(InstrumentHub VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Only Ninja and Unix Makefiles are supported (see plan/DECISIONS.md).
# Single-config generators keep the binary at build/engine/<name>.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# --- Realtime safety harness (T014) ------------------------------------------
#
# RtGuard is a debug-time assertion layer: it replaces operator new and wraps
# malloc/pthread so that allocation, locking or logging inside a
# processBlock() scope is caught (and aborts in --rt-strict mode). Release
# builds compile it out unless explicitly requested.

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
add_subdirectory(tests)
