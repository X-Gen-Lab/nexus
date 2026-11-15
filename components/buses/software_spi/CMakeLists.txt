# Optional caller-owned GPIO SPI; unselected sources have no image residency.
# Author: Nexus Team
add_library(nexus_software_spi STATIC EXCLUDE_FROM_ALL src/software_spi.c)
nexus_apply_target_policy(nexus_software_spi)
target_include_directories(nexus_software_spi PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_software_spi PUBLIC Nexus::IOSPI Nexus::IOGPIO
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::SoftwareSPI ALIAS nexus_software_spi)
