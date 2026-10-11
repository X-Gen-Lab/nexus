# Software CPU runtime linkage; production targets own the source graph.
cmake_minimum_required(VERSION 3.31)
project(nexus_cortex_runtime_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_RUNTIME_ASSEMBLY)
    message(FATAL_ERROR "NEXUS_RUNTIME_ASSEMBLY must name an authored TOML")
endif()
get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${nexus_source}/cmake/platform/Runtime.cmake")
nexus_add_runtime(ASSEMBLY "${NEXUS_RUNTIME_ASSEMBLY}")
add_executable(nexus_cortex_runtime main.c)
target_link_libraries(nexus_cortex_runtime PRIVATE Nexus::Runtime)
target_link_options(nexus_cortex_runtime PRIVATE
    "-T${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/nexus_cortex_runtime.map")
set_target_properties(nexus_cortex_runtime PROPERTIES SUFFIX ".elf")
set_property(TARGET nexus_cortex_runtime APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld")
