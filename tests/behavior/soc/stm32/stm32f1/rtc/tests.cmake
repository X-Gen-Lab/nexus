# Exercise the exact F103 RTC production path and public backup dispatch.
# Author: Nexus Team
set(f1_rtc_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_rtc_backup_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/rtc_test.cpp"
        "${f1_rtc_dir}/drivers/rtc/counter.c"
    INCLUDES
        "${f1_rtc_dir}"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::IORTC
)
nexus_use_cmsis_core(stm32f103_rtc_backup_contracts)
target_include_directories(
    stm32f103_rtc_backup_contracts
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(
    stm32f103_rtc_backup_contracts
    PRIVATE STM32F103xB NX_STM32F1_RTC_RAW_ALARM NX_STM32F1_RTC_PROVISION
)
target_compile_options(
    stm32f103_rtc_backup_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/register_model.h"
)
