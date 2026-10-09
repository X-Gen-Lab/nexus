# Native is a virtual host resource/controller model, not physical silicon.
file(GLOB_RECURSE native_model_sources CONFIGURE_DEPENDS "controllers/*.c" "resources/*.c")
add_library(soc_native OBJECT ${native_model_sources})
target_include_directories(soc_native PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_link_libraries(soc_native PRIVATE hal osal Nexus::HALProviders Nexus::Arch)
set(NEXUS_SOC_COMPONENT_OBJECTS soc_native PARENT_SCOPE)
