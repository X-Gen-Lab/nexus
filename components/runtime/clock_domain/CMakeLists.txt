# Explicit optional clock admission; no default state or provider construction.
# Author: Nexus Team
add_library(nexus_clock_domain STATIC EXCLUDE_FROM_ALL src/clock_domain.c)
target_include_directories(nexus_clock_domain PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_link_libraries(nexus_clock_domain PUBLIC Nexus::Core
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::ClockDomain ALIAS nexus_clock_domain)
