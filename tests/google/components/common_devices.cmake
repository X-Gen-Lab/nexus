# Final GPIO callback timing executes the real synchronous bus components.
# Author: Nexus Team
nexus_google_test(nexus_common_devices_test
    SOURCES "${nexus_google_root}/components/common_devices_test.cpp"
    LIBRARIES Nexus::SoftwareSPI Nexus::SoftwareI2C
)
