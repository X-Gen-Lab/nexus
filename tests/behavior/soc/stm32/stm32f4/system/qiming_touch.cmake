# Behavioral GPIO wire and exact XPT2046 contract models.
# Author: Nexus Team
nexus_google_test(nexus_software_spi_test SOURCES "${nexus_google_root}/components/buses/software_spi_test.cpp"
    LIBRARIES Nexus::SoftwareSPI
)
nexus_google_test(nexus_xpt2046_test SOURCES "${nexus_google_root}/components/devices/xpt2046_test.cpp"
    LIBRARIES Nexus::XPT2046
)
