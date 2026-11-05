# Optional exact H563 polling port; preparation and board proof are explicit.
# Author: Nexus Team
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32h563")
    return()
endif()
add_library(nexus_stm32h563_i3c_port STATIC EXCLUDE_FROM_ALL
    src/stm32h563_i3c_port.c)
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_stm32h563_i3c_port h5)
nexus_apply_target_policy(nexus_stm32h563_i3c_port)
target_include_directories(nexus_stm32h563_i3c_port PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_stm32h563_i3c_port SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h5/Include")
nexus_use_cmsis_core(nexus_stm32h563_i3c_port)
target_compile_definitions(nexus_stm32h563_i3c_port PRIVATE STM32H563xx)
target_link_libraries(nexus_stm32h563_i3c_port PUBLIC Nexus::I3CController
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::H563I3CPort ALIAS nexus_stm32h563_i3c_port)
