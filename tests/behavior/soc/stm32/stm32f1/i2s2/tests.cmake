# Exercise the selected STM32F1 I2S2 finite audio DMA owner.
# Author: Nexus Team
set(f1_i2s_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_i2s2_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/i2s2_test.cpp"
        "${f1_i2s_dir}/drivers/gpio/gpio.c"
        "${f1_i2s_dir}/drivers/audio/i2s2.c"
    INCLUDES "${f1_i2s_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOAudio Nexus::IOGPIO Nexus::IODMA
)
nexus_use_cmsis_core(stm32f103_i2s2_contracts)
target_include_directories(
    stm32f103_i2s2_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_i2s2_contracts PRIVATE STM32F103xE)
target_compile_options(
    stm32f103_i2s2_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
