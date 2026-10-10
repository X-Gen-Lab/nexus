# Real production MPU encoders/programming with a test-only MMIO boundary.
foreach(version 0 7 8)
    set(name "nexus_arch_mpu_v${version}_test")
    nexus_google_test(${name} SOURCES arch_mpu_test.cpp
        "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_mpu.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_MECHANISM_MODEL
        NEXUS_ARCH_MPU_VERSION=${version}
        NEXUS_ARCH_SECURITY_STATE=0 NEXUS_ARCH_HAS_SAU=0
        NEXUS_ARCH_HAS_DWT_CYCCNT=0
        NEXUS_ARCH_DCACHE_LINE_BYTES=0 NEXUS_ARCH_ICACHE_LINE_BYTES=0)
endforeach()
