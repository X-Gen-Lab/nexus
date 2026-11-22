# Ownership translation executes the production adapter and request helpers.
# Author: Nexus Team
nexus_google_test(nexus_uart_byte_stream_test
    SOURCES "${nexus_google_root}/components/uart_byte_stream_test.cpp"
    LIBRARIES Nexus::UARTByteStream
)
