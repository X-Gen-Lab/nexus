# Host model substitutes only the CPU mask boundary of actual M0 algorithms.
nexus_google_test(nexus_atomic_m0_test SOURCES "${nexus_google_root}/core/atomic_fallback_test.cpp"
    "${NEXUS_SOURCE_DIR}/os/baremetal/notify.c"
    "${NEXUS_SOURCE_DIR}/components/services/log/src/log.c"
    LIBRARIES Nexus::OSWait
    INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/components/services/log/include"
)
target_compile_definitions(nexus_atomic_m0_test PRIVATE __ARM_ARCH_6M__=1)

find_package(Threads REQUIRED)
nexus_google_test(nexus_atomic_native_test SOURCES "${nexus_google_root}/core/atomic_native_test.cpp"
    LIBRARIES Nexus::Arch Nexus::Log Threads::Threads
)
