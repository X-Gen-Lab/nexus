# Explicit caller-owned protocol mechanism; no automatic execution.
# Author: Nexus Team
add_library(nexus_framing STATIC EXCLUDE_FROM_ALL src/framing.c)
target_include_directories(
    nexus_framing
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_framing
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::Framing ALIAS nexus_framing)
nexus_apply_target_policy(nexus_framing)
