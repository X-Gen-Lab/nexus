# Explicit bounded mechanism; applications own execution and storage.
# Author: Nexus Team
add_library(nexus_periodic STATIC EXCLUDE_FROM_ALL src/periodic.c)
nexus_apply_target_policy(nexus_periodic)
target_include_directories(
    nexus_periodic
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_periodic
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::Periodic ALIAS nexus_periodic)
