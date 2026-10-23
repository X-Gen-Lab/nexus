nexus_google_test(nexus_g474_pwm_adc_test
    SOURCES g474_pwm_adc_test.cpp g474_pwm_adc_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_group.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_adc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/private")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_g474_pwm_adc_test g4)
target_include_directories(nexus_g474_pwm_adc_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_adc_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_pwm_adc_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_adc_model.h")
