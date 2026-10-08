# Public silicon APIs contain no SDK types. A product explicitly binds the
# reserved Flash port to its storage service.
add_library(soc_stm32f407_interface INTERFACE)
target_include_directories(soc_stm32f407_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
if(CONFIG_SERVICE_STORAGE)
    target_link_libraries(soc_stm32f407_interface INTERFACE nexus_storage)
endif()

set(stm32_platform_dir "${NEXUS_SOURCE_DIR}/platforms/stm32")
set(stm32_device_dir "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
set(soc_storage_sources)
if(CONFIG_SERVICE_STORAGE)
    list(APPEND soc_storage_sources flash.c)
endif()
add_library(soc_stm32f407 OBJECT
    ${soc_storage_sources} identity.c stm32_uart_clock.c
    "${stm32_platform_dir}/src/clock/stm32_clock_common.c"
    "${stm32_platform_dir}/src/clock/stm32f4_clock.c"
    "${stm32_platform_dir}/src/interrupt/stm32_interrupt.c"
    "${stm32_platform_dir}/src/interrupt/stm32_isr_manager.c"
    "${stm32_platform_dir}/src/interrupt/stm32f4_it.c"
    "${stm32_platform_dir}/src/system/stm32_error.c"
    "${stm32_platform_dir}/src/system/stm32_error_handler.c"
    "${stm32_platform_dir}/src/system/stm32_chip_validation.c"
    "${stm32_platform_dir}/src/system/stm32_performance.c"
    "${stm32_platform_dir}/src/system/stm32_profiler.c"
    "${stm32_device_dir}/Source/Templates/system_stm32f4xx.c")
target_include_directories(soc_stm32f407 PRIVATE
    "${stm32_platform_dir}/include" "${stm32_platform_dir}/private"
    "${stm32_platform_dir}/src/hal/uart")
target_link_libraries(soc_stm32f407 PRIVATE
    soc_stm32f407_interface sdk_stm32f4 hal osal nexus_arch)
# The silicon SysTick dispatcher integrates with the selected scheduler. OSAL's
# public contract deliberately does not expose the kernel's implementation API.
if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
    target_link_libraries(soc_stm32f407 PRIVATE freertos_kernel freertos_config)
endif()
