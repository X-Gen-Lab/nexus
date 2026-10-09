# The STM32F407 family implements VE/VG/ZG density through effective config.
# Silicon/controller compilation owns SDK types; public geometry has Nexus types.
add_library(soc_stm32f407_interface INTERFACE)
target_include_directories(soc_stm32f407_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(soc_stm32f407_interface INTERFACE hal_interface)
add_subdirectory(sdk)

set(stm32_device_dir "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
add_library(soc_stm32f407 OBJECT
    flash.c identity.c stm32_uart_clock.c
    clock/stm32_clock_common.c clock/stm32f4_clock.c
    interrupt/stm32_interrupt.c interrupt/stm32_isr_manager.c interrupt/stm32f4_it.c
    system/stm32_error.c system/stm32_error_handler.c system/stm32_chip_validation.c
    system/stm32_performance.c system/stm32_profiler.c
    "${stm32_device_dir}/Source/Templates/system_stm32f4xx.c")
target_include_directories(soc_stm32f407 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private" "${CMAKE_CURRENT_SOURCE_DIR}/controllers/uart")
target_link_libraries(soc_stm32f407 PRIVATE
    soc_stm32f407_interface sdk_stm32f4 Nexus::HALProviders hal_support osal nexus_arch)
if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
    target_link_libraries(soc_stm32f407 PRIVATE freertos_kernel freertos_config)
endif()

set(component_objects soc_stm32f407)
foreach(controller gpio uart spi)
    string(TOUPPER "${controller}" symbol)
    if(CONFIG_STM32_${symbol}_ENABLE)
        file(GLOB controller_sources CONFIGURE_DEPENDS "controllers/${controller}/*.c")
        set(controller_target "controller_stm32_${controller}")
        add_library(${controller_target} OBJECT ${controller_sources})
        target_include_directories(${controller_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
        target_link_libraries(${controller_target} PRIVATE
            sdk_stm32f4 Nexus::HALProviders hal_support osal nexus_arch)
        list(APPEND component_objects ${controller_target})
    endif()
endforeach()
set(NEXUS_SOC_COMPONENT_OBJECTS "${component_objects}" PARENT_SCOPE)
set(NEXUS_SOC_STARTUP_SOURCE "${NEXUS_SOC_STARTUP_SOURCE}" PARENT_SCOPE)
