# Real adapter with pinned kernel declarations and mocked external calls.
function(nexus_freertos_context_model name basepri irq_count common_contracts)
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
        NEXUS_OS_STACK_ALIGNMENT_BYTES=8
        NEXUS_CPU_HAS_BASEPRI=${basepri}
        NEXUS_CPU_EXTERNAL_IRQ_COUNT=${irq_count}
        NEXUS_FREERTOS_COMMON_CONTRACTS=${common_contracts})
    if(basepri EQUAL 0)
        target_compile_definitions(${name} PRIVATE NEXUS_OS_MIN_STACK_WORDS=20)
    else()
        target_compile_definitions(${name} PRIVATE NEXUS_OS_MIN_STACK_WORDS=18)
    endif()
    target_compile_options(${name} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/freertos_context_model.h")
endfunction()
nexus_freertos_context_model(nexus_freertos_context_test 1 240 1)
nexus_freertos_context_model(nexus_freertos_baseline_context_test 0 32 1)
target_compile_definitions(nexus_freertos_baseline_context_test PRIVATE
    __ARM_ARCH_6M__=1)
nexus_freertos_context_model(nexus_freertos_m23_context_test 0 240 0)
nexus_freertos_context_model(nexus_freertos_v8_context_test 1 480 0)
nexus_freertos_context_model(nexus_freertos_priority8_context_test 1 480 0)
target_include_directories(nexus_freertos_priority8_context_test BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/freertos_context_priority8")

nexus_freertos_context_model(nexus_freertos_notifications_disabled_test 1 240 0)
target_include_directories(nexus_freertos_notifications_disabled_test
    BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/freertos_notifications_disabled")
