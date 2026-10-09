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

add_subdirectory("${NEXUS_SOURCE_DIR}/soc/stm32f407" "${NEXUS_BINARY_DIR}/soc/stm32f407")
set(component_objects ${NEXUS_SOC_COMPONENT_OBJECTS})
nexus_add_selected_board()
set(startup_source "${NEXUS_SOC_STARTUP_SOURCE}")
set(linker_script "${NEXUS_GENERATED_LINKER_SCRIPT}")

# The platform is lifecycle/startup glue. SoC and Board retain their own objects.
add_library(platform_stm32 OBJECT
    src/boot/stm32_platform_init.c src/platform/nx_platform_assert.c "${startup_source}")
include("${NEXUS_SOURCE_DIR}/cmake/modules/NexusComponentObjects.cmake")
nexus_forward_component_objects(platform_stm32 ${component_objects})
target_include_directories(platform_stm32 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_link_libraries(platform_stm32 PRIVATE sdk_stm32f4 Nexus::HALProviders hal_support osal nexus_arch)
target_link_libraries(platform_stm32 INTERFACE
    hal osal soc_stm32f407_interface "${NEXUS_BOARD_TARGET}" "$<LINK_ONLY:sdk_stm32f4>")
set_target_properties(platform_stm32 PROPERTIES
    NEXUS_STARTUP_SOURCE "${startup_source}"
    NEXUS_LINKER_SCRIPT "${linker_script}"
    NEXUS_SDK_TARGET sdk_stm32f4)
set(linker_directory "${NEXUS_SOURCE_DIR}/soc/stm32f407/linker")
target_link_options(platform_stm32 INTERFACE "-L${linker_directory}" "-T${linker_script}")
set_property(TARGET platform_stm32 APPEND PROPERTY INTERFACE_LINK_DEPENDS
    "${linker_script}" "${linker_directory}/stm32f407_sections.ld")
