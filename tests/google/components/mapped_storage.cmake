# Production C11 engine and real coherent Native NOR provider, pinned GMock.
nexus_google_test(nexus_mapped_storage_test SOURCES "${nexus_google_root}/components/mapped_storage_test.cpp"
    LIBRARIES Nexus::NativeMappedStorage
)
