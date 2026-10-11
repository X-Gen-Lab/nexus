# Retain the production kernel and its authored optional hook contracts.
cmake_minimum_required(VERSION 3.31)
project(nexus_os_kernel_profile_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_RUNTIME_ASSEMBLY)
    message(FATAL_ERROR "NEXUS_RUNTIME_ASSEMBLY must name one TOML assembly")
endif()
get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${nexus_source}/cmake/platform/Runtime.cmake")
nexus_add_runtime(ASSEMBLY "${NEXUS_RUNTIME_ASSEMBLY}")
add_executable(nexus_os_kernel_profile main.c)
# Runtime keeps resolved CMake facts in its construction scope. The consumer
# explicitly selects this optional component; disabled C paths retain no ring.
target_link_libraries(nexus_os_kernel_profile PRIVATE Nexus::Runtime
    Nexus::OSDiagnostic)
if(NEXUS_TEST_EXTERNAL_TICK_PROVIDER)
    target_compile_definitions(nexus_os_kernel_profile PRIVATE
        NEXUS_TEST_EXTERNAL_TICK_PROVIDER=1)
endif()
if(NEXUS_TEST_RUNTIME_COUNTER_PROVIDER)
    target_compile_definitions(nexus_os_kernel_profile PRIVATE
        NEXUS_TEST_RUNTIME_COUNTER_PROVIDER=1)
endif()
set(runtime_linker "${CMAKE_CURRENT_LIST_DIR}/../cortex_runtime/linker.ld")
target_link_options(nexus_os_kernel_profile PRIVATE "-T${runtime_linker}"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/nexus_os_kernel_profile.map")
set_target_properties(nexus_os_kernel_profile PROPERTIES SUFFIX ".elf")
set_property(TARGET nexus_os_kernel_profile APPEND PROPERTY LINK_DEPENDS
    "${runtime_linker}")
