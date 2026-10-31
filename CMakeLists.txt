cmake_minimum_required(VERSION 3.22)

project(InstrumentHub VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Only Ninja and Unix Makefiles are supported (see plan/DECISIONS.md).
# Single-config generators keep the binary at build/engine/<name>.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(third_party/JUCE)
add_subdirectory(engine)
add_subdirectory(tests)
