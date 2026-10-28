# Real concrete C lifecycle; only hardware writes and CPU context are modeled.
nexus_google_test(nexus_stm32_i2c_cold_test STM32F4_LL
    SOURCES stm32_i2c_cold_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/i2c_async.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/i2c_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v1_async/i2c_v1_async.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_i2c_cold_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_i2c_cold_test)
target_compile_definitions(nexus_stm32_i2c_cold_test PRIVATE
    STM32F407xx)
target_compile_options(nexus_stm32_i2c_cold_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_i2c_cold_model.h")
