# Run the selected production C implementation against the Flash bus model.
nexus_google_test(nexus_stm32f1_flash_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/flash_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/drivers/flash/flash.c"
    LIBRARIES Nexus::IOFlash Nexus::Core
    INCLUDES
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1"
        "${NEXUS_SOURCE_DIR}/arch/include"
)
nexus_use_cmsis_core(nexus_stm32f1_flash_test)
target_include_directories(
    nexus_stm32f1_flash_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(nexus_stm32f1_flash_test PRIVATE STM32F103xB)
target_compile_options(
    nexus_stm32f1_flash_test
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/flash_model.h"
)
