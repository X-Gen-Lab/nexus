# Actual owned F103 code; only external register/IRQ behavior is modeled.
foreach(f1_suite system provider watchdog exti spi)
    set(f1_target "nexus_stm32f103_${f1_suite}_test")
    nexus_google_test(${f1_target}
        SOURCES "stm32f103_${f1_suite}_test.cpp"
            "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model/model.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/system.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/uart.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/watchdog.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/exti.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/spi.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model")
    target_include_directories(${f1_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${f1_target})
    target_compile_definitions(${f1_target} PRIVATE STM32F103xB
        __ARM_ARCH_7M__=1 __ARM_FEATURE_LDREX=7)
    target_compile_options(${f1_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model/model.h")
endforeach()
