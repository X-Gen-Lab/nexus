# Explicit software link fixture using the normal public firmware helper.
if(NOT PROJECT_IS_TOP_LEVEL OR NEXUS_BUILD_WORKLOAD OR NEXUS_BUILD_TESTS)
    message(FATAL_ERROR "I/O fixture needs a top-level build without workloads/tests")
endif()
set(fixture_main
    "${NEXUS_SOURCE_DIR}/tests/contracts/io_evolution_assembly/controller_main.c")
if(NOT NEXUS_IO_EVOLUTION_FIXTURE_MAIN STREQUAL fixture_main)
    message(FATAL_ERROR "I/O fixture requires its explicit maintained main")
endif()
set(fixture_definitions NX_FIXTURE_UART_WORD NX_FIXTURE_SPI_TRANSACTION
    NX_FIXTURE_SPI_DMA_TRANSACTION NX_FIXTURE_I2C_LONG
    NX_FIXTURE_UART_STREAM_PROFILE NX_FIXTURE_ADC_STREAM_PROFILE
    NX_FIXTURE_UART_WORD_IRQ_PROBE)
foreach(definition IN LISTS NEXUS_IO_EVOLUTION_FIXTURE_DEFINITIONS)
    if(NOT definition IN_LIST fixture_definitions)
        message(FATAL_ERROR "Unknown I/O fixture definition: ${definition}")
    endif()
endforeach()
nexus_add_firmware(io_evolution_fixture SOURCES "${fixture_main}")
nexus_target_warnings(io_evolution_fixture STRICT)
target_compile_definitions(io_evolution_fixture PRIVATE
    ${NEXUS_IO_EVOLUTION_FIXTURE_DEFINITIONS})
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
# The maintained ELF checker verifies ARM32 only. Native has separate runtime
# contract tests; its host executable must not be passed off as an ARM proof.
if(NOT NEXUS_SOC_FAMILY STREQUAL "native")
    # Volatile face reads and this check together defeat dead-code GC.
    add_custom_command(TARGET io_evolution_fixture POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/measurement/retention.py"
            --elf "$<TARGET_FILE:io_evolution_fixture>"
            --bindings "${fixture_config}/bindings.c"
            --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
        VERBATIM)
endif()
