# Exercise the private F103 receiver and real source/borrow withdrawal.
# Author: Nexus Team
function(nexus_f1_i2c_target_contract target)
    set(f1_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
    nexus_google_test(
        ${target}
        SOURCES
            "${f1_package}/drivers/i2c/i2c.c"
            "${f1_package}/drivers/i2c/target.c"
            "${f1_package}/drivers/gpio/gpio.c"
            "${CMAKE_CURRENT_LIST_DIR}/target_test.cpp"
        INCLUDES "${f1_package}" "${NEXUS_SOURCE_DIR}/io/private"
        LIBRARIES Nexus::Arch Nexus::Core Nexus::IOI2C Nexus::IOI2CTarget
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(${target} PRIVATE STM32F103xB ${ARGN})
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/target_model.h"
    )
endfunction()

nexus_f1_i2c_target_contract(stm32f103_i2c_target)
nexus_f1_i2c_target_contract(stm32f103_i2c_target_tx NX_STM32F1_I2C_TARGET_TX=1)
nexus_f1_i2c_target_contract(
    stm32f103_i2c_target_write_read
    NX_STM32F1_I2C_TARGET_WRITE_READ=1
)
