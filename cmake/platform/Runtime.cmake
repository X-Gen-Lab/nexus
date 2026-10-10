# Reuse the production targets without inventing a SoC, Board or device factory.
include_guard(GLOBAL)

function(nexus_add_runtime)
    set(runtime_assembly_count 0)
    foreach(argument IN LISTS ARGN)
        if(argument STREQUAL "ASSEMBLY")
            math(EXPR runtime_assembly_count "${runtime_assembly_count} + 1")
        endif()
    endforeach()
    if(NOT runtime_assembly_count EQUAL 1)
        message(FATAL_ERROR "nexus_add_runtime requires exactly one ASSEMBLY")
    endif()
    cmake_parse_arguments(RUNTIME "" "ASSEMBLY" "" ${ARGN})
    if(RUNTIME_UNPARSED_ARGUMENTS OR RUNTIME_KEYWORDS_MISSING_VALUES OR
            NOT RUNTIME_ASSEMBLY)
        message(FATAL_ERROR "nexus_add_runtime requires ASSEMBLY and known arguments")
    endif()
    if(TARGET Nexus::Config)
        message(FATAL_ERROR "One build may contain only one resolved Nexus assembly")
    endif()
    foreach(override NEXUS_CPU_ARCH NEXUS_CPU_FPU NEXUS_FLOAT_ABI
            NEXUS_ENUM_ABI NEXUS_CPU_COMPILE_OPTIONS NEXUS_BACKEND
            NEXUS_FREERTOS_PORT NEXUS_IRQ_PRIORITY_BITS NEXUS_OPTIMIZATION)
        if(DEFINED CACHE{${override}})
            message(FATAL_ERROR "${override} is not a runtime override; use ASSEMBLY")
        endif()
    endforeach()
    get_filename_component(NEXUS_SOURCE_DIR
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(NEXUS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/nexus-runtime")
    if(NEXUS_SOURCE_DIR STREQUAL NEXUS_BINARY_DIR OR
            NEXUS_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
        message(FATAL_ERROR "Nexus runtime requires an out-of-source build")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    # An SDK identity triggers complete verification before generation/build.
    include("${NEXUS_SOURCE_DIR}/cmake/platform/SDK.cmake")
    get_filename_component(runtime_assembly "${RUNTIME_ASSEMBLY}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(NEXUS_CONFIG_DIR "${NEXUS_BINARY_DIR}/generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/runtime.py"
        --assembly "${runtime_assembly}" --output "${NEXUS_CONFIG_DIR}"
        RESULT_VARIABLE runtime_result OUTPUT_VARIABLE runtime_output
        ERROR_VARIABLE runtime_error)
    if(NOT runtime_result EQUAL 0)
        message(FATAL_ERROR "CPU runtime rejected:\n${runtime_output}${runtime_error}")
    endif()
    include("${NEXUS_CONFIG_DIR}/selection.cmake")
    file(READ "${NEXUS_CONFIG_DIR}/input_paths.json" runtime_inputs)
    foreach(input assembly cpu_facts)
        string(JSON runtime_input GET "${runtime_inputs}" "${input}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${runtime_input}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${NEXUS_SOURCE_DIR}/tools/configure/runtime.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/cpu.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/ir.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/common.py")
    include("${NEXUS_SOURCE_DIR}/cmake/platform/Options.cmake")
    foreach(layer core arch io os components)
        add_subdirectory("${NEXUS_SOURCE_DIR}/${layer}"
            "${NEXUS_BINARY_DIR}/${layer}" EXCLUDE_FROM_ALL)
    endforeach()
    add_library(nexus_runtime INTERFACE)
    target_link_libraries(nexus_runtime INTERFACE Nexus::Config Nexus::Core
        Nexus::Arch Nexus::IO)
    if(NEXUS_BACKEND STREQUAL "freertos")
        target_link_libraries(nexus_runtime INTERFACE Nexus::OSFreeRTOS)
    else()
        target_link_libraries(nexus_runtime INTERFACE Nexus::OSBaremetal)
    endif()
    set_target_properties(nexus_runtime PROPERTIES
        NEXUS_SOURCE_DIR "${NEXUS_SOURCE_DIR}"
        NEXUS_CONFIG_DIR "${NEXUS_CONFIG_DIR}"
        NEXUS_CPU_ARCH "${NEXUS_CPU_ARCH}"
        NEXUS_BACKEND "${NEXUS_BACKEND}"
        NEXUS_CONFIG_SHA256 "${NEXUS_CONFIG_SHA256}")
    add_library(Nexus::Runtime ALIAS nexus_runtime)
    message(STATUS "Nexus CPU runtime: ${NEXUS_CPU_ARCH}/${NEXUS_BACKEND}")
endfunction()
