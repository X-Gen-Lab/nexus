# Real private FSMC owner and official LL; only MMIO/CPU boundaries are modeled.
nexus_google_test(nexus_f407_fsmc_test STM32F4_LL
    SOURCES f407_fsmc_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/fsmc.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fsmc.c"
    LIBRARIES Nexus::IO Nexus::ILI9341
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/fsmc_hal"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_f407_fsmc_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_fsmc_test)
target_compile_definitions(nexus_f407_fsmc_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_fsmc_privileged
    nx_arch_in_isr=nx_fsmc_in_isr
    nx_arch_irq_is_masked=nx_fsmc_masked
    nx_arch_dsb=nx_fsmc_dsb)
target_compile_options(nexus_f407_fsmc_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/f407_fsmc_model.h")

# Real reset proof and runtime adoption share the same modeled external ports.
nexus_google_test(nexus_f407_fsmc_boot_test STM32F4_LL
    SOURCES f407_fsmc_boot_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/fsmc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/fsmc_boot.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sram_boot.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/memory_init.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fsmc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/fsmc_hal"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_f407_fsmc_boot_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_fsmc_boot_test)
target_compile_definitions(nexus_f407_fsmc_boot_test PRIVATE STM32F407xx
    NEXUS_STM32_MEMORY_MODEL=1
    nx_arch_is_privileged=nx_fsmc_privileged
    nx_arch_in_isr=nx_fsmc_in_isr
    nx_arch_irq_is_masked=nx_fsmc_masked
    nx_arch_dsb=nx_fsmc_dsb)
target_compile_options(nexus_f407_fsmc_boot_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/f407_fsmc_boot_model.h")
