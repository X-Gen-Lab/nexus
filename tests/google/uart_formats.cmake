# UART nine-bit support is independently linked; default byte state is intact.
nexus_google_test(nexus_uart_formats_test SOURCES uart_formats_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")

nexus_google_test(nexus_stm32_uart_formats_test STM32F4_LL
    SOURCES uart_formats_provider_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_word.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_uart_formats_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_uart_formats_test)
target_compile_definitions(nexus_stm32_uart_formats_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx NX_UART_FORMAT_STM32)
target_compile_options(nexus_stm32_uart_formats_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/uart_format_model.h")
target_compile_options(nexus_stm32_uart_formats_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_official_model.h>")
