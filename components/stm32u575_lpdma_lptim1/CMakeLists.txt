# Fixed optional SRAM4/LPTIM1 receiver, distinct public API and physical role.
# Author: Nexus Team
if(NOT TARGET Nexus::U575RetainedTime OR
        NOT NEXUS_SOC_FAMILY STREQUAL "stm32u575" OR
        NOT NEXUS_EXACT_PART STREQUAL "STM32U575ZIT6Q" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal" OR
        NOT NEXUS_ARCH_SECURITY_STATE EQUAL 0 OR
        NOT NEXUS_MEMORY_SRAM4)
    return()
endif()
add_library(nexus_stm32u575_lpdma_lptim1 STATIC EXCLUDE_FROM_ALL
    "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/lpdma_copy.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/lpdma_lptim1.c")
nexus_apply_target_policy(nexus_stm32u575_lpdma_lptim1)
target_include_directories(nexus_stm32u575_lpdma_lptim1 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${NEXUS_SOURCE_DIR}/soc/stm32u575/private")
target_include_directories(nexus_stm32u575_lpdma_lptim1 SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32u5_hal/Inc")
target_compile_definitions(nexus_stm32u575_lpdma_lptim1 PRIVATE
    STM32U575xx NEXUS_U575_STOP2_LPTIM1)
target_link_libraries(nexus_stm32u575_lpdma_lptim1 PUBLIC
    Nexus::U575RetainedTime PRIVATE Nexus::IO Nexus::Arch
    nexus_build_options nexus_owned_warnings)
set_property(TARGET nexus_stm32u575_lpdma_lptim1 PROPERTY
    INTERFACE_NEXUS_U575_LPDMA_MODE lptim1)
set_property(TARGET nexus_stm32u575_lpdma_lptim1 APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING NEXUS_U575_LPDMA_MODE)
add_library(Nexus::U575LpdmaLptim1 ALIAS nexus_stm32u575_lpdma_lptim1)
