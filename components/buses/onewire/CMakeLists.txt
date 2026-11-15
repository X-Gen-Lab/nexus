# Optional static GPIO wire owner; no task, allocator or vendor dependency.
# Author: Nexus Team
add_library(nexus_onewire STATIC EXCLUDE_FROM_ALL src/onewire.c src/crc8.c)
nexus_apply_target_policy(nexus_onewire)
target_include_directories(nexus_onewire PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_onewire PUBLIC Nexus::IOGPIO
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::OneWire ALIAS nexus_onewire)
