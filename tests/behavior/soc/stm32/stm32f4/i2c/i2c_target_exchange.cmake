# Independent wire contracts use real C owners and external hardware leaves.
nexus_google_test(nexus_stm32_i2c_target_exchange_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_target_exchange_test.cpp" "${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_target_exchange_model.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c/i2c_target.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio/gpio.c"
        "${NEXUS_SOURCE_DIR}/io/src/api/i2c_target.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/io/include"
        "${NEXUS_SOURCE_DIR}/arch/include"
)
target_include_directories(
    nexus_stm32_i2c_target_exchange_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)
nexus_use_cmsis_core(nexus_stm32_i2c_target_exchange_test)
target_compile_definitions(
    nexus_stm32_i2c_target_exchange_test
    PRIVATE
        STM32F407xx
        NEXUS_CMSIS_HOST_MODEL=1
        nx_arch_is_privileged=nx_target_privileged
        nx_arch_in_isr=nx_target_in_isr
        nx_arch_irq_is_masked=nx_target_masked
        nx_arch_dsb=nx_target_dsb
)
target_compile_options(
    nexus_stm32_i2c_target_exchange_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_target_model.h"
)
