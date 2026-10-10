# Each host image compiles the production primitive algorithm. Register and
# instruction mocks remain test-only; these models do not execute an ARM CPU.
foreach(profile m0 m0plus m3 m4 m7 m23 m33)
    set(priority_mask 1)
    set(dwt_cyccnt 0)
    if(profile STREQUAL "m0" OR profile STREQUAL "m0plus")
        set(architecture_macro __ARM_ARCH_6M__)
        set(priority_mask 0)
    elseif(profile STREQUAL "m3")
        set(architecture_macro __ARM_ARCH_7M__)
        set(dwt_cyccnt 1)
    elseif(profile STREQUAL "m4" OR profile STREQUAL "m7")
        set(architecture_macro __ARM_ARCH_7EM__)
        set(dwt_cyccnt 1)
    elseif(profile STREQUAL "m23")
        set(architecture_macro __ARM_ARCH_8M_BASE__)
        set(priority_mask 0)
    else()
        set(architecture_macro __ARM_ARCH_8M_MAIN__)
    endif()
    set(name "nexus_arch_cortex_${profile}_test")
    nexus_google_test(${name} SOURCES arch_cortex_test.cpp
        "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_cortex_m.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_CORTEX_M_MODEL
        ${architecture_macro}=1
        NEXUS_ARCH_TEST_PRIORITY_MASK=${priority_mask}
        NEXUS_ARCH_HAS_DWT_CYCCNT=${dwt_cyccnt})
endforeach()
