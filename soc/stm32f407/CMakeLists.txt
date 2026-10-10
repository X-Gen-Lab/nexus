# Fixed providers use official pinned device/CMSIS headers privately. Drivers
# operate reviewed controller registers, without importing the legacy HAL SDK.
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
set(stm32_cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
foreach(required "${stm32_device}/Include/stm32f407xx.h"
                 "${stm32_cmsis}/core_cm4.h"
                 "${stm32_device}/Source/Templates/gcc/startup_stm32f407xx.s")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing pinned official STM32 SDK input: ${required}")
    endif()
endforeach()
add_library(nexus_soc_stm32f407 STATIC
    drivers/gpio.c drivers/uart.c drivers/uart_dma.c drivers/uart_stream.c
    drivers/spi.c drivers/spi_dma.c drivers/i2c.c drivers/flash.c
    drivers/watchdog.c drivers/exti.c drivers/timer.c drivers/adc.c
    drivers/adc_stream.c)
target_link_libraries(nexus_soc_stm32f407 PUBLIC nexus_io PRIVATE Nexus::Arch
    nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_soc_stm32f407 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_soc_stm32f407 SYSTEM PRIVATE
    "${stm32_device}/Include" "${stm32_cmsis}")
target_compile_definitions(nexus_soc_stm32f407 PRIVATE STM32F407xx)
set(NEXUS_STARTUP_SOURCE
    "${stm32_device}/Source/Templates/gcc/startup_stm32f407xx.s" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" "${stm32_device}/Include"
    "${stm32_cmsis}" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32F407xx PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
