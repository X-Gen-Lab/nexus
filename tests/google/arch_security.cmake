# Domain-negative models execute the same public APIs without Secure MMIO.
foreach(profile single secure secure-no-sau nonsecure)
    set(security_state 0)
    set(sau 0)
    set(cmse 1)
    if(profile MATCHES "^secure")
        set(security_state 1)
        set(cmse 3)
        if(profile STREQUAL "secure")
            set(sau 1)
        endif()
    elseif(profile STREQUAL "nonsecure")
        set(security_state 2)
    endif()
    set(name "nexus_arch_security_${profile}_test")
    nexus_google_test(${name} SOURCES arch_security_test.cpp
        "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_security.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_MECHANISM_MODEL
        NEXUS_ARCH_DCACHE_LINE_BYTES=0 NEXUS_ARCH_ICACHE_LINE_BYTES=0
        NEXUS_ARCH_MPU_VERSION=0 NEXUS_ARCH_SECURITY_STATE=${security_state}
        NEXUS_ARCH_HAS_SAU=${sau} NEXUS_ARCH_HAS_DWT_CYCCNT=0
        __ARM_FEATURE_CMSE=${cmse})
endforeach()
