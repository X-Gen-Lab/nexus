# Explicit single-line software reference; Board wiring is not inferred.
include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/platform/CMSISCore.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/platform/STM32Official.cmake")
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32h743")
    return()
endif()
set(w25q128_hal "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal")
add_library(nexus_h743_w25q128_hal STATIC EXCLUDE_FROM_ALL
    "${w25q128_hal}/Src/stm32h7xx_hal_qspi.c"
    "${w25q128_hal}/Src/stm32h7xx_hal_mdma.c")
nexus_apply_target_policy(nexus_h743_w25q128_hal)
nexus_stm32_official(nexus_h743_w25q128_hal h7)
target_include_directories(nexus_h743_w25q128_hal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_w25q128_hal SYSTEM PRIVATE
    "${w25q128_hal}/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include")
nexus_use_cmsis_core(nexus_h743_w25q128_hal)
target_compile_definitions(nexus_h743_w25q128_hal PRIVATE STM32H743xx
    HAL_GetTick=nx_h743_w25q128_hal_get_tick)
target_compile_options(nexus_h743_w25q128_hal PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/private/hal_platform.h")
target_link_libraries(nexus_h743_w25q128_hal PRIVATE
    Nexus::Config Nexus::Core Nexus::Arch Nexus::Platform nexus_build_options)

add_library(nexus_h743_w25q128 STATIC EXCLUDE_FROM_ALL
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/w25q128_mapped.c")
nexus_apply_target_policy(nexus_h743_w25q128)
nexus_stm32_official(nexus_h743_w25q128 h7)
target_include_directories(nexus_h743_w25q128 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_w25q128 SYSTEM PRIVATE
    "${w25q128_hal}/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include")
nexus_use_cmsis_core(nexus_h743_w25q128)
target_compile_definitions(nexus_h743_w25q128 PRIVATE STM32H743xx)
target_link_libraries(nexus_h743_w25q128 PUBLIC Nexus::MappedStorage
    PRIVATE nexus_h743_w25q128_hal Nexus::Arch
        nexus_build_options nexus_owned_warnings)
add_library(Nexus::H743W25Q128MappedStorage ALIAS nexus_h743_w25q128)
