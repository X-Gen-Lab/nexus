# Public C11 control consumer, identical for source and installed SDKs.
# Author: Nexus Team
cmake_minimum_required(VERSION 3.31)
project(nexus_motor_control_consumer LANGUAGES C ASM)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/native.toml"
    CACHE FILEPATH
    "Exact consumer-owned platform assembly"
)
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(nexus_motor_control_consumer
    SOURCES main.c
    LIBRARIES Nexus::MotorControl Nexus::AxisControl
)
nexus_target_warnings(nexus_motor_control_consumer STRICT)
