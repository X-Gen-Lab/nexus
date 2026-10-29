nexus_google_test(nexus_h743_uart_fifo_test
    SOURCES h743_uart_fifo_test.cpp h743_uart_fifo_model.cpp h743_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_fifo.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/tests/google"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_h743_uart_fifo_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_uart_fifo_test)
target_compile_definitions(nexus_h743_uart_fifo_test PRIVATE STM32H743xx)
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_h743_uart_fifo_test h7)
target_compile_options(nexus_h743_uart_fifo_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/h743_uart_fifo_model.h")
