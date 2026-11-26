# Locked real supplier body; models expose only settled SCCB and time boundaries.
# Author: Nexus Team
nexus_add_ov2640()
nexus_google_test(
    nexus_ov2640_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/ov2640_test.cpp"
    LIBRARIES Nexus::OV2640
)
