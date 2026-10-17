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
file(READ "${u5_fifo_bundle}/input_paths.json" u5_fifo_inputs)
string(JSON u5_fifo_count LENGTH "${u5_fifo_inputs}")
math(EXPR u5_fifo_last "${u5_fifo_count} - 1")
foreach(index RANGE 0 ${u5_fifo_last})
    string(JSON input_name MEMBER "${u5_fifo_inputs}" ${index})
    string(JSON input_path GET "${u5_fifo_inputs}" "${input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
endforeach()
nexus_google_test(nexus_u575_uart_rtscts_factory_test
    SOURCES u575_uart_rtscts_factory_test.cpp
        "${u5_fifo_bundle}/bindings.c"
        u575_uart_rtscts_model.cpp u575_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/uart_rtscts.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_fifo_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_u575_uart_rtscts_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_uart_rtscts_factory_test PRIVATE STM32U575xx
    _init=nx_u575_fifo_fixture_init _fini=nx_u575_fifo_fixture_fini)
target_compile_options(nexus_u575_uart_rtscts_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_rtscts_factory_model.h")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_u575_uart_rtscts_factory_test u5)
