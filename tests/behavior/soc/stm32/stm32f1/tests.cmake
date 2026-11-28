# Run production F1 GPIO and USART against one side-effectful MMIO model.
# Author: Nexus Team
set(nexus_behavior_domain "soc;stm32f1")
set(f1_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_driver_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/drivers_test.cpp"
        "${f1_dir}/drivers/gpio/gpio.c"
        "${f1_dir}/drivers/uart/uart.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOGPIO Nexus::IOUART
)
include("${NEXUS_SOURCE_DIR}/cmake/platform/CMSISCore.cmake")
nexus_use_cmsis_core(stm32f103_driver_contracts)
target_include_directories(
    stm32f103_driver_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_driver_contracts PRIVATE STM32F107xC)
target_compile_options(
    stm32f103_driver_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/drivers_model.h"
)

include("${CMAKE_CURRENT_LIST_DIR}/system/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/flash/flash.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/watchdog/watchdog.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_dma.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/crc/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/i2c/i2c.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/i2c/target.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/i2c/master_irq.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/spi/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/adc/adc.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/exti/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pwm/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/advanced_pwm/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pvd/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/window_watchdog/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/wwdg_early_warning/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/can/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ethernet/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/rtc/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_rx_dma/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dac/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/spi_dma/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/fsmc/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/timer_capture/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/timer_pulse/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/i2s2/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/usb/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/sdio/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/encoder/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_word/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_half_duplex/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_lin/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_flow/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_irda/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_sync/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_smartcard/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cec/tests.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_ports/tests.cmake")
