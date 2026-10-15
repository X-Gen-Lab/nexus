nexus_google_test(nexus_stack_observation_test
    SOURCES stack_observation_test.cpp LIBRARIES Nexus::Core)
nexus_google_test(nexus_freertos_stack_test SOURCES freertos_stack_test.cpp
    "${NEXUS_SOURCE_DIR}/os/freertos/stack_snapshot.c" LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/os_freertos_runtime"
        "${NEXUS_SOURCE_DIR}/os/freertos/include"
        "${NEXUS_SOURCE_DIR}/os/include" "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_freertos_stack_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/ext/freertos/include"
    "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
