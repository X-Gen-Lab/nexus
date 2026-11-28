# Exercise the actual F103 TIM3 one-pulse output and bounded withdrawal.
# Author: Nexus Team
nexus_google_test(
    stm32f103_timer_pulse
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/pulse_test.cpp"
        "${f1_dir}/drivers/gpio/gpio.c"
        "${f1_dir}/drivers/timer_pulse/tim3.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOTimerPulse
)
nexus_use_cmsis_core(stm32f103_timer_pulse)
target_include_directories(
    stm32f103_timer_pulse
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_timer_pulse PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_timer_pulse
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/pulse_model.h"
)
