# Explicit bounded mechanism; applications own execution and storage.
# Author: Nexus Team
add_library(nexus_timers STATIC EXCLUDE_FROM_ALL src/timers.c)
nexus_apply_target_policy(nexus_timers)
target_include_directories(
    nexus_timers
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_timers
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::Timers ALIAS nexus_timers)
