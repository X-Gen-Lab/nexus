# One selected chip role; products explicitly link the software implementation.
# Author: Nexus Team
include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/platform/CMSISCore.cmake")
if(NOT NEXUS_F407_I2S2_SLAVE_TX_SELECTED)
    return()
endif()
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32f407" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal" OR
        NOT NEXUS_F407_I2S2_PROFILE STREQUAL
            "slave-tx-stereo16-pb12-pb13-pb15")
    message(FATAL_ERROR "I2S2 requires the resolved finite F407 hardware role")
endif()
if(NEXUS_EXACT_PART STREQUAL "STM32F407VET6")
    set(i2s2_flash_bytes 524288)
elseif(NEXUS_EXACT_PART STREQUAL "STM32F407ZGT6")
    set(i2s2_flash_bytes 1048576)
else()
    message(FATAL_ERROR "I2S2 requires the reviewed exact F407 VE/ZG part")
endif()
add_library(nexus_f407_i2s2_slave_tx STATIC EXCLUDE_FROM_ALL
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/i2s2_slave_tx.c")
nexus_apply_target_policy(nexus_f407_i2s2_slave_tx)
target_include_directories(nexus_f407_i2s2_slave_tx PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
target_include_directories(nexus_f407_i2s2_slave_tx SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_i2s2_slave_tx)
target_link_libraries(nexus_f407_i2s2_slave_tx PUBLIC Nexus::IO Nexus::Core
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
target_compile_definitions(nexus_f407_i2s2_slave_tx PRIVATE STM32F407xx
    NEXUS_F407_I2S2_SLAVE_TX_SELECTED=1u
    NX_F407_I2S_FLASH_BYTES=${i2s2_flash_bytes})
add_library(Nexus::F407I2S2SlaveTx ALIAS nexus_f407_i2s2_slave_tx)
