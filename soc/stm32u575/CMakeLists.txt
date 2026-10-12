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
if(u5_gpio_modes OR u5_uart_modes)
    list(APPEND u5_sources drivers/gpio.c)
endif()
if(u5_uart_modes)
    list(APPEND u5_sources drivers/uart.c)
endif()
if(u5_sources)
    add_library(nexus_soc_stm32u575 STATIC ${u5_sources})
    target_link_libraries(nexus_soc_stm32u575 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32u575 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    target_include_directories(nexus_soc_stm32u575 SYSTEM PRIVATE
        "${u5_device}/Include" "${u5_cmsis}")
    target_compile_definitions(nexus_soc_stm32u575 PRIVATE STM32U575xx)
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
