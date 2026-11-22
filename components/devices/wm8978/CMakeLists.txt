# Exact selected codec control; actual audio transport remains independently owned.
# Author: Nexus Team
add_library(nexus_wm8978 STATIC EXCLUDE_FROM_ALL src/wm8978.c)
nexus_apply_target_policy(nexus_wm8978)
target_include_directories(
    nexus_wm8978
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_wm8978
    PUBLIC Nexus::IOI2C
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(Nexus::WM8978 ALIAS nexus_wm8978)
