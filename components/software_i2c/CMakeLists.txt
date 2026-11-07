# Bounded GPIO wire owner, with separate strict I2C and SCCB entry points.
# Author: Nexus Team
add_library(nexus_software_i2c STATIC EXCLUDE_FROM_ALL src/software_i2c.c)
nexus_apply_target_policy(nexus_software_i2c)
target_include_directories(nexus_software_i2c PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_software_i2c PUBLIC Nexus::IOI2C Nexus::IOGPIO
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::SoftwareI2C ALIAS nexus_software_i2c)
add_library(Nexus::SoftwareSCCB ALIAS nexus_software_i2c)
