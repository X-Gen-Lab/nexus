set(component_objects)
nexus_add_selected_board()
file(GLOB_RECURSE NATIVE_SOURCES CONFIGURE_DEPENDS "src/*.c")
# Registry support belongs to the init framework, not a platform stub.
list(FILTER NATIVE_SOURCES EXCLUDE REGEX "nx_init_sections_stub\\.c$")
# Object inclusion preserves static device registration without linker-wide flags.
add_library(platform_native OBJECT ${NATIVE_SOURCES})
target_include_directories(platform_native PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
target_link_libraries(platform_native PUBLIC hal osal "${NEXUS_BOARD_TARGET}")
foreach(component IN LISTS component_objects)
    target_link_libraries(platform_native INTERFACE "$<TARGET_OBJECTS:${component}>")
endforeach()
target_link_libraries(platform_native PRIVATE Nexus::HALProviders)
target_compile_definitions(platform_native PUBLIC NEXUS_PLATFORM_NATIVE)
