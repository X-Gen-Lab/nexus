# Execute real Microchip builders and executor through a physical-wire fixture.
# Author: Nexus Team
nexus_add_atecc608b()
nexus_google_test(nexus_common_atecc608b_test
    SOURCES "${nexus_google_root}/components/common_atecc608b_test.cpp"
    LIBRARIES Nexus::ATECC608B
)
