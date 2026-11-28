# Run the real F1 ADC and pad C implementations against a register bus.
# Author: Nexus Team
set(f1_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
function(nexus_f1_adc_poll_model target)
    nexus_google_test(
        ${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/adc_test.cpp"
            "${f1_package}/drivers/adc/adc.c"
            "${f1_package}/drivers/adc/internal.c"
            "${f1_package}/drivers/adc/trigger_dma.c"
            "${f1_package}/drivers/gpio/gpio.c"
        LIBRARIES Nexus::IOADC Nexus::IOGPIO Nexus::IODMA
        INCLUDES
            "${f1_package}"
            "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
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
endfunction()

nexus_f1_adc_poll_model(nexus_stm32f1_adc_test)
nexus_f1_adc_poll_model(nexus_stm32f1_adc_scan_test)
target_compile_definitions(
    nexus_stm32f1_adc_scan_test
    PRIVATE NX_STM32F1_ADC_SCAN_DMA=1
)

nexus_google_test(
    nexus_stm32f1_adc_dual_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/dual_dma_test.cpp"
        "${f1_package}/drivers/adc/dual_dma.c"
        "${f1_package}/drivers/gpio/gpio.c"
    LIBRARIES Nexus::IOADC Nexus::IOGPIO Nexus::IODMA
    INCLUDES "${f1_package}" "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/arch/include"
)
nexus_use_cmsis_core(nexus_stm32f1_adc_dual_test)
target_include_directories(
    nexus_stm32f1_adc_dual_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(nexus_stm32f1_adc_dual_test PRIVATE STM32F103xE)
target_compile_options(
    nexus_stm32f1_adc_dual_test
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/dual_model.h"
)

foreach(converter IN ITEMS 2 3)
    set(target "nexus_stm32f1_adc${converter}_test")
    nexus_google_test(
        ${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/adc2_test.cpp"
            "${f1_package}/drivers/adc/adc.c"
            "${f1_package}/drivers/gpio/gpio.c"
        LIBRARIES Nexus::IOADC Nexus::IOGPIO
        INCLUDES "${f1_package}" "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/arch/include"
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(${target} PRIVATE STM32F103xE)
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
    )
    if(converter EQUAL 3)
        target_compile_definitions(${target} PRIVATE NX_STM32F1_ADC3_POLL=1)
    endif()
endforeach()
