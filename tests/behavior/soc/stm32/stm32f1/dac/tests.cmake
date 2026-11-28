# Compile each selected DAC channel against modeled registers and time.
# Author: Nexus Team
function(nexus_stm32f1_dac_model test_name device channel)
    set(f1_dac "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
    nexus_google_test(${test_name}
        SOURCES
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dac_test.cpp"
            "${f1_dac}/drivers/dac/dac.c"
            "${f1_dac}/drivers/gpio/gpio.c"
        LIBRARIES Nexus::IODAC Nexus::IOGPIO Nexus::Arch Nexus::Core
        INCLUDES "${f1_dac}" "${NEXUS_SOURCE_DIR}/io/private"
    )
    nexus_use_cmsis_core(${test_name})
    target_include_directories(
        ${test_name}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(
        ${test_name}
        PRIVATE ${device} NX_STM32F1_DAC_CHANNEL=${channel}
    )
    target_compile_options(
        ${test_name}
        PRIVATE "-include${CMAKE_CURRENT_FUNCTION_LIST_DIR}/model.h"
    )
endfunction()

nexus_stm32f1_dac_model(nexus_stm32f100_dac_contracts STM32F100xB 1)
nexus_stm32f1_dac_model(nexus_stm32f103_dac2_contracts STM32F103xE 2)
