# Fixed providers reuse official LL privately while preserving Nexus ownership.
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/platform/CMSISCore.cmake")
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
nexus_cmsis_core_include(stm32_cmsis)
if(NOT NEXUS_STM32_DEVICE OR NOT NEXUS_STM32_STARTUP)
    message(FATAL_ERROR "STM32F4 requires resolved exact device and startup facts")
endif()
string(TOLOWER "${NEXUS_STM32_DEVICE}" stm32_device_header)
foreach(required "${stm32_device}/Include/${stm32_device_header}.h"
                 "${stm32_cmsis}/core_cm4.h"
                 "${stm32_device}/Source/Templates/gcc/${NEXUS_STM32_STARTUP}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR
            "Missing pinned official STM32 SDK input: ${required}")
    endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/platform/ControllerModes.cmake")
set(stm32_sources reset.c drivers/platform_coordinator.c drivers/platform_lifecycle.c
    drivers/system_lifecycle.c drivers/chip.c
    drivers/lifecycle_pins.c)
set(stm32_uart_hal OFF)
foreach(kind gpio uart spi i2c flash watchdog exti pwm encoder adc dac crc can usb_device eth eth_frames
        timer_capture advanced_pwm block rtc external_bus)
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
    elseif(kind STREQUAL "advanced_pwm")
        nexus_controller_names(timers "${kind}")
        foreach(timer IN LISTS timers)
            if(timer STREQUAL "TIM1")
                list(APPEND stm32_sources drivers/advanced_pwm.c)
            elseif(timer STREQUAL "TIM8")
                list(APPEND stm32_sources drivers/advanced_pwm_tim8.c)
            else()
                message(FATAL_ERROR "Unreviewed advanced timer: ${timer}")
            endif()
        endforeach()
    elseif(kind STREQUAL "exti")
        if("edge-event" IN_LIST modes OR "pvd-events" IN_LIST modes)
            list(APPEND stm32_sources drivers/exti.c)
        endif()
        if("pvd-level" IN_LIST modes OR "pvd-events" IN_LIST modes)
            list(APPEND stm32_sources drivers/pvd.c)
        endif()
    elseif(kind STREQUAL "watchdog")
        if("independent" IN_LIST modes)
            list(APPEND stm32_sources drivers/watchdog.c)
        endif()
        if("window" IN_LIST modes)
            list(APPEND stm32_sources drivers/window_watchdog.c)
        endif()
    elseif(kind STREQUAL "block")
        list(APPEND stm32_sources drivers/sdio.c)
    elseif(kind STREQUAL "external_bus")
        list(APPEND stm32_sources drivers/fsmc.c)
    else()
        list(APPEND stm32_sources "drivers/${kind}.c")
    endif()
    # Pin acquisition and rollback share GPIO helpers, including CS/PWM pins.
    if(kind MATCHES "^(gpio|uart|spi|i2c|exti|pwm|encoder|adc|dac|can|usb_device|eth|eth_frames|timer_capture|advanced_pwm|block|external_bus)$"
        AND (NOT kind STREQUAL "exti" OR "edge-event" IN_LIST modes))
        list(APPEND stm32_sources drivers/gpio.c)
    endif()
    if(kind STREQUAL "uart")
        list(APPEND stm32_sources drivers/uart_cold.c drivers/uart_hardware.c drivers/lifecycle_uart.c)
        if("irq-words" IN_LIST modes)
            list(APPEND stm32_sources drivers/uart_word.c)
        endif()
        if("dma-tx" IN_LIST modes)
            set(stm32_uart_hal ON)
            list(APPEND stm32_sources drivers/uart_hal.c)
        endif()
        if("dma-duplex" IN_LIST modes
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
    elseif(kind STREQUAL "gpio")
        list(APPEND stm32_sources drivers/lifecycle_gpio.c)
    elseif(kind STREQUAL "spi")
        list(APPEND stm32_sources drivers/lifecycle_spi.c)
        if("dma" IN_LIST modes)
            list(APPEND stm32_sources drivers/spi_dma.c)
        endif()
    elseif(kind STREQUAL "i2c")
        list(APPEND stm32_sources drivers/i2c_cold.c)
        if("irq-messages" IN_LIST modes OR "dma-messages" IN_LIST modes)
            list(APPEND stm32_sources drivers/i2c_async.c
                drivers/ip/i2c_v1_async/i2c_v1_async.c)
        endif()
    elseif(kind STREQUAL "eth")
        list(APPEND stm32_sources
            drivers/ip/legacy_mac/eth_mac.c)
    elseif(kind STREQUAL "eth_frames")
        list(APPEND stm32_sources
            drivers/ip/legacy_mac/eth_frames_mac.c)
    elseif(kind STREQUAL "usb_device")
        list(APPEND stm32_sources usb_clock.c
            drivers/ip/fs_device_pio/usb_device_controller.c)
    elseif(kind STREQUAL "can")
        list(APPEND stm32_sources drivers/ip/classic_can/can_controller.c)
    elseif(kind STREQUAL "dac")
        if("trigger-dma" IN_LIST modes OR "trigger-dma-circular" IN_LIST modes)
            list(APPEND stm32_sources drivers/dac_dma.c)
        endif()
    elseif(kind STREQUAL "adc")
        if("injected-shot" IN_LIST modes)
            list(APPEND stm32_sources drivers/adc_injected.c)
        endif()
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
list(APPEND stm32_sources drivers/platform_cold.c)
add_library(nexus_soc_stm32f4 STATIC ${stm32_sources})
target_link_libraries(nexus_soc_stm32f4 PUBLIC nexus_io
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_soc_stm32f4 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../io/private")
target_include_directories(nexus_soc_stm32f4 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/ip/i2c_v1_async"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/ip/classic_can"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/ip/fs_device_pio"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/ip/legacy_mac"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/ip/advanced_timer")
target_include_directories(nexus_soc_stm32f4 SYSTEM PRIVATE
    "${stm32_device}/Include")
nexus_use_cmsis_core(nexus_soc_stm32f4)
target_compile_definitions(nexus_soc_stm32f4 PRIVATE ${NEXUS_STM32_DEVICE})
nexus_controller_modes(stm32_i2c_modes i2c)
if("irq-messages" IN_LIST stm32_i2c_modes OR
        "dma-messages" IN_LIST stm32_i2c_modes)
    target_compile_definitions(nexus_soc_stm32f4 PRIVATE
        "NX_I2C_V1_PORT_HEADER=\"stm32f407_i2c_port.h\"")
endif()
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32f4_official(nexus_soc_stm32f4)
if(stm32_uart_hal)
    nexus_stm32f407_uart_hal(nexus_soc_stm32f4)
endif()
nexus_controller_modes(stm32_block_modes block)
if(stm32_block_modes)
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32SDIO.cmake")
    nexus_stm32f407_sdio_ll(nexus_soc_stm32f4)
endif()
nexus_controller_modes(stm32_external_bus_modes external_bus)
if(stm32_external_bus_modes)
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32FSMC.cmake")
    nexus_stm32f407_fsmc_ll(nexus_soc_stm32f4)
endif()
set(NEXUS_STARTUP_SOURCE
    "${stm32_device}/Source/Templates/gcc/${NEXUS_STM32_STARTUP}" PARENT_SCOPE)
set(stm32_system_includes
    "${CMAKE_CURRENT_SOURCE_DIR}/private" "${stm32_device}/Include"
    "${stm32_cmsis}")
if(stm32_uart_hal)
    list(APPEND stm32_system_includes
        "${CMAKE_CURRENT_SOURCE_DIR}/private/hal"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Inc")
endif()
set(NEXUS_SYSTEM_PRIVATE_INCLUDES ${stm32_system_includes} PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS ${NEXUS_STM32_DEVICE} PARENT_SCOPE)
set(stm32_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/system.c")
if(NEXUS_MEMORY_CCM OR NEXUS_MEMORY_ADDSRAM)
    list(APPEND stm32_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/memory_init.c")
endif()
if(NEXUS_MEMORY_ADDSRAM)
    if(NOT stm32_external_bus_modes)
        message(FATAL_ERROR "F407 startup SRAM requires its selected FSMC owner")
    endif()
    target_sources(nexus_soc_stm32f4 PRIVATE drivers/fsmc_boot.c)
    list(APPEND stm32_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/sram_boot.c")
    set(NEXUS_SYSTEM_VENDOR_INCLUDES
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Inc" PARENT_SCOPE)
endif()
set(NEXUS_SYSTEM_SOURCE ${stm32_system_sources} PARENT_SCOPE)

# Fixed clock profile sleep is an explicitly selected caller-owned component.
function(nexus_add_stm32f407_sleep target)
    if(NOT ARGC EQUAL 1 OR TARGET "${target}")
        message(FATAL_ERROR "Sleep target requires one unused target name")
    endif()
    if(NOT TARGET Nexus::Platform)
        message(FATAL_ERROR "Sleep requires a selected F4 Platform")
    endif()
    get_target_property(sleep_family Nexus::Platform NEXUS_SOC_FAMILY)
    get_target_property(sleep_device Nexus::Platform NEXUS_STM32_DEVICE)
    if(NOT sleep_family STREQUAL "stm32f4" OR
            NOT sleep_device STREQUAL "STM32F407xx")
        message(FATAL_ERROR
            "Sleep requires a selected F4 Platform with the maintained F407 device")
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
    target_compile_definitions(${target} PRIVATE ${sleep_device})
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
    target_compile_definitions(nx_stm32f407_lowpower_binding PRIVATE ${NEXUS_STM32_DEVICE})
    target_sources(nexus_soc_stm32f4 INTERFACE
        $<TARGET_OBJECTS:nx_stm32f407_lowpower_binding>)
    set(NEXUS_SYSTEM_COMPILE_DEFINITIONS ${NEXUS_STM32_DEVICE}
        NEXUS_TIMEBASE_LOWPOWER=1 PARENT_SCOPE)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/Entropy.cmake")
