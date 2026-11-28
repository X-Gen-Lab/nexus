# Verify TIM3 quadrature ownership and raw counter behavior on the production C path.
# Author: Nexus Team
set(f1_encoder_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_encoder_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/encoder_test.cpp"
        "${f1_encoder_dir}/drivers/gpio/gpio.c"
        "${f1_encoder_dir}/drivers/encoder/encoder.c"
    INCLUDES
        "${f1_encoder_dir}"
        "${f1_encoder_dir}/drivers/encoder"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOEncoder Nexus::IOGPIO
)
nexus_use_cmsis_core(stm32f103_encoder_contracts)
target_include_directories(
    stm32f103_encoder_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_encoder_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_encoder_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
