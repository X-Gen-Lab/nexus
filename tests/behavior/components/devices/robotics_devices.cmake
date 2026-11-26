# Exact serial-sensor protocols use the actual production C implementations.
# Author: Nexus Team
nexus_google_test(nexus_as5047p_test
    SOURCES "${nexus_google_root}/components/devices/as5047p_test.cpp"
    LIBRARIES Nexus::AS5047P
)
nexus_google_test(nexus_ina226_test
    SOURCES "${nexus_google_root}/components/devices/ina226_test.cpp"
    LIBRARIES Nexus::INA226
)
