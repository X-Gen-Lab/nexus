# Exact public receiver; unavailable outside its selected SRAM4 reference.
# Author: Nexus Team
include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/platform/CMSISCore.cmake")
if(NOT TARGET Nexus::U575RetainedTime OR
        NOT NEXUS_SOC_FAMILY STREQUAL "stm32u575" OR
        NOT NEXUS_EXACT_PART STREQUAL "STM32U575ZIT6Q" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal" OR
        NOT NEXUS_ARCH_SECURITY_STATE EQUAL 0 OR
        NOT NEXUS_MEMORY_SRAM4)
    return()
endif()
add_library(nexus_stm32u575_lpdma_copy STATIC EXCLUDE_FROM_ALL
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/lpdma_copy.c")
nexus_apply_target_policy(nexus_stm32u575_lpdma_copy)
target_include_directories(nexus_stm32u575_lpdma_copy PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private")
target_include_directories(nexus_stm32u575_lpdma_copy SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32u5_hal/Inc")
nexus_use_cmsis_core(nexus_stm32u575_lpdma_copy)
target_compile_definitions(nexus_stm32u575_lpdma_copy PRIVATE STM32U575xx)
target_link_libraries(nexus_stm32u575_lpdma_copy PUBLIC Nexus::U575RetainedTime
    PRIVATE Nexus::IO Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::U575LpdmaCopy ALIAS nexus_stm32u575_lpdma_copy)
set_property(TARGET nexus_stm32u575_lpdma_copy PROPERTY
    INTERFACE_NEXUS_U575_LPDMA_MODE copy)
set_property(TARGET nexus_stm32u575_lpdma_copy APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING NEXUS_U575_LPDMA_MODE)
