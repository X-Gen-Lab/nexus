# Fixed providers reuse official LL privately while preserving Nexus ownership.
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/platform/CMSISCore.cmake")
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
nexus_cmsis_core_include(stm32_cmsis)
foreach(required "${stm32_device}/Include/stm32f407xx.h"
                 "${stm32_cmsis}/core_cm4.h"
                 "${stm32_device}/Source/Templates/gcc/startup_stm32f407xx.s")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR
            "Missing pinned official STM32 SDK input: ${required}")
    endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/platform/ControllerModes.cmake")
set(stm32_sources)
foreach(kind gpio uart spi i2c flash watchdog exti pwm adc can usb_device eth eth_frames
        timer_capture advanced_pwm)
    nexus_controller_modes(modes "${kind}")
    if(NOT modes)
        continue()
    endif()
    if(kind STREQUAL "eth")
        list(APPEND stm32_sources drivers/ethernet.c)
    elseif(kind STREQUAL "eth_frames")
        list(APPEND stm32_sources drivers/ethernet_frames.c)
    elseif(kind STREQUAL "pwm")
        list(APPEND stm32_sources drivers/timer.c)
    else()
        list(APPEND stm32_sources "drivers/${kind}.c")
    endif()
    # Pin acquisition and rollback share GPIO helpers, including CS/PWM pins.
    if(kind MATCHES "^(gpio|uart|spi|i2c|exti|pwm|adc|can|usb_device|eth|eth_frames|timer_capture|advanced_pwm)$")
        list(APPEND stm32_sources drivers/gpio.c)
    endif()
    if(kind STREQUAL "uart")
        if("irq-words" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_word.c)
        endif()
        if("dma-tx" IN_LIST modes OR "dma-duplex" IN_LIST modes
            OR "dma-duplex-continuous" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_dma.c)
        endif()
        if("dma-duplex" IN_LIST modes OR "dma-duplex-continuous" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_duplex.c)
        endif()
        if("rx-dma-idle" IN_LIST modes OR "rx-dma-continuous" IN_LIST modes
            OR "dma-duplex" IN_LIST modes
            OR "dma-duplex-continuous" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_receive.c)
        endif()
        if("irq-blocks" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_stream.c)
        endif()
    elseif(kind STREQUAL "spi" AND "dma" IN_LIST modes)
        list(APPEND stm32_sources drivers/spi_dma.c)
    elseif(kind STREQUAL "i2c")
        list(APPEND stm32_sources drivers/i2c_cold.c)
        if("irq-messages" IN_LIST modes OR "dma-messages" IN_LIST modes)
            list(APPEND stm32_sources drivers/i2c_async.c
                "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v1_async/i2c_v1_async.c")
        endif()
    elseif(kind STREQUAL "eth")
        list(APPEND stm32_sources
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../io/src/controllers/eth_mac.c")
    elseif(kind STREQUAL "eth_frames")
        list(APPEND stm32_sources
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../io/src/controllers/eth_frames_mac.c")
    elseif(kind STREQUAL "usb_device")
        list(APPEND stm32_sources usb_clock.c
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../io/src/controllers/usb_device_controller.c")
    elseif(kind STREQUAL "can")
        list(APPEND stm32_sources "${CMAKE_CURRENT_SOURCE_DIR}/../../../io/src/controllers/can_controller.c")
    elseif(kind STREQUAL "adc")
        if("trigger-dma" IN_LIST modes
            OR "trigger-dma-continuous" IN_LIST modes)
            list(APPEND stm32_sources drivers/adc_stream.c)
        endif()
        if("trigger-dma-continuous" IN_LIST modes)
            list(APPEND stm32_sources drivers/adc_continuous.c)
        endif()
    endif()
endforeach()
list(REMOVE_DUPLICATES stm32_sources)
if(stm32_sources)
    add_library(nexus_soc_stm32f407 STATIC ${stm32_sources})
    target_link_libraries(nexus_soc_stm32f407 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32f407 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../common"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../io/private")
    target_include_directories(nexus_soc_stm32f407 SYSTEM PRIVATE
        "${stm32_device}/Include")
    nexus_use_cmsis_core(nexus_soc_stm32f407)
    target_compile_definitions(nexus_soc_stm32f407 PRIVATE STM32F407xx)
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
    nexus_stm32f4_official(nexus_soc_stm32f407)
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
set(stm32_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/system.c")
if(NEXUS_MEMORY_CCM)
    list(APPEND stm32_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/memory_init.c")
endif()
set(NEXUS_SYSTEM_SOURCE ${stm32_system_sources} PARENT_SCOPE)

# Fixed clock profile sleep is an explicitly selected caller-owned component.
function(nexus_add_stm32f407_sleep target)
    if(NOT ARGC EQUAL 1 OR TARGET "${target}")
        message(FATAL_ERROR "Sleep target requires one unused target name")
    endif()
    get_filename_component(sleep_source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../.." ABSOLUTE)
    add_library(${target} STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sleep.c")
    nexus_apply_target_policy(${target})
    target_link_libraries(${target} PRIVATE Nexus::ClockDomain Nexus::Arch
        nexus_build_options nexus_owned_warnings)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/private"
        "${sleep_source}/components/clock_domain/private")
    target_include_directories(${target} SYSTEM PRIVATE
        "${sleep_source}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE STM32F407xx)
endfunction()

# The explicitly selected getter and SysTick entry must precede weak archives.
if(NEXUS_OS_TIMEBASE_LOWPOWER)
    add_library(nx_stm32f407_lowpower_binding OBJECT timebase_lowpower.c)
    target_link_libraries(nx_stm32f407_lowpower_binding PRIVATE
        Nexus::OSFreeRTOS Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nx_stm32f407_lowpower_binding PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${NEXUS_SOURCE_DIR}/os/freertos/private")
    target_include_directories(nx_stm32f407_lowpower_binding SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(nx_stm32f407_lowpower_binding)
    target_compile_definitions(nx_stm32f407_lowpower_binding PRIVATE STM32F407xx)
    target_sources(nexus_soc_stm32f407 INTERFACE
        $<TARGET_OBJECTS:nx_stm32f407_lowpower_binding>)
    set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32F407xx
        NEXUS_TIMEBASE_LOWPOWER=1 PARENT_SCOPE)
endif()
