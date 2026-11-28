# Run the selected F1 I2C and GPIO implementations against a host register bus.
# Author: Nexus Team
set(f1_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    nexus_stm32f1_i2c_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/i2c_test.cpp"
        "${f1_package}/drivers/i2c/i2c.c"
        "${f1_package}/drivers/gpio/gpio.c"
    LIBRARIES Nexus::IOI2C Nexus::IOGPIO
    INCLUDES
        "${f1_package}"
        "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
)
nexus_use_cmsis_core(nexus_stm32f1_i2c_test)
target_include_directories(
    nexus_stm32f1_i2c_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(nexus_stm32f1_i2c_test PRIVATE STM32F103xB)
target_compile_options(
    nexus_stm32f1_i2c_test
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
