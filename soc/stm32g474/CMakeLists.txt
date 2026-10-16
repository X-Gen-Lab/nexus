# Reviewed G474 controller modes; official CMSIS inputs remain private.
set(g4_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4")
set(g4_cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
set(g4_lock "${NEXUS_SOURCE_DIR}/dependencies/stm32g4.lock.json")
set(g4_verifier "${NEXUS_SOURCE_DIR}/tools/maintenance/stm32g4_lock.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${g4_verifier}"
        --source "${g4_device}" --lock "${g4_lock}"
    RESULT_VARIABLE g4_lock_result
    OUTPUT_VARIABLE g4_lock_output
    ERROR_VARIABLE g4_lock_error)
if(NOT g4_lock_result EQUAL 0)
    message(FATAL_ERROR "G474 source lock rejected: ${g4_lock_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${g4_lock}" "${g4_verifier}" "${g4_device}/LICENSE.md"
    "${g4_device}/Include/stm32g474xx.h"
    "${g4_device}/Include/stm32g4xx.h"
    "${g4_device}/Include/system_stm32g4xx.h"
    "${g4_device}/Source/Templates/gcc/startup_stm32g474xx.s")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
set(g4_sources)
nexus_controller_modes(g4_gpio_modes gpio)
nexus_controller_modes(g4_uart_modes uart)
nexus_controller_modes(g4_exti_modes exti)
nexus_controller_modes(g4_watchdog_modes watchdog)
nexus_controller_modes(g4_spi_modes spi)
nexus_controller_modes(g4_pwm_modes pwm)
if(g4_gpio_modes OR g4_uart_modes OR g4_spi_modes OR g4_pwm_modes)
    list(APPEND g4_sources drivers/gpio.c)
endif()
if(g4_uart_modes)
    list(APPEND g4_sources drivers/uart.c)
endif()
if(g4_exti_modes)
    list(APPEND g4_sources drivers/exti.c)
endif()
if(g4_watchdog_modes)
    list(APPEND g4_sources drivers/watchdog.c)
endif()
if(g4_spi_modes)
    list(APPEND g4_sources drivers/spi.c)
endif()
if(g4_pwm_modes)
    list(APPEND g4_sources drivers/pwm.c)
endif()
if(g4_sources)
    add_library(nexus_soc_stm32g474 STATIC ${g4_sources})
    target_link_libraries(nexus_soc_stm32g474 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32g474 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    target_include_directories(nexus_soc_stm32g474 SYSTEM PRIVATE
        "${g4_device}/Include" "${g4_cmsis}")
    target_compile_definitions(nexus_soc_stm32g474 PRIVATE STM32G474xx)
else()
    add_library(nexus_soc_stm32g474 INTERFACE)
    target_link_libraries(nexus_soc_stm32g474 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${g4_device}/Source/Templates/gcc/startup_stm32g474xx.s" PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${g4_device}/Include" "${g4_cmsis}"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32G474xx PARENT_SCOPE)
