# Reviewed U575 GPIO/USART slice; official CMSIS inputs remain private.
set(u5_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5")
set(u5_cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
set(u5_lock "${NEXUS_SOURCE_DIR}/dependencies/stm32u5.lock.json")
set(u5_verifier "${NEXUS_SOURCE_DIR}/tools/maintenance/cmsis_device_lock.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${u5_verifier}"
        --source "${u5_device}" --lock "${u5_lock}"
    RESULT_VARIABLE u5_lock_result
    OUTPUT_VARIABLE u5_lock_output
    ERROR_VARIABLE u5_lock_error)
if(NOT u5_lock_result EQUAL 0)
    message(FATAL_ERROR "U575 source lock rejected: ${u5_lock_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${u5_lock}" "${u5_verifier}" "${u5_device}/LICENSE.md"
    "${u5_device}/Include/stm32u575xx.h"
    "${u5_device}/Include/stm32u5xx.h"
    "${u5_device}/Include/system_stm32u5xx.h"
    "${u5_device}/Source/Templates/gcc/startup_stm32u575xx.s")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
set(u5_sources)
nexus_controller_modes(u5_gpio_modes gpio)
nexus_controller_modes(u5_uart_modes uart)
nexus_controller_modes(u5_watchdog_modes watchdog)
nexus_controller_modes(u5_spi_modes spi)
nexus_controller_modes(u5_i2c_modes i2c)
nexus_controller_modes(u5_adc_modes adc)
nexus_controller_modes(u5_pwm_modes pwm)
nexus_controller_modes(u5_exti_modes exti)
nexus_controller_modes(u5_flash_modes flash)
nexus_controller_modes(u5_lptim_modes lptim)
if(u5_gpio_modes OR u5_uart_modes OR u5_spi_modes OR u5_i2c_modes OR u5_pwm_modes)
    list(APPEND u5_sources drivers/gpio.c)
endif()
if(u5_uart_modes)
    list(APPEND u5_sources drivers/uart.c)
endif()
if("irq-fifo-bytes" IN_LIST u5_uart_modes)
    list(APPEND u5_sources drivers/uart_fifo.c)
endif()
if("irq-fifo-rtscts-bytes" IN_LIST u5_uart_modes)
    list(APPEND u5_sources drivers/uart_rtscts.c)
endif()
if("dma-tx" IN_LIST u5_uart_modes)
    list(APPEND u5_sources drivers/uart_dma.c)
endif()
if("dma-chain-tx" IN_LIST u5_uart_modes)
    list(APPEND u5_sources drivers/uart_dma_chain.c)
endif()
if(u5_lptim_modes)
    list(APPEND u5_sources drivers/lptim.c)
endif()
if(u5_watchdog_modes)
    list(APPEND u5_sources drivers/watchdog.c)
endif()
foreach(kind spi i2c adc pwm exti flash)
    if(u5_${kind}_modes)
        list(APPEND u5_sources "drivers/${kind}.c")
    endif()
endforeach()
if(u5_i2c_modes)
    list(APPEND u5_sources "../common/i2c_v2.c")
endif()
if(u5_sources)
    add_library(nexus_soc_stm32u575 STATIC ${u5_sources})
    target_link_libraries(nexus_soc_stm32u575 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32u575 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${NEXUS_SOURCE_DIR}/io/private")
    target_include_directories(nexus_soc_stm32u575 SYSTEM PRIVATE
        "${u5_device}/Include" "${u5_cmsis}")
    target_compile_definitions(nexus_soc_stm32u575 PRIVATE STM32U575xx)
    if(u5_spi_modes OR u5_i2c_modes OR u5_adc_modes OR u5_pwm_modes OR
            "dma-tx" IN_LIST u5_uart_modes OR
            "dma-chain-tx" IN_LIST u5_uart_modes OR
            "irq-fifo-bytes" IN_LIST u5_uart_modes OR
            "irq-fifo-rtscts-bytes" IN_LIST u5_uart_modes)
        include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
        nexus_stm32_official(nexus_soc_stm32u575 u5)
    endif()
else()
    add_library(nexus_soc_stm32u575 INTERFACE)
    target_link_libraries(nexus_soc_stm32u575 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${u5_device}/Source/Templates/gcc/startup_stm32u575xx.s" PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${u5_device}/Include" "${u5_cmsis}"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32U575xx PARENT_SCOPE)
