# Explicit physical-region transactions; product layout remains external.
# Author: Nexus Team
add_library(nexus_fal STATIC EXCLUDE_FROM_ALL src/fal.c)
nexus_apply_target_policy(nexus_fal)
target_include_directories(
    nexus_fal
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_fal
    PUBLIC Nexus::IOFlash
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::FAL ALIAS nexus_fal)
