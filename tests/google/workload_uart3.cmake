# Production workload tests compile against actual generated factory identities.
include("${NEXUS_SOURCE_DIR}/cmake/platform/Workload.cmake")
function(nexus_uart_workload_test target assembly)
    set(input "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/${assembly}")
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/${target}-generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${input}"
        --output "${generated}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "UART workload reference rejected: ${output}${error}")
    endif()
    nexus_google_configuration_inputs("${generated}")
    nexus_workload_identity("${generated}" UART "" identity)
    nexus_google_test(${target}
        SOURCES "${NEXUS_SOURCE_DIR}/tests/google/workload_uart3_test.cpp"
            "${NEXUS_SOURCE_DIR}/tests/google/workload_uart3_entry.c"
        LIBRARIES Nexus::IO Nexus::Arch INCLUDES "${generated}")
    target_compile_definitions(${target} PRIVATE
        "NEXUS_WORKLOAD_UART_ID=${identity}")
endfunction()
nexus_uart_workload_test(nexus_workload_uart3_test
    nucleo-h743-baremetal-uart.toml)
nexus_uart_workload_test(nexus_workload_gd303_test
    gd32-f303-baremetal-io.toml)
