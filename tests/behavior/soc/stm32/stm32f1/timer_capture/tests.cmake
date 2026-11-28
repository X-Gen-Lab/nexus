# Exercise the F103 TIM4 capture and ES096 collision workaround.
# Author: Nexus Team
nexus_google_test(
    stm32f103_timer_capture
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/capture_test.cpp"
        "${f1_dir}/drivers/gpio/gpio.c"
        "${f1_dir}/drivers/timer_capture/tim4.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOTimerCapture
)
nexus_use_cmsis_core(stm32f103_timer_capture)
target_include_directories(
    stm32f103_timer_capture
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(stm32f103_timer_capture PRIVATE STM32F103xB)
target_compile_options(
    stm32f103_timer_capture
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/capture_model.h"
)
