# Real concrete C lifecycle in both maintained APB clock contexts.
foreach(clock default pll84)
    set(i2c_cold_target nexus_stm32_i2c_cold_test)
    if(clock STREQUAL "pll84")
        set(i2c_cold_target nexus_stm32_i2c_cold_21mhz_test)
    endif()
    nexus_google_test(${i2c_cold_target} STM32F4_LL
        SOURCES stm32_i2c_cold_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_async.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_cold.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"
        LIBRARIES Nexus::IO
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${i2c_cold_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${i2c_cold_target})
    target_compile_definitions(${i2c_cold_target} PRIVATE
        STM32F407xx NX_I2C_V1_PORT_HEADER="stm32f407_i2c_port.h")
    target_compile_options(${i2c_cold_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_i2c_cold_model.h")

    if(clock STREQUAL "pll84")
        target_compile_definitions(${i2c_cold_target} PRIVATE NX_I2C_CLOCK_21)
    endif()
endforeach()
