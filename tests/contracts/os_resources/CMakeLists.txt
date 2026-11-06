# Measure the production Runtime graph; this consumer defines no source graph.
cmake_minimum_required(VERSION 3.31)
project(nexus_os_resource_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_RUNTIME_ASSEMBLY)
    message(FATAL_ERROR "NEXUS_RUNTIME_ASSEMBLY must select one authored TOML")
endif()
get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${nexus_source}/cmake/platform/Runtime.cmake")
nexus_add_runtime(ASSEMBLY "${NEXUS_RUNTIME_ASSEMBLY}")
add_executable(nexus_os_resources main.c)
target_link_libraries(nexus_os_resources PRIVATE Nexus::Runtime)
target_link_options(nexus_os_resources PRIVATE
    "-T${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/nexus_os_resources.map")
set_target_properties(nexus_os_resources PROPERTIES SUFFIX ".elf")
set_property(TARGET nexus_os_resources APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/linker.ld")

option(NEXUS_CORE_ARCH_COSTS "Build opt-in zero/one/two/four Core probes" OFF)
if(NEXUS_CORE_ARCH_COSTS)
    foreach(instances 0 1 2 4)
        set(probe "nexus_core_arch_cost_${instances}")
        add_executable("${probe}" core_arch_probe.c)
        target_compile_definitions("${probe}" PRIVATE
            "NX_COST_INSTANCES=${instances}")
        target_link_libraries("${probe}" PRIVATE Nexus::Runtime
            Nexus::OSWait Nexus::OSDiagnostic)
        target_link_options("${probe}" PRIVATE
            "-T${CMAKE_CURRENT_LIST_DIR}/linker.ld"
            "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${probe}.map")
        set_target_properties("${probe}" PROPERTIES SUFFIX ".elf")
        set_property(TARGET "${probe}" APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_LIST_DIR}/linker.ld")
    endforeach()
endif()
