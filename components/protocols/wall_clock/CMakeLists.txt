# Explicit caller-owned protocol mechanism; no automatic execution.
# Author: Nexus Team
add_library(nexus_wall_clock STATIC EXCLUDE_FROM_ALL src/wall_clock.c)
target_include_directories(
    nexus_wall_clock
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_wall_clock
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::WallClock ALIAS nexus_wall_clock)
nexus_apply_target_policy(nexus_wall_clock)
