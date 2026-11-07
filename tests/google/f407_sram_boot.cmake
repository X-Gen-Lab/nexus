# Pre-runtime C with actual official LL; MMIO/CPU input boundaries only.
set(sram_model "${CMAKE_CURRENT_BINARY_DIR}/f407-sram-reset")
file(MAKE_DIRECTORY "${sram_model}")
file(WRITE "${sram_model}/nexus_config.h"
    "#define NEXUS_MEMORY_CCM 0\n#define NEXUS_MEMORY_ADDSRAM 1\n")
nexus_google_test(nexus_f407_sram_boot_test STM32F4_LL
    SOURCES f407_sram_boot_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sram_boot.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/memory_init.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fsmc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/fsmc_hal"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/components/ili9341/include"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_f407_sram_boot_test BEFORE PRIVATE "${sram_model}")
target_include_directories(nexus_f407_sram_boot_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_sram_boot_test)
target_compile_definitions(nexus_f407_sram_boot_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL NEXUS_STM32_MEMORY_MODEL NX_SRAM_SYSTEM_TEST)
target_compile_options(nexus_f407_sram_boot_test PRIVATE -fno-pie
    -ffunction-sections -fdata-sections
    "-include${CMAKE_CURRENT_SOURCE_DIR}/f407_sram_boot_model.h")
target_link_options(nexus_f407_sram_boot_test PRIVATE -no-pie
    -Wl,--gc-sections -Wl,-Ttext-segment=0x60000000)
