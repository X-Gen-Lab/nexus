# Explicit independent retained clock; default Core TIM2 is unchanged.
# Author: Nexus Team
include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/platform/CMSISCore.cmake")
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32u575" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal" OR
        NOT NEXUS_EXACT_PART STREQUAL "STM32U575ZIT6Q" OR
        NOT NEXUS_ARCH_SECURITY_STATE EQUAL 0)
    return()
endif()
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
nexus_controller_modes(u575_retained_time_rtc_modes rtc)
if(u575_retained_time_rtc_modes)
    # A selected polling AlarmA provider already owns the physical calendar.
    return()
endif()
add_library(nexus_stm32u575_retained_time STATIC EXCLUDE_FROM_ALL
    src/stm32u575_retained_time.c)
nexus_apply_target_policy(nexus_stm32u575_retained_time)
target_include_directories(nexus_stm32u575_retained_time PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
    "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_stm32u575_retained_time SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include")
nexus_use_cmsis_core(nexus_stm32u575_retained_time)
target_compile_definitions(nexus_stm32u575_retained_time PRIVATE STM32U575xx)
target_link_libraries(nexus_stm32u575_retained_time PUBLIC Nexus::Core
    PRIVATE Nexus::IO Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::U575RetainedTime ALIAS nexus_stm32u575_retained_time)
