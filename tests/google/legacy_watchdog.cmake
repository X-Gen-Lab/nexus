# Real production watchdogs with external CPU/MMIO ports. Official STM32 LL
# inline operations and the GD combined-helper reference execute unchanged.
nexus_google_test(nexus_legacy_stm32_watchdog_test STM32F4_LL
    SOURCES legacy_watchdog_stm32_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/watchdog.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_legacy_stm32_watchdog_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_legacy_stm32_watchdog_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_legacy_stm32_watchdog_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/legacy_watchdog_stm32_model.h")

nexus_google_test(nexus_legacy_gd32_watchdog_test
    SOURCES legacy_watchdog_gd32_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/watchdog.c"
        "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/GD32F4xx_standard_peripheral/Source/gd32f4xx_fwdgt.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_legacy_gd32_watchdog_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/GD32F4xx/Include"
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_legacy_gd32_watchdog_test PRIVATE GD32F470)
target_compile_options(nexus_legacy_gd32_watchdog_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/legacy_watchdog_gd32_model.h")
