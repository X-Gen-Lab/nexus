# Explicit uart behavior and private model dependencies.
# Author: Nexus Team
include(
    "${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_cold_factory.cmake"
)

include(
    "${nexus_google_root}/soc/stm32/stm32f4/uart/qiming_rs485_factory.cmake"
)

nexus_google_test(nexus_stm32_uart_dma_test STM32F4_LL SOURCES "${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_dma_test.cpp"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

target_include_directories(
    nexus_stm32_uart_dma_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_stm32_uart_dma_test)

target_compile_definitions(
    nexus_stm32_uart_dma_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx
)

target_sources(
    nexus_stm32_uart_dma_test
    PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_hardware.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_dma.c"
)

target_compile_options(
    nexus_stm32_uart_dma_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_dma_model.h"
)

nexus_google_test(nexus_stm32_uart_receive_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_receive_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_hardware.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_receive.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_duplex.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

target_include_directories(
    nexus_stm32_uart_receive_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_stm32_uart_receive_test)

target_compile_definitions(
    nexus_stm32_uart_receive_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx
)

target_compile_options(
    nexus_stm32_uart_receive_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_receive_model.h"
)

nexus_google_test(nexus_stm32_uart_stream_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_stream_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

target_include_directories(
    nexus_stm32_uart_stream_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_stm32_uart_stream_test)

target_compile_definitions(
    nexus_stm32_uart_stream_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx
)

target_sources(
    nexus_stm32_uart_stream_test
    PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart_stream.c"
)

include("${nexus_google_root}/soc/stm32/stm32f4/uart/stm32_uart_hal.cmake")
