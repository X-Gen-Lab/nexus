# Real production C11 implementation; only cache/hardware/ownership ports mocked.
nexus_google_test(nexus_amp_ipc_test SOURCES "${nexus_google_root}/components/amp_ipc_test.cpp"
    LIBRARIES Nexus::AMPIPC
)
