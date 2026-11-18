# Public construction API. Loading definitions never creates an assembly.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/platform/Firmware.cmake")

function(_nexus_add_common_targets)
    foreach(
        layer
        core
        arch
        io
        os
        components
    )
        add_subdirectory(
            "${NEXUS_SOURCE_DIR}/${layer}"
            "${NEXUS_BINARY_DIR}/${layer}"
            EXCLUDE_FROM_ALL
        )
    endforeach()
endfunction()

function(nexus_add_platform)
    set(assembly_count 0)
    foreach(argument IN LISTS ARGN)
        if(argument STREQUAL "ASSEMBLY")
            math(EXPR assembly_count "${assembly_count} + 1")
        endif()
    endforeach()
    cmake_parse_arguments(PLATFORM "" "ASSEMBLY" "" ${ARGN})
    if(
        NOT assembly_count EQUAL 1
        OR PLATFORM_UNPARSED_ARGUMENTS
        OR PLATFORM_KEYWORDS_MISSING_VALUES
        OR NOT PLATFORM_ASSEMBLY
    )
        message(
            FATAL_ERROR
            "nexus_add_platform requires exactly one ASSEMBLY and known arguments"
        )
    endif()
    if(TARGET Nexus::Config)
        message(
            FATAL_ERROR
            "One build may contain only one resolved Nexus assembly"
        )
    endif()
    get_filename_component(
        NEXUS_SOURCE_DIR
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
        ABSOLUTE
    )
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    # Discovery and construction are separate consumption boundaries.
    include("${NEXUS_SOURCE_DIR}/cmake/platform/SDK.cmake")
    get_filename_component(
        NEXUS_ASSEMBLY_FILE
        "${PLATFORM_ASSEMBLY}"
        ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL NEXUS_SOURCE_DIR)
        set(NEXUS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    else()
        set(NEXUS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/nexus")
    endif()
    if(
        NEXUS_SOURCE_DIR STREQUAL NEXUS_BINARY_DIR
        OR NEXUS_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR
    )
        message(FATAL_ERROR "Nexus requires an out-of-source build")
    endif()
    add_subdirectory(
        "${NEXUS_SOURCE_DIR}/cmake/platform"
        "${NEXUS_BINARY_DIR}/CMakeFiles/platform-graph"
        EXCLUDE_FROM_ALL
    )
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/platform/Runtime.cmake")
