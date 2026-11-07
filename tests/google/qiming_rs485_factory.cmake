# Public factory callbacks compose a real UART and DE GPIO, without a new HAL.
set(qiming_rs485_bundle "${CMAKE_CURRENT_BINARY_DIR}/qiming-rs485-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/boards/stm32f407zg_qiming_v41/assemblies/rs485-transport.toml"
    --output "${qiming_rs485_bundle}"
    RESULT_VARIABLE qiming_rs485_generated
    OUTPUT_VARIABLE qiming_rs485_output ERROR_VARIABLE qiming_rs485_error)
if(NOT qiming_rs485_generated EQUAL 0)
    message(FATAL_ERROR "RS485 factory rejected: ${qiming_rs485_output}${qiming_rs485_error}")
endif()
nexus_google_configuration_inputs("${qiming_rs485_bundle}")
nexus_google_test(nexus_qiming_rs485_factory_test STM32F4_LL
    SOURCES qiming_rs485_factory_test.cpp "${qiming_rs485_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
    LIBRARIES Nexus::IO
    INCLUDES "${qiming_rs485_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
target_include_directories(nexus_qiming_rs485_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_qiming_rs485_factory_test PRIVATE STM32F407xx
    _init=nx_qiming_rs485_fixture_init _fini=nx_qiming_rs485_fixture_fini)
target_compile_options(nexus_qiming_rs485_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/qiming_rs485_factory_model.h")
