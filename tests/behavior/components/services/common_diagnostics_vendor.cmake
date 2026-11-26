# Actual pinned parsers/transports; no substitute vendor algorithms or mocks.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/components/services/rtt/Select.cmake")
include("${NEXUS_SOURCE_DIR}/components/services/shell/Select.cmake")
nexus_add_rtt()
nexus_add_shell()
nexus_google_test(nexus_common_diagnostics_vendor_test
    SOURCES
        "${nexus_google_root}/components/services/common_rtt_test.cpp"
        "${nexus_google_root}/components/services/common_shell_test.cpp"
        "${nexus_google_root}/components/services/common_fault_m4_test.cpp"
        "${nexus_google_root}/components/services/common_health_resources_test.cpp"
    LIBRARIES Nexus::RTT Nexus::ShellStorage Nexus::Fault Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/ext/segger-rtt/RTT"
        "${NEXUS_SOURCE_DIR}/components/services/rtt/profile"
)
