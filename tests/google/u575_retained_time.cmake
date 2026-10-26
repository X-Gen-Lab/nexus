# Actual C11 retained source, existing external RTC model and shared reader.
nexus_google_test(nexus_u575_retained_time_test
    SOURCES u575_retained_time_test.cpp u575_rtc_model.cpp
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/include"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_retained_time_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_retained_time_test)
target_compile_definitions(nexus_u575_retained_time_test PRIVATE STM32U575xx)
set_source_files_properties(
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c"
    PROPERTIES COMPILE_OPTIONS "-include${NEXUS_SOURCE_DIR}/tests/google/u575_rtc_model.h")
