# One private, reviewed CMSIS Core header dependency for MCU implementation.
include_guard(GLOBAL)

function(nexus_cmsis_core_include output)
    if(NOT ARGC EQUAL 1)
        message(FATAL_ERROR "CMSIS Core include requires one output variable")
    endif()
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(core "${source}/vendors/arm/CMSIS_6/CMSIS/Core/Include")
    set(${output} "${core}" PARENT_SCOPE)
endfunction()

function(nexus_use_cmsis_core target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "CMSIS Core target requires one existing target")
    endif()
    nexus_cmsis_core_include(core)
    if(NOT EXISTS "${core}/cmsis_version.h" OR
            NOT EXISTS "${core}/core_cm4.h")
        message(FATAL_ERROR "Missing reviewed CMSIS 6 Core headers: ${core}")
    endif()
    if(NOT TARGET nexus_cmsis_core)
        add_library(nexus_cmsis_core INTERFACE)
        target_include_directories(nexus_cmsis_core SYSTEM INTERFACE "${core}")
        target_compile_definitions(nexus_cmsis_core INTERFACE
            CMSIS_DISABLE_DEPRECATED=1)
    endif()
    get_target_property(kind "${target}" TYPE)
    if(kind STREQUAL "INTERFACE_LIBRARY")
        target_link_libraries(${target} INTERFACE nexus_cmsis_core)
    else()
        target_link_libraries(${target} PRIVATE nexus_cmsis_core)
    endif()
endfunction()

# Existing source lists consume this owned path, never a cache selector.
nexus_cmsis_core_include(NEXUS_CMSIS_CORE_INCLUDE_DIR)
