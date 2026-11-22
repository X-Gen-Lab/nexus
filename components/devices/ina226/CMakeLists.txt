# Exact INA226 single-shot monitor, with caller-owned bus and scale.
# Author: Nexus Team
add_library(nexus_ina226 STATIC EXCLUDE_FROM_ALL src/ina226.c)
nexus_apply_target_policy(nexus_ina226)
target_include_directories(
    nexus_ina226
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_ina226
    PUBLIC Nexus::IOI2C
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::INA226 ALIAS nexus_ina226)
