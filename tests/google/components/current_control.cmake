# Production calibrated-current and observation-source contracts.
# Author: Nexus Team
nexus_google_test(nexus_current_control_test
    SOURCES "${nexus_google_root}/components/current_control_test.cpp"
    LIBRARIES Nexus::CurrentControl
)
