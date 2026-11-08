# Public CAN-FD contracts remain independent of physical controller support.
nexus_google_test(nexus_can_fd_contract_test
    SOURCES can_fd_contract_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
