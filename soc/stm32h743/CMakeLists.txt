# Exact H743 Cache-off software reference; official SDK stays private.
set(h7_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7")
set(h7_cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
set(h7_lock "${NEXUS_SOURCE_DIR}/dependencies/stm32h7.lock.json")
set(h7_verifier "${NEXUS_SOURCE_DIR}/tools/maintenance/cmsis_device_lock.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${h7_verifier}"
        --source "${h7_device}" --lock "${h7_lock}"
    RESULT_VARIABLE h7_lock_result
    OUTPUT_VARIABLE h7_lock_output
    ERROR_VARIABLE h7_lock_error)
if(NOT h7_lock_result EQUAL 0)
    message(FATAL_ERROR "H743 source lock rejected: ${h7_lock_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${h7_lock}" "${h7_verifier}" "${h7_device}/LICENSE.md"
    "${h7_device}/Include/stm32h743xx.h"
    "${h7_device}/Include/stm32h7xx.h"
    "${h7_device}/Include/system_stm32h7xx.h"
    "${h7_device}/Source/Templates/gcc/startup_stm32h743xx.s")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
set(h7_sources)
nexus_controller_modes(h7_gpio_modes gpio)
nexus_controller_modes(h7_uart_modes uart)
if(h7_gpio_modes OR h7_uart_modes)
    list(APPEND h7_sources drivers/gpio.c)
endif()
if(h7_uart_modes)
    list(APPEND h7_sources drivers/uart.c)
endif()
if(h7_sources)
    add_library(nexus_soc_stm32h743 STATIC ${h7_sources})
    target_link_libraries(nexus_soc_stm32h743 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32h743 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    target_include_directories(nexus_soc_stm32h743 SYSTEM PRIVATE
        "${h7_device}/Include" "${h7_cmsis}")
    target_compile_definitions(nexus_soc_stm32h743 PRIVATE STM32H743xx)
else()
    add_library(nexus_soc_stm32h743 INTERFACE)
    target_link_libraries(nexus_soc_stm32h743 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${h7_device}/Source/Templates/gcc/startup_stm32h743xx.s" PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/power_boot.S" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${h7_device}/Include" "${h7_cmsis}"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32H743xx PARENT_SCOPE)
