# Optional calibrated current observations; no hardware or OS dependency.
# Author: Nexus Team
add_library(nexus_current_control STATIC EXCLUDE_FROM_ALL src/current.c)
nexus_apply_target_policy(nexus_current_control)
target_include_directories(
    nexus_current_control
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_current_control
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
if(NOT MSVC)
    target_link_libraries(nexus_current_control PRIVATE m)
endif()
add_library(Nexus::CurrentControl ALIAS nexus_current_control)
