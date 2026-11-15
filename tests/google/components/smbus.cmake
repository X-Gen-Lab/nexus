# Packet PEC executes the production C adapter against external I2C mocks.
# Author: Nexus Team
nexus_google_test(nexus_smbus_test SOURCES "${nexus_google_root}/components/smbus_test.cpp"
    LIBRARIES Nexus::SMBus)
