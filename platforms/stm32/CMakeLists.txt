foreach(peripheral I2C TIMER ADC DAC CAN USB ETHERNET SDIO)
    if(CONFIG_STM32_${peripheral}_ENABLE)
        message(FATAL_ERROR "Nexus STM32 ${peripheral} driver has no maintained implementation")
    endif()
endforeach()
if(NOT CONFIG_STM32F407 OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "The maintained STM32 reference build is STM32F407 with ARM GCC")
endif()
if(NOT CONFIG_CPU_ARCH STREQUAL "cortex-m4")
    message(FATAL_ERROR "STM32F407 requires Cortex-M4 architecture")
endif()

set(cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
set(device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
set(vendor "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4xx_hal_driver")
set(startup_source "${device}/Source/Templates/gcc/startup_stm32f407xx.s")
set(linker_script "${NEXUS_GENERATED_LINKER_SCRIPT}")
foreach(required "${cmsis}/core_cm4.h" "${device}/Include/stm32f407xx.h" "${vendor}/Src/stm32f4xx_hal.c" "${startup_source}" "${linker_script}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing pinned submodule file ${required}; initialize dependencies")
    endif()
endforeach()

# SDK types belong to implementation targets. Direct SDK applications opt in
# through Nexus::STM32SDK; the platform does not export these headers.
add_library(sdk_stm32f4_headers INTERFACE)
target_include_directories(sdk_stm32f4_headers INTERFACE
    "${cmsis}" "${device}/Include" "${vendor}/Inc" "${vendor}/Inc/Legacy"
    "${CMAKE_CURRENT_SOURCE_DIR}/config")
target_compile_definitions(sdk_stm32f4_headers INTERFACE
    STM32F407xx STM32F4 USE_HAL_DRIVER HSE_VALUE=${CONFIG_STM32_HSE_VALUE})
target_link_libraries(sdk_stm32f4_headers INTERFACE nexus_build_options)
file(GLOB vendor_sources CONFIGURE_DEPENDS "${vendor}/Src/*.c")
list(FILTER vendor_sources EXCLUDE REGEX "_template\\.c$")
add_library(sdk_stm32f4 STATIC ${vendor_sources})
add_library(Nexus::STM32SDK ALIAS sdk_stm32f4)
target_link_libraries(sdk_stm32f4 PUBLIC sdk_stm32f4_headers)
target_compile_options(sdk_stm32f4 PRIVATE -Wno-unused-parameter)

add_subdirectory("${NEXUS_SOURCE_DIR}/soc/stm32f407vg" "${NEXUS_BINARY_DIR}/soc/stm32f407vg")
set(component_objects soc_stm32f407)
foreach(peripheral gpio uart spi)
    string(TOUPPER "${peripheral}" symbol)
    if(CONFIG_STM32_${symbol}_ENABLE)
        file(GLOB driver_sources CONFIGURE_DEPENDS "src/hal/${peripheral}/*.c")
        set(driver_target "controller_stm32_${peripheral}")
        add_library(${driver_target} OBJECT ${driver_sources})
        target_include_directories(${driver_target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/private")
        target_link_libraries(${driver_target} PRIVATE sdk_stm32f4 Nexus::HALProviders hal_support osal nexus_arch)
        list(APPEND component_objects ${driver_target})
    endif()
endforeach()
nexus_add_selected_board()
set(board_target "${NEXUS_BOARD_TARGET}")

# Sole firmware assembly: startup is a direct source for application validation.
# Explicit object injection retains registration and strong HAL callbacks.
# KEEP cannot extract unreferenced registration members from static archives.
add_library(platform_stm32 OBJECT
    src/boot/stm32_platform_init.c src/platform/nx_platform_assert.c
    "${startup_source}")
# Listing component objects in this OBJECT target's SOURCES would not forward
# them to consumers. Keep their private compile usage on each component.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/NexusComponentObjects.cmake")
nexus_stm32_forward_components(platform_stm32 ${component_objects})
target_include_directories(platform_stm32 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(platform_stm32 PRIVATE sdk_stm32f4 Nexus::HALProviders hal_support osal nexus_arch)
target_link_libraries(platform_stm32 INTERFACE
    hal osal soc_stm32f407_interface "${board_target}"
    "$<LINK_ONLY:sdk_stm32f4>")
set_target_properties(platform_stm32 PROPERTIES
    NEXUS_STARTUP_SOURCE "${startup_source}"
    NEXUS_LINKER_SCRIPT "${linker_script}"
    NEXUS_SDK_TARGET sdk_stm32f4
    NEXUS_COMPONENT_OBJECT_TARGETS "${component_objects}")
set(linker_directory "${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f4/gcc")
target_link_options(platform_stm32 INTERFACE "-L${linker_directory}" "-T${linker_script}")
set_property(TARGET platform_stm32 APPEND PROPERTY INTERFACE_LINK_DEPENDS
    "${linker_script}" "${linker_directory}/stm32f407_sections.ld")
