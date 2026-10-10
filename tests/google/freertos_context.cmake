# Real adapter with pinned kernel declarations and mocked external calls.
function(nexus_freertos_context_model name basepri irq_count)
    nexus_google_test(${name}
        SOURCES freertos_context_test.cpp
            "${NEXUS_SOURCE_DIR}/os/freertos/freertos.c"
            "${NEXUS_SOURCE_DIR}/core/src/time.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/os_freertos_runtime"
            "${NEXUS_SOURCE_DIR}/os/freertos/include"
            "${NEXUS_SOURCE_DIR}/os/include"
            "${NEXUS_SOURCE_DIR}/core/include"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
    target_compile_definitions(${name} PRIVATE NEXUS_FREERTOS_MODEL
        NEXUS_CPU_HAS_BASEPRI=${basepri}
        NEXUS_CPU_EXTERNAL_IRQ_COUNT=${irq_count})
    target_compile_options(${name} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/freertos_context_model.h")
endfunction()
nexus_freertos_context_model(nexus_freertos_context_test 1 240)
nexus_freertos_context_model(nexus_freertos_baseline_context_test 0 32)
target_compile_definitions(nexus_freertos_baseline_context_test PRIVATE
    __ARM_ARCH_6M__=1)
nexus_freertos_context_model(nexus_freertos_m23_context_test 0 240)
nexus_freertos_context_model(nexus_freertos_v8_context_test 1 480)
nexus_freertos_context_model(nexus_freertos_priority8_context_test 1 480)
target_include_directories(nexus_freertos_priority8_context_test BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/freertos_context_priority8")
