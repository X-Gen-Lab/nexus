# Bounded RTU protocol with explicit register and settled RS485 ports.
# Author: Nexus Team
add_library(nexus_modbus STATIC src/modbus_rtu.c src/register_pdu.c)
target_include_directories(
    nexus_modbus
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private"
)
target_link_libraries(
    nexus_modbus
    PUBLIC nexus_abi_options
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(Nexus::ModbusRTU ALIAS nexus_modbus)
