# Real Core engine and F4 drivers; only external CPU/MMIO acceptance is modeled.
nexus_google_test(nexus_stm32_static_lifecycle_test STM32F4_LL
    SOURCES stm32_static_lifecycle_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/lifecycle_pins.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/lifecycle_gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/lifecycle_uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/lifecycle_spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_static_lifecycle_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_static_lifecycle_test PRIVATE STM32F407xx)
target_compile_options(nexus_stm32_static_lifecycle_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_static_lifecycle_model.h")
