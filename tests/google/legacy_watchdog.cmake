# Real production watchdogs with external CPU/MMIO ports. Official STM32 LL
# inline operations and the GD combined-helper reference execute unchanged.
nexus_google_test(nexus_legacy_stm32_watchdog_test STM32F4_LL
    SOURCES legacy_watchdog_stm32_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/watchdog.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_legacy_stm32_watchdog_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_legacy_stm32_watchdog_test)
target_compile_definitions(nexus_legacy_stm32_watchdog_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_legacy_stm32_watchdog_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/legacy_watchdog_stm32_model.h")
