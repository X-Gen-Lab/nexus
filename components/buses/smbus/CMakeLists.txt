# Optional packet PEC over a preconfigured asynchronous I2C endpoint.
# Author: Nexus Team
add_library(nexus_smbus STATIC EXCLUDE_FROM_ALL src/smbus.c)
nexus_apply_target_policy(nexus_smbus)
target_include_directories(nexus_smbus PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_smbus PUBLIC Nexus::IOI2C
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::SMBus ALIAS nexus_smbus)
