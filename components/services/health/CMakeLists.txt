# Copied observations and optional ordinary event draining, without workers.
# Author: Nexus Team
add_library(
    nexus_health
    STATIC
    EXCLUDE_FROM_ALL
    src/health.c
    src/health_resources.c
)
target_include_directories(
    nexus_health
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_health
    PUBLIC nexus_io nexus_os_diagnostic
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)
add_library(Nexus::Health ALIAS nexus_health)
nexus_apply_target_policy(nexus_health)

add_library(nexus_diagnostic_log STATIC EXCLUDE_FROM_ALL src/diagnostic_log.c)
target_link_libraries(
    nexus_diagnostic_log
    PUBLIC nexus_health nexus_log_formatter nexus_os_diagnostic
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)
add_library(Nexus::DiagnosticLog ALIAS nexus_diagnostic_log)
nexus_apply_target_policy(nexus_diagnostic_log)

if(NEXUS_BACKEND STREQUAL "freertos" AND NEXUS_OS_TRACE)
    add_library(
        nexus_health_freertos
        STATIC
        EXCLUDE_FROM_ALL
        src/health_freertos.c
    )
    target_link_libraries(
        nexus_health_freertos
        PUBLIC nexus_health nexus_os_freertos
        PRIVATE nexus_owned_warnings nexus_build_options
    )
    add_library(Nexus::HealthFreeRTOS ALIAS nexus_health_freertos)
    nexus_apply_target_policy(nexus_health_freertos)
endif()
