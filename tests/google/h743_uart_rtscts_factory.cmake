# Normal authored assembly -> immutable bundle -> real RTSCTS provider.
set(h7_fifo_input "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/h743-uart-rtscts.toml")
set(h7_fifo_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-uart-rtscts-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${h7_fifo_input}"
    --output "${h7_fifo_bundle}" RESULT_VARIABLE h7_fifo_result
    OUTPUT_VARIABLE h7_fifo_output ERROR_VARIABLE h7_fifo_error)
if(NOT h7_fifo_result EQUAL 0)
    message(FATAL_ERROR "FIFO factory fixture rejected: ${h7_fifo_output}${h7_fifo_error}")
endif()
nexus_google_configuration_inputs("${h7_fifo_bundle}")
nexus_google_test(nexus_h743_uart_rtscts_factory_test
    SOURCES h743_uart_rtscts_factory_test.cpp
        "${h7_fifo_bundle}/bindings.c"
        h743_uart_rtscts_model.cpp h743_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_rtscts.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_fifo_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/tests/google"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_h743_uart_rtscts_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_uart_rtscts_factory_test)
target_compile_definitions(nexus_h743_uart_rtscts_factory_test PRIVATE STM32H743xx
    _init=nx_h743_fifo_fixture_init _fini=nx_h743_fifo_fixture_fini)
target_compile_options(nexus_h743_uart_rtscts_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_uart_rtscts_factory_model.h")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_h743_uart_rtscts_factory_test h7)
