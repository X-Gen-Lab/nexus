# Exercise the production CRC and UID paths with the F1 hardware model.
# Author: Nexus Team
nexus_google_test(
    stm32f103_crc_uid_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/crc_uid_test.cpp"
        "${f1_dir}/drivers/crc/crc.c"
        "${f1_dir}/drivers/identity/chip.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOCRC
)
nexus_use_cmsis_core(stm32f103_crc_uid_contracts)
target_include_directories(
    stm32f103_crc_uid_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_crc_uid_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_crc_uid_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
