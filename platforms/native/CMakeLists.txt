add_subdirectory("${NEXUS_SOURCE_DIR}/soc/native" "${NEXUS_BINARY_DIR}/soc/native")
set(component_objects ${NEXUS_SOC_COMPONENT_OBJECTS})
nexus_add_selected_board()
# Host lifecycle is separate from virtual controllers and resource models.
add_library(platform_native OBJECT src/platform/nx_platform_init.c)
include("${NEXUS_SOURCE_DIR}/cmake/modules/NexusComponentObjects.cmake")
nexus_forward_component_objects(platform_native ${component_objects})
target_include_directories(platform_native PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/native/private" "${NEXUS_SOURCE_DIR}/soc/native/controllers")
target_link_libraries(platform_native PRIVATE hal osal Nexus::HALProviders "${NEXUS_BOARD_TARGET}")
target_link_libraries(platform_native INTERFACE hal osal "${NEXUS_BOARD_TARGET}")
target_compile_definitions(platform_native INTERFACE NEXUS_PLATFORM_NATIVE)
