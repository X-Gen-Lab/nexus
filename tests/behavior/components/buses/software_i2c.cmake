# External GPIO wire models exercise the production software I2C/SCCB owner.
# Author: Nexus Team
nexus_google_test(nexus_software_i2c_test SOURCES "${nexus_google_root}/components/buses/software_i2c_test.cpp"
    LIBRARIES Nexus::SoftwareI2C
)
