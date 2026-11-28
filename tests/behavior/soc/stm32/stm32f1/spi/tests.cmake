# Exercise the selected STM32F1 SPI byte and transaction owners.
# Author: Nexus Team
set(f1_spi_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_spi_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/spi_test.cpp"
        "${f1_spi_dir}/drivers/gpio/gpio.c"
        "${f1_spi_dir}/drivers/spi/spi.c"
    INCLUDES "${f1_spi_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOSPI
)
nexus_use_cmsis_core(stm32f103_spi_contracts)
target_include_directories(
    stm32f103_spi_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_spi_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_spi_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/spi_model.h"
)
