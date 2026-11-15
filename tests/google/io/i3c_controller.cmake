# Actual C11 core; only external aggregate FIFO, clock and proof leaves mocked.
nexus_google_test(nexus_i3c_controller_test SOURCES "${nexus_google_root}/io/i3c_controller_test.cpp"
    LIBRARIES Nexus::I3CController)
