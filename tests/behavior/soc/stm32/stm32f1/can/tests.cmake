# Test real F103xB CAN1 through an exact CMSIS register model.
# Author: Nexus Team
set(f1_can_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_can_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/can_test.cpp"
        "${f1_can_dir}/drivers/can/can.c"
    INCLUDES "${f1_can_dir}" "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/arch/include"
    LIBRARIES Nexus::IOCAN Nexus::Core
)
nexus_use_cmsis_core(stm32f103_can_contracts)
target_include_directories(
    stm32f103_can_contracts
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(stm32f103_can_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_can_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/can_model.h"
)

nexus_google_test(
    stm32f103_can_normal_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/can_normal_test.cpp"
        "${f1_can_dir}/drivers/can/can.c"
        "${f1_can_dir}/drivers/can/normal.c"
        "${f1_can_dir}/drivers/gpio/gpio.c"
    INCLUDES "${f1_can_dir}" "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/arch/include"
    LIBRARIES Nexus::IOCAN Nexus::IOGPIO Nexus::Core
)
nexus_use_cmsis_core(stm32f103_can_normal_contracts)
target_include_directories(
    stm32f103_can_normal_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f103_can_normal_contracts
    PRIVATE STM32F103xB NEXUS_CLOCK_APB1_HZ=36000000U NX_STM32F1_CAN_NORMAL=1
)
target_compile_options(
    stm32f103_can_normal_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/can_normal_model.h"
)
