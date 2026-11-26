# Settled parallel-display behavior; firmware never links the host framework.
nexus_google_test(nexus_ili9341_test SOURCES "${nexus_google_root}/components/devices/ili9341_test.cpp"
    LIBRARIES Nexus::ILI9341
)
