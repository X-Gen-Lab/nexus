# Compile the real cache range algorithms with a narrow MMIO/CPU boundary.
# Each image has exact independent cache geometry; no host cache is exercised.
foreach(profile none data instruction both)
    set(data_line 0)
    set(instruction_line 0)
    if(profile STREQUAL "data" OR profile STREQUAL "both")
        set(data_line 32)
    endif()
    if(profile STREQUAL "instruction" OR profile STREQUAL "both")
        set(instruction_line 32)
    endif()
    set(name "nexus_arch_cache_${profile}_test")
    nexus_google_test(${name}
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/arch_cache_test.cpp"
        "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_cache.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_MECHANISM_MODEL
        NEXUS_ARCH_DCACHE_LINE_BYTES=${data_line}
        NEXUS_ARCH_ICACHE_LINE_BYTES=${instruction_line}
        NEXUS_ARCH_MPU_VERSION=0 NEXUS_ARCH_SECURITY_STATE=0
        NEXUS_ARCH_HAS_SAU=0 NEXUS_ARCH_HAS_DWT_CYCCNT=0)
endforeach()
