# Exercise the bounded shared I2C owner with F103 local reset policy.
# Author: Nexus Team
set(f1_irq_engine "${NEXUS_SOURCE_DIR}/soc/stm32/common/i2c_v1_async")
set(f1_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103_i2c_master_irq
    SOURCES
        "${f1_irq_engine}/i2c_v1_async.c"
        "${f1_package}/drivers/i2c/master_irq.c"
        "${f1_package}/drivers/i2c/i2c.c"
        "${f1_package}/drivers/gpio/gpio.c"
        "${CMAKE_CURRENT_LIST_DIR}/master_irq_test.cpp"
    INCLUDES "${f1_irq_engine}" "${f1_package}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOI2C Nexus::IODMA
)
nexus_use_cmsis_core(stm32f103_i2c_master_irq)
target_include_directories(
    stm32f103_i2c_master_irq
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f103_i2c_master_irq
    PRIVATE STM32F103xB NX_I2C_V1_F1_WRITE_ONLY=1
)
target_compile_options(
    stm32f103_i2c_master_irq
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/target_model.h"
)
