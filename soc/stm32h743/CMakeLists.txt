# Exact H743 software reference; official SDK stays private.
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
    "${h7_device}/Source/Templates/gcc/startup_stm32h743xx.s"
    "${h7_device}/Source/Templates/system_stm32h7xx.c")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
set(h7_sources)
nexus_controller_modes(h7_gpio_modes gpio)
nexus_controller_modes(h7_uart_modes uart)
nexus_controller_modes(h7_spi_modes spi)
nexus_controller_modes(h7_pwm_modes pwm)
nexus_controller_modes(h7_exti_modes exti)
nexus_controller_modes(h7_watchdog_modes watchdog)
nexus_controller_modes(h7_i2c_modes i2c)
nexus_controller_modes(h7_adc_modes adc)
nexus_controller_modes(h7_flash_modes flash)
if(h7_gpio_modes OR h7_uart_modes OR h7_spi_modes OR h7_pwm_modes OR h7_exti_modes OR h7_i2c_modes OR h7_adc_modes)
    list(APPEND h7_sources drivers/gpio.c)
endif()
if(h7_uart_modes)
    list(APPEND h7_sources drivers/uart.c)
    if("irq-fifo-bytes" IN_LIST h7_uart_modes)
        list(APPEND h7_sources drivers/uart_fifo.c)
    endif()
    if("irq-fifo-rtscts-bytes" IN_LIST h7_uart_modes)
        list(APPEND h7_sources drivers/uart_rtscts.c)
    endif()
    if("dma-tx" IN_LIST h7_uart_modes)
        list(APPEND h7_sources drivers/uart_dma.c)
    endif()
    if("rx-dma-idle" IN_LIST h7_uart_modes)
        list(APPEND h7_sources drivers/uart_receive.c)
    endif()
endif()
foreach(kind spi pwm exti watchdog adc flash i2c)
    if(h7_${kind}_modes)
        list(APPEND h7_sources drivers/${kind}.c)
    endif()
endforeach()
if(h7_i2c_modes)
    list(APPEND h7_sources "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c")
endif()
if(h7_flash_modes)
    set(h7_flash_ram_sources
        drivers/flash_ram.c drivers/flash_ram_owner.c
        drivers/flash_ram_transaction.c drivers/flash_mpu.c)
    list(APPEND h7_sources drivers/flash_execution.c drivers/flash_ram_enter.S
        ${h7_flash_ram_sources})
    # Busy-bank execution cannot gain implicit FP, stack-protector, libc or
    # LTO-generated calls. Actual linked instruction closure is checked too.
    set_source_files_properties(${h7_flash_ram_sources} PROPERTIES
        COMPILE_OPTIONS "-mgeneral-regs-only;-fno-lto;-fno-builtin;-fno-stack-protector;-fno-jump-tables;-fno-tree-loop-distribute-patterns;-fno-unwind-tables;-fno-asynchronous-unwind-tables;-fstack-usage")
    set(h7_flash_su_files)
    foreach(source IN LISTS h7_flash_ram_sources)
        set(stack_file "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/nexus_soc_stm32h743.dir/${source}.su")
        set_property(SOURCE "${source}" APPEND PROPERTY OBJECT_OUTPUTS "${stack_file}")
        list(APPEND h7_flash_su_files "${stack_file}")
    endforeach()
    set(h7_stack_witness "${CMAKE_CURRENT_BINARY_DIR}/flash-stack-outputs.stamp")
    add_custom_target(nexus_h743_flash_stack_outputs
        COMMAND "${CMAKE_COMMAND}" "-DNEXUS_STACK_WITNESS=${h7_stack_witness}"
            "-DNEXUS_STACK_FILES=${h7_flash_su_files}"
            -P "${NEXUS_SOURCE_DIR}/cmake/platform/H743StackUsage.cmake"
        BYPRODUCTS "${h7_stack_witness}"
        VERBATIM)
    set_property(SOURCE ${h7_flash_ram_sources} APPEND PROPERTY
        OBJECT_DEPENDS "${h7_stack_witness}")
endif()
if(h7_sources)
    add_library(nexus_soc_stm32h743 STATIC ${h7_sources})
    target_link_libraries(nexus_soc_stm32h743 PUBLIC nexus_io
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_stm32h743 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private"
        "${NEXUS_SOURCE_DIR}/soc/common"
        "${NEXUS_SOURCE_DIR}/io/private")
    target_include_directories(nexus_soc_stm32h743 SYSTEM PRIVATE
        "${h7_device}/Include" "${h7_cmsis}")
    target_compile_definitions(nexus_soc_stm32h743 PRIVATE STM32H743xx)
    if(h7_flash_modes)
        add_dependencies(nexus_soc_stm32h743 nexus_h743_flash_stack_outputs)
        set_property(TARGET nexus_soc_stm32h743 PROPERTY
            NEXUS_H743_FLASH_SU_FILES "${h7_flash_su_files}")
    endif()
    if(h7_spi_modes OR h7_adc_modes OR "irq-fifo-bytes" IN_LIST h7_uart_modes OR
            "irq-fifo-rtscts-bytes" IN_LIST h7_uart_modes)
        include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
        nexus_stm32_official(nexus_soc_stm32h743 h7)
    endif()
else()
    add_library(nexus_soc_stm32h743 INTERFACE)
    target_link_libraries(nexus_soc_stm32h743 INTERFACE nexus_io)
endif()
set(NEXUS_STARTUP_SOURCE
    "${h7_device}/Source/Templates/gcc/startup_stm32h743xx.s" PARENT_SCOPE)
set(h7_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/system.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/power_boot.S")
if(NEXUS_MEMORY_SRAM1)
    list(APPEND h7_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/memory_init.c")
endif()
file(READ "${NEXUS_CONFIG_DIR}/resolved.json" h7_resolved)
string(JSON h7_clock_profile GET "${h7_resolved}" clock_profile)
if(h7_clock_profile STREQUAL "hsi64-pll384-cache-off")
    list(APPEND h7_system_sources "${CMAKE_CURRENT_SOURCE_DIR}/clock.c")
endif()
set(NEXUS_SYSTEM_SOURCE ${h7_system_sources} PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${h7_device}/Include" "${h7_cmsis}"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS STM32H743xx PARENT_SCOPE)
