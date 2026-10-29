# Test real C resource ownership; only CPU/MMIO ports are external models.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(gd_platform_root "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470")
    set(gd_platform_sdk
        "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
    nexus_google_test(nexus_gd32_platform_cold_test
        SOURCES gd32_platform_cold_test.cpp
            "${gd_platform_root}/drivers/platform_cold.c"
            "${gd_platform_root}/drivers/gpio.c"
            "${gd_platform_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
        LIBRARIES Nexus::IO
        INCLUDES "${gd_platform_root}" "${gd_platform_root}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(nexus_gd32_platform_cold_test SYSTEM PRIVATE
        "${gd_platform_sdk}/CMSIS/GD/GD32F4xx/Include"
        "${gd_platform_sdk}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(nexus_gd32_platform_cold_test PRIVATE
        GD32F470 HXTAL_VALUE=25000000
        nx_arch_is_privileged=nx_gd32_platform_privileged
        nx_arch_in_isr=nx_gd32_platform_in_isr
        nx_arch_irq_is_masked=nx_gd32_platform_masked
        nx_arch_dsb=nx_gd32_platform_dsb
        nx_arch_isb=nx_gd32_platform_isb)
    target_compile_options(nexus_gd32_platform_cold_test PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_cold_pins_model.h")
    target_link_options(nexus_gd32_platform_cold_test PRIVATE -Wl,--gc-sections)
endif()
