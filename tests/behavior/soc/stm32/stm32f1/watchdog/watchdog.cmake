# Exercise the F103xB watchdog through the real I/O face and vendor LL.
# Author: Nexus Team
nexus_google_test(
    nexus_stm32f1_watchdog_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/watchdog_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/drivers/watchdog/watchdog.c"
    LIBRARIES Nexus::IOWatchdog
    INCLUDES
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1"
        "${NEXUS_SOURCE_DIR}/arch/include"
)
nexus_use_cmsis_core(nexus_stm32f1_watchdog_test)
target_include_directories(
    nexus_stm32f1_watchdog_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(nexus_stm32f1_watchdog_test PRIVATE STM32F103xB)
target_compile_options(
    nexus_stm32f1_watchdog_test
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
