# Exact optional SoC transition. Products explicitly link this public target.
# Author: Nexus Team
if(NEXUS_SOC_FAMILY STREQUAL "stm32f4" AND
        NEXUS_BACKEND STREQUAL "baremetal" AND
        NEXUS_EXACT_PART MATCHES "^STM32F407(VET6|ZGT6)$")
    if(NEXUS_EXACT_PART STREQUAL "STM32F407VET6")
        set(stm32_handoff_flash_bytes 524288)
    else()
        set(stm32_handoff_flash_bytes 1048576)
    endif()
    get_filename_component(stm32_handoff_package
        "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    add_library(nexus_stm32f407_handoff STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/handoff.c")
    nexus_apply_target_policy(nexus_stm32f407_handoff)
    target_include_directories(nexus_stm32f407_handoff PUBLIC
        "${stm32_handoff_package}/include" PRIVATE
        "${stm32_handoff_package}"
        "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/private")
    target_include_directories(nexus_stm32f407_handoff SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(nexus_stm32f407_handoff)
    target_link_libraries(nexus_stm32f407_handoff PUBLIC Nexus::ClockDomain
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_compile_definitions(nexus_stm32f407_handoff PRIVATE
        "${NEXUS_STM32_DEVICE}"
        NX_F407_HANDOFF_FLASH_BYTES=${stm32_handoff_flash_bytes})
    add_library(Nexus::STM32F407Handoff ALIAS nexus_stm32f407_handoff)
endif()
