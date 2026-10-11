# Production Secure leases: model only CPU registers and reviewed ASM boundary.
foreach(name nexus_os_secure_context_test nexus_os_secure_fp_context_test)
    nexus_google_test(${name} SOURCES os_secure_context_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/secure/context.c"
        "${NEXUS_SOURCE_DIR}/os/freertos/secure/init.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/google/secure_profile"
            "${NEXUS_SOURCE_DIR}/tests/google"
            "${NEXUS_SOURCE_DIR}/os/freertos/include"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/core/include")
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/GCC/ARM_CM33/secure")
    target_compile_definitions(${name} PRIVATE NEXUS_FREERTOS_SECURE_MODEL)
endforeach()
target_compile_definitions(nexus_os_secure_fp_context_test PRIVATE
    NEXUS_SECURE_TEST_FP_BANK=1)
