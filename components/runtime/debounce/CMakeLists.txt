add_library(nexus_debounce STATIC src/debounce.c)
target_include_directories(
    nexus_debounce
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_debounce
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::Debounce ALIAS nexus_debounce)
nexus_apply_target_policy(nexus_debounce)
