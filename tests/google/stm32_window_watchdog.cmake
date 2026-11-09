# One maintained whole program owns the new exact WWDG contract.
nexus_google_test(nexus_stm32_window_watchdog_test STM32F4_LL
    SOURCES stm32_window_watchdog_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/window_watchdog.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_window_watchdog_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_window_watchdog_test)
target_compile_definitions(nexus_stm32_window_watchdog_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_window_watchdog_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_window_watchdog_model.h")
