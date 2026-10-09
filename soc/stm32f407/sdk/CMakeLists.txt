set(cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
set(device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
set(vendor "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4xx_hal_driver")
set(startup_source "${device}/Source/Templates/gcc/startup_stm32f407xx.s")
foreach(required "${cmsis}/core_cm4.h" "${device}/Include/stm32f407xx.h" "${vendor}/Src/stm32f4xx_hal.c" "${startup_source}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing pinned submodule file ${required}; initialize dependencies")
    endif()
endforeach()

# SDK types belong to implementation targets. Direct SDK applications opt in
# through Nexus::STM32SDK; the platform does not export these headers.
add_library(sdk_stm32f4_headers INTERFACE)
target_include_directories(sdk_stm32f4_headers INTERFACE
    "${cmsis}" "${device}/Include" "${vendor}/Inc" "${vendor}/Inc/Legacy"
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(sdk_stm32f4_headers INTERFACE
    STM32F407xx STM32F4 USE_HAL_DRIVER HSE_VALUE=${CONFIG_STM32_HSE_VALUE})
target_link_libraries(sdk_stm32f4_headers INTERFACE nexus_build_options)
# Keep this list explicit. Peripheral support requires a provider, reviewed
# Board resources and validation; copying an SDK source must not enable it.
set(vendor_sources "${vendor}/Src/stm32f4xx_hal.c")
foreach(module cortex rcc pwr flash flash_ex gpio)
    list(APPEND vendor_sources "${vendor}/Src/stm32f4xx_hal_${module}.c")
endforeach()
# Vendor UART/SPI object code references HAL DMA even when Nexus exposes only
# interrupt/polling operations. This dependency is not a UART DMA capability.
if(CONFIG_STM32_UART_ENABLE OR CONFIG_STM32_SPI_ENABLE)
    list(APPEND vendor_sources "${vendor}/Src/stm32f4xx_hal_dma.c")
endif()
foreach(controller uart spi)
    string(TOUPPER "${controller}" symbol)
    if(CONFIG_STM32_${symbol}_ENABLE)
        list(APPEND vendor_sources "${vendor}/Src/stm32f4xx_hal_${controller}.c")
    endif()
endforeach()
add_library(sdk_stm32f4 STATIC ${vendor_sources})
add_library(Nexus::STM32SDK ALIAS sdk_stm32f4)
target_link_libraries(sdk_stm32f4 PUBLIC sdk_stm32f4_headers)
target_compile_options(sdk_stm32f4 PRIVATE -Wno-unused-parameter)


set(NEXUS_SOC_STARTUP_SOURCE "${startup_source}" PARENT_SCOPE)
