# The production mediation uses the existing real BusOwner/UARTOwner/Core.
# Mocks supply only the external kernel copy/authority and UART provider ports.
nexus_google_test(nexus_user_io_async_test SOURCES user_io_async_test.cpp
    LIBRARIES Nexus::UserIoBroker Nexus::Arch)
