# Production bounded Task/IRQ exchanges and explicit controller composition.
# Author: Nexus Team
nexus_google_test(nexus_drive_control_test
    SOURCES "${nexus_google_root}/components/drive_control_test.cpp"
    LIBRARIES Nexus::DriveControl
)
