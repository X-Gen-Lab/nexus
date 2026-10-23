# Execute actual permission-admission code at the external CMSE boundary.
foreach(profile native single nonsecure secure secure-no-sau)
    set(security_state 0)
    set(sau 0)
    set(cmse 0)
    if(profile MATCHES "^secure")
        set(security_state 1)
        set(cmse 3)
        if(profile STREQUAL "secure")
            set(sau 1)
        endif()
    elseif(profile STREQUAL "nonsecure")
        set(security_state 2)
        set(cmse 1)
    elseif(profile STREQUAL "single")
        set(cmse 1)
    endif()
    set(name "nexus_arch_nonsecure_${profile}_test")
    nexus_google_test(${name} SOURCES arch_nonsecure_test.cpp
        arch_nonsecure_probe.c
        "${NEXUS_SOURCE_DIR}/arch/arm/cortex_m/nx_arch_nonsecure.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_MECHANISM_MODEL
        NEXUS_ARCH_DCACHE_LINE_BYTES=0 NEXUS_ARCH_ICACHE_LINE_BYTES=0
        NEXUS_ARCH_MPU_VERSION=0 NEXUS_ARCH_SECURITY_STATE=${security_state}
        NEXUS_ARCH_HAS_SAU=${sau} NEXUS_ARCH_HAS_DWT_CYCCNT=0
        __ARM_FEATURE_CMSE=${cmse})
endforeach()
