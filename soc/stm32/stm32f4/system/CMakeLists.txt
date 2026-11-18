# System lifecycle objects live in the SoC archive. Reset/clock/runtime code
# that must precede ordinary archives is exported separately to Firmware.cmake.
include("${CMAKE_CURRENT_LIST_DIR}/Handoff.cmake")
target_sources(
    nexus_soc_stm32f4
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/reset.c"
        "${CMAKE_CURRENT_LIST_DIR}/supervision.c"
        "${CMAKE_CURRENT_LIST_DIR}/platform_coordinator.c"
        "${CMAKE_CURRENT_LIST_DIR}/platform_lifecycle.c"
        "${CMAKE_CURRENT_LIST_DIR}/system_lifecycle.c"
        "${CMAKE_CURRENT_LIST_DIR}/lifecycle_pins.c"
        "${CMAKE_CURRENT_LIST_DIR}/platform_cold.c"
)
set(stm32_system_sources
    "${stm32_package_dir}/system/clock.c"
    "${stm32_package_dir}/system/startup.c"
    "${stm32_package_dir}/system/runtime.c"
)
if(NEXUS_MEMORY_CCM OR NEXUS_MEMORY_ADDSRAM)
    list(APPEND stm32_system_sources "${stm32_package_dir}/system/memory.c")
endif()
if(NEXUS_MEMORY_ADDSRAM)
    nexus_controller_modes(startup_memory_modes external_bus)
    if(NOT startup_memory_modes)
        message(
            FATAL_ERROR
            "F407 startup SRAM requires its selected FSMC owner"
        )
    endif()
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${stm32_package_dir}/drivers/memory/fsmc_boot.c"
    )
    list(APPEND stm32_system_sources "${stm32_package_dir}/system/sram_boot.c")
endif()
set_property(
    TARGET nexus_soc_stm32f4
    PROPERTY
        NEXUS_STARTUP_SOURCES
            "${stm32_device}/${NEXUS_VENDOR_DEVICE_STARTUP}"
            ${stm32_system_sources}
)

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
    set(sleep_devices
        STM32F405xx
        STM32F407xx
        STM32F415xx
        STM32F417xx
        STM32F411xE
        STM32F446xx
        STM32F479xx
    )
    if(
        NOT sleep_family STREQUAL "stm32f4"
        OR NOT sleep_device IN_LIST sleep_devices
    )
        message(
            FATAL_ERROR
            "Sleep requires a selected F4 Platform with a maintained exact device"
        )
    endif()
    get_target_property(sleep_source Nexus::Platform NEXUS_SOURCE_DIR)
    if(NOT IS_ABSOLUTE "${sleep_source}")
        message(FATAL_ERROR "Sleep requires explicit Platform source metadata")
    endif()
    get_filename_component(
        sleep_package
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
        ABSOLUTE
    )
    add_library(
        ${target}
        STATIC
        EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sleep.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/power.c"
    )
    nexus_apply_target_policy(${target})
    target_link_libraries(
        ${target}
        PUBLIC Nexus::ClockDomain Nexus::IORTC
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings
    )
    target_include_directories(
        ${target}
        PRIVATE
            "${sleep_package}"
            "${sleep_source}/components/runtime/clock_domain/private"
    )
    target_include_directories(${target} PUBLIC "${sleep_package}/include")
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE "${sleep_source}/vendors/st/cmsis_device_f4/Include"
    )
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE ${sleep_device})
endfunction()

# The explicitly selected getter and SysTick entry must precede weak archives.
if(NEXUS_OS_TIMEBASE_LOWPOWER)
    add_library(
        nx_stm32f407_lowpower_binding
        OBJECT
        "${CMAKE_CURRENT_LIST_DIR}/timebase_lowpower.c"
    )
    target_link_libraries(
        nx_stm32f407_lowpower_binding
        PRIVATE
            Nexus::OSFreeRTOS
            Nexus::Arch
            nexus_build_options
            nexus_owned_warnings
    )
    target_include_directories(
        nx_stm32f407_lowpower_binding
        PRIVATE "${stm32_package_dir}" "${NEXUS_SOURCE_DIR}/os/freertos/private"
    )
    target_include_directories(
        nx_stm32f407_lowpower_binding
        SYSTEM
        PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    )
    nexus_use_cmsis_core(nx_stm32f407_lowpower_binding)
    target_compile_definitions(
        nx_stm32f407_lowpower_binding
        PRIVATE ${NEXUS_STM32_DEVICE}
    )
    target_sources(
        nexus_soc_stm32f4
        INTERFACE $<TARGET_OBJECTS:nx_stm32f407_lowpower_binding>
    )
    target_compile_definitions(
        nexus_stm32f4_startup_usage
        INTERFACE NEXUS_TIMEBASE_LOWPOWER=1
    )
endif()
