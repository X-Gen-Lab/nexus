# Explicit bounded mechanism; applications own execution and storage.
# Author: Nexus Team
add_library(nexus_event_fanout STATIC EXCLUDE_FROM_ALL src/event_fanout.c)
nexus_apply_target_policy(nexus_event_fanout)
target_include_directories(
    nexus_event_fanout
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_event_fanout
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::EventFanout ALIAS nexus_event_fanout)
