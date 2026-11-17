# Public CAN-FD contracts remain independent of physical controller support.
nexus_google_test(nexus_can_fd_contract_test
    SOURCES "${nexus_google_root}/soc/can_fd_contract_test.cpp"
    LIBRARIES Nexus::IO Nexus::Arch
)
include("${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sdk/STM32Official.cmake")
