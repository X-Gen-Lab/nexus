# Complete protocol behavior through the real typed polling SPI boundary.
nexus_google_test(nexus_w25_test SOURCES "${nexus_google_root}/components/w25_test.cpp"
    LIBRARIES Nexus::W25)
