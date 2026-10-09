if(NOT CONFIG_GD32F470ZG OR NOT (CONFIG_BOARD_GD32F470_LIANGSHAN OR CONFIG_BOARD_EXTERNAL))
    message(FATAL_ERROR "GD32F470 requires the maintained GD32F470ZGT6 Liangshan board binding")
endif()
if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR NOT CONFIG_CPU_ARCH STREQUAL "cortex-m4")
    message(FATAL_ERROR "GD32F470 requires ARM GCC and Cortex-M4")
endif()
if(NOT CONFIG_GD32_SYSCLK_FREQ EQUAL 200000000 OR NOT CONFIG_GD32_HXTAL_VALUE EQUAL 25000000)
    message(FATAL_ERROR "Liangshan clock binding requires 25 MHz HXTAL and the maintained 200 MHz profile")
endif()
add_subdirectory("${NEXUS_SOURCE_DIR}/soc/gd32f470" "${NEXUS_BINARY_DIR}/soc/gd32f470")
set(component_objects ${NEXUS_SOC_COMPONENT_OBJECTS})
nexus_add_selected_board()
set(startup_source "${NEXUS_SOC_STARTUP_SOURCE}")
set(linker_script "${NEXUS_GENERATED_LINKER_SCRIPT}")

add_library(platform_gd32f470 OBJECT src/platform.c "${startup_source}")
include("${NEXUS_SOURCE_DIR}/cmake/modules/NexusComponentObjects.cmake")
nexus_forward_component_objects(platform_gd32f470 ${component_objects})
target_include_directories(platform_gd32f470 PRIVATE "${NEXUS_SOURCE_DIR}/soc/gd32f470/include")
target_link_libraries(platform_gd32f470 PRIVATE
    sdk_gd32f4xx Nexus::HALProviders hal_support osal nexus_arch "${NEXUS_BOARD_TARGET}")
target_link_libraries(platform_gd32f470 INTERFACE hal osal nexus_arch
    soc_gd32f470_interface "${NEXUS_BOARD_TARGET}" "$<LINK_ONLY:sdk_gd32f4xx>")
set_target_properties(platform_gd32f470 PROPERTIES
    NEXUS_STARTUP_SOURCE "${startup_source}"
    NEXUS_LINKER_SCRIPT "${linker_script}"
    NEXUS_SDK_TARGET sdk_gd32f4xx)
target_link_options(platform_gd32f470 INTERFACE "-T${linker_script}")
set_property(TARGET platform_gd32f470 APPEND PROPERTY INTERFACE_LINK_DEPENDS "${linker_script}"
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/linker/gd32f470_sections.ld")
