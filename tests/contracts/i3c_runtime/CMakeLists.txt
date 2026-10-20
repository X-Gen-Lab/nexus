# Public CPU-only compile/link reference; never Board or physical I3C support.
cmake_minimum_required(VERSION 3.31)
project(nexus_i3c_runtime_contract LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT)
    get_filename_component(NEXUS_SOURCE_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
endif()
include("${NEXUS_SOURCE_ROOT}/cmake/platform/Runtime.cmake")
nexus_add_runtime(ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/runtime.toml")
foreach(forbidden Nexus::Platform Nexus::Factory Nexus::Board Nexus::NativeModel)
    if(TARGET ${forbidden})
        message(FATAL_ERROR "Unexpected platform target ${forbidden}")
    endif()
endforeach()
add_executable(nexus_i3c_runtime main.c)
target_link_libraries(nexus_i3c_runtime PRIVATE
    Nexus::Runtime Nexus::I3CController)
target_link_options(nexus_i3c_runtime PRIVATE
    "-T${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/nexus_i3c_runtime.map")
set_target_properties(nexus_i3c_runtime PROPERTIES SUFFIX ".elf")
set_property(TARGET nexus_i3c_runtime APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld")

# Explicit external official-header paths affect this private object only.
# The object is compiled but never linked as a physical provider implementation.
if(NEXUS_H563_REFERENCE_ROOT OR NEXUS_CMSIS_CORE_INCLUDE)
    if(NOT NEXUS_H563_REFERENCE_ROOT OR NOT NEXUS_CMSIS_CORE_INCLUDE)
        message(FATAL_ERROR "H563 reference requires both official source paths")
    endif()
    foreach(required
            "${NEXUS_H563_REFERENCE_ROOT}/cmsis-closure/Include/stm32h5xx.h"
            "${NEXUS_H563_REFERENCE_ROOT}/cmsis-closure/Include/system_stm32h5xx.h"
            "${NEXUS_H563_REFERENCE_ROOT}/h563-facts/stm32h563xx.h"
            "${NEXUS_H563_REFERENCE_ROOT}/st-hal/Inc/stm32h5xx_ll_i3c.h"
            "${NEXUS_CMSIS_CORE_INCLUDE}/core_cm33.h")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR "Missing official H563 header: ${required}")
        endif()
    endforeach()
    add_library(nexus_h563_i3c_compile_reference OBJECT h563_reference.c)
    target_compile_definitions(nexus_h563_i3c_compile_reference PRIVATE
        STM32H563xx)
    target_include_directories(nexus_h563_i3c_compile_reference SYSTEM PRIVATE
        "${NEXUS_H563_REFERENCE_ROOT}/cmsis-closure/Include"
        "${NEXUS_H563_REFERENCE_ROOT}/st-hal/Inc"
        "${NEXUS_H563_REFERENCE_ROOT}/h563-facts"
        "${NEXUS_CMSIS_CORE_INCLUDE}")
    target_link_libraries(nexus_h563_i3c_compile_reference PRIVATE Nexus::Runtime
        nexus_owned_warnings)
    add_dependencies(nexus_i3c_runtime nexus_h563_i3c_compile_reference)
endif()
