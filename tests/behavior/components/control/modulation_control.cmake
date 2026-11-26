# Production integer-carrier plans and current-observability contracts.
# Author: Nexus Team
nexus_google_test(nexus_modulation_control_test
    SOURCES "${nexus_google_root}/components/control/modulation_control_test.cpp"
    LIBRARIES Nexus::ModulationControl
)
