# Exercise the actual F100 CEC operation without a wire.
# Author: Nexus Team
nexus_google_test(
    stm32f100_cec_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/cec_test.cpp"
        "${f1_dir}/drivers/cec/cec.c"
        "${f1_dir}/drivers/gpio/gpio.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOGPIO
)
nexus_use_cmsis_core(stm32f100_cec_contracts)
target_include_directories(
    stm32f100_cec_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f100_cec_contracts PRIVATE STM32F100xB)
target_compile_options(
    stm32f100_cec_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
