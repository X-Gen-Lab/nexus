# Real provider isolation, including the normal STM32 NVIC path. Only external
# CPU/MMIO behavior is modeled; firmware and SDK consumers never enter here.
foreach(i2c_multi_family stm32f4)
    set(i2c_multi_target "nexus_${i2c_multi_family}_i2c_multi_controller_test")
    nexus_google_test(${i2c_multi_target} SOURCES i2c_multi_controller_test.cpp
        i2c_instruction_barrier_model.c
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"
        "${NEXUS_SOC_SOURCE_${i2c_multi_family}}/drivers/i2c_async.c"
        "${NEXUS_SOC_SOURCE_${i2c_multi_family}}/drivers/i2c_cold.c"
        "${NEXUS_SOC_SOURCE_${i2c_multi_family}}/drivers/gpio.c"
        LIBRARIES Nexus::IO
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async"
            "${NEXUS_SOC_SOURCE_${i2c_multi_family}}"
            "${NEXUS_SOC_SOURCE_${i2c_multi_family}}/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_compile_options(${i2c_multi_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_multi_controller_model.h")
    target_sources(${i2c_multi_target} PRIVATE
        stm32_i2c_storage_model.c)
    include("${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/cmake/STM32Official.cmake")
    nexus_stm32f4_official(${i2c_multi_target})
    target_include_directories(${i2c_multi_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${i2c_multi_target} PRIVATE
        STM32F407xx NX_I2C_MULTI_STM32)
endforeach()
