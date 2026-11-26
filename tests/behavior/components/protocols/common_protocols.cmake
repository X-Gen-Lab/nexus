# Contract tests for byte-stream protocols and managed network services.
# Author: Nexus Team
nexus_google_test(nexus_common_protocols_test
    SOURCES "${nexus_google_root}/components/protocols/common_protocols_test.cpp"
        "${nexus_google_root}/components/protocols/modbus_rtu_test.cpp"
    INCLUDES
        "${NEXUS_SOURCE_DIR}/components/protocols/modbus_tcp/include"
        "${NEXUS_SOURCE_DIR}/components/protocols/modbus/include"
        "${NEXUS_SOURCE_DIR}/components/protocols/framing/include"
        "${NEXUS_SOURCE_DIR}/components/protocols/wall_clock/include"
        "${NEXUS_SOURCE_DIR}/components/protocols/xmodem/include"
        "${NEXUS_SOURCE_DIR}/components/protocols/at_client/include"
        "${NEXUS_SOURCE_DIR}/components/protocols/network/include"
    LIBRARIES Nexus::ModbusTCP Nexus::ModbusRTU Nexus::Framing Nexus::WallClock
        Nexus::XModem Nexus::ATClient
)

nexus_add_lwip()
nexus_google_test(nexus_common_network_services_test
    SOURCES "${nexus_google_root}/components/protocols/common_network_services_test.cpp"
    LIBRARIES Nexus::LwIPManagedServicesStorage Nexus::LwIPManagedTCPStorage
)
