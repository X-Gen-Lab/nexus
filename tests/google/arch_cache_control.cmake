# Real cold-transition C with a narrow CPU/MMIO and stack-free leaf boundary.
foreach(profile m7 unsupported)
    set(reviewed 0)
    if(profile STREQUAL "m7")
        set(reviewed 1)
    endif()
    nexus_google_test(nexus_arch_cache_control_${profile}_test
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/arch_cache_control_test.cpp"
            "${NEXUS_SOURCE_DIR}/arch/arm/cortex_m/nx_arch_cache_control.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_compile_definitions(nexus_arch_cache_control_${profile}_test
        PRIVATE NEXUS_ARCH_MECHANISM_MODEL
            NEXUS_ARCH_CACHE_CONTROL_M7=${reviewed}
            NEXUS_ARCH_DCACHE_LINE_BYTES=32
            NEXUS_ARCH_ICACHE_LINE_BYTES=32
            NEXUS_ARCH_MPU_VERSION=0 NEXUS_ARCH_SECURITY_STATE=0
            NEXUS_ARCH_HAS_SAU=0 NEXUS_ARCH_HAS_DWT_CYCCNT=0)
endforeach()
