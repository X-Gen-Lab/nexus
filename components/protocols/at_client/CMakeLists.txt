# One explicit AT parser over an existing copying byte stream.
# Author: Nexus Team
add_library(nexus_at_client STATIC EXCLUDE_FROM_ALL src/at_client.c)
target_include_directories(
    nexus_at_client
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../network/include"
)
target_link_libraries(
    nexus_at_client
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::ATClient ALIAS nexus_at_client)
nexus_apply_target_policy(nexus_at_client)
