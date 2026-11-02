# CMakeLists.txt
# Explicit settled device targets; unused code adds no image residency.
# Author: Nexus Team
add_library(nexus_tmp117 STATIC EXCLUDE_FROM_ALL src/tmp117.c)
nexus_apply_target_policy(nexus_tmp117)
target_include_directories(nexus_tmp117 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_tmp117 PUBLIC Nexus::IO
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::TMP117 ALIAS nexus_tmp117)

add_library(nexus_tmp117_i2c STATIC EXCLUDE_FROM_ALL src/i2c_transport.c)
nexus_apply_target_policy(nexus_tmp117_i2c)
target_link_libraries(nexus_tmp117_i2c PUBLIC Nexus::TMP117 Nexus::IO
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::TMP117I2C ALIAS nexus_tmp117_i2c)
