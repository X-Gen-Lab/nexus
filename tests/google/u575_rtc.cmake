# Real production C11 RTC with external register and clock boundaries only.
nexus_google_test(nexus_u575_rtc_test
    SOURCES u575_rtc_test.cpp u575_rtc_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/rtc.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_rtc_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32u5_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_rtc_test PRIVATE STM32U575xx)
set_source_files_properties(
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/rtc.c" PROPERTIES
    COMPILE_OPTIONS "-include${NEXUS_SOURCE_DIR}/tests/google/u575_rtc_model.h")
