# Real C cold preparation; generated code contributes immutable connections.
nexus_google_test(nexus_native_cold_test SOURCES native_cold_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
