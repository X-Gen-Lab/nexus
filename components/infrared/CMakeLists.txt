# Optional allocation-free NEC software reference; no source/worker ownership.
# Author: Nexus Team
add_library(nexus_infrared STATIC EXCLUDE_FROM_ALL src/infrared.c)
nexus_apply_target_policy(nexus_infrared)
target_include_directories(nexus_infrared PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_infrared PUBLIC Nexus::IOEXTI
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::Infrared ALIAS nexus_infrared)
