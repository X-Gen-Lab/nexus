# Native C programs and cross-project fixtures share the production graph.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/tests/cmake/IntegrationTest.cmake")
find_package(Threads REQUIRED)

nexus_c_integration_test(contract_core_contract
    SOURCES c_api/core_contract_test.c
    LIBRARIES Nexus::Core Nexus::Arch Threads::Threads
)
nexus_c_integration_test(contract_native_io
    SOURCES c_api/native_io_test.c LIBRARIES Nexus::NativeModel
)
nexus_c_integration_test(contract_os_wait
    SOURCES kernels/os_wait.c LIBRARIES nexus_os_baremetal nexus_os_native
)
nexus_c_integration_test(contract_bus_owner
    SOURCES c_api/components_owner.c
    LIBRARIES nexus_bus_owner nexus_os_native Nexus::NativeModel
)
nexus_c_integration_test(contract_storage
    SOURCES protocols/storage/components_storage.c LIBRARIES nexus_file_flash
)
nexus_c_integration_test(contract_flash_storage
    SOURCES c_api/components_flash_storage.c
    LIBRARIES Nexus::FlashStorage Nexus::NativeModel
)
nexus_c_integration_test(contract_spi_adapter
    SOURCES c_api/components_spi.c
    LIBRARIES Nexus::BMP280SPI Nexus::SpiOwner Nexus::OSNative Nexus::NativeModel
)
nexus_c_integration_test(contract_uart_adapter
    SOURCES c_api/components_uart.c
    LIBRARIES Nexus::UARTOwner Nexus::OSNative Nexus::NativeModel
)

set_property(TEST contract_core_contract APPEND PROPERTY LABELS core)
set_property(TEST contract_native_io APPEND PROPERTY LABELS io)
set_property(TEST contract_os_wait APPEND PROPERTY LABELS os)
set_property(TEST contract_bus_owner APPEND PROPERTY LABELS runtime)
set_property(
    TEST contract_storage contract_flash_storage
    APPEND
    PROPERTY LABELS storage
)
set_property(TEST contract_spi_adapter APPEND PROPERTY LABELS spi)
set_property(TEST contract_uart_adapter APPEND PROPERTY LABELS uart)

add_subdirectory(kernels/os_freertos_runtime)
add_subdirectory(providers/stm32_model)
