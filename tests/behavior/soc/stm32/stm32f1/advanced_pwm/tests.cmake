nexus_google_test(
    nexus_stm32f1_advanced_pwm_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/advanced_pwm_test.cpp"
        "${f1_dir}/drivers/advanced_pwm/advanced_pwm.c"
        "${f1_dir}/drivers/gpio/gpio.c"
    INCLUDES
        "${f1_dir}"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::IOGPIO Nexus::IOAdvancedPWM
)
nexus_use_cmsis_core(nexus_stm32f1_advanced_pwm_test)
target_include_directories(
    nexus_stm32f1_advanced_pwm_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(nexus_stm32f1_advanced_pwm_test PRIVATE STM32F103xB)
target_compile_options(
    nexus_stm32f1_advanced_pwm_test
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
