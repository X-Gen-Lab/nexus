# Explicit single-hardware RNG construction; no default source or global pool.
# Author: Nexus Team
include_guard(GLOBAL)

function(nexus_add_stm32f407_entropy target)
    if(NOT ARGC EQUAL 1 OR NOT target OR TARGET "${target}")
        message(FATAL_ERROR
            "F407 entropy requires one unused target name")
    endif()
    if(NOT TARGET Nexus::Platform)
        message(FATAL_ERROR "F407 entropy requires an STM32F407 Platform")
    endif()
    get_target_property(family Nexus::Platform NEXUS_SOC_FAMILY)
    if(NOT family STREQUAL "stm32f4")
        message(FATAL_ERROR "F407 entropy requires an STM32F407 Platform")
    endif()
    get_target_property(device Nexus::Platform NEXUS_STM32_DEVICE)
    if(NOT device OR "${device}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "F4 entropy requires the resolved device identity")
    endif()
    get_property(existing GLOBAL PROPERTY NEXUS_STM32F407_ENTROPY_TARGET)
    if(existing)
        message(FATAL_ERROR "Select only one explicit F407 entropy target")
    endif()
    get_target_property(source Nexus::Platform NEXUS_SOURCE_DIR)
    if(NOT source OR "${source}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR
            "F407 entropy requires the Platform source identity")
    endif()
    # Empty-controller Platforms do not otherwise select any official driver.
    include("${source}/cmake/platform/STM32Official.cmake")
    add_library("${target}" STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/drivers/entropy.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entropy_instance.c")
    nexus_apply_target_policy("${target}")
    target_link_libraries("${target}" PUBLIC Nexus::IO
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories("${target}"
        PUBLIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/private")
    target_include_directories("${target}" SYSTEM PRIVATE
        "${source}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core("${target}")
    nexus_stm32f4_official("${target}")
    target_compile_definitions("${target}" PRIVATE "${device}")
    set_property(GLOBAL PROPERTY NEXUS_STM32F407_ENTROPY_TARGET "${target}")
endfunction()
