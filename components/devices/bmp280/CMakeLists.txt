# Explicit forced-conversion core and settled SPI transport.
# Author: Nexus Team
add_library(nexus_bmp280 STATIC src/bmp280.c)
target_include_directories(
    nexus_bmp280
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_bmp280
    PUBLIC nexus_core
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(nexus_bmp280_spi STATIC src/spi_transport.c)
target_link_libraries(
    nexus_bmp280_spi
    PUBLIC nexus_bmp280 Nexus::IOSPI
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(Nexus::BMP280 ALIAS nexus_bmp280)
add_library(Nexus::BMP280SPI ALIAS nexus_bmp280_spi)
