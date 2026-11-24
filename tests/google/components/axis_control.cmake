# Axis contracts execute the production C component without hardware models.
# Author: Nexus Team
nexus_google_test(nexus_axis_control_test
    SOURCES "${nexus_google_root}/components/axis_control_test.cpp"
    LIBRARIES Nexus::AxisControl
)
