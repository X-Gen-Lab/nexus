# Pre-runtime C with actual official LL; MMIO/CPU input boundaries only.
set(sram_model "${CMAKE_CURRENT_BINARY_DIR}/f407-sram-reset")
file(MAKE_DIRECTORY "${sram_model}")
file(
    CONFIGURE
    OUTPUT "${sram_model}/nexus_config.h"
    CONTENT "#define NEXUS_MEMORY_CCM 0\n#define NEXUS_MEMORY_ADDSRAM 1\n"
    @ONLY
)
nexus_google_test(nexus_f407_sram_boot_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/f407_sram_boot_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/sram_boot.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/memory.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/clock.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/startup.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fsmc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sdk/hal_config/fsmc"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/components/devices/ili9341/include"
        "${nexus_google_root}/soc"
)
target_include_directories(
    nexus_f407_sram_boot_test
    BEFORE
    PRIVATE "${sram_model}"
)
target_include_directories(
    nexus_f407_sram_boot_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)
nexus_use_cmsis_core(nexus_f407_sram_boot_test)
target_compile_definitions(
    nexus_f407_sram_boot_test
    PRIVATE
        STM32F407xx
        NEXUS_STM32_MODEL
        NEXUS_STM32_MEMORY_MODEL
        NX_SRAM_SYSTEM_TEST
)
target_compile_options(
    nexus_f407_sram_boot_test
    PRIVATE
        -fno-pie
        -ffunction-sections
        -fdata-sections
        "-include${nexus_google_root}/soc/f407_sram_boot_model.h"
)
target_link_options(
    nexus_f407_sram_boot_test
    PRIVATE -no-pie -Wl,--gc-sections -Wl,-Ttext-segment=0x60000000
)
