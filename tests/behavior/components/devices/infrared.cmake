# Real deterministic NEC decoder; GoogleMock replaces only the EXTI boundary.
# Author: Nexus Team
nexus_google_test(nexus_infrared_test SOURCES "${nexus_google_root}/components/devices/infrared_test.cpp"
    LIBRARIES Nexus::Infrared
)
