# Compile the actual F100 provider against modeled register and time receipts.
# Author: Nexus Team
set(f1_dac "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(nexus_stm32f100_dac_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/dac_test.cpp"
        "${f1_dac}/drivers/dac/dac.c"
        "${f1_dac}/drivers/gpio/gpio.c"
    LIBRARIES Nexus::IODAC Nexus::IOGPIO Nexus::Arch Nexus::Core
    INCLUDES "${f1_dac}" "${NEXUS_SOURCE_DIR}/io/private"
)
nexus_use_cmsis_core(nexus_stm32f100_dac_contracts)
target_include_directories(
    nexus_stm32f100_dac_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(nexus_stm32f100_dac_contracts PRIVATE STM32F100xB)
target_compile_options(
    nexus_stm32f100_dac_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
