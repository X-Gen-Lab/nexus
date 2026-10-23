# Three real C11 owners/Core; one independent DMA bus model plus RTC instructions.
# Author: Nexus Team
set(u575_copy_includes
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_stop2/include"
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_lpdma_copy/include"
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_lpdma_copy/private"
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/include"
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/private"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
    "${NEXUS_SOURCE_DIR}/io/private"
    "${NEXUS_SOURCE_DIR}/tests/google/u575_lpdma_copy")
set(u575_copy_sdk
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32u5_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
add_library(nexus_u575_stop2_copy_bus OBJECT
    "${NEXUS_SOURCE_DIR}/tests/google/u575_lpdma_copy/model.cpp")
nexus_apply_target_policy(nexus_u575_stop2_copy_bus)
target_link_libraries(nexus_u575_stop2_copy_bus PRIVATE Nexus::IO Nexus::Arch
    nexus_owned_warnings nexus_build_options)
target_include_directories(nexus_u575_stop2_copy_bus PRIVATE ${u575_copy_includes})
target_include_directories(nexus_u575_stop2_copy_bus SYSTEM PRIVATE ${u575_copy_sdk})
target_compile_definitions(nexus_u575_stop2_copy_bus PRIVATE STM32U575xx
    NX_U575_LPDMA_COPY_MODEL
    nx_u575_lpdma_copy_read=nx_u575_lpdma_copy_bus_read
    nx_u575_lpdma_copy_write=nx_u575_lpdma_copy_bus_write)
nexus_google_test(nexus_u575_stop2_copy_test
    SOURCES test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/stop2.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/lpdma_copy.c"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c"
        $<TARGET_OBJECTS:nexus_u575_stop2_copy_bus>
    LIBRARIES Nexus::ClockDomain Nexus::IO Nexus::Arch
    INCLUDES ${u575_copy_includes})
target_include_directories(nexus_u575_stop2_copy_test SYSTEM PRIVATE ${u575_copy_sdk})
target_compile_definitions(nexus_u575_stop2_copy_test PRIVATE STM32U575xx
    NX_U575_LPDMA_COPY_MODEL NX_U575_STOP2_MODEL NEXUS_U575_STOP2_COPY)
set_source_files_properties(
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/src/stm32u575_retained_time.c"
    PROPERTIES COMPILE_OPTIONS "-include${CMAKE_CURRENT_SOURCE_DIR}/clock_port.h")
