set(workload_uart3_input
    "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/nucleo-h743-baremetal-uart.toml")
set(workload_uart3_generated "${CMAKE_CURRENT_BINARY_DIR}/workload-uart3")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${workload_uart3_input}"
    --output "${workload_uart3_generated}"
    RESULT_VARIABLE workload_uart3_result
    OUTPUT_VARIABLE workload_uart3_output ERROR_VARIABLE workload_uart3_error)
if(NOT workload_uart3_result EQUAL 0)
    message(FATAL_ERROR "UART3 workload reference rejected: "
        "${workload_uart3_output}${workload_uart3_error}")
endif()
file(READ "${workload_uart3_generated}/input_paths.json" workload_uart3_inputs)
string(JSON workload_uart3_count LENGTH "${workload_uart3_inputs}")
math(EXPR workload_uart3_last "${workload_uart3_count} - 1")
foreach(index RANGE 0 ${workload_uart3_last})
    string(JSON name MEMBER "${workload_uart3_inputs}" ${index})
    string(JSON input GET "${workload_uart3_inputs}" "${name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
endforeach()
nexus_google_test(nexus_workload_uart3_test
    SOURCES "${NEXUS_SOURCE_DIR}/tests/google/workload_uart3_test.cpp"
        "${NEXUS_SOURCE_DIR}/tests/google/workload_uart3_entry.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${workload_uart3_generated}")
