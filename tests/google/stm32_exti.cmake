# Real F407 EXTI source with external register/NVIC write acknowledgements.
nexus_google_test(nexus_stm32_exti_test SOURCES stm32_exti_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/exti.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_exti_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_exti_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL=1)
target_compile_options(nexus_stm32_exti_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_exti_model.h")
