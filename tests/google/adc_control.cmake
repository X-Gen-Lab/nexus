# Optional integer layout tools do not enlarge existing ADC device state.
nexus_google_test(nexus_adc_format_test SOURCES adc_format_test.cpp
    LIBRARIES Nexus::IO)

# Register models replace external MMIO only; production owns every transition.
nexus_google_test(nexus_g474_pwm_group_test SOURCES g474_pwm_group_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/pwm_group.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32g474/private")
target_include_directories(nexus_g474_pwm_group_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_group_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_pwm_group_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_group_model.h")
