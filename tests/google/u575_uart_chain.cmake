# Normal TOML generation; external ports preserve the real private node array.
set(u5_chain_input "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/u575-uart-chain.toml")
set(u5_chain_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-uart-chain-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u5_chain_input}"
    --output "${u5_chain_bundle}" RESULT_VARIABLE u5_chain_generated
    OUTPUT_VARIABLE u5_chain_output ERROR_VARIABLE u5_chain_error)
if(NOT u5_chain_generated EQUAL 0)
    message(FATAL_ERROR "U575 chain fixture rejected: ${u5_chain_output}${u5_chain_error}")
endif()
nexus_google_configuration_inputs("${u5_chain_bundle}")
nexus_google_test(nexus_u575_uart_chain_test SOURCES
    u575_uart_chain_test.cpp u575_uart_chain_model.cpp
    u575_uart_gpdma_model.cpp u575_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart_dma_chain.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${CMAKE_CURRENT_SOURCE_DIR}")
nexus_google_test(nexus_u575_uart_chain_factory_test SOURCES
    u575_uart_chain_factory_test.cpp u575_uart_chain_model.cpp
    u575_uart_gpdma_model.cpp u575_model.cpp
    "${u5_chain_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/platform_cold.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart_dma_chain.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_chain_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${CMAKE_CURRENT_SOURCE_DIR}")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
foreach(target nexus_u575_uart_chain_test nexus_u575_uart_chain_factory_test)
    target_include_directories(${target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE STM32U575xx)
    nexus_stm32_official(${target} u5)
endforeach()
target_compile_options(nexus_u575_uart_chain_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_chain_model.h")
target_compile_options(nexus_u575_uart_chain_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_chain_factory_model.h")
target_compile_definitions(nexus_u575_uart_chain_factory_test PRIVATE
    _init=nx_u575_chain_fixture_init _fini=nx_u575_chain_fixture_fini)
