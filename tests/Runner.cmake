function(ihub_add_test_runner runner)
    cmake_parse_arguments(ARG "RT;WRAP_ALLOC" "ENGINE;USAGE" "TARGETS;SCENARIOS;SHARED;DEFINITIONS" ${ARGN})

    set(source "${CMAKE_CURRENT_BINARY_DIR}/${runner}.cpp")
    set(content "#include <cstring>\n#include <iostream>\n\n")

    foreach(entry IN LISTS ARG_SCENARIOS)
        string(REPLACE "=" ";" parts "${entry}")
        list(GET parts 0 name)
        list(GET parts 1 function)
        list(LENGTH parts count)
        if(count GREATER 2)
            list(GET parts 2 signature)
        else()
            set(signature "plain")
        endif()
        if(signature STREQUAL "args")
            string(APPEND content "extern int ${function}(int, char**);\n")
        else()
            string(APPEND content "extern int ${function}();\n")
        endif()
    endforeach()

    string(APPEND content "\nnamespace\n{\n")

    foreach(entry IN LISTS ARG_SCENARIOS)
        string(REPLACE "=" ";" parts "${entry}")
        list(GET parts 0 name)
        list(GET parts 1 function)
        list(LENGTH parts count)
        if(count GREATER 2)
            list(GET parts 2 signature)
        else()
            set(signature "plain")
        endif()
        if(signature STREQUAL "args")
            string(APPEND content "int run_${function}(int argc, char** argv) { return ${function}(argc, argv); }\n")
        else()
            string(APPEND content "int run_${function}(int, char**) { return ${function}(); }\n")
        endif()
    endforeach()

    string(APPEND content "\nstruct Entry\n{\n    const char* name;\n    int (*run)(int, char**);\n};\n\nconst Entry entries[] = {\n")

    foreach(entry IN LISTS ARG_SCENARIOS)
        string(REPLACE "=" ";" parts "${entry}")
        list(GET parts 0 name)
        list(GET parts 1 function)
        string(APPEND content "    { \"${name}\", run_${function} },\n")
    endforeach()

    string(APPEND content "};\n}\n\nint main(int argc, char** argv)\n{\n    if (argc < 2)\n    {\n        std::cerr << \"usage: ${runner} <scenario> [args...]\\n\";\n        return 2;\n    }\n\n    for (const auto& entry : entries)\n        if (std::strcmp(argv[1], entry.name) == 0)\n            return entry.run(argc - 2, argv + 2);\n\n    std::cerr << \"unknown scenario: \" << argv[1] << \"\\n\";\n    return 2;\n}\n")

    file(WRITE "${source}" "${content}")

    add_executable(${runner} "${source}")
    target_sources(${runner} PRIVATE ${ARG_SHARED})
    target_compile_definitions(${runner} PRIVATE ${ARG_DEFINITIONS})
    target_link_libraries(${runner} PRIVATE ${ARG_TARGETS} ${ARG_ENGINE} ${ARG_USAGE})

    if(ARG_WRAP_ALLOC AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${runner} PRIVATE
            "-Wl,--wrap=malloc"
            "-Wl,--wrap=calloc"
            "-Wl,--wrap=realloc"
        )
    endif()

    if(ARG_RT AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${runner} PRIVATE
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
    endif()

    set_target_properties(${runner} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}"
    )
endfunction()
