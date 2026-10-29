# Normal authored assembly -> immutable bundle -> real RTSCTS provider.
set(u5_fifo_input "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/u575-uart-rtscts.toml")
set(u5_fifo_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-uart-rtscts-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u5_fifo_input}"
    --output "${u5_fifo_bundle}" RESULT_VARIABLE u5_fifo_result
    OUTPUT_VARIABLE u5_fifo_output ERROR_VARIABLE u5_fifo_error)
if(NOT u5_fifo_result EQUAL 0)
    message(FATAL_ERROR "FIFO factory fixture rejected: ${u5_fifo_output}${u5_fifo_error}")
endif()
nexus_google_configuration_inputs("${u5_fifo_bundle}")
nexus_google_test(nexus_u575_uart_rtscts_factory_test
    SOURCES u575_uart_rtscts_factory_test.cpp
        "${u5_fifo_bundle}/bindings.c"
        u575_uart_rtscts_model.cpp u575_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart_rtscts.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_fifo_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_u575_uart_rtscts_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_uart_rtscts_factory_test)
target_compile_definitions(nexus_u575_uart_rtscts_factory_test PRIVATE STM32U575xx
    _init=nx_u575_fifo_fixture_init _fini=nx_u575_fifo_fixture_fini)
target_compile_options(nexus_u575_uart_rtscts_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_rtscts_factory_model.h")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_u575_uart_rtscts_factory_test u5)
