# Raw capture/health/format contracts before optional vendor integrations.
# Author: Nexus Team
nexus_google_test(nexus_common_diagnostics_test
    SOURCES
        "${nexus_google_root}/components/common_diagnostics_test.cpp"
        "${nexus_google_root}/components/common_log_uart_test.cpp"
        "${nexus_google_root}/components/common_diagnostic_log_test.cpp"
        "${nexus_google_root}/components/common_fault_storage_test.cpp"
    LIBRARIES Nexus::LogFormatter Nexus::LogUART Nexus::Fault
        Nexus::FaultStorage Nexus::Health Nexus::DiagnosticLog
        Nexus::FlashStorage
    INCLUDES
        "${NEXUS_SOURCE_DIR}/components/services/fault/include"
        "${NEXUS_SOURCE_DIR}/components/services/health/include"
)
