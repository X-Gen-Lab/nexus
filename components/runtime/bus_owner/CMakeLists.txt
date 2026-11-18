# Explicit bounded queue and typed executors; no default task or object pool.
# Author: Nexus Team
add_library(nexus_bus_owner STATIC src/bus_owner.c)
target_include_directories(
    nexus_bus_owner
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_bus_owner
    PUBLIC nexus_core nexus_os_wait
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(nexus_spi_owner STATIC src/spi_owner.c)
target_link_libraries(
    nexus_spi_owner
    PUBLIC nexus_bus_owner Nexus::IOSPI
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(nexus_spi_async_owner STATIC src/spi_async_owner.c)
target_link_libraries(
    nexus_spi_async_owner
    PUBLIC nexus_bus_owner Nexus::IOSPI
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(nexus_spi_dma_owner STATIC src/spi_dma_owner.c)
target_link_libraries(
    nexus_spi_dma_owner
    PUBLIC nexus_bus_owner Nexus::IOSPI
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)
add_library(nexus_uart_owner STATIC src/uart_owner.c)
target_link_libraries(
    nexus_uart_owner
    PUBLIC nexus_bus_owner Nexus::IOUART
    PRIVATE nexus_owned_warnings nexus_build_options
)

add_library(Nexus::BusOwner ALIAS nexus_bus_owner)
add_library(Nexus::SpiOwner ALIAS nexus_spi_owner)
add_library(Nexus::SPIAsyncOwner ALIAS nexus_spi_async_owner)
add_library(Nexus::SPIDMATransactionOwner ALIAS nexus_spi_dma_owner)
add_library(Nexus::UARTOwner ALIAS nexus_uart_owner)
