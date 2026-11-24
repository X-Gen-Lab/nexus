# Explicit rotary-axis computation; applications own execution and hardware.
# Author: Nexus Team
add_library(nexus_axis_control STATIC EXCLUDE_FROM_ALL src/axis.c)
nexus_apply_target_policy(nexus_axis_control)
target_include_directories(
    nexus_axis_control
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_axis_control
    PUBLIC Nexus::Core Nexus::Algorithms
    PRIVATE nexus_build_options nexus_owned_warnings
)
if(NOT MSVC)
    target_link_libraries(nexus_axis_control PRIVATE m)
endif()
add_library(Nexus::AxisControl ALIAS nexus_axis_control)
