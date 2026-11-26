# Shared public contracts execute the actual selected Native implementation.
nexus_google_test(nexus_os_portable_native_test
    SOURCES "${nexus_google_root}/os/portable_native_test.cpp"
        "${nexus_google_root}/os/portable_admission_test.cpp"
        "${nexus_google_root}/os/portable_consumer.c"
    LIBRARIES Nexus::OS Nexus::OSWait Threads::Threads
)
target_link_options(
    nexus_os_portable_native_test
    PRIVATE
        "-Wl,--wrap=pthread_cond_wait"
        "-Wl,--wrap=pthread_cond_timedwait"
        "-Wl,--wrap=pthread_tryjoin_np"
)

nexus_google_test(nexus_os_portable_native_process_test
    SOURCES "${nexus_google_root}/os/portable_native_process_test.cpp"
    LIBRARIES Nexus::OS Threads::Threads
)

# Transition regressions run the locked official ThreadX Linux scheduler.
nexus_google_test(nexus_os_portable_threadx_test
    SOURCES "${nexus_google_root}/os/portable_threadx_test.cpp"
        "${nexus_google_root}/os/portable_admission_test.cpp"
        "${nexus_google_root}/os/portable_consumer.c"
        "${NEXUS_SOURCE_DIR}/os/threadx/threadx.c"
        "${NEXUS_SOURCE_DIR}/tests/integration/kernels/os_threadx_runtime/clock.c"
    LIBRARIES contract_threadx_linux Nexus::OSWait Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/os/threadx/include"
)
_nexus_os_backend(nexus_os_portable_threadx_test threadx)

nexus_google_test(nexus_os_portable_freertos_test
    SOURCES "${nexus_google_root}/os/portable_freertos_test.cpp"
        "${nexus_google_root}/os/portable_admission_test.cpp"
        "${nexus_google_root}/os/portable_consumer.c"
    LIBRARIES contract_os_freertos_posix
)
