# Verify exact EXTI13 source ownership and retained cleanup using production C.
# Author: Nexus Team
set(f1_exti_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_exti_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/exti_test.cpp"
        "${f1_exti_dir}/drivers/gpio/gpio.c"
        "${f1_exti_dir}/drivers/exti/exti.c"
    INCLUDES "${f1_exti_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOEXTI
)
nexus_use_cmsis_core(stm32f103_exti_contracts)
target_include_directories(
    stm32f103_exti_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_exti_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_exti_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/exti_model.h"
)
