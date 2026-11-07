# Real F407 PE2 IRQ observations retain coalescing through the NEC reference.
# Author: Nexus Team
nexus_google_test(nexus_infrared_f407_exti_test
    SOURCES infrared_exti_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/exti.c"
    LIBRARIES Nexus::Infrared Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
target_include_directories(nexus_infrared_f407_exti_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_infrared_f407_exti_test)
target_compile_definitions(nexus_infrared_f407_exti_test PRIVATE
    STM32F407xx NEXUS_STM32_MODEL=1)
target_compile_options(nexus_infrared_f407_exti_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_exti_model.h")
