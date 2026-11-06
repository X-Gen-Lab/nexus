# Link-only H563 transport consumer; no physical startup/clock/Board support.
# Author: Nexus Team
cmake_minimum_required(VERSION 3.31)
project(h563_i3c_link_reference LANGUAGES C ASM)
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
set(NEXUS_SOURCE_DIR "${NEXUS_SOURCE_ROOT}")
# Runtime CPU facts select the ABI. This local selector only opens the exact
# optional private port leaf; it never establishes a supported SoC or Board.
set(NEXUS_SOC_FAMILY stm32h563)
if(NOT TARGET Nexus::H563I3CPort)
    add_subdirectory(
        "${NEXUS_SOURCE_ROOT}/components/soc/stm32/stm32h563_i3c" h563-port)
endif()
add_library(reference_composition STATIC composition.c)
target_include_directories(reference_composition PRIVATE
    "${NEXUS_SOURCE_ROOT}/components/soc/stm32/stm32h563_i3c/private")
target_include_directories(reference_composition SYSTEM PRIVATE
    "${NEXUS_SOURCE_ROOT}/vendors/st/cmsis_device_h5/Include")
nexus_stm32_official(reference_composition h5)
nexus_use_cmsis_core(reference_composition)
target_compile_definitions(reference_composition PRIVATE STM32H563xx)
target_link_libraries(reference_composition PRIVATE Nexus::Runtime
    Nexus::H563I3CPort nexus_owned_warnings)
add_executable(reference main.c)
target_link_libraries(reference PRIVATE Nexus::Runtime Nexus::I3CController
    reference_composition
    nexus_owned_warnings
    # Runtime's transitive --gc-sections follows direct target link options.
    # This final link item deliberately retains the complete measured bodies.
    "-Wl,--no-gc-sections")
target_link_options(reference PRIVATE
    "-T${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/reference.map"
    -Wl,--no-gc-sections -Wl,--fatal-warnings)
set_target_properties(reference PROPERTIES SUFFIX ".elf")
set_property(TARGET reference APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld")
