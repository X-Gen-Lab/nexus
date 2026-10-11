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
foreach(kind gpio uart spi i2c flash watchdog exti pwm adc can usb_device eth)
    nexus_controller_modes(modes "${kind}")
    if(NOT modes)
        continue()
    endif()
    if(kind STREQUAL "eth")
        list(APPEND gd_sources drivers/ethernet.c)
    elseif(kind STREQUAL "pwm")
        list(APPEND gd_sources drivers/timer.c)
    else()
        list(APPEND gd_sources "drivers/${kind}.c")
    endif()
    # Generated UART/SPI/I2C acquisition and rollback use Nexus pin helpers.
    if(kind MATCHES "^(gpio|uart|spi|i2c|can|usb_device|eth)$")
        list(APPEND gd_sources drivers/gpio.c)
    endif()
    # EXTI/PWM/ADC configure pins through the SDK without Nexus GPIO helpers.
    if(kind MATCHES "^(gpio|uart|spi|i2c|exti|pwm|adc|can|usb_device|eth)$")
        list(APPEND gd_sdk_units gpio)
    endif()
    if(kind STREQUAL "uart")
        list(APPEND gd_sdk_units usart)
        if("dma-tx" IN_LIST modes OR "dma-duplex" IN_LIST modes
            OR "dma-duplex-continuous" IN_LIST modes)
            list(APPEND gd_sources drivers/uart_dma.c)
        endif()
        if("rx-dma-idle" IN_LIST modes OR "rx-dma-continuous" IN_LIST modes
            OR "dma-duplex" IN_LIST modes
            OR "dma-duplex-continuous" IN_LIST modes)
            list(APPEND gd_sources drivers/uart_receive.c drivers/uart_dma.c)
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
        if("irq-messages" IN_LIST modes OR "dma-messages" IN_LIST modes)
            list(APPEND gd_sources drivers/i2c_async.c
                "${CMAKE_CURRENT_SOURCE_DIR}/../common/i2c_v1_async.c")
        endif()
    elseif(kind STREQUAL "watchdog")
        list(APPEND gd_sdk_units fwdgt)
    elseif(kind STREQUAL "eth")
        list(APPEND gd_sources
            "${CMAKE_CURRENT_SOURCE_DIR}/../../io/src/eth_mac.c")
    elseif(kind STREQUAL "usb_device")
        list(APPEND gd_sources usb_clock.c
            "${CMAKE_CURRENT_SOURCE_DIR}/../../io/src/usb_device_controller.c")
    elseif(kind STREQUAL "can")
        list(APPEND gd_sources "${CMAKE_CURRENT_SOURCE_DIR}/../../io/src/can_controller.c")
    elseif(kind STREQUAL "adc")
        list(APPEND gd_sdk_units adc)
        if("trigger-dma" IN_LIST modes
            OR "trigger-dma-continuous" IN_LIST modes)
            list(APPEND gd_sources drivers/adc_stream.c)
        endif()
        if("trigger-dma-continuous" IN_LIST modes)
            list(APPEND gd_sources drivers/adc_continuous.c)
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
    "${CMAKE_CURRENT_SOURCE_DIR}/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../io/private")
target_include_directories(nexus_soc_gd32f470 SYSTEM PRIVATE
    "${gd_firmware}/CMSIS" "${gd_device}/Include"
    "${gd_peripheral}/Include"
    "${gd_firmware}/GD32F4xx_usb_library/driver/Include")
target_compile_definitions(nexus_soc_gd32f470 PRIVATE GD32F470
    HXTAL_VALUE=25000000)
set(NEXUS_STARTUP_SOURCE "${gd_device}/Source/GCC/startup_gd32f450_470.S"
    PARENT_SCOPE)
set(gd_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/system.c")
if(NEXUS_MEMORY_ADDSRAM OR NEXUS_MEMORY_TCM)
    list(APPEND gd_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/memory_init.c")
endif()
set(NEXUS_SYSTEM_SOURCE ${gd_system_sources} PARENT_SCOPE)
# system.c is a firmware source, not an archive: startup must resolve SystemInit.
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/private"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${gd_firmware}/CMSIS"
    "${gd_device}/Include" "${gd_peripheral}/Include" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS GD32F470 HXTAL_VALUE=25000000 PARENT_SCOPE)

# Fixed clock profile sleep is an explicitly selected caller-owned component.
function(nexus_add_gd32f470_sleep target)
    if(NOT ARGC EQUAL 1 OR TARGET "${target}")
        message(FATAL_ERROR "Sleep target requires one unused target name")
    endif()
    get_filename_component(sleep_source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    add_library(${target} STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sleep.c")
    target_link_libraries(${target} PRIVATE Nexus::ClockDomain Nexus::Arch
        nexus_build_options nexus_owned_warnings)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/private"
        "${sleep_source}/components/clock_domain/private")
    set(sleep_vendor "${sleep_source}/vendors/gigadevice/gd32f4xx/Firmware")
    target_include_directories(${target} SYSTEM PRIVATE
        "${sleep_vendor}/CMSIS" "${sleep_vendor}/CMSIS/GD/GD32F4xx/Include"
        "${sleep_vendor}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(${target} PRIVATE GD32F470 HXTAL_VALUE=25000000)
endfunction()

# The explicitly selected getter and SysTick entry must precede weak archives.
if(NEXUS_OS_TIMEBASE_LOWPOWER)
    add_library(nx_gd32f470_lowpower_binding OBJECT timebase_lowpower.c)
    target_link_libraries(nx_gd32f470_lowpower_binding PRIVATE
        Nexus::OSFreeRTOS Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nx_gd32f470_lowpower_binding PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${NEXUS_SOURCE_DIR}/os/freertos/private")
    target_include_directories(nx_gd32f470_lowpower_binding SYSTEM PRIVATE
        "${gd_firmware}/CMSIS" "${gd_device}/Include"
        "${gd_peripheral}/Include")
    target_compile_definitions(nx_gd32f470_lowpower_binding PRIVATE GD32F470
        HXTAL_VALUE=25000000)
    target_compile_definitions(nexus_soc_gd32f470 PRIVATE
        NEXUS_TIMEBASE_LOWPOWER=1)
    target_sources(nexus_soc_gd32f470 INTERFACE
        $<TARGET_OBJECTS:nx_gd32f470_lowpower_binding>)
    set(NEXUS_SYSTEM_COMPILE_DEFINITIONS GD32F470 HXTAL_VALUE=25000000
        NEXUS_TIMEBASE_LOWPOWER=1 PARENT_SCOPE)
endif()
