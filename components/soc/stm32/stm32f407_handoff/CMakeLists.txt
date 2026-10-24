# Exact optional SoC transition. Products explicitly link this public target.
# Author: Nexus Team
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32f407" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal")
    return()
endif()
if(NEXUS_EXACT_PART STREQUAL "STM32F407VET6")
    set(handoff_flash_bytes 524288)
elseif(NEXUS_EXACT_PART STREQUAL "STM32F407ZGT6")
    set(handoff_flash_bytes 1048576)
else()
    message(FATAL_ERROR "F407 handoff requires a reviewed exact VE/ZG part")
endif()
add_library(nexus_stm32f407_handoff STATIC EXCLUDE_FROM_ALL
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/handoff.c")
nexus_apply_target_policy(nexus_stm32f407_handoff)
target_include_directories(nexus_stm32f407_handoff PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/private")
target_include_directories(nexus_stm32f407_handoff SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_link_libraries(nexus_stm32f407_handoff PUBLIC Nexus::ClockDomain
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
target_compile_definitions(nexus_stm32f407_handoff PRIVATE STM32F407xx
    NX_F407_HANDOFF_FLASH_BYTES=${handoff_flash_bytes})
add_library(Nexus::STM32F407Handoff ALIAS nexus_stm32f407_handoff)
