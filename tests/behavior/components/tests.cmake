# One directory scope preserves each reviewed private model context.
# Author: Nexus Team
foreach(
    nexus_behavior_group
    algorithms
    control
    protocols
    devices
    ui
    runtime
    services
    buses
)
    set(nexus_behavior_domain "components;${nexus_behavior_group}")
    include("${CMAKE_CURRENT_LIST_DIR}/${nexus_behavior_group}/tests.cmake")
endforeach()
