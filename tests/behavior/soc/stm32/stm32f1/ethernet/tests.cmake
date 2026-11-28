# Exercise the private F107 management transaction and cold receipts.
# Author: Nexus Team
nexus_google_test(
    stm32f107_mdio
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/mdio_test.cpp"
        "${f1_dir}/drivers/ethernet/mdio.c"
        "${f1_dir}/drivers/gpio/gpio.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOMDIO
)
nexus_use_cmsis_core(stm32f107_mdio)
target_include_directories(
    stm32f107_mdio
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f107_mdio
    PRIVATE STM32F107xC NEXUS_CLOCK_SOURCE_HSE=1 NEXUS_CLOCK_HSE_BYPASS=0
)
target_compile_options(
    stm32f107_mdio
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/mdio_model.h"
)
