# Real adapter with pinned kernel declarations and mocked external calls.
nexus_google_test(nexus_freertos_context_test
    SOURCES freertos_context_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/freertos.c"
        "${NEXUS_SOURCE_DIR}/core/src/time.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/os_freertos_runtime"
        "${NEXUS_SOURCE_DIR}/os/freertos/include"
        "${NEXUS_SOURCE_DIR}/os/include"
        "${NEXUS_SOURCE_DIR}/core/include"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_freertos_context_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/ext/freertos/include"
    "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
target_compile_definitions(nexus_freertos_context_test PRIVATE
    NEXUS_FREERTOS_MODEL)
target_compile_options(nexus_freertos_context_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/freertos_context_model.h")
