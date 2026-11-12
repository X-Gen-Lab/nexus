# Real F407 EXTI source with external register/NVIC write acknowledgements.
nexus_google_test(nexus_stm32_exti_test SOURCES stm32_exti_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/exti.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_exti_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_exti_test)
target_compile_definitions(nexus_stm32_exti_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL=1)
target_compile_options(nexus_stm32_exti_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_exti_model.h")

# EMR event mode has no IRQ producer; exercise its actual C owner separately.
nexus_google_test(nexus_stm32_exti_hardware_test STM32F4_LL
    SOURCES stm32_exti_hardware_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/exti_hardware.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_exti_hardware_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_exti_hardware_test)
target_compile_definitions(nexus_stm32_exti_hardware_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_exti_hardware_privileged
    nx_arch_in_isr=nx_exti_hardware_in_isr
    nx_arch_irq_is_masked=nx_exti_hardware_masked)
target_compile_options(nexus_stm32_exti_hardware_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_exti_hardware_model.h")
