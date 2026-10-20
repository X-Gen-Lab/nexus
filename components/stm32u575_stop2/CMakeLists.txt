# Exact optional SoC Stop2 owner. External products explicitly link this target.
# Author: Nexus Team
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32u575" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal" OR
        NOT NEXUS_ARCH_SECURITY_STATE EQUAL 0)
    return()
endif()
if(NOT NEXUS_EXACT_PART STREQUAL "STM32U575ZIT6Q")
    message(FATAL_ERROR "U575 Stop2 requires the reviewed exact ZIT6Q part")
endif()
if(NOT TARGET Nexus::U575RetainedTime)
    return()
endif()
add_library(nexus_stm32u575_stop2 STATIC EXCLUDE_FROM_ALL
    "${NEXUS_SOURCE_DIR}/soc/stm32u575/stop2.c")
nexus_apply_target_policy(nexus_stm32u575_stop2)
target_include_directories(nexus_stm32u575_stop2 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32u575/private"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/private")
target_include_directories(nexus_stm32u575_stop2 SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_link_libraries(nexus_stm32u575_stop2 PUBLIC Nexus::U575RetainedTime
    Nexus::ClockDomain PRIVATE Nexus::Arch nexus_build_options
    nexus_owned_warnings)
target_compile_definitions(nexus_stm32u575_stop2 PRIVATE STM32U575xx)
add_library(Nexus::U575Stop2 ALIAS nexus_stm32u575_stop2)
