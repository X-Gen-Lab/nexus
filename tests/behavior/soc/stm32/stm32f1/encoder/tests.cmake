# Verify TIM3 quadrature ownership and raw counter behavior on the production C path.
# Author: Nexus Team
set(f1_encoder_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
function(nexus_f103_encoder_contract target)
    nexus_google_test(
        ${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/encoder_test.cpp"
            "${f1_encoder_dir}/drivers/gpio/gpio.c"
            "${f1_encoder_dir}/drivers/encoder/encoder.c"
        INCLUDES
            "${f1_encoder_dir}"
            "${f1_encoder_dir}/drivers/encoder"
        LIBRARIES Nexus::Arch Nexus::Core Nexus::IOEncoder Nexus::IOGPIO
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(${target} PRIVATE STM32F103xB)
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
    )
    if(ARGN)
        target_compile_definitions(
            ${target}
            PRIVATE NX_TEST_F103_ENCODER_X2=1 "NX_STM32F1_ENCODER_MODE=${ARGN}"
        )
    endif()
endfunction()

nexus_f103_encoder_contract(stm32f103_encoder_contracts)
nexus_f103_encoder_contract(stm32f103_encoder_x2_a NX_ENCODER_MODE_X2_A)
nexus_f103_encoder_contract(stm32f103_encoder_x2_b NX_ENCODER_MODE_X2_B)
