# Device protocol tests execute production C/CRC with only wire ports mocked.
nexus_google_test(nexus_ds18b20_test
    SOURCES "${nexus_google_root}/components/ds18b20_test.cpp"
        "${NEXUS_SOURCE_DIR}/components/devices/ds18b20/src/ds18b20.c"
        "${NEXUS_SOURCE_DIR}/components/buses/onewire/src/crc8.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/components/devices/ds18b20/include"
        "${NEXUS_SOURCE_DIR}/components/buses/onewire/include"
    LIBRARIES Nexus::IO)
