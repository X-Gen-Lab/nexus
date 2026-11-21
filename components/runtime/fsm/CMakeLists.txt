add_library(nexus_fsm STATIC src/fsm.c)
target_include_directories(
    nexus_fsm
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_fsm
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::FSM ALIAS nexus_fsm)
nexus_apply_target_policy(nexus_fsm)
