# Exercise the private F103 receiver and real source/borrow withdrawal.
# Author: Nexus Team
set(f1_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_i2c_target
    SOURCES
        "${f1_package}/drivers/i2c/i2c.c"
        "${f1_package}/drivers/i2c/target.c"
        "${f1_package}/drivers/gpio/gpio.c"
        "${CMAKE_CURRENT_LIST_DIR}/target_test.cpp"
    INCLUDES "${f1_package}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOI2C Nexus::IOI2CTarget
)
nexus_use_cmsis_core(stm32f103_i2c_target)
target_include_directories(
    stm32f103_i2c_target
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_i2c_target PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_i2c_target
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/target_model.h"
)
