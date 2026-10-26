# Exact medium-density F103RB; official CMSIS/LL remain private to owned C.
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/platform/CMSISCore.cmake")
set(f1_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1")
set(f1_hal "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal")
nexus_cmsis_core_include(f1_core)
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/maintenance/stm32f1_hal_lock.py"
    --source "${f1_hal}"
    --lock "${NEXUS_SOURCE_DIR}/dependencies/stm32f1-hal.lock.json"
    --cmsis-source "${f1_device}"
    RESULT_VARIABLE f1_result ERROR_VARIABLE f1_error OUTPUT_QUIET)
if(NOT f1_result EQUAL 0)
    message(FATAL_ERROR "F103 official source lock rejected: ${f1_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/dependencies/stm32f1.lock.json"
    "${NEXUS_SOURCE_DIR}/dependencies/stm32f1-hal.lock.json"
    "${NEXUS_SOURCE_DIR}/tools/maintenance/stm32f1_hal_lock.py"
    "${NEXUS_SOURCE_DIR}/tools/maintenance/cmsis_device_lock.py"
    "${f1_device}/Include/stm32f103xb.h"
    "${f1_device}/Include/stm32f1xx.h"
    "${f1_device}/Include/system_stm32f1xx.h"
    "${f1_device}/Source/Templates/gcc/startup_stm32f103xb.s"
    "${f1_device}/License.md" "${f1_hal}/LICENSE.md"
    "${f1_hal}/Inc/stm32f1xx_ll_gpio.h"
    "${f1_hal}/Inc/stm32f1xx_ll_usart.h")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
nexus_controller_modes(f1_gpio gpio)
nexus_controller_modes(f1_uart uart)
nexus_controller_modes(f1_watchdog watchdog)
nexus_controller_modes(f1_exti exti)
nexus_controller_modes(f1_spi spi)
nexus_controller_modes(f1_i2c i2c)
nexus_controller_modes(f1_adc adc)
nexus_controller_modes(f1_pwm pwm)
nexus_controller_modes(f1_flash flash)
set(f1_sources)
if(f1_gpio OR f1_uart OR f1_spi)
    list(APPEND f1_sources drivers/gpio.c)
endif()
if(f1_uart)
    if("irq-bytes" IN_LIST f1_uart)
        list(APPEND f1_sources drivers/uart.c)
    endif()
    if("dma-tx" IN_LIST f1_uart OR "dma-duplex" IN_LIST f1_uart)
        list(APPEND f1_sources drivers/uart_dma.c
            "${NEXUS_SOURCE_DIR}/soc/common/uart_v1_channel_dma.c")
    endif()
endif()
if(f1_i2c)
    list(APPEND f1_sources drivers/i2c_async.c
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v1_async.c")
endif()
if(f1_watchdog)
    list(APPEND f1_sources drivers/watchdog.c)
endif()
if(f1_exti)
    list(APPEND f1_sources drivers/exti.c)
endif()
if(f1_spi)
    list(APPEND f1_sources drivers/spi.c)
endif()
foreach(kind adc pwm flash)
    if(f1_${kind})
        list(APPEND f1_sources
            "${NEXUS_SOURCE_DIR}/soc/common/legacy_${kind}.c")
    endif()
endforeach()
if(f1_adc OR f1_pwm OR f1_flash)
    list(APPEND f1_sources drivers/devices.c)
endif()
if(f1_sources)
    add_library(nexus_soc_stm32f103 STATIC ${f1_sources})
    target_link_libraries(nexus_soc_stm32f103 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32f103 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    target_include_directories(nexus_soc_stm32f103 SYSTEM PRIVATE
        "${f1_device}/Include" "${f1_hal}/Inc")
    nexus_use_cmsis_core(nexus_soc_stm32f103)
    target_compile_definitions(nexus_soc_stm32f103 PRIVATE STM32F103xB)
else()
    add_library(nexus_soc_stm32f103 INTERFACE)
    target_link_libraries(nexus_soc_stm32f103 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${f1_device}/Source/Templates/gcc/startup_stm32f103xb.s" PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES
    "${f1_device}/Include" "${f1_core}" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32F103xB PARENT_SCOPE)
