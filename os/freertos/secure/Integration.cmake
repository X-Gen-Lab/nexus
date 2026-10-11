# Explicit Secure companion reuses the selected public Runtime CPU graph.
include_guard(GLOBAL)
function(nexus_enable_secure_context)
    if(ARGC OR NOT TARGET Nexus::Runtime)
        message(FATAL_ERROR
            "nexus_enable_secure_context requires an existing Runtime and no arguments")
    endif()
    if(TARGET Nexus::SecureContext)
        message(FATAL_ERROR "Secure context companion is already enabled")
    endif()
    get_target_property(NEXUS_SOURCE_DIR nexus_runtime NEXUS_SOURCE_DIR)
    get_target_property(NEXUS_CONFIG_DIR nexus_runtime NEXUS_CONFIG_DIR)
    get_target_property(runtime_backend nexus_runtime NEXUS_BACKEND)
    include("${NEXUS_CONFIG_DIR}/selection.cmake")
    if(NOT NEXUS_CPU_ARCH MATCHES "^cortex-m(23|33|55|85)$" OR
            NOT NEXUS_ARCH_SECURITY_STATE STREQUAL "1" OR
            NOT runtime_backend STREQUAL "baremetal")
        message(FATAL_ERROR
            "Secure companion requires explicit v8-M Secure baremetal CPU facts")
    endif()
    add_subdirectory("${NEXUS_SOURCE_DIR}/os/freertos/secure"
        "${CMAKE_CURRENT_BINARY_DIR}/nexus-secure-context")
endfunction()
