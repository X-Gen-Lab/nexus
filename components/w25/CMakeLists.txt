# Explicit single-line NOR protocol; no storage policy or bus is implicit.
add_library(nexus_w25 STATIC EXCLUDE_FROM_ALL src/w25.c)
nexus_apply_target_policy(nexus_w25)
target_include_directories(nexus_w25 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_w25 PUBLIC Nexus::IOSPI
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::W25 ALIAS nexus_w25)
