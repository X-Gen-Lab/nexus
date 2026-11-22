# Run the actual upstream protocol objects with external CAN hardware fixtures.
# Author: Nexus Team
nexus_add_canopen()
nexus_google_test(nexus_common_canopen_test
    SOURCES "${nexus_google_root}/components/common_canopen_test.cpp"
    LIBRARIES Nexus::CANopen
)
