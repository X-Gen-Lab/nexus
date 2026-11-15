# Checked public faces remain independent from the private STM32 provider.
nexus_google_test(nexus_external_bus_api_test SOURCES "${nexus_google_root}/io/external_bus_api_test.cpp"
    LIBRARIES Nexus::IOExternalBus Nexus::IOMemoryWindow Nexus::IOParallel16)
