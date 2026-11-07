# Real synchronous GPIO transport; only external pin/clock boundaries mocked.
nexus_google_test(nexus_onewire_test
    SOURCES onewire_test.cpp
        "${NEXUS_SOURCE_DIR}/components/onewire/src/onewire.c"
        "${NEXUS_SOURCE_DIR}/components/onewire/src/crc8.c"
        "${NEXUS_SOURCE_DIR}/components/ds18b20/src/ds18b20.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/components/onewire/include"
        "${NEXUS_SOURCE_DIR}/components/ds18b20/include"
    LIBRARIES Nexus::IO)
