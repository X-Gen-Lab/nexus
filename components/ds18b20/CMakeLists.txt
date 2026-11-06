# Exact external-VDD sensor protocol; applications own scheduling and policy.
# Author: Nexus Team
add_library(nexus_ds18b20 STATIC EXCLUDE_FROM_ALL src/ds18b20.c)
nexus_apply_target_policy(nexus_ds18b20)
target_include_directories(nexus_ds18b20 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_ds18b20 PUBLIC Nexus::OneWire
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::DS18B20 ALIAS nexus_ds18b20)
