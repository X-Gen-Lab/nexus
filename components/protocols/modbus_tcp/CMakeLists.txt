# Explicit caller-owned protocol mechanism; no automatic execution.
# Author: Nexus Team
add_library(nexus_modbus_tcp STATIC EXCLUDE_FROM_ALL src/modbus_tcp.c)
target_include_directories(
    nexus_modbus_tcp
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../modbus/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../modbus/private"
)
target_link_libraries(
    nexus_modbus_tcp
    PUBLIC Nexus::Core
    PRIVATE nexus_modbus nexus_build_options nexus_owned_warnings
)
add_library(Nexus::ModbusTCP ALIAS nexus_modbus_tcp)
nexus_apply_target_policy(nexus_modbus_tcp)
