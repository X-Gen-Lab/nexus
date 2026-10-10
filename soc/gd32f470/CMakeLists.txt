# Exact GD32F470ZGT6 implementation; SDK headers stay private to this target.
set(gd_vendor "${PROJECT_SOURCE_DIR}/vendors/gigadevice/gd32f4xx")
if(DEFINED NEXUS_SOURCE_DIR)
    set(gd_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${gd_vendor}/../../../scripts/ci/verify_vendor_source.py"
    --root "${gd_vendor}"
    RESULT_VARIABLE gd_sdk_verified
    OUTPUT_VARIABLE gd_sdk_output ERROR_VARIABLE gd_sdk_error)
if(NOT gd_sdk_verified EQUAL 0)
    message(FATAL_ERROR "GD32 SDK lock failed: ${gd_sdk_output}${gd_sdk_error}")
endif()
set(gd_firmware "${gd_vendor}/Firmware")
set(gd_device "${gd_firmware}/CMSIS/GD/GD32F4xx")
set(gd_peripheral "${gd_firmware}/GD32F4xx_standard_peripheral")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/platform/ControllerModes.cmake")
# SystemInit needs RCU/FMC; the always-present monotonic timebase needs TIMER.
set(gd_sdk_units rcu fmc timer)
set(gd_sources timebase.c)
foreach(kind gpio uart spi i2c flash watchdog exti pwm adc)
    nexus_controller_modes(modes "${kind}")
    if(NOT modes)
        continue()
    endif()
    if(kind STREQUAL "pwm")
        list(APPEND gd_sources drivers/timer.c)
    else()
        list(APPEND gd_sources "drivers/${kind}.c")
    endif()
    # Generated UART/SPI/I2C acquisition and rollback use Nexus pin helpers.
    if(kind MATCHES "^(gpio|uart|spi|i2c)$")
        list(APPEND gd_sources drivers/gpio.c)
    endif()
    # EXTI/PWM/ADC configure pins through the SDK without Nexus GPIO helpers.
    if(kind MATCHES "^(gpio|uart|spi|i2c|exti|pwm|adc)$")
        list(APPEND gd_sdk_units gpio)
    endif()
    if(kind STREQUAL "uart")
        list(APPEND gd_sdk_units usart)
        if("dma-tx" IN_LIST modes)
            list(APPEND gd_sources drivers/uart_dma.c)
        endif()
        if("irq-blocks" IN_LIST modes)
            list(APPEND gd_sources drivers/uart_stream.c)
        endif()
    elseif(kind STREQUAL "spi")
        list(APPEND gd_sdk_units spi)
        if("dma-full-duplex" IN_LIST modes)
            list(APPEND gd_sources drivers/spi_dma.c)
        endif()
    elseif(kind STREQUAL "i2c")
        list(APPEND gd_sdk_units i2c)
    elseif(kind STREQUAL "watchdog")
        list(APPEND gd_sdk_units fwdgt)
    elseif(kind STREQUAL "adc")
        list(APPEND gd_sdk_units adc)
        if("trigger-dma" IN_LIST modes)
            list(APPEND gd_sources drivers/adc_stream.c)
        endif()
    endif()
endforeach()
list(REMOVE_DUPLICATES gd_sources)
list(REMOVE_DUPLICATES gd_sdk_units)
add_library(nexus_gd32_sdk STATIC)
foreach(unit IN LISTS gd_sdk_units)
    target_sources(nexus_gd32_sdk PRIVATE
        "${gd_peripheral}/Source/gd32f4xx_${unit}.c")
endforeach()
target_include_directories(nexus_gd32_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_gd32_sdk SYSTEM PRIVATE "${gd_firmware}/CMSIS"
    "${gd_device}/Include" "${gd_peripheral}/Include")
target_link_libraries(nexus_gd32_sdk PRIVATE nexus_build_options)
target_compile_definitions(nexus_gd32_sdk PRIVATE GD32F470 HXTAL_VALUE=25000000)
add_library(nexus_soc_gd32f470 STATIC ${gd_sources})
target_link_libraries(nexus_soc_gd32f470 PUBLIC nexus_io
    PRIVATE nexus_arch nexus_gd32_sdk nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_soc_gd32f470 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_soc_gd32f470 SYSTEM PRIVATE
    "${gd_firmware}/CMSIS" "${gd_device}/Include"
    "${gd_peripheral}/Include")
target_compile_definitions(nexus_soc_gd32f470 PRIVATE GD32F470
    HXTAL_VALUE=25000000)
set(NEXUS_STARTUP_SOURCE "${gd_device}/Source/GCC/startup_gd32f450_470.S"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
# system.c is a firmware source, not an archive: startup must resolve SystemInit.
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/private"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${gd_firmware}/CMSIS"
    "${gd_device}/Include" "${gd_peripheral}/Include" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS GD32F470 HXTAL_VALUE=25000000 PARENT_SCOPE)
