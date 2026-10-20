# Internal instrumentation fixture consumes an external public-only workload.
# Default SDK/provider/factory/application targets receive no probe.
cmake_minimum_required(VERSION 3.31)
project(nexus_fifo_acquisition_fixture LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT IS_DIRECTORY "${NEXUS_SOURCE}" OR
   NOT IS_DIRECTORY "${FIFO_ACQUISITION_APP_DIR}")
    message(FATAL_ERROR
        "Explicit SDK source and external acquisition app are required")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in target firmware" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL
    "Use actual external acquisition main" FORCE)
set(NEXUS_IO_EVOLUTION_FIXTURE OFF CACHE BOOL
    "Not a retention-only fixture" FORCE)
add_subdirectory("${NEXUS_SOURCE}" nexus EXCLUDE_FROM_ALL)
get_target_property(fifo_soc Nexus::Platform NEXUS_SOC_FAMILY)
get_target_property(fifo_backend Nexus::Platform NEXUS_BACKEND)
get_target_property(fifo_lto nexus_build_options NEXUS_LTO_REQUIRED)
if(NOT fifo_soc MATCHES "^(stm32h743|stm32u575)$" OR
   NOT fifo_backend STREQUAL "baremetal" OR fifo_lto)
    message(FATAL_ERROR
        "Only exact baremetal non-LTO H743/U575 FIFO references")
endif()
if(fifo_soc STREQUAL "stm32h743")
    set(fifo_name h743)
    set(fifo_definition FIFO_FIXTURE_H743=1)
else()
    set(fifo_name u575)
    set(fifo_definition FIFO_FIXTURE_U575=1)
endif()
file(REAL_PATH "${NEXUS_ASSEMBLY_FILE}" fifo_actual_assembly)
set(fifo_references "${NEXUS_SOURCE}/cmake/package/verification/io_evolution")
file(REAL_PATH "${fifo_references}/${fifo_name}-uart-fifo.toml"
    fifo_expected_assembly)
if(NOT fifo_actual_assembly STREQUAL fifo_expected_assembly)
    message(FATAL_ERROR
        "Use the existing exact FIFO assembly; no inferred routes/modes")
endif()
set(fifo_app_sources "${FIFO_ACQUISITION_APP_DIR}/main.c"
    "${FIFO_ACQUISITION_APP_DIR}/workload.c")
nexus_add_firmware(fifo_acquisition_reference SOURCES ${fifo_app_sources})
nexus_target_warnings(fifo_acquisition_reference STRICT)

# Private provider/SDK include scope is confined to this platform-owned object.
add_library(fifo_acquisition_irq OBJECT irq_probe.c)
nexus_apply_target_policy(fifo_acquisition_irq)
target_link_libraries(fifo_acquisition_irq PRIVATE Nexus::IOCycleProbe Nexus::Arch
    nexus_build_options nexus_owned_warnings)
target_compile_definitions(fifo_acquisition_irq PRIVATE ${fifo_definition})
foreach(property NEXUS_SYSTEM_PRIVATE_INCLUDES NEXUS_SYSTEM_VENDOR_INCLUDES
                 NEXUS_SYSTEM_COMPILE_DEFINITIONS)
    get_target_property(${property} Nexus::Platform ${property})
endforeach()
target_include_directories(fifo_acquisition_irq PRIVATE
    "${NEXUS_SOURCE}/soc/${fifo_soc}/private" ${NEXUS_SYSTEM_PRIVATE_INCLUDES})
target_include_directories(fifo_acquisition_irq SYSTEM PRIVATE
    ${NEXUS_SYSTEM_VENDOR_INCLUDES})
target_compile_definitions(fifo_acquisition_irq PRIVATE
    ${NEXUS_SYSTEM_COMPILE_DEFINITIONS})
if(FIFO_ACQUISITION_MARKER_SOURCE)
    if(NOT EXISTS "${FIFO_ACQUISITION_MARKER_SOURCE}")
        message(FATAL_ERROR
            "Explicit reviewed station marker source does not exist")
    endif()
    target_sources(fifo_acquisition_irq PRIVATE
        "${FIFO_ACQUISITION_MARKER_SOURCE}")
    target_compile_definitions(fifo_acquisition_irq PRIVATE
        FIFO_FIXTURE_REVIEWED_MARKERS=1)
endif()
nexus_add_firmware(fifo_acquisition_instrumented SOURCES ${fifo_app_sources}
    LIBRARIES Nexus::IOCycleProbe)
nexus_target_warnings(fifo_acquisition_instrumented STRICT)
target_sources(fifo_acquisition_instrumented PRIVATE
    $<TARGET_OBJECTS:fifo_acquisition_irq>)
target_link_options(fifo_acquisition_instrumented PRIVATE
    "-Wl,--wrap=nx_${fifo_soc}_uart_fifo_irq")
# Building these targets is not the linked vector/call-transfer review. Until
# that actual ARM proof and real station traces execute, qualification is open.
