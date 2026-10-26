# Exact HD RCT6 official startup and private register definitions.
include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/platform/CMSISCore.cmake")
set(gd303_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x")
set(gd303_device "${gd303_vendor}/Firmware/CMSIS/GD/GD32F30x")
set(gd303_headers "${gd303_vendor}/Firmware/GD32F30x_standard_peripheral/Include")
nexus_cmsis_core_include(gd303_core)
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${NEXUS_SOURCE_DIR}/scripts/ci/verify_vendor_source.py"
    --root "${gd303_vendor}"
    RESULT_VARIABLE gd303_verified
    OUTPUT_VARIABLE gd303_output ERROR_VARIABLE gd303_error)
if(NOT gd303_verified EQUAL 0)
    message(FATAL_ERROR "GD32F303 SDK lock rejected: ${gd303_output}${gd303_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${gd303_vendor}/source.lock.json")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
nexus_controller_modes(gd303_gpio gpio)
nexus_controller_modes(gd303_uart uart)
nexus_controller_modes(gd303_spi spi)
nexus_controller_modes(gd303_watchdog watchdog)
nexus_controller_modes(gd303_i2c i2c)
nexus_controller_modes(gd303_adc adc)
nexus_controller_modes(gd303_pwm pwm)
nexus_controller_modes(gd303_flash flash)
nexus_controller_modes(gd303_exti exti)
set(gd303_sources)
if(gd303_gpio OR gd303_uart OR gd303_spi OR gd303_adc OR gd303_pwm OR gd303_exti)
    list(APPEND gd303_sources drivers/gpio.c)
endif()
if(gd303_uart)
    if("irq-bytes" IN_LIST gd303_uart)
        list(APPEND gd303_sources drivers/uart.c)
    endif()
    if("dma-tx" IN_LIST gd303_uart OR "dma-duplex" IN_LIST gd303_uart)
        list(APPEND gd303_sources drivers/uart_dma.c
            "${NEXUS_SOURCE_DIR}/drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.c")
    endif()
endif()
if(gd303_i2c)
    list(APPEND gd303_sources drivers/i2c_async.c
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v1_async/i2c_v1_async.c")
endif()
if(gd303_spi)
    list(APPEND gd303_sources drivers/spi.c)
endif()
if(gd303_watchdog)
    list(APPEND gd303_sources drivers/watchdog.c)
endif()
if(gd303_adc)
    list(APPEND gd303_sources
        "${NEXUS_SOURCE_DIR}/drivers/adc/legacy_adc/legacy_adc.c")
endif()
if(gd303_pwm)
    list(APPEND gd303_sources
        "${NEXUS_SOURCE_DIR}/drivers/timer/legacy_pwm/legacy_pwm.c")
endif()
if(gd303_flash)
    list(APPEND gd303_sources
        "${NEXUS_SOURCE_DIR}/drivers/flash/legacy_flash/legacy_flash.c")
endif()
if(gd303_adc OR gd303_pwm OR gd303_flash)
    list(APPEND gd303_sources drivers/devices.c)
endif()
if(gd303_exti)
    list(APPEND gd303_sources drivers/exti.c)
endif()
if(gd303_sources)
    add_library(nexus_soc_gd32f303 STATIC ${gd303_sources})
    target_link_libraries(nexus_soc_gd32f303 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_gd32f303 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private")
    target_include_directories(nexus_soc_gd32f303 SYSTEM PRIVATE
        "${gd303_device}/Include" "${gd303_headers}")
    nexus_use_cmsis_core(nexus_soc_gd32f303)
    target_compile_definitions(nexus_soc_gd32f303 PRIVATE GD32F30X_HD)
else()
    add_library(nexus_soc_gd32f303 INTERFACE)
    target_link_libraries(nexus_soc_gd32f303 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${gd303_device}/Source/GCC/startup_gd32f30x_hd.S" PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES
    "${gd303_device}/Include" "${gd303_headers}" "${gd303_core}" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS GD32F30X_HD PARENT_SCOPE)
