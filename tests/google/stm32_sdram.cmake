# Real F479 controller and official LL; external MMIO/CPU/clock leaves only.
nexus_google_test(nexus_stm32_sdram_test STM32F4_LL
    SOURCES stm32_sdram_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/fmc_sdram.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fmc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/sdram_hal"
        "${NEXUS_SOURCE_DIR}/arch/include")
set(sdram_model_config "${CMAKE_CURRENT_BINARY_DIR}/sdram-model-config")
file(MAKE_DIRECTORY "${sdram_model_config}")
file(WRITE "${sdram_model_config}/nexus_config.h" [=[
#ifndef NEXUS_SDRAM_MODEL_CONFIG_H
#define NEXUS_SDRAM_MODEL_CONFIG_H
#define NEXUS_IRQ_KERNEL_POLICY 0U
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 93U
#define NEXUS_IRQ_PRIORITY_BITS 4U
#define NEXUS_CORE_HZ 16000000U
#define NEXUS_CLOCK_SOURCE_HSE 0U
#define NEXUS_CLOCK_PLL_ENABLED 0U
#define NEXUS_CLOCK_INPUT_HZ 16000000U
#define NEXUS_CLOCK_PLLM 16U
#define NEXUS_CLOCK_PLLN 192U
#define NEXUS_CLOCK_PLLP 2U
#define NEXUS_CLOCK_PLLQ 4U
#define NEXUS_CLOCK_SYSCLK_PLL 0U
#define NEXUS_CLOCK_AHB_DIV 1U
#define NEXUS_CLOCK_APB1_DIV 1U
#define NEXUS_CLOCK_APB2_DIV 1U
#define NEXUS_CLOCK_ADC_DIV 4U
#define NEXUS_CLOCK_CSS 0U
#define NEXUS_FLASH_WAIT_STATES 0U
#define NEXUS_APB1_HZ 16000000U
#define NEXUS_APB2_HZ 16000000U
#define NEXUS_APB1_TIMER_HZ 16000000U
#define NEXUS_APB2_TIMER_HZ 16000000U
#define NEXUS_ADC_HZ 4000000U
#define NEXUS_PLL48_HZ 0U
#endif
]=])
target_include_directories(nexus_stm32_sdram_test BEFORE PRIVATE "${sdram_model_config}")
target_include_directories(nexus_stm32_sdram_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_sdram_test)
target_compile_definitions(nexus_stm32_sdram_test PRIVATE
    STM32F479xx NEXUS_STM32_SDRAM_MODEL=1 NX_STM32_SDRAM_GRAPHICS=1
    nx_arch_is_privileged=nx_sdram_privileged
    nx_arch_in_isr=nx_sdram_in_isr
    nx_arch_irq_is_masked=nx_sdram_masked
    nx_arch_dsb=nx_sdram_dsb)
target_compile_options(nexus_stm32_sdram_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_sdram_model.h")
