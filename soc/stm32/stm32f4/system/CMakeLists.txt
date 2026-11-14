# System lifecycle objects live in the SoC archive. Reset/clock/runtime code
# that must precede ordinary archives is exported separately to Firmware.cmake.
target_sources(nexus_soc_stm32f4 PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/reset.c"
    "${CMAKE_CURRENT_LIST_DIR}/supervision.c"
    "${CMAKE_CURRENT_LIST_DIR}/platform_coordinator.c"
    "${CMAKE_CURRENT_LIST_DIR}/platform_lifecycle.c"
    "${CMAKE_CURRENT_LIST_DIR}/system_lifecycle.c"
    "${CMAKE_CURRENT_LIST_DIR}/lifecycle_pins.c"
    "${CMAKE_CURRENT_LIST_DIR}/platform_cold.c")
set(NEXUS_STARTUP_SOURCE
    "${stm32_device}/Source/Templates/gcc/${NEXUS_STM32_STARTUP}" PARENT_SCOPE)
set(stm32_system_includes
    "${stm32_package_dir}" "${stm32_device}/Include"
    "${stm32_cmsis}" ${stm32_ip_includes})
if(stm32_uart_hal)
    list(APPEND stm32_system_includes
        "${stm32_package_dir}/sdk/hal_config/uart"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Inc")
endif()
if(stm32_usb_host_hal OR stm32_sai_hal
        OR "qspi-indirect-sdr" IN_LIST stm32_external_bus_modes
        OR "qspi-indirect-dma" IN_LIST stm32_external_bus_modes
        OR "cipher" IN_LIST NEXUS_SELECTED_KINDS
        OR "hash" IN_LIST NEXUS_SELECTED_KINDS
        OR "display" IN_LIST NEXUS_SELECTED_KINDS
        OR "image" IN_LIST NEXUS_SELECTED_KINDS)
    list(APPEND stm32_system_includes
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Inc")
endif()
set(NEXUS_SYSTEM_PRIVATE_INCLUDES ${stm32_system_includes} PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS ${NEXUS_STM32_DEVICE} PARENT_SCOPE)
set(stm32_system_sources "${stm32_package_dir}/system/clock.c"
    "${stm32_package_dir}/system/startup.c"
    "${stm32_package_dir}/system/runtime.c")
if(NEXUS_MEMORY_CCM OR NEXUS_MEMORY_ADDSRAM)
    list(APPEND stm32_system_sources "${stm32_package_dir}/system/memory.c")
endif()
if(NEXUS_MEMORY_ADDSRAM)
    if(NOT stm32_external_bus_modes)
        message(FATAL_ERROR "F407 startup SRAM requires its selected FSMC owner")
    endif()
    target_sources(nexus_soc_stm32f4 PRIVATE "${stm32_package_dir}/drivers/memory/fsmc_boot.c")
    list(APPEND stm32_system_sources "${stm32_package_dir}/system/sram_boot.c")
    set(NEXUS_SYSTEM_VENDOR_INCLUDES
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Inc" PARENT_SCOPE)
endif()
set(NEXUS_SYSTEM_SOURCE ${stm32_system_sources} PARENT_SCOPE)

# Fixed clock profile sleep is an explicitly selected caller-owned component.
function(nexus_add_stm32f4_sleep target)
    if(NOT ARGC EQUAL 1 OR TARGET "${target}")
        message(FATAL_ERROR "Sleep target requires one unused target name")
    endif()
    if(NOT TARGET Nexus::Platform OR NOT TARGET nexus_soc_stm32f4)
        message(FATAL_ERROR "Sleep requires a selected F4 Platform")
    endif()
    get_target_property(sleep_family Nexus::Platform NEXUS_SOC_FAMILY)
    get_target_property(sleep_device nexus_soc_stm32f4 NEXUS_STM32_DEVICE)
    set(sleep_devices STM32F405xx STM32F407xx STM32F415xx STM32F417xx
        STM32F411xE STM32F446xx STM32F479xx)
    if(NOT sleep_family STREQUAL "stm32f4" OR
            NOT sleep_device IN_LIST sleep_devices)
        message(FATAL_ERROR
            "Sleep requires a selected F4 Platform with a maintained exact device")
    endif()
    get_target_property(sleep_source Nexus::Platform NEXUS_SOURCE_DIR)
    if(NOT IS_ABSOLUTE "${sleep_source}")
        message(FATAL_ERROR "Sleep requires explicit Platform source metadata")
    endif()
    get_filename_component(sleep_package
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_library(${target} STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sleep.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/power.c")
    nexus_apply_target_policy(${target})
    target_link_libraries(${target} PUBLIC Nexus::ClockDomain Nexus::IORTC
        PRIVATE Nexus::Arch
        nexus_build_options nexus_owned_warnings)
    target_include_directories(${target} PRIVATE
        "${sleep_package}"
        "${sleep_source}/components/clock_domain/private")
    target_include_directories(${target} PUBLIC
        "${sleep_package}/include")
    target_include_directories(${target} SYSTEM PRIVATE
        "${sleep_source}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE ${sleep_device})
endfunction()

# The explicitly selected getter and SysTick entry must precede weak archives.
if(NEXUS_OS_TIMEBASE_LOWPOWER)
    add_library(nx_stm32f407_lowpower_binding OBJECT "${CMAKE_CURRENT_LIST_DIR}/timebase_lowpower.c")
    target_link_libraries(nx_stm32f407_lowpower_binding PRIVATE
        Nexus::OSFreeRTOS Nexus::Arch nexus_build_options nexus_owned_warnings)
    target_include_directories(nx_stm32f407_lowpower_binding PRIVATE
        "${stm32_package_dir}"
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
