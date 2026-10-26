# Exact finite USART1 GPDMA fixture; only CPU/MMIO leaves are modeled.
set(u5_gpdma_factory_input "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/u575-uart-gpdma.toml")
set(u5_gpdma_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-uart-gpdma-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32u575/__init__.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32u575/bindings.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32u575/emission.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u5_gpdma_factory_input}"
    --output "${u5_gpdma_factory_bundle}" RESULT_VARIABLE u5_generated
    OUTPUT_VARIABLE u5_output ERROR_VARIABLE u5_error)
if(NOT u5_generated EQUAL 0)
    message(FATAL_ERROR "U575 factory fixture rejected: ${u5_output}${u5_error}")
endif()
file(READ "${u5_gpdma_factory_bundle}/input_paths.json" u5_inputs)
string(JSON u5_input_count LENGTH "${u5_inputs}")
math(EXPR u5_last_input "${u5_input_count} - 1")
foreach(index RANGE 0 ${u5_last_input})
    string(JSON u5_input_name MEMBER "${u5_inputs}" ${index})
    string(JSON u5_input_path GET "${u5_inputs}" "${u5_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${u5_input_path}")
endforeach()
nexus_google_test(nexus_u575_uart_gpdma_factory_test SOURCES u575_uart_gpdma_factory_test.cpp
    "${u5_gpdma_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/google/u575_model.cpp"
    "${NEXUS_SOURCE_DIR}/tests/google/u575_uart_gpdma_model.cpp"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_gpdma_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_uart_gpdma_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_uart_gpdma_factory_test)
target_compile_definitions(nexus_u575_uart_gpdma_factory_test PRIVATE STM32U575xx
    _init=nx_u575_gpdma_fixture_init _fini=nx_u575_gpdma_fixture_fini)
target_compile_options(nexus_u575_uart_gpdma_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_gpdma_factory_model.h")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_u575_uart_gpdma_factory_test u5)
