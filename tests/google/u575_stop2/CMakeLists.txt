# Both production C11 owners, real ClockDomain, external MMIO/instructions only.
# Author: Nexus Team
set(u575_stop2_includes
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_stop2/include"
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/include"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/private"
    "${NEXUS_SOURCE_DIR}/io/private"
    "${NEXUS_SOURCE_DIR}/tests/google")
foreach(kind unit combined)
    set(name "nexus_u575_stop2_${kind}_test")
    set(sources "${NEXUS_SOURCE_DIR}/tests/google/u575_stop2_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/stop2.c")
    if(kind STREQUAL "combined")
        list(APPEND sources
            "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c")
    endif()
    nexus_google_test(${name} SOURCES ${sources}
        LIBRARIES Nexus::ClockDomain Nexus::IO Nexus::Arch
        INCLUDES ${u575_stop2_includes})
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(${name} PRIVATE STM32U575xx NX_U575_STOP2_MODEL)
    if(kind STREQUAL "combined")
        target_compile_definitions(${name} PRIVATE NX_U575_STOP2_COMBINED)
    endif()
endforeach()
set_source_files_properties(
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c"
    PROPERTIES COMPILE_OPTIONS
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_stop2_clock_bridge.h")
