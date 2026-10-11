# Actual weak hooks, pinned FreeRTOS declarations and external port boundary.
nexus_google_test(nexus_freertos_failstop_test
    SOURCES freertos_failstop_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/hooks.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/os_freertos_runtime"
        "${NEXUS_SOURCE_DIR}/os/freertos/include"
        "${NEXUS_SOURCE_DIR}/os/include"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_freertos_failstop_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/ext/freertos/include"
    "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
