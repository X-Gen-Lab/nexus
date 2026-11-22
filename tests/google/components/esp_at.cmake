# Module framing/commands execute real C over a copying UART boundary model.
# Author: Nexus Team
nexus_google_test(nexus_esp_at_test
    SOURCES "${nexus_google_root}/components/esp_at_test.cpp"
    LIBRARIES Nexus::ESPAT Nexus::UARTByteStream
)
