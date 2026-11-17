# Caller-owned bounded key/value snapshot over an explicit store.
# Author: Nexus Team
add_library(nexus_settings STATIC src/settings.c)
target_include_directories(
    nexus_settings
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_settings
    PUBLIC nexus_storage
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(Nexus::Settings ALIAS nexus_settings)
