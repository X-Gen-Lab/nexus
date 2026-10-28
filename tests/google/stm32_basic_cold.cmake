# Concrete basic C preparation without replacing the implementation under test.
nexus_google_test(nexus_stm32_basic_cold_test STM32F4_LL
    SOURCES stm32_basic_cold_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/watchdog.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_basic_cold_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_basic_cold_test)
# Inject only the CPU facts exercised here. Other Arch operations retain their
# real Native implementation without defining competing global Arch symbols.
target_compile_definitions(nexus_stm32_basic_cold_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_basic_cold_privileged
    nx_arch_in_isr=nx_basic_cold_in_isr
    nx_arch_irq_is_masked=nx_basic_cold_masked
    nx_arch_dsb=nx_basic_cold_dsb)
