# Validate F103xB polling comparison and cold PWR ownership.
# Author: Nexus Team
set(f1_pvd_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_pvd_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/pvd_test.cpp"
        "${f1_pvd_dir}/drivers/pvd/pvd.c"
    INCLUDES "${f1_pvd_dir}" "${NEXUS_SOURCE_DIR}/arch/include"
    LIBRARIES Nexus::IOEXTI
)
nexus_use_cmsis_core(stm32f103_pvd_contracts)
target_include_directories(
    stm32f103_pvd_contracts
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(stm32f103_pvd_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_pvd_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/register_model.h"
)
