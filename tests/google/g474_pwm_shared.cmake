# Real shared TIM3 provider with two existing public PWM channel faces.
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_google_test(nexus_g474_pwm_shared_test
    SOURCES g474_pwm_shared_test.cpp g474_pwm_shared_model.cpp
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/pwm_shared.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts"
        "${NEXUS_SOURCE_DIR}/soc/stm32g474/private")
nexus_stm32_official(nexus_g474_pwm_shared_test g4)
target_include_directories(nexus_g474_pwm_shared_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_shared_test PRIVATE STM32G474xx
    _init=nx_g474_pwm_shared_fixture_init _fini=nx_g474_pwm_shared_fixture_fini)
target_compile_options(nexus_g474_pwm_shared_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_shared_model.h")
