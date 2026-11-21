# Include after the maintained real-kernel coordinator target is created.
# Author: Nexus Team
if(NOT TARGET nexus_freertos_services_kernel_test)
    message(
        FATAL_ERROR
        "Common diagnostics require the real FreeRTOS test coordinator"
    )
endif()
target_sources(
    nexus_freertos_services_kernel_test
    PRIVATE
        "${nexus_google_root}/components/common_health_freertos_test.cpp"
        "${NEXUS_SOURCE_DIR}/components/services/health/src/health_freertos.c"
)
target_link_libraries(nexus_freertos_services_kernel_test PRIVATE Nexus::Health)
