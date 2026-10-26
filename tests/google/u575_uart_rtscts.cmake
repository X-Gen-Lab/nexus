# Verify the selected RTSCTS provider using complete register-level contracts.
nexus_google_test(nexus_u575_uart_rtscts_test
    SOURCES u575_uart_rtscts_test.cpp u575_uart_rtscts_model.cpp u575_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart_fifo.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart_rtscts.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_u575_uart_rtscts_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_uart_rtscts_test)
target_compile_definitions(nexus_u575_uart_rtscts_test PRIVATE STM32U575xx)
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_u575_uart_rtscts_test u5)
target_compile_options(nexus_u575_uart_rtscts_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_uart_rtscts_model.h")
