# Fixed providers use official pinned device/CMSIS headers privately. Drivers
# operate reviewed controller registers, without importing the legacy HAL SDK.
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
set(stm32_cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
foreach(required "${stm32_device}/Include/stm32f407xx.h"
                 "${stm32_cmsis}/core_cm4.h"
                 "${stm32_device}/Source/Templates/gcc/startup_stm32f407xx.s")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR
            "Missing pinned official STM32 SDK input: ${required}")
    endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/platform/ControllerModes.cmake")
set(stm32_sources)
foreach(kind gpio uart spi i2c flash watchdog exti pwm adc)
    nexus_controller_modes(modes "${kind}")
    if(NOT modes)
        continue()
    endif()
    if(kind STREQUAL "pwm")
        list(APPEND stm32_sources drivers/timer.c)
    else()
        list(APPEND stm32_sources "drivers/${kind}.c")
    endif()
    # Pin acquisition and rollback share GPIO helpers, including CS/PWM pins.
    if(kind MATCHES "^(gpio|uart|spi|i2c|exti|pwm|adc)$")
        list(APPEND stm32_sources drivers/gpio.c)
    endif()
    if(kind STREQUAL "uart")
        if("dma-tx" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_dma.c)
        endif()
        if("irq-blocks" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_stream.c)
        endif()
    elseif(kind STREQUAL "spi" AND "dma" IN_LIST modes)
        list(APPEND stm32_sources drivers/spi_dma.c)
    elseif(kind STREQUAL "adc" AND "trigger-dma" IN_LIST modes)
        list(APPEND stm32_sources drivers/adc_stream.c)
    endif()
endforeach()
list(REMOVE_DUPLICATES stm32_sources)
if(stm32_sources)
    add_library(nexus_soc_stm32f407 STATIC ${stm32_sources})
    target_link_libraries(nexus_soc_stm32f407 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32f407 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    target_include_directories(nexus_soc_stm32f407 SYSTEM PRIVATE
        "${stm32_device}/Include" "${stm32_cmsis}")
    target_compile_definitions(nexus_soc_stm32f407 PRIVATE STM32F407xx)
else()
    # Startup/system belong to the firmware; an empty assembly needs no driver.
    add_library(nexus_soc_stm32f407 INTERFACE)
    target_link_libraries(nexus_soc_stm32f407 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${stm32_device}/Source/Templates/gcc/startup_stm32f407xx.s" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" "${stm32_device}/Include"
    "${stm32_cmsis}" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32F407xx PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
