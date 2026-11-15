# Optional exact EEPROM component; no allocation, scheduler or vendor headers.
# Author: Nexus Team
add_library(nexus_at24c02 STATIC EXCLUDE_FROM_ALL src/at24c02.c)
nexus_apply_target_policy(nexus_at24c02)
target_include_directories(nexus_at24c02 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_at24c02 PUBLIC Nexus::IOI2C
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::AT24C02 ALIAS nexus_at24c02)
