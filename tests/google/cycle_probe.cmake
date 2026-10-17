# Optional observation exercises the real CPU algorithm and external leaf model.
nexus_google_test(nexus_cycle_probe_test
    SOURCES cycle_probe_test.cpp
        "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_cortex_m.c"
    LIBRARIES Nexus::IOCycleProbe
    INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_compile_definitions(nexus_cycle_probe_test PRIVATE
    NEXUS_ARCH_CORTEX_M_MODEL __ARM_ARCH_7EM__=1
    NEXUS_ARCH_TEST_PRIORITY_MASK=1 NEXUS_ARCH_HAS_DWT_CYCCNT=1)

nexus_google_test(nexus_cycle_probe_disabled_test
    SOURCES cycle_probe_disabled_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_cycle_probe_native_test
    SOURCES cycle_probe_native_test.cpp
    LIBRARIES Nexus::IOCycleProbe Nexus::Arch)
