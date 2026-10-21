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

# Fixed alternatives name one physical owner; they cannot coexist in an image.
set_property(TARGET nexus_stm32u575_stop2 PROPERTY
    INTERFACE_NEXUS_U575_STOP2_MODE wut)
set_property(TARGET nexus_stm32u575_stop2 APPEND PROPERTY
    COMPATIBLE_INTERFACE_STRING NEXUS_U575_STOP2_MODE)
if(TARGET Nexus::U575LpdmaCopy)
    add_library(nexus_stm32u575_stop2_copy STATIC EXCLUDE_FROM_ALL
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/stop2.c")
    nexus_apply_target_policy(nexus_stm32u575_stop2_copy)
    target_include_directories(nexus_stm32u575_stop2_copy PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
        "${NEXUS_SOURCE_DIR}/components/stm32u575_lpdma_copy/private")
    target_include_directories(nexus_stm32u575_stop2_copy SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_link_libraries(nexus_stm32u575_stop2_copy PUBLIC
        Nexus::U575LpdmaCopy Nexus::U575RetainedTime Nexus::ClockDomain
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_compile_definitions(nexus_stm32u575_stop2_copy PRIVATE
        STM32U575xx NEXUS_U575_STOP2_COPY)
    set_property(TARGET nexus_stm32u575_stop2_copy PROPERTY
        INTERFACE_NEXUS_U575_STOP2_MODE copy)
    set_property(TARGET nexus_stm32u575_stop2_copy APPEND PROPERTY
        COMPATIBLE_INTERFACE_STRING NEXUS_U575_STOP2_MODE)
    add_library(Nexus::U575Stop2Copy ALIAS nexus_stm32u575_stop2_copy)
endif()

if(TARGET Nexus::U575LpdmaLptim1)
    add_library(nexus_stm32u575_stop2_lptim1 STATIC EXCLUDE_FROM_ALL
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/stop2.c")
    nexus_apply_target_policy(nexus_stm32u575_stop2_lptim1)
    target_include_directories(nexus_stm32u575_stop2_lptim1 PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
        "${NEXUS_SOURCE_DIR}/components/stm32u575_lpdma_lptim1/private")
    target_include_directories(nexus_stm32u575_stop2_lptim1 SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_link_libraries(nexus_stm32u575_stop2_lptim1 PUBLIC
        Nexus::U575LpdmaLptim1 Nexus::U575RetainedTime Nexus::ClockDomain
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_compile_definitions(nexus_stm32u575_stop2_lptim1 PRIVATE
        STM32U575xx NEXUS_U575_STOP2_LPTIM1)
    set_property(TARGET nexus_stm32u575_stop2_lptim1 PROPERTY
        INTERFACE_NEXUS_U575_STOP2_MODE lptim1)
    set_property(TARGET nexus_stm32u575_stop2_lptim1 APPEND PROPERTY
        COMPATIBLE_INTERFACE_STRING NEXUS_U575_STOP2_MODE)
    add_library(Nexus::U575Stop2Lptim1 ALIAS nexus_stm32u575_stop2_lptim1)
endif()
