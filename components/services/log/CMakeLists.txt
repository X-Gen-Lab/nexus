# Instance-owned synchronous log port; no default sink or worker.
# Author: Nexus Team
add_library(nexus_log STATIC src/log.c)
target_include_directories(nexus_log PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_log PUBLIC nexus_core
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options)
add_library(Nexus::Log ALIAS nexus_log)
